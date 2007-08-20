#!/bin/sh
# installs o9fs stuff in kernel source under /sys
# written by Iruata Souza
# See LICENSE
# JUL/2007

# add o9fs' files to kernel compilation
cp /sys/conf/files /sys/conf/files.old
sed '/file xfs\/xfs_syscalls-dummy/{p;s/.*/file miscfs\/o9fs\/o9fs_9p\.c              o9fs\
file miscfs\/o9fs\/o9fs_convM2D\.c         o9fs\
file miscfs\/o9fs\/o9fs_convM2S\.c         o9fs\
file miscfs\/o9fs\/o9fs_convS2M\.c         o9fs\
file miscfs\/o9fs\/o9fs_io\.c              o9fs\
file miscfs\/o9fs\/o9fs_subr\.c            o9fs\
file miscfs\/o9fs\/o9fs_vfsops\.c          o9fs\
file miscfs\/o9fs\/o9fs_vnops\.c           o9fs/;}' /sys/conf/files #> /sys/conf/files


