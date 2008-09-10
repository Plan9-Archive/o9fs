tgz: 
	(cd .. && tar zcvf o9fs.tgz o9fs/ && sync)

kern:
	(cd /sys/arch/i386/compile/GENERIC && sudo sh -c "make && cp bsd /usr/iru/src/gsoc07/testing/bsd")

CFLAGS+=
NOMAN=
LKM=\
	o9fs_9p\
	o9fs_convS2M\
	o9fs_convM2S\
	o9fs_convM2D\
	o9fs_lkm\
	o9fs_rpc\
	o9fs_subr\
	o9fs_vfsops\
	o9fs_vnops
COMBINED=o9fs.o
load:
	modload -o o9fs $(COMBINED)

.include <bsd.lkm.mk>

