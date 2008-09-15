#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include "o9fs.h"
#include "o9fs_extern.h"

void
o9fs_fidclunk(struct o9fs *fs, struct o9fsfid *f)
{
	struct o9fsfcall tx, rx;

	DPRINT("fidclunk: enter\n");
	tx.type = O9FS_TCLUNK;
	tx.fid = f->fid;
	o9fs_rpc(fs, &tx, &rx);

	f->opened = 0;
	f->mode = -1;
	o9fs_putfid(fs, f);
	DPRINT("fidclunk: return\n");
}

struct o9fsfid*
o9fs_twalk(struct o9fs *fs, struct o9fsfid *f, struct o9fsfid *nf, char *oname)
{
	struct o9fsfcall tx, rx;
	int n;
	char *name;

	name = oname;
	if (nf == NULL) {
		DPRINT("twalk: cloning fid=%d\n", f->fid);
		nf = o9fs_fidclone(fs, f);
		n = 0;
	}
	
	if (name) {
		uint64_t len;
		len = strlen(name);
		tx.wname[0] = malloc(len, M_O9FS, M_WAITOK);
		memcpy(tx.wname[0], name, len);
		tx.wname[0][len] = 0;
		n = 1;
	}
	tx.type = O9FS_TWALK;
	tx.fid = f->fid;
	tx.newfid = nf->fid;
	tx.nwname = n;

	if ((o9fs_rpc(fs, &tx, &rx)) < 0) {
		printf("walk: rpc failed\n");
		o9fs_putfid(fs, nf);
		return NULL;
	}
	if (rx.nwqid < n) {
		printf("walk: nwqid < n\n");
		return NULL;
	}
	
	/* no negative numbers on the qid */
	if (rx.nwqid > 0)
		nf->qid = rx.wqid[n-1];
	else
		nf->qid = rx.wqid[0];
	return nf;
}

struct o9fsstat *
o9fs_fstat(struct o9fs *fs, struct o9fsfid *fid)
{
	struct o9fsstat *stat;
	struct o9fsfcall tx, rx;

	tx.type = O9FS_TSTAT;
	tx.fid = fid->fid;
	if ((o9fs_rpc(fs, &tx, &rx)) == -1)
		return NULL;

	stat = malloc(sizeof(struct o9fsstat) + rx.nstat,	M_O9FS, M_WAITOK);
	if (!o9fs_convM2D(rx.stat, rx.nstat, stat, (char*)rx.stat))
		return NULL;

	return stat;
}

long
o9fs_rdwr(struct o9fs *fs, int type, struct o9fsfid *f, void *buf, 
		u_long n, int64_t offset)
{
	char *uba;
	u_long nr;
	
	uba = buf;
	fs->request.type = type;
	fs->request.fid = f->fid;

	if (offset == -1)
		fs->request.offset= f->offset;
	else
		fs->request.offset = offset;

	if (type == O9FS_TWRITE)
		fs->request.data = uba;

	nr = n;
	if (nr > fs->msize)
		nr = fs->msize;
	fs->request.count = nr;

	if ((o9fs_rpc(fs, &fs->request, &fs->reply)) < 0)
		return -1;

	nr = fs->reply.count;
	if (nr <= 0)
		return nr;

	if (type == O9FS_TREAD)
		memcpy((u_char *)uba, fs->reply.data, nr);

	if (offset == -1)
		f->offset += fs->reply.count;
	return nr;
}

int
o9fs_opencreate(int type, struct o9fs *fs, struct o9fsfid *f, int mode, ulong perm, char *name)
{
	int omode, error;
	struct o9fsfcall tx, rx;
	
	omode = o9fs_uflags2omode(mode);

	tx.type = type;
	tx.fid = f->fid;
	tx.mode = omode;
	if (type == O9FS_TCREATE) {
		tx.name = name;
		tx.mode = O9FS_ORDWR; /* try ORDWR on open */
		tx.perm = o9fs_utopmode(perm);
	}

	error = o9fs_rpc(fs, &tx, &rx);
	if (error)
		return error;

	f->qid = rx.qid;
	f->offset = 0;
	f->opened = 1;
	f->mode = omode;
	return 0;
}

