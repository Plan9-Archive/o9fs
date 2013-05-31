#	$OpenBSD: Makefile,v 1.2 2005/03/29 18:28:49 pedro Exp $

PROG=	mount_o9fs
SRCS=	mount_o9fs.c getmntopts.c
NOMAN=

MOUNT= /usr/src/sbin/mount
CFLAGS+= -I${MOUNT} -I..
.PATH:	${MOUNT}

.include <bsd.prog.mk>