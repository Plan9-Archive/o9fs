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
#include <sys/queue.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 0,
};

enum {
	Chunk = 64
};

uint8_t verbose;

void
o9fs_dump(u_char *buf, long n)
{
	long i;

	if (buf == NULL)
		return;

	for (i = 0; i < n; i++) {
		printf("%02x", buf[i]);
		if (i % 16 == 15)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");
}

struct o9fid *
o9fs_getfid(struct o9fs *fs)
{
	struct o9fid *f;

	if (TAILQ_EMPTY(&fs->freeq)) {
		f = (struct o9fid *) malloc(sizeof(struct o9fid), M_O9FS, M_WAITOK);
		f->fid = fs->nextfid++;
		TAILQ_INSERT_TAIL(&fs->activeq, f, next);
	} else {
		f = TAILQ_FIRST(&fs->freeq);
		TAILQ_REMOVE(&fs->freeq, f, next);
	}

	f->ref = 1;
	f->parent = NULL;
	f->offset = 0;
	f->mode = -1;
	f->iounit = 0;
	f->qid.path = 0;
	f->qid.vers = 0;
	f->qid.type = 0;
	return f;
}

void
o9fs_putfid(struct o9fs *fs, struct o9fid *f)
{
	if (f == NULL)
		panic("o9fs_putfid: cannot put a nil fid");

	TAILQ_REMOVE(&fs->activeq, f, next);
	TAILQ_INSERT_TAIL(&fs->freeq, f, next);
}

char *
o9fs_putstr(char *buf, char *s)
{
	long n;

	if (buf == NULL || s == NULL)
		panic("putstring was given nil pointers");

	n = strlen(s);
	O9FS_PBIT16(buf, n);
	memcpy(buf + 2, s, n);
	return buf + 2 + n;
}

char *
o9fs_getstr(char *buf)
{
	uint16_t n;
	char *s;

	if (buf == NULL)
		panic("getstring was given a nul pointer");

	n = O9FS_GBIT16(buf);
	s = malloc(n + 1, M_O9FS, M_WAITOK);
	memcpy(s, buf + 2, n);
	s[n] = '\0';
	return s;
}	

int
o9fs_allocvp(struct mount *mp, struct o9fid *f, struct vnode **vpp, u_long flag)
{
	struct vnode *vp;
	int error;
	DIN();

	error = getnewvnode(VT_O9FS, mp, &o9fs_vops, vpp);
	if (error) {
		*vpp = NULL;
		printf("error in getnewvnode %d\n", error);
		DRET();
		return error;
	}
	vp = *vpp;

	if (f->qid.type == O9FS_QTDIR)
		vp->v_type = VDIR;
	else
		vp->v_type = VREG;

	vp->v_data = f;
	vp->v_flag = flag;
	printvp(vp);
	DRET();
	return 0;
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
	
	omode = 0;
	switch(uflags & O_ACCMODE) {
	case FREAD:
		omode = O9FS_OREAD;
		break;
	case FWRITE:
		omode = O9FS_OWRITE;
		break;
	case (FREAD|FWRITE):
		omode = O9FS_ORDWR;
		break;
	}

	/* XXX u9fs specific? */
	if (uflags & O_CREAT)
		omode |= O9FS_OEXEC;
	if (uflags & O_TRUNC)
		omode |= O9FS_OTRUNC;

	return omode;
}

void *
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

void
_printvp(struct vnode *vp)
{
	struct o9fid *f;

	f = VTO9(vp);
	if (vp == NULL || f == NULL) {
		printf("vp %p fid %p\n",  vp, f);
		return;
	}
	printf("[%p] %p fid %d ref %d qid (%.16llx %lu %d) mode %d iounit %ld\n", vp, f, f->fid, f->ref, f->qid.path, f->qid.vers, f->qid.type, f->mode, f->iounit);
}

static long
rdwr(struct o9fs *fs, void *buf, long count, off_t *offset, int write)
{
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long cnt;
	int error;

	error = 0;
	fp = fs->servfp;
	aiov.iov_base = buf;
	cnt = aiov.iov_len = auio.uio_resid = count;
	auio.uio_iov = &aiov;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = curproc;

	if (write) {
		auio.uio_rw = UIO_WRITE;
		error = (*fp->f_ops->fo_write)(fp, offset, &auio, fp->f_cred);
		cnt -= auio.uio_resid;

		fp->f_wxfer++;
		fp->f_wbytes += cnt;
	} else {
		auio.uio_rw = UIO_READ;
		error = (*fp->f_ops->fo_read)(fp, offset, &auio, fp->f_cred);
		cnt -= auio.uio_resid;

		fp->f_rxfer++;
		fp->f_rbytes += cnt;
	}
	if (error)
		return -error;
	return cnt;
}

long
o9fs_mio(struct o9fs *fs, u_long len)
{
	long n;

	n = rdwr(fs, fs->outbuf, len, &fs->servfp->f_offset, 1);
	if (n <= 0)
		return n;

	n = rdwr(fs, fs->inbuf, 4, &fs->servfp->f_offset, 0);
	if (n <= 0) {
		printf("o9fs_mio: Error reading message size\n");
		return n;
	}
	
	len = O9FS_GBIT32(fs->inbuf);
	if (len <= 4) {
		printf("R-message with length < 4\n");
		return -1;
	}

	n = rdwr(fs, fs->inbuf + Offtype, len - Offtype, &fs->servfp->f_offset, 0);
	if (n <= 0)
		return n;

	if (O9FS_GBIT8(fs->inbuf + Offtype) == O9FS_RERROR) {
		if (verbose)
			printf("%.*s\n", O9FS_GBIT16(fs->inbuf + Minhd), fs->inbuf + Minhd + 2);
		return -1;
	}

	return len;
}

uint16_t
o9fs_tag(void)
{
	return (uint16_t)arc4random()%0xFFFF;
}

uint32_t
o9fs_sanelen(struct o9fs *fs, uint32_t n)
{
	if (n > fs->msize - Maxhd)
		n = fs->msize - Maxhd;
	return n;
}
