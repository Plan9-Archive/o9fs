#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/syscallargs.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

int o9fs_open(void *);
int o9fs_close(void *);
int o9fs_lookup(void *);
int o9fs_create(void *);
#define	o9fs_mknod	eopnotsupp
int o9fs_access(void *);
int o9fs_getattr(void *);
int o9fs_setattr(void *);
int o9fs_read(void *);
#define	o9fs_write	eopnotsupp
#define	o9fs_ioctl	(int (*)(void *))enoioctl
#define	o9fs_fsync	nullop
#define	o9fs_remove	eopnotsupp
#define	o9fs_link	eopnotsupp
#define	o9fs_rename	eopnotsupp
#define	o9fs_mkdir	eopnotsupp
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
	mode = ap->a_mode;
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);

	/* lock to avoid deadlocks */
	if (f->opened)
		return 0;

	tx.tag = 0;
	tx.type = O9FS_TOPEN;
	tx.fid = f->fid;
	tx.mode = 0;
	
	error = o9fs_rpc(omnt, &tx, &rx, 0);
	if (error)
		return -1;

	f->opened = 1;
	return 0;
}

static void
fidclunk(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	struct o9fsfcall tx, rx;

	tx.type = O9FS_TCLUNK;
	tx.fid = fid->fid;
	o9fs_rpc(omnt, &tx, &rx, 0);
	o9fs_putfid(omnt, fid);
}

int
o9fs_close(void *v)
{
	struct vop_close_args *ap;
	struct vnode *vp;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	
	ap = v;
	vp = ap->a_vp;
	f = VTO9(vp);

	if (f == NULL)
		return 0;

	omnt = VFSTOO9FS(vp->v_mount);

	return 0;
}


/* we don't care, everyone is permitted everything */
int
o9fs_access(void *v)
{		
	struct vop_access_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v; 

	struct vnode *vp;
	mode_t a_mode;
	int error;
	struct ucred *cred;
	struct vattr va;
	struct proc *p;

/*	printf(">access\n"); */
	vp = ap->a_vp;
	a_mode = ap->a_mode;
	cred = ap->a_cred;
	p = ap->a_p;

	error = VOP_GETATTR(vp, &va, cred, p);
	if (error)
		return (error);
	
	error = vaccess(va.va_mode, va.va_uid, va.va_gid, a_mode, cred);
/*	printf("<access\n"); */

	return (error);
}

/* consider writing a generic function for handling mkdir and create */
int
o9fs_create(void *v)
{
	struct vop_create_args /* {
			struct vnode *a_dvp;
			struct vnode **a_vpp;
			struct componentname *a_cnp;
			struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp, **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	struct o9fsfcall tx, rx;
	struct o9fsfid *dfid, *f;
	struct o9fsmount *omnt;
	struct o9fsstat *stat;
	int error;
	
	printf(">create\n");
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	vpp = ap->a_vpp;
	printf("set\n");

	*vpp = NULL;
	dfid = VTO9(dvp);
	f = o9fs_getfid(omnt);
	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat), M_O9FS, M_WAITOK);
	omnt = VFSTOO9FS(dvp->v_mount);

	stat->mode = 0;
	stat->atime = 0;
	stat->mtime = 0;
	stat->name = cnp->cn_nameptr;
	stat->uid = 0;
	stat->gid = 0;

	error = o9fs_allocvp(dvp->v_mount, f, vpp, 0);
	if (error) {
		o9fs_freevp(*vpp);
		goto out;
	}
	
	/* attach new file to it's parent dir */
	error = o9fs_insertfid(f, dfid);
	if (error)
		goto out;

	tx.type = O9FS_TCREATE;
	tx.name = cnp->cn_nameptr;
	tx.fid = f->fid;
	tx.mode = vap->va_mode; /* XXX convert */
	tx.perm = 0777;

	error = o9fs_rpc(omnt, &tx, &rx, 0);

out:
/*	if (error || !(cnp->cn_flags & SAVESTART))
		PNBUF_PUT(cnp->cn_pnbuf); */
	vput(dvp);
	
	return error;
}

int
o9fs_read(void *v)
{
	struct vop_read_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	struct o9fsfid *f;
	int ioflag, error;
	off_t offset;
	char *buf;
	long n;

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	cred = ap->a_cred;
	ioflag = ap->a_ioflag;
	f = VTO9(vp);
	error = 0;

	if (uio->uio_offset < 0)
		return EINVAL;

	if (uio->uio_resid == 0)
		return 0;

	buf = (char *) malloc(2048, M_TEMP, M_WAITOK);
	printf("resid = %d\n", uio->uio_resid);
	offset = uio->uio_offset;
	n = o9fs_tread(VFSTOO9FS(vp->v_mount), f, buf, 4096, offset);
	if (n > 0)
		error = uiomove(buf, n, uio);

	free(buf, M_TEMP);

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

	/* if requesting . return parent */
	if (ap->a_cnp->cn_namelen == 1 && ap->a_cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
		return 0;
	}
	
/*	o9fs_dumpchildlist(dfid); */

	/* check if we have already looked up this */
	fid = o9fs_fid_lookup(omnt, dfid, namep);
	if (fid == NULL) {
		printf("fid not found, walking\n");
		/* this is the first lookup on this file
		 * we have to walk to it from parent
		 */
		fid = o9fs_twalk(omnt, dfid, namep);
		if (fid) {
			/* fid was found, get it's info */
			if ((o9fs_fstat(omnt, fid)) == NULL) {
				printf("cannot stat %s\n", namep);
				goto bad;
			}

			/* add new fid as a child of it's parent */
			o9fs_insertfid(fid, dfid);
		} else {
			/* it is fine to fail walking when we are creating
			 * or renaming on the last component of the path name
			 */
			if (islast && (nameiop == CREATE || nameiop == RENAME)) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
				if (error)
					goto bad;
				/* save the name. it's gonna be used soon */
				cnp->cn_flags |= SAVENAME;
				error = EJUSTRETURN;
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

	ap = v;
	vp = ap->a_vp;
	vap = ap->a_vap;
	f = VTO9(vp);
	printf("getattr\n");
 	
	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = DEV_BSIZE;
	vap->va_blocksize = (*f->fs).msize;
	getnanotime(&vap->va_atime);
	vap->va_mtime.tv_sec = 0;
	vap->va_mtime.tv_nsec = f->stat->mtime; /* convert to struct timespec */
	vap->va_ctime.tv_sec = 0;
	vap->va_ctime.tv_nsec = f->stat->mtime; /* convert to struct timespec */
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;
	if (vp->v_flag & VROOT) {
		vap->va_type = VDIR;
		vap->va_mode = S_IRUSR|S_IWUSR|S_IXUSR|
						S_IRGRP|S_IWGRP|S_IXGRP|
						S_IROTH|S_IWOTH|S_IXOTH;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
	} else {
		vap->va_type = vp->v_type;
		vap->va_mode = o9fs_ptoumode(f->stat->mode); /* convert p9 -> unix mode */
		vap->va_nlink = 1;
		vap->va_fileid = f->qid.path; /* path is bigger. truncate? */
	}

	return 0;
}

int
o9fs_setattr(void *v)
{
        struct vop_setattr_args /* {
                struct vnode *a_vp;
                struct vattr *a_vap;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ *ap = v;

        /*
         * Can't mess with the root vnode
         */
		printf(">setattr\n");

        if (ap->a_vp->v_flag & VROOT)
                return (EACCES);

        if (ap->a_vap->va_flags != VNOVAL)
                return (EOPNOTSUPP);
		printf("<setattr\n");

        return (0);
}

/* return an empty dirent */
int
o9fs_readdir(void *v)
{	
	struct vop_readdir_args *ap;
	struct vnode *vp;
	struct uio *uio;
	struct o9fsfid *f;
	struct o9fsmount *omnt;
	struct o9fsstat *stat;
	struct dirent d;
	char *buf;
	long n, ts;
	int error;

	printf(">readdir\n");

	ap = v;
	vp = ap->a_vp;
	uio = ap->a_uio;
	f = VTO9(vp);
	omnt = VFSTOO9FS(vp->v_mount);
	ts = error = 0;

	buf = (char *) malloc(O9FS_DIRMAX, M_TEMP, M_WAITOK); 
	n = o9fs_tread(omnt, f, buf, O9FS_DIRMAX, 0);
	ts += n;

	if (ts >= 0) {
		ts = o9fs_dirpackage(buf+sizeof(struct o9fsstat), ts, &stat);
		if (ts < 0)
			printf("malformed directory contents\n");
	}

	d.d_fileno = (u_int32_t)f->qid.path;
	d.d_type = vp->v_type == VDIR ? DT_DIR : DT_REG;
	d.d_namlen = strlen(f->stat->name);
	strlcpy(d.d_name, f->stat->name, d.d_namlen+1);
	d.d_reclen = DIRENT_SIZE(&d);

	printf("fileno	= %d\n", d.d_fileno);
	printf("name	= %s\n", d.d_name);
	printf("reclen	= %d\n", d.d_reclen);	

	error = uiomove(&d, d.d_reclen, uio);

	free(buf, M_TEMP);

	if (ts == 0 && n < 0)
		return -1;

	return error ? error : ts;
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
	o9fs_freevp(vp);
	fidclunk(VFSTOO9FS(vp->v_mount), VTO9(vp));

	return 0;
}
