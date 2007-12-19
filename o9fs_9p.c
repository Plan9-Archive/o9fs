#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

int
o9fs_tversion(struct o9fsmount *omnt, int msize, char *version)
{
	struct o9fsfcall tx, rx;
	int error;

	error = 0;
	
	tx.tag = O9FS_NOTAG;
	tx.type = O9FS_TVERSION;
	tx.version = version;
	tx.msize = msize;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error == -1)
		return -1;

	omnt->om_o9fs.msize = rx.msize;
	return 0;
}

struct o9fsfid *
o9fs_tattach(struct o9fsmount *omnt, struct o9fsfid *afid,
			char *user, char *aname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *fid;
	struct o9fs *fs;
	int error;

	error = 0;
	fs = &omnt->om_o9fs;
	
	if (aname == NULL)
		aname = "";
	
	fid = o9fs_getfid(omnt);
	
	tx.type = O9FS_TATTACH;
	tx.afid = afid ? afid->fid : O9FS_NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error) {
		o9fs_putfid(omnt, fid);
		return NULL;
	}
	fid->qid = rx.qid;
	
	return fid;
}

void
o9fs_fidclunk(struct o9fsmount *omnt, struct o9fsfid *f)
{
	struct o9fsfcall tx, rx;

	tx.type = O9FS_TCLUNK;
	tx.fid = f->fid;
	o9fs_rpc(omnt, &tx, &rx);
	f->opened = 0;
	f->mode = -1;
	o9fs_putfid(omnt, f);
}


struct o9fsfid *
o9fs_twalk(struct o9fsmount *omnt, int fid, char *oname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *f;
	int n;
	char *name;
	
	name = oname;
/*	if (name) {
		size_t len;
		len = strlen(name);
		temp =  malloc(len + 1, M_TEMP, M_WAITOK);
		memcpy(temp, name, len);
		temp[len] = '\0';
		name = temp;
	}*/
	
	n = o9fs_tokenize(tx.wname, nelem(tx.wname), name, '/');
	f = o9fs_getfid(omnt);
	
	tx.type = O9FS_TWALK;
	tx.fid = fid;
	tx.newfid = f->fid;
	tx.nwname = n;
		
	if ((o9fs_rpc(omnt, &tx, &rx)) < 0)
		goto fail;
	if (rx.nwqid < n)
		goto fail;
	
	f->qid = rx.wqid[n - 1];

//	free(temp, M_TEMP);
	return f;

fail:
	o9fs_fidclunk(omnt, f);
	return NULL;
}

struct o9fsstat *
o9fs_fstat(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	struct o9fsstat *stat;
	struct o9fsfcall tx, rx;
	struct o9fsmsg *m;
	int error;

	tx.type = O9FS_TSTAT;
	tx.fid = fid->fid;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error)
		return NULL;

	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat) + rx.nstat,
			M_O9FS, M_WAITOK);

	m = o9fs_msg(rx.stat, rx.nstat, O9FS_MLOAD);

	o9fs_msgstat(m, stat);
	o9fs_freefcall(&rx);
	if (m->m_cur > m->m_end) {
		free(stat, M_O9FS);
		stat = NULL;
	}

	/* is this the right place? */
	fid->stat = stat;

/*	printf("stat: name=%s uid=%s len=%ld qid.path=%ld\n", 
			stat->name, stat->uid, stat->length, stat->qid.path); */

	return stat;
}

int
o9fs_topen(struct o9fsmount *omnt, struct o9fsfid *fid, int mode)
{
	struct o9fsfcall tx, rx;
	int error;

	tx.type = O9FS_TOPEN;
	tx.fid = fid->fid;
	tx.mode = mode;
	
	error = o9fs_rpc(omnt, &tx, &rx);
	if (error)
		return -1;
	fid->mode = mode;

	return 0;
}

long
o9fs_tread(struct o9fsmount *omnt, struct o9fsfid *f, void *buf, 
			u_long n, int64_t offset)
{
	struct o9fsfcall tx, rx;
	u_int msize;

	msize = f->fs->msize - O9FS_IOHDRSZ;
	if (n > msize)
		n = msize;
	
	tx.type = O9FS_TREAD;
	tx.fid = f->fid;
	
	if (offset == -1) {
		rw_enter_read(&f->rwl);
		tx.offset = f->offset;
		rw_exit_read(&f->rwl);
	} else
		tx.offset = offset;
	tx.count = n;

	if ((o9fs_rpc(omnt, &tx, &rx)) < 0)
		return -1;
	
	if (rx.count) {
		bcopy(rx.data, buf, rx.count);
		if (offset == -1) {
			rw_enter_write(&f->rwl);
			f->offset += rx.count;
			rw_exit_write(&f->rwl);
		}
	}
	return rx.count;
}

long
o9fs_twrite(struct o9fsmount *omnt, struct o9fsfid *f, void *buf, 
			long n, int64_t offset)
{
	struct o9fsfcall tx, rx;

	tx.type = O9FS_TWRITE;
	tx.fid = f->fid;
	
	if (offset == -1) {
		rw_enter_read(&f->rwl);
		tx.offset = f->offset;
		rw_exit_read(&f->rwl);
	} else
		tx.offset = offset;

	tx.count = n;
	tx.data = buf;

	if ((o9fs_rpc(omnt, &tx, &tx)) < 0)
		return -1;
	
	if (offset == -1 && rx.count) {
		rw_enter_write(&f->rwl);
		f->offset += rx.count;
		rw_exit_write(&f->rwl);
	}

	return rx.count;
}
