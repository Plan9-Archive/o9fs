#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netinet/in.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>


int
o9fs_tcp_connect(struct o9fsmount *omnt)
{
	struct socket *so;
	struct mbuf *nam;
	struct sockaddr *saddr;
	struct sockaddr_in *sin;
	char *str;
	int error, s;

	if (omnt == NULL)
		return EINVAL;

	so = (struct socket *) malloc(sizeof(struct socket), M_TEMP, M_WAITOK);
	error = sockargs(&nam, omnt->om_saddr, omnt->om_saddrlen, MT_SONAME);
	if (error)
		return error;

	saddr = mtod(nam, struct sockaddr *);

	sin = (struct sockaddr_in *) saddr;
	str = inet_ntoa(sin->sin_addr);
	printf("%s\n", str);

	error = socreate(PF_INET, &so, SOCK_STREAM, IPPROTO_TCP);
	if (error)
		return error;

	error = soconnect(so, nam);
	if (error)
		return error;

	/* ripped from nfs */
	/*
	 * Wait for the connection to complete. Cribbed from the
	 * connect system call but with the wait timing out so
	 * that interruptible mounts don't hang here for a long time.
	 */
	s = splsoftnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		(void) tsleep((caddr_t)&so->so_timeo, PSOCK,
			"o9fscon", 2 * hz);
		if ((so->so_state & SS_ISCONNECTING) &&
			 so->so_error == 0) {
			so->so_state &= ~SS_ISCONNECTING;
			splx(s);
			goto bad;
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		goto bad;
	}
	splx(s);
	
	omnt->om_so = so;
	return error;

bad:
	if (so) {
		soshutdown(so, SHUT_WR);
		soclose(so);
	}
	return error;
}

int
o9fs_tcp_write(struct o9fsmount *omnt, struct uio *uio)
{
	struct socket *so;
	int error;

	if ((omnt == NULL) || (uio == NULL))
		return EINVAL;

	so = omnt->om_so;

	error = sosend(so, (struct mbuf *)0, uio,
					(struct mbuf *)0, (struct mbuf *)0, 0);

	/* XXX shouldn't we return the # of bytes written? */
	return error;
}
	
int
o9fs_tcp_read(struct o9fsmount *omnt, struct uio *uio)
{
	struct socket *so;
	int error;

	if ((omnt == NULL) || (uio == NULL))
		return (EINVAL);

	so = omnt->om_so;

	error = soreceive(so, (struct mbuf **)0, uio,
					(struct mbuf **)0, (struct mbuf **)0, 0);

	/* XXX shouldn't we return the # of bytes read? */
	return error;
}

int
o9fs_tcp_close(struct o9fsmount *omnt)
{
	struct socket *so;

	if (omnt == NULL)
		return EINVAL;

	so = omnt->om_so;
	soshutdown(so, SHUT_RDWR);
	soclose(so);
	return 0;
}

struct o9fs_io io_tcp = {
	.connect= o9fs_tcp_connect,
	.write	= o9fs_tcp_write,
	.read	= o9fs_tcp_read,
	.close	= o9fs_tcp_close
};
