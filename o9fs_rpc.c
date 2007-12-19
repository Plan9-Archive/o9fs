#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

u_int
o9fs_recvmsg(struct o9fsmount *omnt, struct o9fsmsg *m)
{
	u_int msize, size;
	int error;
	struct uio auio;
	struct iovec aiov;

	m->m_mode = O9FS_MLOAD;
	m->m_cur = m->m_base;
	m->m_end = m->m_base + m->m_size;
	
	aiov.iov_base = m->m_cur;
	aiov.iov_len = auio.uio_resid = 4;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = curproc;

	error = omnt->io->read(omnt, &auio);
	if (error) {
		printf("failed receiving 9p message\n");
		return error;
	}

	m->m_cur = m->m_base;
	o9fs_msgdword(m, &msize);
//	printf("recvmsg: msize=%d\n", msize);

	size = msize - 4;
	if (o9fs_msgoverflow(m, size)) {
		printf("recvmsg overflow\n");
		return -1;
	}

	aiov.iov_base = m->m_cur;
	aiov.iov_len = auio.uio_resid = size;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = curproc;

	error = omnt->io->read(omnt, &auio);
	if (error) {
		printf("failed receiving 9p message\n");
		return error;
	}

/* XXX libixp seem to work with this
	m->m_end = m->m_cur; */
	return msize;
}
	
u_int
o9fs_sendmsg(struct o9fsmount *omnt, struct o9fsmsg *m)
{
	struct uio auio;
	struct iovec aiov;
	int error;

	m->m_cur = m->m_base;
	aiov.iov_base = m->m_cur;
	aiov.iov_len = auio.uio_resid = m->m_end - m->m_cur;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = curproc;

	error = omnt->io->write(omnt, &auio);
	if (error) {
		printf("failed sending 9p message\n");
		return error;
	}
	return m->m_cur - m->m_base;
}

int
o9fs_rpc(struct o9fsmount *omnt, struct o9fsfcall *tx, struct o9fsfcall *rx)
{
	struct o9fsmsg *m;
	int error;

	m = o9fs_msgalloc(8192);
	
	error = o9fs_fcalltomsg(m, tx);
	if (error == 0)
		return -1;
	
//	printf("rpc: fid=%d\n", rx->fid);
	/* what should I do with this return? */
	o9fs_sendmsg(omnt, m);
	
	bzero(m->m_base, m->m_size);
	error = o9fs_recvmsg(omnt, m);
	if (error == 0)
		return -1;

	error = o9fs_msgtofcall(m, rx);
	if (error == 0)
		return -1;

	if (rx->type == O9FS_RERROR) {
		printf("%s\n", rx->ename);
		goto fail;
	}
	
/*	if (rx->type != (tx->type^1)) {
		printf("mismatched fcall received\n");
		goto fail;
	} */

	free(m->m_base, M_O9FS);
	free(m, M_O9FS);
	return 0;

fail:
	o9fs_freefcall(rx);
	free(m->m_base, M_O9FS);
	free(m, M_O9FS);
	return -1;
}
