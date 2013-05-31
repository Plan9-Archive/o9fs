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
#include <sys/queue.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 0,
};

#define o9fs_init ((int (*)(struct vfsconf *))nullop)

int o9fs_mount(struct mount *, const char *, void *, struct nameidata *, struct proc *);
int o9fs_unmount(struct mount *, int, struct proc *);
int o9fs_statfs(struct mount *, struct statfs *, struct proc *);
int o9fs_start(struct mount *, int, struct proc *);
int o9fs_root(struct mount *, struct vnode **);
static int	mounto9fs(struct mount *, struct file *);
struct o9fid *o9fs_attach(struct o9fs *, struct o9fid *, char *, char *);

/*
 * TODO: Check if we are are not overflowing our i/o buffers.
 */

		
	
static uint32_t
o9fs_version(struct o9fs *fs, uint32_t msize)
{
	long n;
	u_char *p;

	if (fs == NULL)
		return 0;

	p = fs->outbuf;

	O9FS_PBIT32(p, 19);
	O9FS_PBIT8(p + Offtype, O9FS_TVERSION);
	O9FS_PBIT16(p + Offtag, O9FS_NOTAG);
	O9FS_PBIT32(p + Minhd, msize);
	o9fs_putstr(p + Minhd + 4, "9P2000");

	n = o9fs_mio(fs, 19);
	if (n <= 0)
		return 0;
	return O9FS_GBIT16(fs->inbuf + Minhd);
}	

struct o9fid *
o9fs_auth(struct o9fs *fs, char *user, char *aname)
{
	long n;
	u_char *p;
	struct o9fid *f;

	if (fs == NULL)
		return NULL;
	
	user = user ? user : "";
	aname = aname ? aname : "";

	p = fs->outbuf;
	O9FS_PBIT8(p + Offtype, O9FS_TAUTH);
	O9FS_PBIT16(p + Offtag, o9fs_tag());

	f = o9fs_getfid(fs);
	O9FS_PBIT32(p + Minhd, f->fid);
	p = o9fs_putstr(p + Minhd + 4, user);
	p = o9fs_putstr(p, aname);
	n = p - fs->outbuf;
	O9FS_PBIT32(fs->outbuf, n);

	n = o9fs_mio(fs, n);
	if (n <= 0) {
		o9fs_putfid(fs, f);
		return NULL;
	}
	return f;
}

struct o9fid *
o9fs_attach(struct o9fs *fs, struct o9fid *afid, char *user, char *aname)
{
	long n;
	u_char *p;
	struct o9fid *f;

	if (fs == NULL)
		return NULL;

	user = user ? user : "";
	aname = aname ? aname : "";
	
	p = fs->outbuf;
	O9FS_PBIT8(p + Offtype, O9FS_TATTACH);
	O9FS_PBIT16(p + Offtag, o9fs_tag());
	
	f = o9fs_getfid(fs);
	O9FS_PBIT32(p + Minhd, f->fid);
	O9FS_PBIT32(p + Minhd + 4, afid ? afid->fid : -1);
	p = o9fs_putstr(p + Minhd + 4 + 4, user);
	p = o9fs_putstr(p, aname);
	
	n = p - fs->outbuf;
	O9FS_PBIT32(fs->outbuf, n);
	n = o9fs_mio(fs, n);
	if (n <= 0) {
		o9fs_putfid(fs, f);
		return NULL;
	}

	f->qid.type = O9FS_GBIT8(fs->inbuf + Minhd);
	f->qid.vers = O9FS_GBIT32(fs->inbuf + Minhd + 1);
	f->qid.path = O9FS_GBIT64(fs->inbuf + Minhd + 1 + 4);
	return f;
}

int
mounto9fs(struct mount *mp, struct file *fp)
{
	struct o9fs *fs;
	struct vnode *rvp;
	struct o9fid *fid;
	uint32_t msize;

	fs = (struct o9fs *) malloc(sizeof(struct o9fs), M_MISCFSMNT, M_WAITOK | M_ZERO);
	fs->mp = mp;
	fs->servfp = fp;
	mp->mnt_data = (qaddr_t) fs;
	vfs_getnewfsid(mp);	

	fs->inbuf = malloc(8192+Maxhd, M_O9FS, M_WAITOK | M_ZERO);
	fs->outbuf = malloc(8192+Maxhd, M_O9FS, M_WAITOK | M_ZERO);
	TAILQ_INIT(&fs->activeq);
	TAILQ_INIT(&fs->freeq);
	fs->nextfid = 0;	

	msize = o9fs_version(fs, 8192+Maxhd);
	if (msize < Maxhd)
		return EIO;
	fs->msize = msize;

	fid = o9fs_attach(fs, o9fs_auth(fs, "none", ""), "iru", "");
	if (fid == NULL)
		return EIO;

	return o9fs_allocvp(fs->mp, fid, &fs->vroot, VROOT);
}
	

int
o9fs_mount(struct mount *mp, const char *path, void *data, struct nameidata *ndp, struct proc *p)
{
	struct o9fs_args args;
	int error;
	size_t len;
	struct file *fp;

	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	error = copyin(data, &args, sizeof(struct o9fs_args));
	if (error)
		return error;

	if ((fp = fd_getfile(p->p_fd, args.fd)) == NULL)
		return EBADF;
	FREF(fp);

	if (args.verbose)
		verbose = 1;

	if (mounto9fs(mp, fp) != 0)
		return EIO;
	printvp(VFSTOO9FS(mp)->vroot);

	bzero(mp->mnt_stat.f_mntonname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntonname, path, MNAMELEN);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromname, args.hostname, MNAMELEN);
	return 0;
}

int
o9fs_root(struct mount *mp, struct vnode **vpp)
{
	struct o9fid *f;
	struct o9fs *fs;
	int error;

	DIN();

	fs = VFSTOO9FS(mp);
	f = o9fs_walk(fs, VTO9(fs->vroot), NULL, NULL);
	if (f == NULL) {
		DRET();
		return -1;
	}

	error = o9fs_allocvp(mp, f, vpp, VROOT);
	if (error) {
		DBG("failed to alloc vnode\n");
		DRET();
		return error;
	}
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, curproc);
	DRET();
	return 0;
}

int
o9fs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct o9fs *fs;
	struct vnode *vp;
	struct o9fid *f;
	struct file *fp;
	int error, flags;
	DIN();

	flags = 0;
	fs = VFSTOO9FS(mp);
	fp = fs->servfp;
	vp = fs->vroot;
	f = VTO9(vp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	error = vflush(mp, NULL, flags);
	if (error) {
		DBG("error in vflush %d\n", error);
		DRET();
		return error;
	}

	free(fs->inbuf, M_O9FS);
	free(fs->outbuf, M_O9FS);
	free(fs, M_O9FS);
	fs = mp->mnt_data = (qaddr_t)0;

	DRET();
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
	sbp->f_iosize = VFSTOO9FS(mp)->msize;
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

