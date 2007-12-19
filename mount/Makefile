#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:09 deraadt Exp $

PROG=	mount_o9fs
SRCS=	mount_o9fs.c getmntopts.c
MAN=	mount_o9fs.8

MOUNT=	/usr/src/sbin/mount
CFLAGS+= -I${MOUNT} -I/sys
LDFLAGS+=-static
.PATH:	${MOUNT}

.include <bsd.prog.mk>
