LKM=o9fs
COMBINED=$(LKM).o
NOMAN=

SRCS=\
	o9fs_9p.c\
	o9fs_convM2D.c\
	o9fs_lkm.c\
	o9fs_subr.c\
	o9fs_vfsops.c\
	o9fs_vnops.c

.include <bsd.lkm.mk>

