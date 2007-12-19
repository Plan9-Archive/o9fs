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

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

void *xrealloc(void **, size_t, size_t);
static void *o9fsrealloc(void *, size_t);

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
	{ &vop_default_desc, vn_default_error },
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
	struct o9fsmount *omnt;
	struct o9fsfid *f;
	struct o9fsfcall tx, rx;
	int error, mode;
	printf(">open\n");
	ap = v;
	vp = ap->a_vp;
	p = ap->a_p;
	mode = o9fs_uflags2omode(ap->a_mode);
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);

//	printf("entering readlock\n");
	rw_enter_read(&f->rwl);
//	printf("[in realock]\n");
	if (f->opened == 1) {
		printf("fid %d already opened\n", f->fid);
//		printf("exiting readlock\n");
		rw_exit_read(&f->rwl);

		return 0;
	} else {
//		printf("exiting readlock\n");
		rw_exit_read(&f->rwl);
	}
//	printf("[out readlock\n");

	tx.type = O9FS_TOPEN;
	tx.fid = f->fid;
	tx.mode = mode;
	
	error = o9fs_rpc(omnt, &tx, &rx, 0);
	if (error)
		return -1;
	
//	printf("entering writelock\n");
	rw_enter_write(&f->rwl);
//	printf("[in writelock]\n");
	f->mode = mode;
	f->opened = 1;
//	printf("exiting writelock\n");
	rw_exit_write(&f->rwl);
//	printf("[out writelock]\n");
	printf("<open\n");
	return 0;
}


int
o9fs_close(void *v)
{
	struct vop_close_args *ap;
	struct vnode *vp;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	
	printf(">close\n");
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);

	if (f == NULL)
		return 0;

	omnt = VFSTOO9FS(vp->v_mount);
	o9fs_fidclunk(omnt, f);
	printf("<close\n");

	return 0;
}


int
o9fs_access(void *v)
{		
	struct vop_access_args *ap; 
	struct vnode *vp;
	mode_t a_mode;
	int error;
	struct ucred *cred;
	struct vattr va;
	struct proc *p;

	ap = v;
	vp = ap->a_vp;
	a_mode = ap->a_mode;
	cred = ap->a_cred;
	p = ap->a_p;

	error = VOP_GETATTR(vp, &va, cred, p);
	if (error)
		return error;
	error = vaccess(va.va_mode, va.va_uid, va.va_gid, a_mode, cred);

	return error;
}

int
o9fs_create(void *v)
{
	struct vop_create_args *ap;
	struct vnode *dvp, **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	struct o9fsfcall tx, rx;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	struct o9fsstat *stat;
	int error, isdir;
	
	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;

	isdir = vap->va_mode & S_IFDIR;

	*vpp = NULL;
	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat), M_O9FS, M_WAITOK);
	omnt = VFSTOO9FS(dvp->v_mount);
	f = VTO9(dvp);

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
	if (isdir)
		tx.perm = o9fs_utopmode(vap->va_mode) & (~0777 | (f->stat->mode & 0777));
	else
		tx.perm = o9fs_utopmode(vap->va_mode) & (~0666 | (f->stat->mode & 0666));

	error = o9fs_rpc(omnt, &tx, &rx, 0);
	if (error == -1)
		goto out;

	error = o9fs_allocvp(dvp->v_mount, f, vpp, 0);
	if (error) {
		o9fs_freevp(*vpp);
		goto out;
	}

	rw_enter_write(&f->rwl);
	f->mode = O9FS_OWRITE;
	if (!isdir)
		f->opened = 1; /* XXX is this always true? */
	rw_exit_write(&f->rwl);

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

	ap = v;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;

	vap->va_mode |= S_IFDIR;
	return VOP_CREATE(dvp, vpp, cnp, vap);
}

int
o9fs_read(void *v)
{
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	int ioflag, error, msize;
	off_t offset;
	char buf[RWSIZE];	
	long n;
	int64_t len;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	cred = ap->a_cred;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);
	error = 0;

	if (uio->uio_offset < 0)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	len = uio->uio_resid;
	msize = f->fs->msize - O9FS_IOHDRSZ;
	if (len > msize)
		len = msize;
	offset = uio->uio_offset;
	n = o9fs_tread(omnt, f, buf, RWSIZE, offset);
	if (n > 0)
		error = uiomove(buf, n, uio);
	else if (n < 0)
		error = n;
/*	else if (n < 0
		o9fs_fidclunk(omnt, f); */

	return error;
}

/*
int
o9fs_readdir(void *v)
{	
	struct vop_readdir_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	struct o9fsstat *stat;
	u_char *buf;
	long n, ts;
	int error, *eofflag;
	size_t len;

	printf(">readdir\n");

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	eofflag = ap->a_eofflag;
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);
	error = 0;

	ts = uio->uio_offset;

	len = omnt->om_o9fs.msize;
	buf = (u_char *) malloc(omnt->om_o9fs.msize, M_O9FS, M_WAITOK | M_ZERO);

	do {
		int i;
		
		i = 1;
		do {
			buf = (u_char *)xrealloc((void *)&buf, len, i*len);
			len *= i++;
			printf("buf %p size %d\n", buf, len);
			n = o9fs_tread(omnt, f, buf, O9FS_DIRMAX, -1);
			ts += n;
			printf("read %d bytes\n", n);
		} while(n);
		
		printf("readdir: n=%d\n", n);
		if (n < 0)
			return ENOTDIR;
	*	if (n == 0)
			return 0; *

		if (ts >= 0) {
			ts = o9fs_dirpackage(buf, ts, &stat);
			if (ts < 0)
				printf("malformed directory contents\n");
			else {

				for (i = 0; i < ts; i++) {
					struct dirent d;

					d.d_fileno = (u_int32_t)stat[i].qid.path;
					d.d_type = vp->v_type == VDIR ? DT_DIR : DT_REG;
					d.d_namlen = strlen(stat[i].name);
					strlcpy(d.d_name, stat[i].name, d.d_namlen+1);
					d.d_reclen = DIRENT_SIZE(&d);

					error = uiomove(&d, d.d_reclen, uio);
					printf("stat[%d]->name = %s\n", i, d.d_name);
				} 
			}
		}
		if (ts == 0 && n < 0)
			return -1;
	} while(n > 0);

	return error;
}*/


int
o9fs_readdir(void *v)
{	
	struct vop_readdir_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	struct o9fsstat *stat;
	struct o9fsmsg *m;
	struct dirent d;
	u_char *buf;
	long n, ts;
	int error, *eofflag, nstat, mstat;
	size_t len;

	printf(">readdir\n");

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	eofflag = ap->a_eofflag;
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);
	error = 0;

	ts = uio->uio_offset;

	len = omnt->om_o9fs.msize;
	buf = (u_char *) malloc(omnt->om_o9fs.msize, M_O9FS, M_WAITOK | M_ZERO);

	nstat = 0;
	mstat = 16;
	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat) * mstat, M_O9FS, M_WAITOK);
	
	while ((n = o9fs_tread(omnt, f, buf, O9FS_DIRMAX, -1)) > 0) {
		m = o9fs_msg(buf, n, O9FS_MLOAD);
		/* XXX why? */
		while (m->m_cur < m->m_end) {
			if (nstat == mstat) {
				mstat <<= 1;
				stat = o9fsrealloc(stat, sizeof(struct o9fsstat) * mstat);
			}
			o9fs_msgstat(m, &stat[nstat]);
			d.d_fileno = (u_int32_t)stat[nstat].qid.path;
			d.d_type = vp->v_type == VDIR ? DT_DIR : DT_REG;
			d.d_namlen = strlen(stat[nstat].name);
			strlcpy(d.d_name, stat[nstat].name, d.d_namlen+1);
			d.d_reclen = DIRENT_SIZE(&d);

			error = uiomove(&d, d.d_reclen, uio);
			printf("stat[%d]->name = %s\n", nstat, d.d_name);
			o9fs_freestat(&stat[nstat]);
			nstat++;
		}
	}
	free(stat, M_O9FS);
	free(buf, M_O9FS);

	return 0;
}

int
o9fs_write(void *v)
{	
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	struct o9fsfid *f;
	int ioflag, error, msize;
	char buf[RWSIZE];
	long n;
	int64_t len;
	off_t offset;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	cred = ap->a_cred;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	error = n = 0;

//	printf(">write\n");
/*	if (uio->uio_offset < 0 || vp->v_type != VREG)
		return EINVAL;*/

	if (uio->uio_resid == 0)
		return 0;

	offset = -1;
//	if (ioflag & IO_APPEND)
//		uio->uio_offset = offset = f->stat->length;

	len = uio->uio_resid;
	msize = f->fs->msize - O9FS_IOHDRSZ;
	if (len > msize)
		len = msize;
/*	buf = (char *) malloc(len, M_TEMP, M_WAITOK); */
	error = uiomove(buf, len, uio);
	n = o9fs_twrite(VFSTOO9FS(vp->v_mount), f, buf, len, -1);
	if (n < 0)
		return -1;

/*	free(buf, M_TEMP); */
	return error;
}

int
o9fs_lookup(void *v)
{
	struct vop_lookup_args *ap;
	struct componentname *cnp;
	char *namep;
	struct vnode *dvp;
	struct vnode **vpp;
	struct o9fsfid *fid, *dfid;
	struct o9fsmount *omnt;
	struct proc *p;
	int flags, nameiop, error, islast;

	ap = v;
	dvp = ap->a_dvp;			/* parent dir where to look */
	vpp = ap->a_vpp;			/* resulting vnode */
	cnp = ap->a_cnp;
	namep = cnp->cn_nameptr;	/* name to lookup */
	flags = cnp->cn_flags;		/* lookup options */
	p = curproc;
	nameiop = cnp->cn_nameiop;	/* lookup operation */
	islast = flags & ISLASTCN;

	*vpp = NULL;
	dfid = VTO9(dvp);			/* parent dir fid */
	omnt = VFSTOO9FS(dvp->v_mount);
	
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, p);
	if (error)
		return error;

	/* if requesting dot return parent */
	if (ap->a_cnp->cn_namelen == 1 && ap->a_cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
		return 0;
	}

	/* check if we have already looked up this */
	fid = o9fs_fid_lookup(omnt, dfid, namep);
	if (fid == NULL) {
		/* fid was not found, walk to it */
		fid = o9fs_twalk(omnt, dfid->fid, namep);
		printf("after walk\n");
		if (fid) {
			if ((o9fs_fstat(omnt, fid)) == NULL) {
				printf("cannot stat %s\n", namep);
				goto bad;
			}
		} else { 
			/* it is fine to fail walking when we are creating
	 		 * or renaming on the last component of the path name
	 		 */
			if (islast && (nameiop == CREATE || nameiop == RENAME)) {
				printf("lookup: creating\n");
				/* save the name. it's gonna be used soon */
				cnp->cn_flags |= SAVENAME;
				error = EJUSTRETURN;
				return error;
				goto lockngo;
			} else
				/* it wasn't found at all */
				goto bad;
		}
	}

	error = o9fs_allocvp(omnt->om_mp, fid, vpp, 0);
	if (error)
		goto bad;

lockngo:
	printf("lookup: lockngo\n");
	/* fix locking on parent dir */
	if (islast && !(cnp->cn_flags & LOCKPARENT)) {
		VOP_UNLOCK(dvp, 0, p);
		flags |= PDIRUNLOCK;
	}

	return error;

bad:
	*vpp = NULL;
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

	ap = v;
	vp = ap->a_vp;
	vap = ap->a_vap;
	f = VTO9(vp);
	
	stat = o9fs_fstat(VFSTOO9FS(vp->v_mount), f);
	if (stat == NULL)
		return -1;
 	
	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = f->stat->length;
	vap->va_blocksize = (*f->fs).msize;
	vap->va_atime.tv_sec = f->stat->atime;
	vap->va_mtime.tv_sec = f->stat->mtime;
	vap->va_ctime.tv_sec = f->stat->atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;
	if (vp->v_flag & VROOT) {
		vap->va_type = VDIR;
		vap->va_fileid = 2;
	}
	vap->va_type = vp->v_type;
	vap->va_mode = o9fs_ptoumode(f->stat->mode);
	vap->va_nlink = 0;
	vap->va_fileid = f->qid.path; /* path is bigger. truncate? */
	vap->va_filerev = f->qid.vers;

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
/* from usbf_realloc */
void *
xrealloc(void **ptr, size_t oldsize, size_t newsize)
{
	void *p;

	if (oldsize == newsize)
		return *ptr;

	p = malloc(newsize, M_O9FS, M_WAITOK | M_ZERO);

	if (oldsize > 0) {
		printf("real realloc\n");
		bcopy(*ptr, p, oldsize);
	}
	*ptr = p;
	return p;
}

static void *
o9fsrealloc(void *ptr, size_t size)
{
	void *p;
	
	p = malloc(size, M_O9FS, M_WAITOK);
	bcopy(ptr, p, sizeof(ptr));
	ptr = p;
	return p;
}


/* this should suffice for now */
int
o9fs_reclaim(void *v)
{
	struct vop_reclaim_args *ap;
	struct vnode *vp;

	ap = v;
	vp = ap->a_vp;

	cache_purge(vp);
	o9fs_fidclunk(VFSTOO9FS(vp->v_mount), VTO9(vp));
	o9fs_freevp(vp);

	return 0;
}
