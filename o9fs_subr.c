#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/lock.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum {
	fidchunk = 64
};

struct o9fsfid *
o9fs_getfid(struct o9fs *fs)
{
        struct o9fsfid *f;
        int i;
        
        if (fs->freefid == NULL) {
				f = (struct o9fsfid *) malloc(sizeof(struct o9fsfid) * fidchunk,
                	M_O9FS, M_WAITOK);
				for (i = 0; i < fidchunk; i++) {
						f[i].fid = fs->nextfid++;
						f[i].next = &f[i+1];
						f[i].opened = 0;
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
        return f;
}

void
o9fs_putfid(struct o9fs *fs, struct o9fsfid *f)
{        
		DPRINT("putfid: enter\n");
		if (f == NULL)
			return;
		f->next = fs->freefid;
		fs->freefid = f;
		DPRINT("putfid: return\n");
}

/* only clone a fid internally */
struct o9fsfid *
o9fs_fidclone(struct o9fs *fs, struct o9fsfid *f)
{
	struct o9fsfid *nf;
	
	if (f->opened)
	//	DPRINT("fidclone: fid=%d already opened\n", f->fid);
		panic("clone of open fid=%d\n", f->fid);
	
	nf = o9fs_getfid(fs);
	nf->mode = f->mode;
	nf->qid.path = f->qid.path;
	nf->qid.vers = f->qid.vers;
	nf->qid.type = f->qid.type;
	DPRINT("fidclone: fid=%d type=%d\n", f->fid, f->qid.type);
//	memcpy(&nf->qid, &f->qid, sizeof(nf->qid));
	nf->offset = f->offset;
	return nf;
}

/* clone a fid both on the client and the server */
struct o9fsfid *
o9fs_clone(struct o9fs *fs, struct o9fsfid *f)
{
	struct o9fsfid *nf;
	return o9fs_twalk(fs, f, 0);
}

/* guarantee the only copy of f */
struct o9fsfid *
o9fs_fidunique(struct o9fs *fs, struct o9fsfid *f)
{
	struct o9fsfid *nf;
	nf = o9fs_clone(fs, f);
	o9fs_fidclunk(fs, f);
	f = nf;
	return f;
}

void
o9fs_freestat(struct o9fsstat *s)
{
	/* let'em leak! */
/*	if(s->name != NULL)
		free(s->name, M_O9FS);
	if(s->uid != NULL)
		free(s->uid, M_O9FS);
	if(s->gid != NULL)
		free(s->gid, M_O9FS);
	if(s->muid)
		free(s->muid, M_O9FS); */
	s->name = s->uid = s->gid = s->muid = NULL;
}

void
o9fs_freefcall(struct o9fsfcall *fcall)
{
	void *p;

	switch(fcall->type) {
	case O9FS_RSTAT:
		p = fcall->stat;
		break;
	case O9FS_RREAD:
		p = fcall->data;
		break;
	case O9FS_RVERSION:
		p = fcall->version;
		break;
	case O9FS_RERROR:
		p = fcall->ename;
		break;
	default:
		return;
	}
	
	free(p, M_O9FS);
	p = NULL;
}


u_int
o9fs_tokenize(char *res[], u_int reslen, char *str, char delim)
{
	char *s;
	u_int i;
	
	i = 0;
	s = str;

	while (i < reslen && *s) {
		while (*s == delim)
			*(s++) = '\0';
		if (*s)
			res[i++] = s;
		while (*s && *s != delim)
			s++;
	}
	return i;
}

int
o9fs_allocvp(struct mount *mp, struct o9fsfid *fid, struct vnode **vpp, u_long flags)
{
	struct vnode *vp;
	struct proc *p;
	int error;

	p = curproc;
	error = 0;
	
	/* Get a new vnode and associate it with our fid */
	error = getnewvnode(VT_O9FS, mp, o9fs_vnodeop_p, &vp);
	if (error)
		goto out;

	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (error) {
		vp->v_data = NULL;
		vp = NULL;
		goto out;
	}

	if (fid->qid.type == O9FS_QTDIR)
		vp->v_type = VDIR;
	else
		vp->v_type = VREG;

	vp->v_data = fid;
	vp->v_flag = 0;
	vp->v_flag |= flags;

out:
	*vpp = vp;
	return error;
}

void
o9fs_freevp(struct vnode *vp)
{
	vp->v_data = NULL;
}

struct o9fsfid *
o9fs_findfid(struct o9fs *fs, int fid)
{
	struct o9fsfid *f;

	for (f = fs->rootfid; f; f = f->next)
		if (f->fid == fid)
			return f;
	return NULL;
}

int
o9fs_ptoumode(int mode)
{
	int umode;
	
	umode = mode & (0777|O9FS_DMDIR);
	if ((mode & O9FS_DMDIR) == O9FS_DMDIR)
		umode |= S_IFDIR;

	return umode;
}

int
o9fs_utopmode(int mode)
{
	int pmode;

	pmode = mode & (0777|S_IFDIR);
	if ((mode & S_IFDIR) == S_IFDIR)
		pmode |= O9FS_DMDIR;

	return pmode;
}

int
o9fs_uflags2omode(int uflags)
{
	int omode;
	
	omode = 0;
	switch(uflags & O_ACCMODE) {
	case FREAD:
		omode = O9FS_OREAD;
		break;
	case FWRITE:
		omode = O9FS_OWRITE;
		break;
	case (FREAD|FWRITE):
		omode = O9FS_ORDWR;
		break;
	}

	/* XXX u9fs specific? */
	if (uflags & O_CREAT)
		omode |= O9FS_OEXEC;
	if (uflags & O_TRUNC)
		omode |= O9FS_OTRUNC;

	return omode;
}

void
printfidvp(struct o9fsfid *f)
{
	if(!f)
		return;
	printf("f=%p f->fid=%d ", f, f->fid);
}

	
