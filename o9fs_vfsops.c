#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/file.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

#define o9fs_init ((int (*)(struct vfsconf *))nullop)

int o9fs_mount(struct mount *, const char *, void *, struct nameidata *, struct proc *);
int o9fs_unmount(struct mount *, int, struct proc *);
int o9fs_statfs(struct mount *, struct statfs *, struct proc *);
int o9fs_start(struct mount *, int, struct proc *);
int o9fs_root(struct mount *, struct vnode **);
int	mounto9fs(struct o9fs_args *, struct mount *, char *, char *, struct file *);

static long
o9fs_version(struct o9fsmount *omnt, char *version, int msize)
{
	struct o9fsfcall f, rx;
	
	f.type = O9FS_TVERSION;
	f.tag = O9FS_NOTAG;
	f.msize = msize;
	f.version = version;

	if ((o9fs_rpc(omnt, &f, &rx)) == -1)
		return 0;

	return omnt->om_o9fs.msize = rx.msize;
}

static struct o9fsfid *
o9fs_auth(struct o9fsmount *omnt, char *user, char *aname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *afid;
	int error;

	if (aname == NULL)
		aname = "";

	tx.type = O9FS_TAUTH;
	afid = o9fs_getfid(omnt);

	tx.afid = afid ? afid->fid : O9FS_NOFID;
	tx.uname = user;
	tx.aname = aname;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error) {
		o9fs_putfid(omnt, afid);
		return NULL;
	}
	
	afid->qid = rx.qid;
	return afid;
}

struct o9fsfid *
o9fs_attach(struct o9fsmount *omnt, struct o9fsfid *afid,
			char *user, char *aname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *fid;
	int error;

	error = 0;
	
	if (aname == NULL)
		aname = "";
	
	fid = o9fs_getfid(omnt);
	
	tx.type = O9FS_TATTACH;
	tx.afid = afid ? afid->fid : O9FS_NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error) {
		o9fs_putfid(omnt, fid);
		return NULL;
	}

	fid->qid = rx.qid;
	return fid;
}
	
int
o9fs_mount(struct mount *mp, const char *path, void *data, 
		struct nameidata *ndp, struct proc *p)
{
	struct o9fs_args args;
	char host[MNAMELEN], root[MNAMELEN];
	int error;
	size_t len;
	struct file *fp;

	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	error = copyin(data, &args, sizeof(args));
	if (error)
		return error;

	error = copyinstr(path, root, MNAMELEN - 1, &len);
	if (error)
		return error;
	bzero(&root[len], MNAMELEN - len);
	
	error = copyinstr(args.hostname, host, MNAMELEN - 1, &len);
	if (error)
		return error;
	bzero(&host[len], MNAMELEN - len);
	
	if ((fp = fd_getfile(p->p_fd, args.fd)) == NULL)
		return EBADF;
	fp->f_count++;
	FREF(fp);

	error = mounto9fs(&args, mp, root, host, fp);
	return error;
}

int
mounto9fs(struct o9fs_args *args, struct mount *mp, char *path, char *host, 
	struct file *fp)
{
	struct o9fsmount *omnt;
	struct vnode *rvp;
	struct o9fsfid *fid;
	struct o9fsstat *stat; 
	int error;

	error = 0;
	omnt = (struct o9fsmount *) malloc(sizeof(struct o9fsmount), M_MISCFSMNT, M_WAITOK);

	error = getnewvnode(VT_O9FS, mp, o9fs_vnodeop_p, &rvp);
	if (error)
		return (error);

	omnt->om_root = rvp;
	omnt->om_mp = mp;
	omnt->om_fp = fp;
	omnt->om_o9fs.nextfid = 0;
	omnt->om_o9fs.freefid = NULL;

	omnt->om_root->v_type = VDIR;
	omnt->om_root->v_flag = VROOT;
	
	mp->mnt_data = (qaddr_t) omnt;
	vfs_getnewfsid(mp);

	bcopy(host, mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy(path, mp->mnt_stat.f_mntonname, MNAMELEN);

	omnt->om_o9fs.rpc = malloc(8192, M_O9FS, M_WAITOK);
	if (!o9fs_version(omnt, O9FS_VERSION9P, 8192))
		return EIO;

	fid = o9fs_attach(omnt, o9fs_auth(omnt, "none", ""), "iru", "");
	if (fid == NULL)
		return EIO;

	omnt->om_o9fs.rootfid = fid;
//	omnt->om_o9fs.rootfid->stat = stat;
	omnt->om_root->v_data = fid;
	error = o9fs_allocvp(omnt->om_mp, fid, &omnt->om_root, VROOT);

	return error;
}
	
int
o9fs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct proc *p;
	struct o9fsfid *f;
	struct o9fsstat *stat;
	struct o9fsmount *omnt;
	int error;

	p = curproc;
	omnt = VFSTOO9FS(mp);

/*	vp = VFSTOO9FS(mp)->om_root;
	vref(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;*/

	f = o9fs_fclone(omnt, VTO9(omnt->om_root));
	if (f == NULL)
		return -1;
//	f->stat = o9fs_fstat(omnt, f);
	if(error = o9fs_allocvp(omnt->om_mp, f, &vp, VROOT))
		return error;
	vp->v_flag = VROOT;
	vp->v_type = VDIR;
	*vpp = vp; 
	return 0;
}

/* this seems weird to me. i based much of the work on
 * portal_unmount. need to check how to do it correctly.
 */
int
o9fs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct o9fsmount *omnt;
	struct vnode *vp;
	struct o9fsfid *f;
	struct file *fp;
	int error, flags;
	
	flags = 0;
	omnt = VFSTOO9FS(mp);
	fp = omnt->om_fp;

	vp = omnt->om_root;
	f = VTO9(vp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	error = vflush(mp, vp, flags);
	if (error)
		return (error);

	vput(vp);
	o9fs_fidclunk(omnt, f);
	o9fs_putfid(omnt, f);
	fp->f_count--;
	FRELE(fp);
	free(omnt->om_o9fs.rpc, M_O9FS);
	free(omnt, M_MISCFSMNT);
	omnt = mp->mnt_data = (qaddr_t)0;
	
	return 0;
}

int
o9fs_start(struct mount *mp, int flags, struct proc *p)
{
	return 0;
}

int
o9fs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = VFSTOO9FS(mp)->om_o9fs.msize;
	sbp->f_blocks = 2;              /* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;               /* Allow for "." */
	sbp->f_ffree = 0;               /* See comments above */
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strlcpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);

	return 0;
}


#define o9fs_sync ((int (*)(struct mount *, int, struct ucred *, \
                                  struct proc *))nullop)
#define o9fs_fhtovp ((int (*)(struct mount *, struct fid *, \
            struct vnode **))eopnotsupp)
#define o9fs_quotactl ((int (*)(struct mount *, int, uid_t, caddr_t, \
            struct proc *))eopnotsupp)
#define o9fs_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
            size_t, struct proc *))eopnotsupp)
#define o9fs_vget ((int (*)(struct mount *, ino_t, struct vnode **)) \
            eopnotsupp)
#define o9fs_vptofh ((int (*)(struct vnode *, struct fid *))eopnotsupp)
#define o9fs_checkexp ((int (*)(struct mount *, struct mbuf *,        \
        int *, struct ucred **))eopnotsupp)

const struct vfsops o9fs_vfsops = {
        o9fs_mount,
        o9fs_start,
        o9fs_unmount,
        o9fs_root,
        o9fs_quotactl,
        o9fs_statfs,
        o9fs_sync,
        o9fs_vget,
        o9fs_fhtovp,
        o9fs_vptofh,
        o9fs_init,
        o9fs_sysctl,
        o9fs_checkexp
};

