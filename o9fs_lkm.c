#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 0,
};

extern struct vnodeopv_desc o9fs_vnodeop_opv_desc;
extern struct vfsops o9fs_vfsops;

struct vfsconf o9fs_vfsconf = { &o9fs_vfsops, MOUNT_O9FS, 0, 0, 0, NULL };
MOD_VFS("o9fs", -1, &o9fs_vfsconf);

int
o9fs_lkmentry(struct lkm_table *table, int cmd, int ver)
{
	DISPATCH(table, cmd, ver, lkm_nofunc, lkm_nofunc, lkm_nofunc);
}
