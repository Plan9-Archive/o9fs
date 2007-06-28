#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

enum {
	fidchunk = 32
};

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
	if (error)
		return (error);
	
	omnt->om_o9fs.msize = rx.msize;
	return (0);
}

/*
static struct o9fsfid *
getfid(struct o9fsmount *omnt)
{
	struct o9fsfid *f;
	struct o9fs *fs;
	int i;
	
	fs = &omnt->om_o9fs;
	
	* XXX need to lock here *
	if (fs->freefid == NULL) {
		printf("freefid is nil, alloc\n");
		f = (struct o9fsfid *) 
			malloc(sizeof(struct o9fsfid) * fidchunk,
			M_O9FS, M_WAITOK);
	
		for (i = 0; i < fidchunk; i++) {
			f[i].fid = fs->nextfid++;
			f[i].next = &f[i+1];
			f[i].fs = fs;
		}
		f[i - 1].next = NULL;
		fs->freefid = f;
	}
	printf("freefid not nil\n");
	f = fs->freefid;
	fs->freefid = f->next;

	f->offset = 0;
	f->mode = -1;
	f->qid.path = 0;
	f->qid.vers = 0;
	f->qid.type = 0;
	
	return (f);
}

static void
putfid(struct o9fsfid *f)
{
	struct o9fs *fs;
	
	fs = f->fs;
	* XXX lock here *
	f->next = fs->freefid;
	fs->freefid = f;
	* XXX unlock here *
}*/

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
	
/*	if ((fid = getfid(omnt)) == NULL)
		return (NULL);*/
	fid = (struct o9fsfid *) pool_get(&fs->freefid, PR_WAITOK);
	fid->fid = ++fs->nextfid;
	fid->offset = 0;
	fid->mode = -1;
	fid->qid.path = 0;
	fid->qid.vers = 0;
	fid->qid.type = 0;
		
	tx.tag = 0;
	tx.type = O9FS_TATTACH;
	tx.afid = afid ? afid->fid : O9FS_NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error) {
		pool_put(&omnt->om_o9fs.freefid, fid);
		return (NULL);
	}
	fid->qid = rx.qid;
	return (fid);
}
	

