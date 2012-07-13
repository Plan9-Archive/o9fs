#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/file.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 0,
};

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
		printf("%.*s\n", O9FS_GBIT16(fs->inbuf + Minhd), fs->inbuf + Minhd + 2);
		return -1;
	}

	return len;
}

int
o9fs_rpc(struct o9fs *fs, struct o9fsfcall *tx, struct o9fsfcall *rx)
{
	int n, nn, error;
	u_char buf[4];
	struct proc *p;
	struct file *fp;
	
	p = curproc;
	error = 0;
	fp = fs->servfp;

	/* calculate the size and allocate a new fcall */
	n = o9fs_sizeS2M(tx);
	nn = o9fs_convS2M(tx, fs->rpc, n);
	if (nn != n) {
		printf("size mismatch\n");
		return EIO;
	}

	n = rdwr(fs, fs->rpc, nn, &fp->f_offset, 1);
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;

	o9fs_freefcall(tx);
//	bzero(fs->rpc, 8192); /* xxx */
	n = rdwr(fs, buf, O9FS_BIT32SZ, &fp->f_offset, 0);
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;

	n = O9FS_GBIT32(buf);
	if (n <= O9FS_BIT32SZ) {
		printf("bogus message\n");
		return -1;
	}

	/* read the rest of the msg */
	O9FS_PBIT32(fs->rpc, n);

	n = rdwr(fs, fs->rpc + O9FS_BIT32SZ, n - O9FS_BIT32SZ, &fp->f_offset, 0);
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;
	
	n = O9FS_GBIT32((u_char *) fs->rpc);
	nn = o9fs_convM2S(fs->rpc, n, rx);
	if (nn != n) {
		printf("size mismatch\n");
		return EIO;
	}

	if (rx->type == O9FS_RERROR) {
		DBG("%s\n", rx->ename);
		return -1;
	}
	return error;
}
