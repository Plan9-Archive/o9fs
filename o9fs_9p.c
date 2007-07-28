#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

enum {
	fidchunk = 64
};

static struct o9fsfid *
getfid(struct o9fsmount *omnt)
{
        struct o9fsfid *f;
        struct o9fs *fs;
        int i;
        
        fs = &omnt->om_o9fs;

        if (fs->freefid == NULL) {
				f = (struct o9fsfid *) malloc(sizeof(struct o9fsfid) * fidchunk,
                	M_O9FS, M_WAITOK);
				for (i = 0; i < fidchunk; i++) {
                        f[i].fid = fs->nextfid++;
						f[i].next = &f[i+1];
						f[i].fs = fs;
						f[i].stat = NULL;
						f[i].vp = NULL;
                }
				f[i-1].next = NULL;
				fs->freefid = f;
        }
        f = fs->freefid;
        fs->freefid = f->next;
        f->offset = 0;
        f->mode = -1;
        f->qid.path = 0;
        f->qid.vers = 0;
        f->qid.type = 0;
/*		printf("fid = %d\n", f->fid); */
        return f;
}

static void
putfid(struct o9fsmount *omnt, struct o9fsfid *f)
{
		struct o9fs *fs;
        
		fs = &omnt->om_o9fs;
		f->next = fs->freefid;
		fs->freefid = f;
}

/*
static void
fidclunk(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	struct o9fsfcall tx, rx;
	
	tx.type = O9FS_TCLUNK;
	tx.fid = fid->fid;
	o9fs_rpc(omnt, &tx, &rx);
	putfid(omnt, fid);
}

static void
fidclose(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	if (fid == NULL)
		return;
	
	fidclunk(omnt, fid);
}*/

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
		return error;
	
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
	
	fid = getfid(omnt);
	
	tx.tag = 0;
	tx.type = O9FS_TATTACH;
	tx.afid = afid ? afid->fid : O9FS_NOFID;
	tx.fid = fid->fid;
	tx.uname = user;
	tx.aname = aname;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error) {
		putfid(omnt, fid);
		return NULL;
	}
	fid->qid = rx.qid;
	return fid;
}

struct o9fsfid *
o9fs_twalk(struct o9fsmount *omnt, struct o9fsfid *fid, char *oname)
{
	struct o9fsfcall tx, rx;
	struct o9fsfid *f;
	int n;
	char *temp, *name;

	name = oname;
	if (name) {
		temp = (char *) malloc((strlen(name)+1) * sizeof(char),
				M_TEMP, M_WAITOK);
		strlcpy(temp, name, strlen(name)+1);
		name = temp;
	}
	
	n = o9fs_tokenize(tx.wname, nelem(tx.wname), name, '/');
	f = getfid(omnt);
	
	tx.tag = 0;
	tx.type = O9FS_TWALK;
	tx.fid = fid->fid;
	tx.newfid = f->fid;
	tx.nwname = n;
		
	if ((o9fs_rpc(omnt, &tx, &rx)) != 0)
		goto fail;
	if (rx.nwqid < n)
		goto fail;
	
	f->qid = rx.wqid[n - 1];

	free(temp, M_TEMP);
	return f;

fail:
	putfid(omnt, f);
	return NULL;
}

struct o9fsstat *
o9fs_tstat(struct o9fsmount *omnt, char *name)
{
	struct o9fsstat *stat;
	struct o9fsfid *fid;

	if ((fid = o9fs_twalk(omnt, omnt->om_o9fs.rootfid, name)) == NULL)
		return NULL;
		
	stat = o9fs_fstat(omnt, fid);
/*	fidclose(omnt, fid); */
	
	return stat;
}
	
struct o9fsstat *
o9fs_fstat(struct o9fsmount *omnt, struct o9fsfid *fid)
{
	struct o9fsstat *stat;
	struct o9fsfcall tx, rx;
	int error, n;

	tx.type = O9FS_TSTAT;
	tx.tag = 0;
	tx.fid = fid->fid;

	error = o9fs_rpc(omnt, &tx, &rx);
	if (error)
		return NULL;

	stat = (struct o9fsstat *) malloc(sizeof(struct o9fsstat) + rx.nstat,
			M_TEMP, M_WAITOK);

	n = o9fs_convM2D(rx.stat, rx.nstat, stat, (char *)&stat[1]);
	if (n != rx.nstat)
		printf("rx.nstat and convM2D disagree abour dir lenght\n");

	/* is this the right place? */
	fid->stat = stat;
	
	return stat;
}
