#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include "o9fs.h"
#include "o9fs_extern.h"

void
o9fs_fidclunk(struct o9fsmount *omnt, struct o9fsfid *f)
{
	struct o9fsfcall tx, rx;

	DPRINT("fidclunk: enter\n");
	tx.type = O9FS_TCLUNK;
	tx.fid = f->fid;
	o9fs_rpc(omnt, &tx, &rx);

	f->opened = 0;
	f->mode = -1;
	o9fs_putfid(omnt, f);
	DPRINT("fidclunk: return\n");
}


struct o9fsfid *
o9fs_twalk(struct o9fsmount *omnt, int dirfid, char *oname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *f;
	int n;
	char *name;

	name = oname;
	n = o9fs_tokenize(tx.wname, nelem(tx.wname), oname, '/');
	f = o9fs_getfid(omnt);

	tx.type = O9FS_TWALK;
	tx.fid = dirfid;
	tx.newfid = f->fid;
	tx.nwname = n;
		
	if ((o9fs_rpc(omnt, &tx, &rx)) < 0) {
		printf("walk: rpc failed\n");
		goto fail;
	}
	if (rx.nwqid < n) {
		printf("walk: nwqid < n\n");
		goto fail;
	}
	f->qid = rx.wqid[n - 1];
	return f;
fail:
	o9fs_putfid(omnt, f);
	return NULL;
}

struct o9fsfid *
o9fs_fclone(struct o9fsmount *omnt, struct o9fsfid *ofid)
{
	struct o9fsfid *cfid;

	cfid = o9fs_twalk(omnt, ofid->fid, "");
	cfid->opened = cfid->offset = 0;
	cfid->mode = -1;
	memcpy(&cfid->qid, &ofid->qid, sizeof(struct o9fsqid));
	cfid->fs = ofid->fs;
	return cfid;
}

struct o9fsfid *
o9fs_funique(struct o9fsmount *omnt, struct o9fsfid *f)
{
	struct o9fsfid *nf;
	
	nf = o9fs_fclone(omnt, f);
//	o9fs_fidclunk(omnt, f);
//	f = nf;
	return nf;
}

struct o9fsstat *
o9fs_fstat(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	struct o9fsstat *stat;
	struct o9fsfcall tx, rx;
	int error;

	tx.type = O9FS_TSTAT;
	tx.fid = fid->fid;
	if ((o9fs_rpc(omnt, &tx, &rx)) == -1)
		return NULL;

	stat = malloc(sizeof(struct o9fsstat) + rx.nstat,	M_O9FS, M_WAITOK);
	if (!o9fs_convM2D(rx.stat, rx.nstat, stat, (char*)rx.stat))
		return NULL;

	return stat;
}

long
o9fs_tread(struct o9fsmount *omnt, struct o9fsfid *f, void *buf, 
			u_long n, int64_t offset)
{
	struct o9fsfcall *tx, *rx;
	u_int msize;

	msize = omnt->om_o9fs.msize - O9FS_IOHDRSZ;
	if (n > msize)
		n = msize;

	tx = &omnt->om_o9fs.request;
	rx = &omnt->om_o9fs.reply;

	tx->type = O9FS_TREAD;
	tx->fid = f->fid;
	
	if (offset == -1) {
		tx->offset = f->offset;
	} else
		tx->offset = offset;
	tx->count = n;

	if ((o9fs_rpc(omnt, tx, rx)) < 0)
		return -1;
	
	if (rx->count) {
		bcopy(rx->data, buf, rx->count);
		if (offset == -1) {
			f->offset += rx->count;
		}
		return rx->count;
	}

	return 0;
}

long
o9fs_twrite(struct o9fsmount *omnt, struct o9fsfid *f, void *buf, 
			long n, int64_t offset)
{
	struct o9fsfcall tx, rx;

	tx.type = O9FS_TWRITE;
	tx.fid = f->fid;
	
	if (offset == -1) {
		tx.offset = f->offset;
	} else
		tx.offset = offset;

	tx.count = n;
	tx.data = buf;

	if ((o9fs_rpc(omnt, &tx, &rx)) < 0)
		return -1;
	
	if (offset == -1 && rx.count) {
		f->offset += rx.count;
	}

	return rx.count;
}
