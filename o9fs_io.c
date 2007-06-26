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
o9fs_tcp_open(struct o9fsmount *mp)
{
	struct socket *so;
	struct mbuf *nam;
	struct sockaddr *saddr;
	struct sockaddr_in *sin;
	char *str;
	int error, s;

	so = (struct socket *) malloc(sizeof(struct socket), M_TEMP, M_WAITOK);
	error = sockargs(&nam, mp->om_saddr, mp->om_saddrlen, MT_SONAME);
	if (error)
		return (error);
	printf("MT_SONAME set\n");

	saddr = mtod(nam, struct sockaddr *);
	printf("saddr set\n");

	sin = (struct sockaddr_in *) saddr;
	str = inet_ntoa(sin->sin_addr);
		printf("%s\n", str);

	error = socreate(PF_INET, &so, SOCK_STREAM, IPPROTO_TCP);
	if (error)
		return (error);
	printf("socket created\n");

	error = soconnect(so, nam);
	if (error)
		return (error);

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
	
	mp->om_so = so;
	return (error);

bad:
	if (so) {
		soshutdown(so, SHUT_WR);
		printf("shutdown\n");
		soclose(so);
		printf("close\n");
	}
	return (error);
}
