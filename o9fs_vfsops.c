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
		return (EOPNOTSUPP);

	error = copyin(data, &args, sizeof(args));
	if (error)
		return (error);

	error = copyinstr(path, root, MNAMELEN - 1, &len);
	if (error)
		return (error);
	bzero(&root[len], MNAMELEN - len);
	
	error = copyinstr(args.hostname, host, MNAMELEN - 1, &len);
	if (error)
		return (error);
	bzero(&host[len], MNAMELEN - len);
	
	error = mounto9fs(&args, mp, root, host);
	return (error);
}

int
mounto9fs(struct o9fs_args *args, struct mount *mp, 
			char *node, char *host)
{
	struct o9fsmount *omnt;
	struct o9fsnode *rnode;
	struct vnode *rvp;
	struct o9fsfid *fid;
	int error;

	error = 0;

	omnt = (struct o9fsmount *) malloc(sizeof(struct o9fsmount), M_MISCFSMNT, M_WAITOK);
	if (omnt == NULL)
		return (ENOSPC);

	rnode = (struct o9fsnode *) malloc(sizeof(struct o9fsnode), M_MISCFSMNT, M_WAITOK);
	if (rnode == NULL)
		return (ENOSPC);
	
	error = getnewvnode(VT_O9FS, mp, o9fs_vnodeop_p, &rvp);
	if (error)
		return (error);

	rnode->on_vnode = rvp;
	rvp->v_flag |= VROOT;
	rvp->v_type = VDIR;
	
	omnt->om_root = rnode;
	omnt->om_mp = mp;
	omnt->om_saddr = args->saddr;
	omnt->om_saddrlen = args->saddrlen;
	omnt->om_o9fs.nextfid = 1;
	omnt->om_o9fs.freefid = NULL;
/*	pool_init(&omnt->om_o9fs.freefid, sizeof(struct o9fsfid), 
		0, 0, 0, "o9fsfid", &pool_allocator_nointr); */
	
	/* XXX io must be transparent */
	omnt->io = &io_tcp;

	mp->mnt_data = (qaddr_t) omnt;
	vfs_getnewfsid(mp);

	bcopy(host, mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy(node, mp->mnt_stat.f_mntonname, MNAMELEN);

	error = omnt->io->open(omnt);

	error = o9fs_tversion(omnt, 8192, "9P2000");
	if (error)
		return (error);
		
	fid = o9fs_tattach(omnt, 0, "iru", 0);
	if (fid == NULL)
		return (EIO);
	omnt->om_o9fs.rootfid = fid;

 	o9fs_tstat(omnt, "/shit"); 
	o9fs_tstat(omnt, "/etc/X11"); 
	o9fs_tstat(omnt, "/etc/shit");
	
	return (error);
}
	
int
o9fs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct proc *p;
	
	p = curproc;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOO9FS(mp)->om_root->on_vnode;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;

	return (0);
}

/* this seems weird to me. i based much of the work on
 * portal_unmount. need to check how to do it correctly.
 */
int
o9fs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct o9fsmount *omnt;
	struct vnode *vp;
	int error, flags;
	
	flags = 0;
	omnt = VFSTOO9FS(mp);

	vp = omnt->om_root->on_vnode;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	error = vflush(mp, NULL, flags);
	if (error)
		return (error);

	omnt->io->close(omnt);
	
	free(omnt, M_MISCFSMNT);
	omnt = NULL;
	
	return (0);
}

int
o9fs_start(struct mount *mp, int flags, struct proc *p)
{
	return (0);
}

int
o9fs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
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
	return (0);
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

