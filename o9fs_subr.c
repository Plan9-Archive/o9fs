#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/un.h>
#include <sys/lock.h>

#include <netinet/in.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>


void *
o9fs_rpc(struct o9fsmount *omnt, struct o9fsfcall *tx, struct o9fsfcall *rx)
{
	int n, nn, error;
	void *tpkt;
	u_char buf[4], *rpkt;
	struct uio auio;
	struct iovec aiov;
	struct proc *p;
	
	p = curproc;
	error = 0;
	
	/* calculate the size and allocate a new fcall */
	n = o9fs_sizeS2M(tx);
	tpkt = malloc(n, M_TEMP, M_WAITOK);
	if (tpkt == NULL)
		return (NULL);
	
	nn = o9fs_convS2M(tx, tpkt, n);
	if (nn != n) {
		free(tpkt, M_TEMP);
		printf("size mismatch\n");
		return (NULL);
	}

	aiov.iov_base = tpkt;
	aiov.iov_len = auio.uio_resid = n;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = p;

	error = omnt->io->write(omnt, &auio);
	if (error) {
		printf("failed sending 9p message\n");
		return (NULL);
	}

	/* get msg size */
	aiov.iov_base = buf;
	aiov.iov_len = auio.uio_resid = 4;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;

	omnt->io->read(omnt, &auio);
	if (error) {
		printf("receive error\n");
		return (NULL);
	}

	nn = O9FS_GBIT32(buf);
	printf("msg size %d\n", nn);
	rpkt = (void *) malloc(nn + 4, M_TEMP, M_WAITOK);

	/* read the rest of the msg */
	O9FS_PBIT32(rpkt, nn);

	aiov.iov_base = rpkt + 4;
	aiov.iov_len = auio.uio_resid = nn -4;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	
	error = omnt->io->read(omnt, &auio);
	if (error) {
		printf("receive error\n");
		return (NULL);
	}

	n = O9FS_GBIT32((u_char *)rpkt);
	nn = o9fs_convM2S(rpkt, n, rx);
	if (nn != n) {
		free(rpkt, M_TEMP);
		return (NULL);
	}

	if (rx->type == O9FS_RERROR) {
		printf("%s\n", rx->ename);
		return (NULL);
	}

	return (rpkt);
}
	
	
int
o9fs_alloc_node(struct o9fsnode **node, enum vtype type)
{
	struct o9fsnode *nnode;
	
	printf(">alloc_node\n");
	nnode = (struct o9fsnode *) malloc(sizeof(struct o9fsnode), M_TEMP, M_WAITOK);
	if (nnode == NULL)
		return (ENOSPC);

	nnode->on_vnode = NULL;
	nnode->on_type = type;

	switch (type) {
	case VBAD:
	case VBLK:
	case VCHR:
	case VFIFO:
	case VLNK:
	case VNON:
	case VREG:
	case VSOCK:
		break;
	case VDIR:
		TAILQ_INIT(&nnode->on_dir);
		break;
	}
	
	*node = nnode;
	printf("<alloc_node\n");
	return (0);
}

/*
 * Allocates a new vnode for the node node or returns a new reference to
 * an existing one if the node had already a vnode referencing it.  The
 * resulting locked vnode is returned in *vpp.
 */	
int
o9fs_alloc_vp(struct mount *mp, struct o9fsnode *node, struct vnode **vpp, u_long flags)
{
	struct vnode *vp;
	struct proc *p;
	int error;

	p = curproc;
	
	if (node->on_vnode != NULL) {
		vp = node->on_vnode;
		vget(vp, LK_EXCLUSIVE | LK_RETRY, p);
		error = 0;
		goto out;
	}
	
	/* Get a new vnode and associate it with our node */
	error = getnewvnode(VT_O9FS, mp, o9fs_vnodeop_p, &vp);
	if (error)
		goto out;
	
	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (error) {
		vp->v_data = NULL;
		vp = NULL;
		goto out;
	}
	
	vp->v_data = node;
	vp->v_type = node->on_type;
	vp->v_flag |= flags;
	node->on_flags = vp->v_flag;

	error = 0;

out:
	*vpp = node->on_vnode = vp;
	return (error);
}

/*
 * Allocates a new directory entry for the node node with a name of name.
 * The new directory entry is returned in *de.
 */
int
o9fs_alloc_dirent(struct o9fsnode *node, const char *name, 
				uint16_t namelen, struct o9fsdirentx **de)
{
	struct o9fsdirentx *nde;

	printf(">alloc_dirent\n");
	nde = (struct o9fsdirentx *) malloc(sizeof(struct o9fsdirentx), M_TEMP, M_WAITOK);
	if (nde == NULL)
		return (ENOSPC);

	nde->od_namelen = namelen;	/* am i being too overzealous? */
	strlcpy(nde->od_name, name, namelen);
	nde->od_node = node;
	
	*de = nde;
	printf("<alloc_dirent\n");
	return (0);
}

void
o9fs_dir_attach(struct vnode *vp, struct o9fsdirentx *de)
{
	struct o9fsnode *dnode;

	printf(">dir_attach\n");
	dnode = VTO9(vp);
	TAILQ_INSERT_TAIL(&dnode->on_dir, de, od_entries);
	printf("<dir_attach\n");
}

int
o9fs_create_file(struct vnode *dvp, struct vnode **vpp, struct vattr *vap,
	struct componentname *cnp)
{
	struct o9fsdirentx *de;
	struct o9fsnode *dnode;
	struct o9fsnode *node;
	int error;

	dnode = VTO9(dvp);
	*vpp = NULL;

	/* Allocate a node that represents the new file. */
	error = o9fs_alloc_node(&node, VREG);
	if (error)
		goto out;

	/* Allocate a directory entry that points to the new file. */
	error = o9fs_alloc_dirent(node, cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error)
		goto out;

	/* Allocate a vnode for the new file. */
	error = o9fs_alloc_vp(dvp->v_mount, node, vpp, 0);
	if (error)
		goto out;

	/* Now that all required items are allocated, we can proceed to
	 * insert the new node into the directory, an operation that
	 * cannot fail. */
	o9fs_dir_attach(dvp, de);

out:
	vput(dvp);
	printf("create file will return with %d\n", error);
	return (error);
}

struct o9fsdirentx *
o9fs_dir_lookup(struct o9fsnode *node, struct componentname *cnp)
{
	short found;
	struct o9fsdirentx *de;
	
	found = 0;
	TAILQ_FOREACH(de, &node->on_dir, od_entries) {
		if (de->od_namelen == (uint16_t)cnp->cn_namelen &&
			memcmp(de->od_name, cnp->cn_nameptr, de->od_namelen) == 0) {
			found = 1;
			break;
		}
	}
	
	return (found?de:NULL);
}


