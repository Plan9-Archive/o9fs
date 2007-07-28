#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/lock.h>


#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>


int
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
	
	nn = o9fs_convS2M(tx, tpkt, n);
	if (nn != n) {
		free(tpkt, M_TEMP);
		printf("size mismatch\n");
		return (EIO);
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
		return (error);
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
		return (error);
	}

	nn = O9FS_GBIT32(buf);
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
		return (error);
	}

	n = O9FS_GBIT32((u_char *)rpkt);
	nn = o9fs_convM2S(rpkt, n, rx);
	if (nn != n) {
		free(rpkt, M_TEMP);
		return (EIO);
	}

	if (rx->type == O9FS_RERROR) {
		printf("%s\n", rx->ename);
		return (EIO);
	}

	return (error);
}

/* XXX should match qid.path? */
struct o9fsfid *
o9fs_fid_lookup(struct o9fsmount *omnt, struct o9fsfid *dir, char *path)
{
	struct o9fsfid *f;
	int found, len;
	char *name[O9FS_MAXWELEM];

	/* since the name in stat doesn't have any
	 * slashes, tokenize it
	 */
	o9fs_tokenize(name, nelem(name), path, '/');

	found = 0;
	len = strlen(*name);
	
	TAILQ_FOREACH(f, &dir->child, fidlist) {
		if (strlen(f->stat->name) == len && 
			(memcmp(f->stat->name, *name, len)) == 0) {
			found = 1;
			break;
		}
	}

	return found ? f : NULL;
}

u_int
o9fs_tokenize(char *res[], u_int reslen, char *str, char delim)
{
	char *s;
	u_int i;
	
	i = 0;
	s = str;
	while (i < reslen && *s) {
		while (*s == delim)
			*(s++) = '\0';
		if (*s)
			res[i++] = s;
		while (*s && *s != delim)
			s++;
	}
	return i;
}

/*
 * Allocates a new vnode for the fid or returns a new reference to
 * an existing one if the fid had already a vnode referencing it.  The
 * resulting locked vnode is returned in *vpp.
 */	
int
o9fs_allocvp(struct mount *mp, struct o9fsfid *fid, struct vnode **vpp, u_long flags)
{
	struct vnode *vp;
	struct proc *p;
	int error;

	p = curproc;
	
	if (fid->vp != NULL) {
		vp = fid->vp;
		vget(vp, LK_EXCLUSIVE | LK_RETRY, p);
		error = 0;
		goto out;
	}
	
	/* Get a new vnode and associate it with our fid */
	error = getnewvnode(VT_O9FS, mp, o9fs_vnodeop_p, &vp);
	if (error)
		goto out;

	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (error) {
		vp->v_data = NULL;
		vp = NULL;
		goto out;
	}

	/* XXX circular stuff scares me */
	fid->vp = vp;
	if (fid->qid.type == O9FS_QTDIR) {
		vp->v_type = VDIR;
		TAILQ_INIT(&fid->child);
		printf("%d dir head inited\n", fid->fid);
	}

	vp->v_data = fid;
	vp->v_flag |= flags;

out:
	*vpp = fid->vp = vp;
	return error;
}

/* files are inserted in the dir child list */
int
o9fs_insertfid(struct o9fsfid *fid, struct o9fsfid *dir)
{

	if (fid->stat == NULL || dir->stat == NULL) {
		printf("child or dir don't have stat info\n");
		return 1;
	}
	
	TAILQ_INSERT_TAIL(&dir->child, fid, fidlist);

	return 0;
}

void
o9fs_dumpchildlist(struct o9fsfid *fid)
{
	struct o9fsfid *f;

	if (fid->stat == NULL)
		return;

	printf("child list for %s\n", fid->stat->name);
	TAILQ_FOREACH(f, &fid->child, fidlist)
		printf("%d %s\n", f->fid, f->stat->name);
}
