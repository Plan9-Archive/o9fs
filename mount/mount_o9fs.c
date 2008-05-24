#define O9FS 
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
	fprintf(stderr, "usage: %s [-o options] type!address[!port] mountpoint\n", __progname);
	exit(1);
}

int
connunix(char *path)
{
        int s;
        struct sockaddr_un channel;

        s = socket(PF_UNIX, SOCK_STREAM, 0);
        if (s < 0)
                err(1, "socket");

        bzero(&channel, sizeof(channel));
        channel.sun_family = PF_UNIX;
        channel.sun_len = strlen(path);
        strlcpy(channel.sun_path, path, 104); /* XXX openbsd specific? */

        if ((connect(s, (struct sockaddr *) &channel, sizeof(channel))) < 0)
                err(1, "connect");

        return s;
}

int
conninet(char *host, int port)
{
        int s;
        struct hostent *hp;
        struct sockaddr_in con;

        hp = gethostbyname(host);
        if (!hp)
                err(1, "gethostbyname");

        s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s < 0)
                err(1, "socket");

        bzero(&con, sizeof(con));
        con.sin_family = AF_INET;
        memcpy(&con.sin_addr.s_addr, hp->h_addr, hp->h_length);
        con.sin_port = htons(port);
        if((connect(s, (struct sockaddr *) &con, sizeof(struct sockaddr_in))) < 0)
                err(1, "connect");
        return s;
}

struct addr9p {
        char type[4];           /* unix or net */
        char addr[256];         /* address or path */
        int port;				/* port if net */
};

struct addr9p *
parseaddr(char *arg)
{
        struct addr9p *a;
        char *p;
        int i;

        p = arg;
		if (!p)
			return NULL;
        a = malloc(sizeof(struct addr9p));
        if (!a)
                err(1, "parseaddr: malloc");

        /* get type */
        for (i = 0; *p && *p != '!'; i++)
                a->type[i] = *p++;
        a->type[i] = 0;
        p++; /* skip ! */

        /* get address */
        for (i = 0; *p && *p != '!'; i++)
                a->addr[i] = *p++;
        a->addr[i] = 0;
        p++; /* skip ! */

        p[5] = 0; /* port are 5 chars max */
        a->port = -1;
        if (!strcmp(a->type, "net"))
                a->port = atoi(p);
        return a;
}

	
int
dial(char *addr)
{
	struct addr9p *a;
	
	a = parseaddr(addr);
	if (a->port != -1)
		return conninet(a->addr, a->port);
	return connunix(a->addr);
}
	
	
int
main(int argc, char *argv[])
{
	struct o9fs_args args;
	char node[MAXPATHLEN], host[MAXPATHLEN];
	int ch, flags;

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

	if (argc != 2)
		usage();

	strlcpy(host, argv[0], MAXPATHLEN);
	args.hostname = host;
	args.fd = dial(host);

	printf("args.hostname = %s\n", args.hostname);
	printf("args.fd = %d\n", args.fd);

	if (realpath(argv[1], node) == NULL)
		err(1, "realpath %s", argv[1]);

	if (mount(MOUNT_O9FS, node, flags, &args) < 0)
		err(1, "mount");
	printf("mounted ok\n");
	return 0;
}

