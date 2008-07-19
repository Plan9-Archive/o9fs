#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/rwlock.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/syscallargs.h>

#include "o9fs.h"
#include "o9fs_extern.h"

static void *o9fsrealloc(void *, size_t, size_t);

int o9fs_open(void *);
int o9fs_close(void *);
int o9fs_lookup(void *);
int o9fs_create(void *);
#define	o9fs_mknod	eopnotsupp
int o9fs_access(void *);
int o9fs_getattr(void *);
int o9fs_setattr(void *);
int o9fs_read(void *);
int o9fs_write(void *);
int o9fs_mkdir(void *);
#define	o9fs_ioctl	(int (*)(void *))enoioctl
#define	o9fs_fsync	nullop
#define	o9fs_remove	eopnotsupp
#define	o9fs_link	eopnotsupp
#define	o9fs_rename	eopnotsupp
#define	o9fs_rmdir	eopnotsupp
#define	o9fs_symlink	eopnotsupp
int o9fs_readdir(void *);
#define o9fs_revoke	vop_generic_revoke
#define	o9fs_readlink	eopnotsupp
#define	o9fs_inactive	eopnotsupp
int o9fs_reclaim(void *);
#define	o9fs_lock	vop_generic_lock
#define	o9fs_unlock	vop_generic_unlock
#define	o9fs_bmap	eopnotsupp
#define	o9fs_strategy	eopnotsupp	
#define	o9fs_print	eopnotsupp
#define	o9fs_islocked	vop_generic_islocked
#define	o9fs_pathconf	eopnotsupp
#define	o9fs_advlock	eopnotsupp
#define	o9fs_bwrite	eopnotsupp
#define	o9fs_poll	eopnotsupp

int (**o9fs_vnodeop_p)(void *);
struct vnodeopv_entry_desc o9fs_vnodeop_entries[] = {
	{ &vop_default_desc, eopnotsupp },
	{ &vop_lookup_desc, o9fs_lookup },		/* lookup */
	{ &vop_create_desc, o9fs_create },		/* create */
	{ &vop_mknod_desc, o9fs_mknod },		/* mknod */
	{ &vop_open_desc, o9fs_open },		/* open */
	{ &vop_close_desc, o9fs_close },		/* close */
	{ &vop_access_desc, o9fs_access },		/* access */
	{ &vop_getattr_desc, o9fs_getattr },		/* getattr */
	{ &vop_setattr_desc, o9fs_setattr },		/* setattr */
	{ &vop_read_desc, o9fs_read },		/* read */
	{ &vop_write_desc, o9fs_write },		/* write */
	{ &vop_ioctl_desc, o9fs_ioctl },		/* ioctl */
	{ &vop_poll_desc, o9fs_poll },		/* poll */
	{ &vop_revoke_desc, o9fs_revoke },            /* revoke */
	{ &vop_fsync_desc, o9fs_fsync },		/* fsync */
	{ &vop_remove_desc, o9fs_remove },		/* remove */
	{ &vop_link_desc, o9fs_link },		/* link */
	{ &vop_rename_desc, o9fs_rename },		/* rename */
	{ &vop_mkdir_desc, o9fs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, o9fs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, o9fs_symlink },		/* symlink */
	{ &vop_readdir_desc, o9fs_readdir },		/* readdir */
	{ &vop_readlink_desc, o9fs_readlink },	/* readlink */
	{ &vop_abortop_desc, vop_generic_abortop },		/* abortop */
	{ &vop_inactive_desc, o9fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, o9fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, o9fs_lock },		/* lock */
	{ &vop_unlock_desc, o9fs_unlock },		/* unlock */
	{ &vop_bmap_desc, o9fs_bmap },		/* bmap */
	{ &vop_strategy_desc, o9fs_strategy },	/* strategy */
	{ &vop_print_desc, o9fs_print },		/* print */
	{ &vop_islocked_desc, o9fs_islocked },	/* islocked */
	{ &vop_pathconf_desc, o9fs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, o9fs_advlock },		/* advlock */
	{ &vop_bwrite_desc, o9fs_bwrite },		/* bwrite */
	{ NULL, NULL }
};
struct vnodeopv_desc o9fs_vnodeop_opv_desc =
	{ &o9fs_vnodeop_p, o9fs_vnodeop_entries };

int
o9fs_open(void *v)
{
	struct vop_open_args *ap;
	struct vnode *vp;
	struct proc *p;
	struct o9fs *fs;
	struct o9fsfid *f;
	struct o9fsfcall tx, rx;
	int error, mode;

	DPRINT("open: enter\n");
	ap = v;
	vp = ap->a_vp;
	p = ap->a_p;
	mode = o9fs_uflags2omode(ap->a_mode);
	fs = VFSTOO9FS(vp->v_mount);

//	f = o9fs_fidunique(fs, VTO9(vp));
	f = o9fs_clone(fs, VTO9(vp));
	if (f->opened == 1) {
		DPRINT("open: already opened: fid->fid=%d vp=%p vp->v_data->fid=%d\n",
			f->fid, vp, VTO9(vp)->fid);
		DPRINT("open: return\n");
		return 0;
	}

	tx.type = O9FS_TOPEN;
	tx.fid = f->fid;
	tx.mode = mode;
	
	error = o9fs_rpc(fs, &tx, &rx);
	if (error)
		return error;
	
	f->mode = mode;
	f->opened = 1;
	f->offset = 0;
	vp->v_data = f;
	DPRINT("open: return\n");
	return 0;
}

int
o9fs_close(void *v)
{
	struct vop_close_args *ap;
	struct vnode *vp;
	struct o9fsfid *f;
	struct o9fs *fs;
	
	DPRINT("close: enter\n");
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);

	if (f == NULL) {
		DPRINT("close: nil fid\n");
		DPRINT("close: return\n");
		return 0;
	}

	DPRINT("close: fid=%d\n", f->fid);
	fs = VFSTOO9FS(vp->v_mount);
	o9fs_fidclunk(fs, f);
	vp->v_data = NULL;
	DPRINT("close: return\n");
	return 0;
}

int
o9fs_access(void *v)
{		
	struct vop_access_args *ap; 
	struct vnode *vp;
	mode_t a_mode;
	struct ucred *cred;
	struct proc *p;

	ap = v;
	vp = ap->a_vp;
	a_mode = ap->a_mode;
	cred = ap->a_cred;
	p = ap->a_p;

	return 0;
}

int
o9fs_create(void *v)
{
	struct vop_create_args *ap;
	struct vnode *dvp, **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	struct o9fsfcall tx, rx;
	struct o9fsfid *f, *ofid;
	struct o9fs *fs;
	struct o9fsstat *stat;
	int error, isdir;
	
	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;
	ofid = VTO9(dvp);
	isdir = vap->va_mode & S_IFDIR;

	*vpp = NULL;
	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat), M_O9FS, M_WAITOK);
	fs = VFSTOO9FS(dvp->v_mount);

	if (f == NULL)
		return -1;

	stat->mode = 0;
	stat->atime = 0;
	stat->mtime = 0;
	stat->name = cnp->cn_nameptr;
	stat->uid = 0;
	stat->gid = 0;

	tx.type = O9FS_TCREATE;
	tx.name = cnp->cn_nameptr;
	tx.fid = f->fid;
	tx.mode = o9fs_uflags2omode(vap->va_mode);
	tx.perm = o9fs_utopmode(vap->va_mode);

	error = o9fs_rpc(fs, &tx, &rx);
	if (error == -1)
		goto out;

	error = o9fs_allocvp(dvp->v_mount, f, vpp, 0);
	if (error) {
		o9fs_freevp(*vpp);
		goto out;
	}
	printfidvp(f);
	printfidvp(ofid);

	f->mode = rx.mode;
	if (!isdir)
		f->opened = 1;
	else {
		/* since Tcreate opens a fid, if it is a dir, close it in order to use it */
		o9fs_fidclunk(fs, f);
	}

out:
	vput(dvp);
	return error;
}

int
o9fs_mkdir(void *v)
{
	struct vop_create_args *ap;
	struct vnode *dvp, **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	int error;

	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;

	vap->va_mode |= S_IFDIR;
	error = VOP_CREATE(dvp, vpp, cnp, vap);
	return error;
}

int
o9fs_read(void *v)
{
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	struct o9fs *fs;
	int ioflag, error, msize;
	char *buf;
	long n;
	int64_t len;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);
	error = 0;

	if (uio->uio_offset < 0)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	len = uio->uio_resid;
	msize = fs->msize - O9FS_IOHDRSZ;
	if (len > msize)
		len = msize;
	buf = malloc(len, M_O9FS, M_WAITOK | M_ZERO);
	n = o9fs_rdwr(fs, O9FS_TREAD, f, buf, 1024, uio->uio_offset);
	if (n > 0)
		error = uiomove(fs->reply.data, n, uio);
	else if (n < 0)
		error = n;
	return error;
}

static long
dirpackage(u_char *buf, long ts, struct o9fsstat **d)
{
	char *s;
	long ss, i, n, nn, m;

	*d = NULL;
	if (ts <= 0)
		return 0;

	/*
	 * first find number of all stats, check they look like stats, & size all associated strings
	 */
	ss = 0;
	n = 0;
	for (i = 0; i < ts; i += m) {
		m = O9FS_BIT16SZ + O9FS_GBIT16(&buf[i]);
		if (o9fs_statcheck(&buf[i], m) < 0)
			break;
		ss += m;
		n++;
	}

	if (i != ts)
		return -1;

	*d = malloc(n * sizeof(struct o9fsstat) + ss, M_O9FS, M_WAITOK);

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(struct o9fsstat);
	nn = 0;
	for (i = 0; i < ts; i += m) {
		m = O9FS_BIT16SZ + O9FS_GBIT16((u_char*)&buf[i]);
		if (nn >= n || o9fs_convM2D(&buf[i], m, *d + nn, s) != m) {
			free(*d, M_O9FS);
			*d = NULL;
			return -1;
		}
		nn++;
		s += m;
	}

	return nn;
}

int
o9fs_readdir(void *v)
{	
	struct vop_readdir_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	struct o9fs *fs;
	struct o9fsstat *stat;
	struct dirent d;
	u_char *buf, *nbuf;
	long n, ts;
	int error, *eofflag, i, msize;
	int64_t len;

	DPRINT("readdir: enter\n");
	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	eofflag = ap->a_eofflag;
	fs = VFSTOO9FS(vp->v_mount);
	f = VTO9(vp);
	error = i = n = 0;

	if (uio->uio_resid == 0)
		return 0;

	len = uio->uio_resid;
	msize = fs->msize - O9FS_IOHDRSZ;
	if (len > msize)
		len = msize;

	buf = malloc(O9FS_DIRMAX, M_O9FS, M_WAITOK);
	ts = n = 0;
	for (;;) {
		nbuf = o9fsrealloc(buf, ts+O9FS_DIRMAX-n, ts+O9FS_DIRMAX);
		buf = nbuf;
		n = o9fs_rdwr(fs, O9FS_TREAD, f, buf+ts, 1024, -1);
		if (n <= 0)
			break;
		ts += n;
	}
	if (ts >= 0) {
		ts = dirpackage(buf, ts, &stat);
		if (ts < 0)
			printf("malformed directory contents\n");
	}
	free(buf, M_O9FS);

	if (ts == 0 && n < 0) {
		DPRINT("readdir: return\n");
		return -1;
	}
	for (i = 0; i < ts; i++) {
		d.d_fileno = (uint32_t)stat[i].qid.path;
		d.d_type = vp->v_type == VDIR ? DT_DIR : DT_REG;
		d.d_namlen = strlen(stat[i].name);
		memcpy(d.d_name, stat[i].name, d.d_namlen);
		d.d_name[d.d_namlen] = '\0';
		d.d_reclen = DIRENT_SIZE(&d);
		error = uiomove(&d, d.d_reclen, uio);
		if (error) {
			printf("readdir: uiomove error\n");
			DPRINT("readdir: return\n");
			return -1;
		}
	}
	DPRINT("readdir: return\n");
	return 0;
}

int
o9fs_write(void *v)
{	
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	int ioflag, error, msize;
	char *buf;
	long n;
	int64_t len;
	off_t offset;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	error = n = 0;

	if (uio->uio_offset < 0 || vp->v_type != VREG)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	offset = uio->uio_offset;
	if (ioflag & IO_APPEND) {
		struct stat st;
		if(vn_stat(vp, &st, curproc) == 0)
			uio->uio_offset = offset = st.st_size;
	}

	len = uio->uio_resid;
	msize = VFSTOO9FS(vp->v_mount)->msize - O9FS_IOHDRSZ;
	if (len > msize)
		len = msize;
	buf = malloc(len, M_O9FS, M_WAITOK);

	error = uiomove(buf, len, uio);
	n = o9fs_rdwr(VFSTOO9FS(vp->v_mount), O9FS_TWRITE, f, buf, len, offset);
	if (n < 0)
		return -1;

	free(buf, M_O9FS);
	return error;
}

int
o9fs_lookup(void *v)
{
	struct vop_lookup_args *ap;
	struct componentname *cnp;
	char namep[MAXPATHLEN], *path[O9FS_MAXWELEM];
	struct vnode *dvp;
	struct vnode **vpp;
	struct o9fsfid *fid, *dfid;
	struct o9fs *fs;
	struct proc *p;
	int flags, nameiop, error, islast;

	DPRINT("lookup: enter\n");
	ap = v;
	dvp = ap->a_dvp;			/* parent dir where to look */
	vpp = ap->a_vpp;			/* resulting vnode */
	cnp = ap->a_cnp;
	flags = cnp->cn_flags;		/* lookup options */
	p = curproc;
	nameiop = cnp->cn_nameiop;	/* lookup operation */
	islast = flags & ISLASTCN;

	*vpp = NULL;
	dfid = VTO9(dvp);			/* parent dir fid */
	fs = VFSTOO9FS(dvp->v_mount);

	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		DPRINT("lookup: dot, real name is %s\n", dfid->name);
		fid = o9fs_clone(fs, dfid);
		if (fid == NULL)
			goto bad;
		error = o9fs_allocvp(fs->mp, fid, vpp, 0); 
		if (error)
			goto bad; 
		VREF(*vpp);
		DPRINT("lookup: return\n");
		return 0;
	} else {
		strlcpy(namep, cnp->cn_nameptr, cnp->cn_namelen+1);
		o9fs_tokenize(path, nelem(path), namep, '/');
	}

	DPRINT("lookup: gonna walk\n");
	fid = o9fs_twalk(fs, dfid, *path);
	if (fid == NULL) {
		/* it is fine to fail walking when we are creating
	 	 * or renaming on the last component of the path name
	 	 */
		if (islast && (nameiop == CREATE || nameiop == RENAME)) {
			printf("lookup: creating\n");
			/* save the name. it's gonna be used soon */
			cnp->cn_flags |= SAVENAME;
			error = EJUSTRETURN;
			DPRINT("lookup: return\n");
			return error;
		} else
			/* it wasn't found at all */
			goto bad;
	}

	DPRINT("lookup: name=%s QTDIR=%d\n", fid->name, fid->qid.type & O9FS_QTDIR);
	error = o9fs_allocvp(fs->mp, fid, vpp, 0);
	if (error)
		goto bad;

	/* fix locking on parent dir */
	if (islast && !(cnp->cn_flags & LOCKPARENT)) {
		VOP_UNLOCK(dvp, 0, p);
		flags |= PDIRUNLOCK;
	}
	DPRINT("lookup: return\n");
	return error;

bad:
	*vpp = NULL;
	DPRINT("lookup: return\n");
	return ENOENT;
}	


int
o9fs_getattr(void *v)
{
	struct vop_getattr_args *ap;
	struct vnode *vp;
	struct vattr *vap;
	struct o9fsfid *f;
	struct o9fsstat *stat;
	struct o9fs *fs;

	DPRINT("getattr: enter\n");
	ap = v;
	vp = ap->a_vp;
	vap = ap->a_vap;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);

	if (!f)
		return 0;
	/* lookup fetches stat for us since the only way for
	  * a file to have is attributes updated are through a new walk
           */
	stat = o9fs_fstat(fs, f);
	if (stat == NULL) {
		DPRINT("getattr: Tstat failed\n");
		DPRINT("getattr: return\n");
		return -1;
	}

	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = stat->length;
	vap->va_blocksize = VFSTOO9FS(vp->v_mount)->msize;
	vap->va_atime.tv_sec = stat->atime;
	vap->va_mtime.tv_sec = stat->mtime;
	vap->va_ctime.tv_sec = stat->atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;
	if (vp->v_flag & VROOT) {
		vap->va_type = VDIR;
		vap->va_fileid = 2;
	}
	vap->va_type = vp->v_type;
	vap->va_mode = o9fs_ptoumode(stat->mode);
	vap->va_nlink = 0;
	vap->va_fileid = f->qid.path; /* path is bigger. truncate? */
	vap->va_filerev = f->qid.vers;

	o9fs_freestat(stat);
	free(stat, M_O9FS);
	o9fs_fidclunk(fs, f);
	DPRINT("getattr: return\n");
	return 0;
}

int
o9fs_setattr(void *v)
{
 	struct vop_setattr_args *ap;
	ap = v;

	/*
	 * Can't mess with the root vnode
	 */
	printf(">setattr\n");

	if (ap->a_vp->v_flag & VROOT)
		return EACCES;

	if (ap->a_vap->va_flags != VNOVAL)
		return EOPNOTSUPP;
	printf("<setattr\n");
	return 0;
}

static void *
o9fsrealloc(void *ptr, size_t oldsize, size_t newsize)
{
	void *p;
	
	if (newsize == oldsize)
		return ptr;
	p = malloc(newsize, M_O9FS, M_WAITOK);
	if (ptr)
		bcopy(ptr, p, oldsize);
	ptr = p;
	return p;
}

/* this should suffice for now */
int
o9fs_reclaim(void *v)
{
	struct vop_reclaim_args *ap;
	struct vnode *vp;

	DPRINT("reclaim: enter\n");
	ap = v;
	vp = ap->a_vp;

	cache_purge(vp);
	DPRINT("reclaiming fid %d\n", VTO9(vp)->fid);
	o9fs_freevp(vp);

	DPRINT("reclaim: return\n");
	return 0;
}
