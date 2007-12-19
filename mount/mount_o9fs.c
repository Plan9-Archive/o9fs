#define O9FS 
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/vnode.h>

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <miscfs/o9fs/o9fs.h>

#include "mntopts.h"

const struct mntopt opts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-o options] server node port\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct o9fs_args args;
	char node[MAXPATHLEN];
	int ch, flags;
	struct sockaddr_in saddr;
	struct hostent *hp;

	flags = 0;

	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, opts, &flags);
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 3)
		usage();

	/*
	 * Handle an internet host address
	 */
	if (inet_aton(argv[0], &saddr.sin_addr) == 0) {
		hp = gethostbyname(argv[0]);
		if (hp == NULL) {
			warnx("can't resolve address for host %s", argv[0]);
			return (0);
		}
		memcpy(&saddr.sin_addr, hp->h_addr, hp->h_length);
	}

	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(atoi(argv[2]));

	args.saddr = (struct sockaddr *) &saddr;
	args.saddrlen = sizeof(saddr);
	args.hostname = argv[0];
	
	if (realpath(argv[1], node) == NULL)
		err(1, "realpath %s", argv[1]);

	printf("%s\n", inet_ntoa(saddr.sin_addr));

	if (mount(MOUNT_O9FS, node, flags, &args) < 0)
		err(1, "mount");

	exit(0);
}

