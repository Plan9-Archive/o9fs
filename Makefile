all: tgz kern

tgz: 
	(cd .. && tar zcvf o9fs.tgz o9fs/ && sync)

kern:
	(cd /sys/arch/i386/compile/GENERIC && sudo sh -c "make && cp bsd /usr/iru/src/gsoc07/testing/bsd")
