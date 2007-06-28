#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/syscallargs.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

int o9fs_open(void *);
int o9fs_lookup(void *);
int	o9fs_create(void *);
#define	o9fs_mknod	eopnotsupp
#define	o9fs_close	nullop	
int o9fs_access(void *);
int o9fs_getattr(void *);
int o9fs_setattr(void *);
#define	o9fs_read	eopnotsupp
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
	printf("open\n");

	return (0);
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

	printf(">access\n");
	vp = ap->a_vp;
	a_mode = ap->a_mode;
	cred = ap->a_cred;
	p = ap->a_p;

	error = VOP_GETATTR(vp, &va, cred, p);
	if (error)
		return (error);
	
	error = vaccess(va.va_mode, va.va_uid, va.va_gid, a_mode, cred);
	printf("<access\n");

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
	
	printf(">create\n");
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;
	printf("set\n");

	return(o9fs_create_file(dvp, vpp, vap, cnp));	
}

int
o9fs_lookup(void *v)
{
	struct vop_lookup_args /* {
			struct vnode * a_dvp;
			struct vnode ** a_vpp;
			struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp;
	char *namep;
	struct vnode *dvp;		/* parent dir vnode */
	struct vnode **vpp;		/* resulting vnode */
	struct o9fsdirentx *de;
	struct o9fsnode *dnode;
	struct proc *p;
	int flags, nameiop, error, islast;

	p = curproc;
	cnp = ap->a_cnp;
	namep = cnp->cn_nameptr;
	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	*vpp = NULLVP;
	flags = cnp->cn_flags;
	nameiop = cnp->cn_nameiop;
	islast = flags & ISLASTCN;
	
	dnode = VTO9(dvp);
	*vpp = NULL;
	
	printf(">lookup\n");
	de = o9fs_dir_lookup(dnode, cnp);
	if (de == NULL) {
		/* no entry found, ok if creating or renaming */
		if (islast && (nameiop == CREATE || nameiop == RENAME)) {
			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
			if (error)
				goto out;
			/* save the componentname for reuse */
			cnp->cn_flags |= SAVENAME;

			error = EJUSTRETURN;
		} else {
			error = ENOENT;
		}
	} else {
		struct o9fsnode *onode;
		
 		/* The entry was found, so get its associated node */
		onode = de->od_node;
		error = o9fs_alloc_vp(dvp->v_mount, onode, vpp, 0);

		/* we don't like names other than last ones not being dirs */
		if (onode->on_type != VDIR && !islast) {
			error = ENOTDIR;
			goto out;
		}

		error = o9fs_alloc_vp(dvp->v_mount, onode, vpp, 0);
	}

	printf("<lookup\n");
out: 
	return (error);
}

int
o9fs_getattr(void *v)
{
        struct vop_getattr_args /* {
                struct vnode *a_vp;
                struct vattr *a_vap;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ *ap = v;
        struct vnode *vp = ap->a_vp;
        struct vattr *vap = ap->a_vap;

        bzero(vap, sizeof(*vap));
        vattr_null(vap);
        vap->va_uid = 0;
        vap->va_gid = 0;
        vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
        vap->va_size = DEV_BSIZE;
        vap->va_blocksize = DEV_BSIZE;
        getnanotime(&vap->va_atime);
        vap->va_mtime = vap->va_atime;
        vap->va_ctime = vap->va_atime;
        vap->va_gen = 0;
        vap->va_flags = 0;
        vap->va_rdev = 0;
        /* vap->va_qbytes = 0; */
        vap->va_bytes = 0;
        /* vap->va_qsize = 0; */
        if (vp->v_flag & VROOT) {
                vap->va_type = VDIR;
                vap->va_mode = S_IRUSR|S_IWUSR|S_IXUSR|
                                S_IRGRP|S_IWGRP|S_IXGRP|
                                S_IROTH|S_IWOTH|S_IXOTH;
                vap->va_nlink = 2;
                vap->va_fileid = 2;
        } else {
                vap->va_type = VREG;
                vap->va_mode = S_IRUSR|S_IWUSR|
                                S_IRGRP|S_IWGRP|
                                S_IROTH|S_IWOTH;
                vap->va_nlink = 1;
                vap->va_fileid = VNOVAL;
        }

        return (0);
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
	printf(">readdir\n");
	return (0);
}

/* this should suffice for now */
int
o9fs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
			struct vnodeop_desc *a_desc;
			struct vnode *a_vp;
			struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	cache_purge(vp);
	if (vp->v_data != NULL) {
		FREE(vp->v_data, M_TEMP);
		vp->v_data = NULL;
	}
	return (0);
}
