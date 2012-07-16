#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"
#include "o9fs.h"

const struct mntopt opts[] = { MOPT_STDOPTS, { NULL } };

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-v] [-o options] type!address[!port] mountpoint\n", __progname);
	exit(1);
}

int
connunix(char *path)
{
	int s;
	struct sockaddr_un channel;

	s = socket(PF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		err(1, "Failed to create UNIX socket");

	bzero(&channel, sizeof(channel));
	channel.sun_family = PF_UNIX;
	channel.sun_len = strlen(path);
	strlcpy(channel.sun_path, path, sizeof(channel.sun_path));

	if ((connect(s, (struct sockaddr *) &channel, sizeof(channel))) < 0)
		err(1, "Failed to connect");
	return s;
}

int
conninet(char *host, int port)
{
	int s;
	struct hostent *hp;
	struct sockaddr_in con;

	hp = gethostbyname(host);
	if (hp == NULL)
		err(1, "Failed to resolve name %s", host);

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0)
		err(1, "Failed to create INET socket");

	bzero(&con, sizeof(con));
	con.sin_family = AF_INET;
	memmove(&con.sin_addr.s_addr, hp->h_addr, hp->h_length);
	con.sin_port = htons(port);

	if((connect(s, (struct sockaddr *) &con, sizeof(struct sockaddr_in))) < 0)
		err(1, "Failed to connect");
	return s;
}

/*
 * Parse either unix or network address argument and connect accordingly.
 * Return the connected file descriptor.
 */
int
dial(char *arg)
{
	char *p, addr[MAXPATHLEN];
	int i, port;

	if (arg == NULL)
		return -1;

	p = arg;
	i = 0;
	while (*p != '\0' && *p != '!')
		addr[i++] = *p++;
	addr[i] = '\0';

	if (*p == '!') {
		p++;
		port = atoi(p);
		return conninet(addr, port);
	}

	return connunix(addr);
}
	
	
int
main(int argc, char *argv[])
{
	struct o9fs_args args;
	char node[MAXPATHLEN];
	int ch, flags;

	args.verbose = 0;
	flags = 0;
	while ((ch = getopt(argc, argv, "o:v")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, opts, &flags);
			break;
		case 'v':
			args.verbose = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	args.hostname = argv[0];
	args.fd = dial(argv[0]);
	if (args.fd < 0)
		err(1, "Failed to dial");

	if (realpath(argv[1], node) == NULL)
		err(1, "realpath %s", argv[1]);

	if (mount(MOUNT_O9FS, node, flags, &args) < 0)
		err(1, "mount");
	return 0;
}

