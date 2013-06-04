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
	Debug = 0,
};

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
int o9fs_remove(void *);
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
	.vop_create = o9fs_create,
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
	.vop_remove = o9fs_remove,
	.vop_rename = eopnotsupp,
	.vop_revoke = vop_generic_revoke,
	.vop_mkdir = o9fs_mkdir,
	.vop_rmdir = o9fs_remove,
	.vop_setattr = o9fs_setattr,
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
	struct o9fid *f, *nf;
	DIN();

	ap = v;
	vp = ap->a_vp;
	p = ap->a_p;
	fs = VFSTOO9FS(vp->v_mount);
	f = VTO9(vp);

	printvp(vp);

	/* BUG: old fid leakage */
	nf = o9fs_walk(fs, f, NULL, NULL);
	if (nf == NULL) {
		DRET();
		return -1;
	}

	if (o9fs_opencreate(fs, nf, O9FS_TOPEN, ap->a_mode, 0, 0) < 0) {
		DBG("failed open\n");
		o9fs_putfid(fs, nf);
		DRET();
		return -1;
	}

	nf->parent = f;
	vp->v_data = nf; /* walk has set other properties */

out:
	DRET();
	return 0;
}

int
o9fs_close(void *v)
{
	struct vop_close_args *ap;
	struct vnode *vp;
	struct o9fid *f;
	struct o9fs *fs;
	DIN();

	ap = v;
	vp = ap->a_vp;

	printvp(vp);
	DRET();
	return 0;
}

/* TODO */
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
	struct o9fid *f, *nf;
	struct o9fs *fs;
	int error;
	DIN();

	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;

	f = VTO9(dvp);
	*vpp = NULL;

	fs = VFSTOO9FS(dvp->v_mount);
	if (f == NULL) {
		DRET();
		return -1;
	}

	/* BUG: old fid leakage */
	nf = o9fs_walk(fs, f, NULL, NULL);
	if (nf == NULL) {
		o9fs_putfid(fs, nf);
		DRET();
		return -1;
	}

	if (o9fs_opencreate(fs, nf, O9FS_TCREATE, 0, vap->va_mode, cnp->cn_nameptr) < 0) {
		o9fs_putfid(fs, nf);
		DRET();
		return -1;
	}
	o9fs_clunkremove(fs, nf, O9FS_TCLUNK);

	/* walk from parent dir to get an unopened fid, break create+open atomicity of 9P */
	nf = o9fs_walk(fs, f, NULL, cnp->cn_nameptr);
	if (nf == NULL) {
		o9fs_putfid(fs, nf);
		DRET();
		return -1;
	}
	
	error = o9fs_allocvp(dvp->v_mount, nf, vpp, 0);
	vput(dvp);
	DRET();
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
	struct o9fid *f;
	struct o9fs *fs;
	uint32_t n;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);

	if (uio->uio_offset < 0)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	n = o9fs_rdwr(fs, f, O9FS_TREAD, o9fs_sanelen(fs, uio->uio_resid), uio->uio_offset);
	if (n > 0)
		return uiomove(fs->inbuf + Minhd + 4, n, uio);
	return n;
}

static long
dirpackage(u_char *buf, long ts, struct o9stat **d)
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

	*d = malloc(n * sizeof(struct o9stat) + ss, M_O9FS, M_WAITOK);

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(struct o9stat);
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
	struct o9fid *f;
	struct o9fs *fs;
	struct o9stat *stat;
	struct dirent d;
	u_char *buf, *nbuf;
	uint32_t n, ts;
	int error, i;
	int64_t len;
	DIN();

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	fs = VFSTOO9FS(vp->v_mount);
	f = VTO9(vp);
	error = 0;

	if (vp->v_type != VDIR) {
		DRET();
		return ENOTDIR;
	}
	if (uio->uio_resid == 0) {
		DRET();
		return 0;
	}

	len = uio->uio_resid;
	ts = n = 0;
	buf = malloc(O9FS_DIRMAX, M_O9FS, M_WAITOK);

	for (;;) {
		len = o9fs_sanelen(fs, len);
		nbuf = o9fsrealloc(buf, ts+O9FS_DIRMAX-n, ts+O9FS_DIRMAX);
		buf = nbuf;
		n = o9fs_rdwr(fs, f, O9FS_TREAD, len, f->offset);
		if (n <= 0)
			break;
		memcpy(buf + ts, fs->inbuf + Minhd + 4, n);
		f->offset += n;
		ts += n;
		len -= n;
	}

	if (ts >= 0) {
		ts = dirpackage(buf, ts, &stat);
		if (ts < 0)
			printf("malformed directory contents\n");
	}
	free(buf, M_O9FS);

	if (ts == 0 && n < 0) {
		DRET();
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
			DBG("uiomove error\n");
			DRET();
			return -1;
		}
	}
	DRET();
	return 0;
}

int
o9fs_remove(void *v)
{
	struct vop_remove_args *ap;
	struct vnode *vp;
	DIN();

	ap = v;
	vp = ap->a_vp;

	o9fs_clunkremove(VFSTOO9FS(vp->v_mount), VTO9(vp), O9FS_TREMOVE);
	DRET();
	return 0;
}

int
o9fs_write(void *v)
{	
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fid *f;
	struct o9fs *fs;
	int ioflag, error;
	uint32_t n;
	off_t offset;
	DIN();

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);
	error = n = 0;

	if (uio->uio_offset < 0 || vp->v_type != VREG) {
		DRET();
		return EINVAL;
	}

	if (uio->uio_resid == 0) {
		DRET();
		return 0;
	}

	offset = uio->uio_offset;
	if (ioflag & IO_APPEND) {
		struct stat st;
		if(vn_stat(vp, &st, curproc) == 0)
			offset = st.st_size;
	}

	n = o9fs_sanelen(fs, uio->uio_resid);
	error = uiomove(fs->outbuf + Minhd + 4 + 8 + 4, n, uio);
	if (error) {
		DRET();
		return -1;
	}

	n = o9fs_rdwr(fs, f, O9FS_TWRITE, n, offset);
	if (n < 0) {
		DRET();
		return -1;
	}

	f->offset = offset + n;
	DRET();
	return 0;
}

int
o9fs_lookup(void *v)
{
	struct vop_lookup_args *ap;
	struct componentname *cnp;
	struct vnode **vpp, *dvp;
	struct proc *p;
	struct o9fs *fs;
	struct o9fid *f, *parf, *nf;
	int flags, op, islast, error;
	long n;
	char *path;
	
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
	parf = VTO9(dvp);			/* parent fid */
	error = 0;
	*vpp = NULL;
	path = NULL;

	if (parf->mode != -1) {
		DBG("parent %d is open, moving to %d\n", parf->fid, parf->parent->fid);
		vref(dvp);
		f = parf;
		parf = parf->parent;
	}

	/* On dot, clone */
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.')
		nf = NULL;
	else {
		path = malloc(cnp->cn_namelen, M_O9FS, M_WAITOK);
		strlcpy(path, cnp->cn_nameptr, cnp->cn_namelen+1);
		nf = o9fs_getfid(fs);
	}
	printvp(dvp);

	/* BUG: fid and path leakage */
	f = o9fs_walk(fs, parf, nf, path);
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
		DBG("could not alloc vnode\n");
		DRET();
		return ENOENT;
	}
	
	/* fix locking on parent dir */
	if (islast && !(cnp->cn_flags & LOCKPARENT)) {
		VOP_UNLOCK(dvp, 0, p);
		flags |= PDIRUNLOCK;
	}

	if(f->qid.type == O9FS_QTDIR)
		(*vpp)->v_type = VDIR;
	else
		(*vpp)->v_type = VREG;
	printvp(*vpp);

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
	struct o9stat *stat;
	struct o9fs *fs;
	DIN();

	ap = v;
	vp = ap->a_vp;
	vap = ap->a_vap;
	f = VTO9(vp);
	fs = VFSTOO9FS(vp->v_mount);

	if (f == NULL) {
		DRET();
		return 0;
	}

	stat = o9fs_stat(fs, f);
	if (stat == NULL) {
		DRET();
		return 0;
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
	vap->va_type = vp->v_type;
	vap->va_mode = o9fs_permtou(stat->mode);
	vap->va_nlink = 0;
	vap->va_fileid = f->qid.path;	/* qid.path is 64bit, va_fileid 32bit */
	vap->va_filerev = f->qid.vers;

	free(stat, M_O9FS);
	DRET();
	return 0;
}

int
o9fs_setattr(void *v)
{
 	struct vop_setattr_args *ap;
	ap = v;

	if (ap->a_vp->v_flag & VROOT)
		return EACCES;

	if (ap->a_vap->va_flags != VNOVAL)
		return EOPNOTSUPP;
	return 0;
}

int
o9fs_inactive(void *v)
{
	struct vop_inactive_args *ap;
	struct vnode *vp;
	struct o9fid *f;
	DIN();

	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);
	if(!(vp->v_flag & VXLOCK))
		vgone(vp);
	DRET();
	return 0;
}

int
o9fs_reclaim(void *v)
{
	struct vop_reclaim_args *ap;
	struct vnode *vp;
	struct o9fid *f;
	DIN();
	
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);
	printvp(vp);

	/* TODO: Removed fids should not be clunked again */
	o9fs_clunkremove(VFSTOO9FS(vp->v_mount), f, O9FS_TCLUNK);
//	o9fs_putfid(VFSTOO9FS(vp->v_mount), f);
	vp->v_data = NULL;
	DRET();
	return 0;
}
