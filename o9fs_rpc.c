#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/file.h>

#include "o9fs.h"
#include "o9fs_extern.h"

static long
rdwr(struct o9fsmount *omnt, void *buf, long count, off_t *offset, int write)
{
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long cnt;
	int error;

	error = 0;
	fp = omnt->om_fp;
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

int
o9fs_rpc(struct o9fsmount *omnt, struct o9fsfcall *tx, struct o9fsfcall *rx)
{
	int n, nn, error;
	u_char buf[4], *tpkt;
	struct uio auio;
	struct iovec aiov;
	struct proc *p;
	struct o9fs *fs;
	struct file *fp;
	
	p = curproc;
	error = 0;
	fs = &omnt->om_o9fs;
	fp = omnt->om_fp;

	/* calculate the size and allocate a new fcall */
	n = o9fs_sizeS2M(tx);
	tpkt = malloc(n, M_O9FS, M_WAITOK);
	
	nn = o9fs_convS2M(tx, tpkt, n);
	if (nn != n) {
		free(tpkt, M_O9FS);
		printf("size mismatch\n");
		return EIO;
	}

	n = rdwr(omnt, tpkt, nn, &fp->f_offset, 1);
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;

	n = rdwr(omnt, buf, O9FS_BIT32SZ, &fp->f_offset, 0);
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

	n = rdwr(omnt, fs->rpc + O9FS_BIT32SZ, n - O9FS_BIT32SZ, &fp->f_offset, 0);
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;
	
	free(tpkt, M_O9FS);

	n = O9FS_GBIT32((u_char *) fs->rpc);
	nn = o9fs_convM2S(fs->rpc, n, rx);
	if (nn != n) {
		printf("size mismatch\n");
		return EIO;
	}

	if (rx->type == O9FS_RERROR) {
		DPRINT("%s\n", rx->ename);
		return -1;
	}
	return error;
}
