#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/lock.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

enum {
	fidchunk = 64
};

struct o9fsfid *
o9fs_getfid(struct o9fsmount *omnt)
{
        struct o9fsfid *f;
        struct o9fs *fs;
        int i;
        
        fs = &omnt->om_o9fs;

        if (fs->freefid == NULL) {
				f = (struct o9fsfid *) malloc(sizeof(struct o9fsfid) * fidchunk,
                	M_O9FS, M_WAITOK);
				for (i = 0; i < fidchunk; i++) {
                        f[i].fid = fs->nextfid++;
						f[i].next = &f[i+1];
						f[i].fs = fs;
						f[i].stat = NULL;
						f[i].vp = NULL;
						f[i].opened = 0;
                }
				f[i-1].next = NULL;
				fs->freefid = f;
        }
		rw_init(&f->rwl, "fidlock");
        f = fs->freefid;
        fs->freefid = f->next;
        f->offset = 0;
        f->mode = -1;
        f->qid.path = 0;
        f->qid.vers = 0;
        f->qid.type = 0;
        return f;
}

void
o9fs_putfid(struct o9fsmount *omnt, struct o9fsfid *f)
{
		struct o9fs *fs;
        
		fs = &omnt->om_o9fs;
		if (f->stat)
			free(f->stat, M_TEMP);
		f->stat = NULL;
		if (f->vp)
			f->vp = NULL;
		f->next = fs->freefid;
		fs->freefid = f;
}

int
o9fs_rpc(struct o9fsmount *omnt, struct o9fsfcall *tx, struct o9fsfcall *rx, 
	void **freep)
{
	int n, nn, error;
	void *tpkt, *rpkt;
	u_char buf[4];
	struct uio auio;
	struct iovec aiov;
	struct proc *p;
	
	p = curproc;
	error = 0;
	
	/* calculate the size and allocate a new fcall */
	n = o9fs_sizeS2M(tx);
//	printf("tpkt = malloc(%d)\n", n);
	tpkt = malloc(n, M_O9FS, M_WAITOK);

	if (freep)
		*freep = NULL;

	nn = o9fs_convS2M(tx, tpkt, n);
	if (nn != n) {
		free(tpkt, M_O9FS);
		printf("size mismatch\n");
		return EIO;
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
		return error;
	}

	/* get msg size */
	aiov.iov_base = buf;
	aiov.iov_len = auio.uio_resid = 4;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;

	error = omnt->io->read(omnt, &auio);
	if (error) {
		printf("receive error\n");
		return error;
	}

	n = O9FS_GBIT32(buf);
//	printf("buf = %s\n", buf);
//	printf("rpkt = malloc(%d)\n", n+4);
	rpkt = malloc(n + 4, M_O9FS, M_WAITOK);

	/* read the rest of the msg */
	O9FS_PBIT32((u_char *)rpkt, n);

	aiov.iov_base = rpkt + 4;
	aiov.iov_len = auio.uio_resid = n - 4;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	
	error = omnt->io->read(omnt, &auio);
	if (error) {
		printf("receive error\n");
		return error;
	}
	free(tpkt, M_O9FS);

	n = O9FS_GBIT32((u_char *) rpkt);
	nn = o9fs_convM2S(rpkt, n, rx);
	if (nn != n) {
		free(rpkt, M_O9FS);
		return EIO;
	}

	if (rx->type == O9FS_RERROR) {
		free(rpkt, M_O9FS);
		printf("%s\n", rx->ename);
		return -1;
	}

	if (freep)
		*freep = rpkt;
	else
		free(rpkt, M_O9FS);

	return error;
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
	
/*	TAILQ_FOREACH(f, &dir->child, fidlist) {
		if (strlen(f->stat->name) == len && 
			(memcmp(f->stat->name, *name, len)) == 0) {
			found = 1;
			break;
		}
	} */
	
	for (f = omnt->om_o9fs.rootfid; f && f->stat; f = f->next) {
//		printf("fidlookup: %s\n", f->stat->name);
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
ed in *vpp.
 */	
int
o9fs_allocvp(struct mount *mp, struct o9fsfid *fid, struct vnode **vpp, u_long flags)
{
	struct vnode *vp;
	struct proc *p;
	int error;

	p = curproc;
	
	if (fid->vp) {
		vget(fid->vp, LK_EXCLUSIVE | LK_RETRY, p);
		error = 0;
		vp = fid->vp;
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
//		TAILQ_INIT(&fid->child);
	}
	else
		vp->v_type = VREG;

	vp->v_data = fid;
	vp->v_flag |= flags;

out:
	*vpp = fid->vp = vp;
	return error;
}

void
o9fs_freevp(struct vnode *vp)
{
	struct o9fsfid *f;
	
	f = VTO9(vp);
	f->vp = NULL;
	vp->v_data = NULL;
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

/*
int
o9fs_removefid(struct o9fsfid *fid, struct o9fsfid *dir)
{
	if (fid->stat == NULL || dir->stat == NULL) {
		printf("invalid fid or dir\n");
		return 1;
	}

	TAILQ_REMOVE(&dir->child, fid, fidlist);
	return 0;
}
*/

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

long
o9fs_dirpackage(u_char *buf, long ts, struct o9fsstat **d)
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
		if ((o9fs_statcheck(&buf[i], m)) < 0)
			break;
		ss += m;
		n++;
	}

	if (i != ts) {
		printf("i != ts\n");
		return -1;
 	}

	*d = malloc(n * sizeof(struct o9fsstat) + ss, M_O9FS, M_WAITOK);

	/*
	 * then convert all buffers
	 */
	s = (char *) *d + n * sizeof(struct o9fsstat);
	nn = 0;
	for (i = 0; i < ts; i += m) {
		m = O9FS_BIT16SZ + O9FS_GBIT16((u_char *) &buf[i]);
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
o9fs_ptoumode(int mode)
{
	int umode;
	
	umode = mode & (0777|O9FS_DMDIR);

	if ((mode & O9FS_DMDIR) == O9FS_DMDIR)
		umode |= S_IFDIR;

	return umode;
}

int
o9fs_utopmode(int mode)
{
	int pmode;

	pmode = mode & (0777|S_IFDIR);

	if ((mode & S_IFDIR) == S_IFDIR)
		pmode |= O9FS_DMDIR;

	return pmode;
}

int
o9fs_uflags2omode(int uflags)
{
	int omode;
	
	switch(uflags & O_ACCMODE) {
	case FREAD:
		printf("uf: READ\n");
		omode = O9FS_OREAD;
		break;
	case FWRITE:
		printf("uf: WRITE\n");
		omode = O9FS_OWRITE;
		break;
	case (FREAD|FWRITE):
		printf("uf: RDWR\n");
		omode = O9FS_ORDWR;
		break;
	}

	/* XXX u9fs specific? */
	if (uflags & O_CREAT) {
		printf("uf: creating\n");
		omode = O9FS_OEXEC;
	}

	return omode;
}
