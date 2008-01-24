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

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

#define o9fs_init ((int (*)(struct vfsconf *))nullop)

int o9fs_mount(struct mount *, const char *, void *, struct nameidata *, struct proc *);
int o9fs_unmount(struct mount *, int, struct proc *);
int o9fs_statfs(struct mount *, struct statfs *, struct proc *);
int o9fs_start(struct mount *, int, struct proc *);
int o9fs_root(struct mount *, struct vnode **);
int	mounto9fs(struct o9fs_args *, struct mount *, char *, char *);

int
o9fs_mount(struct mount *mp, const char *path, void *data, 
		struct nameidata *ndp, struct proc *p)
{
	struct o9fs_args args;
	char host[MNAMELEN], root[MNAMELEN];
	int error;
	size_t len;

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
	
	error = mounto9fs(&args, mp, root, host);
	return error;
}

int
mounto9fs(struct o9fs_args *args, struct mount *mp, char *path, char *host)
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

	rvp->v_flag |= VROOT;
	rvp->v_type = VDIR;
	
	omnt->om_root = rvp;
	omnt->om_mp = mp;
	omnt->om_saddr = args->saddr;
	omnt->om_saddrlen = args->saddrlen;
	omnt->om_o9fs.nextfid = 0;
	omnt->om_o9fs.freefid = NULL;
	
	/* XXX net io must be transparent */
	omnt->io = &io_tcp;

	mp->mnt_data = (qaddr_t) omnt;
	vfs_getnewfsid(mp);

	bcopy(host, mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy(path, mp->mnt_stat.f_mntonname, MNAMELEN);

	error = omnt->io->connect(omnt);
	if (error) {
		omnt->io->close(omnt);
		return error;
	}

	error = o9fs_tversion(omnt, 8192, O9FS_VERSION9P);
	if (error)
		return error;
	
	fid = o9fs_tattach(omnt, o9fs_tauth(omnt, "iru", ""), "iru", "");
	if (fid == NULL)
		return EIO;

	stat = o9fs_fstat(omnt, fid);
	if (stat == NULL)
		return EIO;

	omnt->om_o9fs.rootfid = fid;
	omnt->om_o9fs.rootfid->stat = stat;
	omnt->om_root->v_data = fid;
	error = o9fs_allocvp(omnt->om_mp, fid, &omnt->om_root, VROOT);

	return error;
}
	
int
o9fs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct proc *p;
//	struct o9fsfid *f;
//	struct o9fsstat *stat;
	struct o9fsmount *omnt;
	printf(">o9fs_root\n");
	p = curproc;
	omnt = VFSTOO9FS(mp);

/*
	f = o9fs_twalk(omnt, 0, "/usr");
	if (f == NULL) {
		printf("could not walk to root fid\n");
		return -1;
	}
	printf("root walk ok\n");

	stat = o9fs_fstat(omnt, f);
	if (stat == NULL) {
		printf("could not stat root fid\n");
		return -1;
	}
	printf("root stat ok\n");
	
	* return locked reference to root *
	if ((o9fs_allocvp(mp, f, &vp, 0)) < 0) {
		printf("could not alloc a vnode for root fid\n");
		return -1;
	}

	* XXX hack *
	vp->v_type = VDIR;

	*vpp = vp; */
	vp = VFSTOO9FS(mp)->om_root;
	vref(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	printf("<o9fs_root\n");
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
	int error, flags;
	
	flags = 0;
	omnt = VFSTOO9FS(mp);

	vp = omnt->om_root;
	f = VTO9(vp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	o9fs_fidclunk(omnt, f);
	error = vflush(mp, NULL, flags);
	if (error)
		return (error);

	omnt->io->close(omnt);

	free(omnt, M_MISCFSMNT);
	omnt = NULL;
	
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

