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

enum{
	Debug = 1,
};

static void *o9fsrealloc(void *, size_t, size_t);

int o9fs_open(void *);
int o9fs_close(void *);
int o9fs_lookup(void *);
int o9fs_create(void *);
int o9fs_access(void *);
int o9fs_getattr(void *);
int o9fs_setattr(void *);
int o9fs_read(void *);
int o9fs_write(void *);
int o9fs_mkdir(void *);
int o9fs_readdir(void *);
int o9fs_inactive(void *);
int o9fs_reclaim(void *);

int (**o9fs_vnodeop_p)(void *);

struct vops o9fs_vops = {
	.vop_lock = vop_generic_lock,
	.vop_unlock = vop_generic_unlock,
	.vop_islocked = vop_generic_islocked,
	.vop_abortop = vop_generic_abortop,
	.vop_access = o9fs_access,
	.vop_advlock = eopnotsupp,
	.vop_bmap = eopnotsupp,
	.vop_bwrite = eopnotsupp,
	.vop_close = o9fs_close,
	.vop_create = eopnotsupp,
	.vop_fsync = nullop,
	.vop_getattr = o9fs_getattr,
	.vop_inactive = o9fs_inactive,
	.vop_ioctl = (int (*)(void *))enoioctl,
	.vop_link = eopnotsupp,
	.vop_lookup = o9fs_lookup,
	.vop_mknod = eopnotsupp, 
	.vop_open = o9fs_open,
	.vop_pathconf = eopnotsupp,
	.vop_poll = eopnotsupp,
	.vop_print = eopnotsupp,
	.vop_read = o9fs_read,
	.vop_readdir = o9fs_readdir,
	.vop_readlink = eopnotsupp,
	.vop_reallocblks = eopnotsupp,
	.vop_reclaim = o9fs_reclaim,
	.vop_remove = eopnotsupp,
	.vop_rename = eopnotsupp,
	.vop_revoke = vop_generic_revoke,
	.vop_mkdir = eopnotsupp,
	.vop_rmdir = eopnotsupp,
	.vop_setattr = eopnotsupp,
	.vop_strategy = eopnotsupp,
	.vop_symlink = eopnotsupp,
	.vop_write = o9fs_write,
	.vop_kqfilter = eopnotsupp,
};
	
	
int 
o9fs_open(void *v)
{
	struct vop_open_args *ap;
	struct vnode *vp;
	struct proc *p;
	struct o9fs *fs;
	struct o9fsfid *f;
	int error, mode;

	DBG("open: enter\n");
	ap = v;
	vp = ap->a_vp;
	p = ap->a_p;
	mode = o9fs_uflags2omode(ap->a_mode);
	fs = VFSTOO9FS(vp->v_mount);
	f = VTO9(vp);

	if (f->opened == 1) {
		DBG("open: already opened\n");
		goto out;
	}
	
	f = o9fs_clone(fs, f);
	if (f == NULL)
		return -1;
	
	if(o9fs_opencreate(O9FS_TOPEN, fs, f, ap->a_mode, 0, 0))
		return -1;

	vp->v_data = f; /* walk has set other properties */

	if(f->qid.type == O9FS_QTDIR)
		vp->v_type = VDIR;
	else
		vp->v_type = VREG;

out:
	DBG("open: return (%p)->v_type=%d\n", vp, vp->v_type);
	return 0;
}

int
o9fs_close(void *v)
{
	struct vop_close_args *ap;
	struct vnode *vp;
	struct o9fsfid *f;
	struct o9fs *fs;
	
	DBG("close: enter\n");
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);

	if (f == NULL) {
		DBG("close: nil fid\n");
		DBG("close: return\n");
		return 0;
	}

	DBG("close: fid=%d\n", f->fid);
	fs = VFSTOO9FS(vp->v_mount);

	o9fs_fidclunk(fs, f);
	vp->v_data = NULL;
	DBG("close: return\n");
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
	struct o9fsfid *f, *dirfid;
	struct o9fs *fs;
	int error;
	
	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;
	dirfid = VTO9(dvp);

	*vpp = NULL;
	fs = VFSTOO9FS(dvp->v_mount);
	if (f == NULL)
		return -1;


	f = o9fs_clone(fs, dirfid);
	if (f == NULL)
		goto out;

	if(o9fs_opencreate(O9FS_TCREATE, fs, f, 0, vap->va_mode, cnp->cn_nameptr))
		goto out;

	error = o9fs_allocvp(dvp->v_mount, f, vpp, 0);
	if (error) {
		o9fs_freevp(*vpp);
		goto out;
	}

	if(f->qid.type == O9FS_QTDIR)
		(*vpp)->v_type = VDIR;
	else
		(*vpp)->v_type = VREG;
	o9fs_fidclunk(fs, dirfid);
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

	free(buf, M_O9FS);
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

	DBG("readdir: enter\n");
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
		DBG("readdir: return\n");
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
			DBG("readdir: return\n");
			return -1;
		}
	}
	DBG("readdir: return\n");
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

	DBG("write: enter\n");
	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	error = n = 0;

	DBG("write: vp=%p v_type=%d\n", vp, vp->v_type); 

	if (uio->uio_offset < 0 || vp->v_type != VREG) {
		DBG("write: return EINVAL\n");
		return EINVAL;
	}

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
	DBG("write: return\n");
	return error;
}

int
o9fs_lookup(void *v)
{
	struct vop_lookup_args *ap;
	struct componentname *cnp;
	struct vnode **vpp, *dvp;
	struct proc *p;
	struct o9fs *fs;
	struct o9fid *f, *parf;
	int flags, op, islast, error;
	
	DIN();
	ap = v;
	dvp = ap->a_dvp;			/* parent dir where to look */
	vpp = ap->a_vpp;			/* resulting vnode */
	cnp = ap->a_cnp;			/* path component name */
	flags = cnp->cn_flags;		/* lookup options */
	p = curproc;
	op = cnp->cn_nameiop;		/* lookup operation */
	islast = flags & ISLASTCN;

	fs = VFSTOO9FS(dvp->v_mount);
	parf = VTO92(dvp);			/* parent fid */
	error = 0;
	*vpp = NULL;
	
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		DBG("dot\n");
		vref(dvp);
		*vpp = dvp;
		DRET();
		return 0;
	}

	DBG("name is %s\n", cnp->cn_nameptr);

	f = o9fs_walk(fs, parf, o9fs_xgetfid(fs), cnp->cn_nameptr);
	if (f == NULL) {
		DBG("%s not found\n", cnp->cn_nameptr);
		if (islast && (op == CREATE || op == RENAME)) {
			/* save the name. it's gonna be used soon */
			cnp->cn_flags |= SAVENAME;
			DRET();
			return EJUSTRETURN;
		}
		*vpp = NULL;
		DRET();
		return ENOENT;
	}
	
	error = o9fs_allocvp(fs->mp, f, vpp, 0);
	if (error) {
		*vpp = NULL;
		DRET();
		return ENOENT;
	}
	
	/* fix locking on parent dir */
	if (islast && !(cnp->cn_flags & LOCKPARENT)) {
		VOP_UNLOCK(dvp, 0, p);
		flags |= PDIRUNLOCK;
	}
	
	DRET();
	return 0;
}



int
o9fs_getattr(void *v)
{
	struct vop_getattr_args *ap;
	struct vnode *vp;
	struct vattr *vap;
	struct o9fid *f;
	struct o9fsstat *stat;
	struct o9fs *fs;

	DBG("getattr: enter\n");

	ap = v;
	vp = ap->a_vp;
	vap = ap->a_vap;
	f = VTO92(vp);
	fs = VFSTOO9FS(vp->v_mount);

	if (f == NULL)
		return 0;

/*	stat = o9fs_fstat(fs, f);
	if (stat == NULL) {
		DBG("getattr: Tstat failed\n");
		DBG("getattr: return\n");
		return -1;
	}
*/

	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = 512; //stat->length;
	vap->va_blocksize = VFSTOO9FS(vp->v_mount)->msize;
	vap->va_atime.tv_sec = 0; //stat->atime;
	vap->va_mtime.tv_sec = 0; // stat->mtime;
	vap->va_ctime.tv_sec = 0; //stat->atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;
	if (vp->v_flag & VROOT) {
		vap->va_type = VDIR;
		vap->va_fileid = 2;
	}

	/*
	 * vap->va_type being VDIR does not guarantee that user applications
	 * will see us as a dir, we have to set the mode explicitly.
	 */
	vap->va_type = vp->v_type;
	vap->va_mode = 0777; //o9fs_ptoumode(stat->mode);
	if(vap->va_type == VDIR)
		vap->va_mode |= S_IFDIR;

	vap->va_nlink = 0;
	vap->va_fileid = f->qid.path; /* path is bigger. truncate? */
	vap->va_filerev = f->qid.vers;

//	o9fs_freestat(stat);
//	free(stat, M_O9FS);
	DBG("getattr: return\n");
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

	if (ap->a_vp->v_flag & VROOT)
		return EACCES;

	if (ap->a_vap->va_flags != VNOVAL)
		return EOPNOTSUPP;
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

int
o9fs_inactive(void *v)
{
	struct vop_inactive_args *ap;
	DBG("inactive: enter\n");
	DBG("inactive: return\n");
	return 0;
}

/* this should suffice for now */
int
o9fs_reclaim(void *v)
{
	struct vop_reclaim_args *ap;
	struct vnode *vp;
	struct o9fsfid *f;
	struct o9fs *fs;

	DBG("reclaim: enter\n");
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);

	cache_purge(vp);
	DBG("reclaiming fid %d\n", f->fid);
	if(f)
		o9fs_fidclunk(fs, f);
	o9fs_freevp(vp);

	DBG("reclaim: return\n");
	return 0;
}
