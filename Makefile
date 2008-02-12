tgz: 
	(cd .. && tar zcvf o9fs.tgz o9fs/ && sync)

kern:
	(cd /sys/arch/i386/compile/GENERIC && sudo sh -c "make && cp bsd /usr/iru/src/gsoc07/testing/bsd")

NOMAN=
LKM=\
	o9fs_9p\
	o9fs_conv\
	o9fs_debug\
	o9fs_io\
	o9fs_lkm\
	o9fs_msg\
	o9fs_rpc\
	o9fs_subr\
	o9fs_vfsops\
	o9fs_vnops

.include <bsd.lkm.mk>

