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
#include <sys/queue.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 1,
};

enum {
	Chunk = 64
};

void
o9fs_dump(u_char *buf, long n)
{
	long i;

	if (buf == NULL)
		return;

	for (i = 0; i < n; i++) {
		printf("%02x", buf[i]);
		if (i % 16 == 15)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");
}

struct o9fid *
o9fs_xgetfid(struct o9fs *fs)
{
	struct o9fid *f;

	if (TAILQ_EMPTY(&fs->freeq)) {
		f = (struct o9fid *) malloc(sizeof(struct o9fid), M_O9FS, M_WAITOK);
		f->fid = fs->nextfid++;
		TAILQ_INSERT_TAIL(&fs->activeq, f, next);
	} else {
		f = TAILQ_FIRST(&fs->freeq);
		TAILQ_REMOVE(&fs->freeq, f, next);
	}

	f->ref = 0;
	f->offset = 0;
	f->mode = -1;
	f->iounit = 0;
	f->qid.path = 0;
	f->qid.vers = 0;
	f->qid.type = 0;
	return f;
}

void
o9fs_xputfid(struct o9fs *fs, struct o9fid *f)
{
	if (f == NULL)
		panic("o9fs_xputfid: cannot put a nil fid");

	TAILQ_REMOVE(&fs->activeq, f, next);
	TAILQ_INSERT_TAIL(&fs->freeq, f, next);
}

char *
putstring(char *buf, char *s)
{
	long n;

	if (buf == NULL || s == NULL)
		panic("putstring was given nil pointers");

	n = strlen(s);
	O9FS_PBIT16(buf, n);
	memcpy(buf + 2, s, n);
	return buf + 2 + n;
}

char *
getstring(char *buf)
{
	long n;
	char *s;

	if (buf == NULL)
		panic("getstring was given a nul pointer");

	n = O9FS_GBIT16(buf);
	s = malloc(n + 1, M_O9FS, M_WAITOK);
	memcpy(s, buf + 2, n);
	s[n] = '\0';
	return s;
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

int
o9fs_allocvp(struct mount *mp, struct o9fid *f, struct vnode **vpp, u_long flag)
{
	struct vnode *vp;
	struct proc *p;
	int error;

	DIN();

	p = curproc;
	error = 0;
	
	error = getnewvnode(VT_O9FS, mp, &o9fs_vops, vpp);
	if (error) {
		*vpp = NULL;
		printf("error in getnewvnode %d\n", error);
		DRET();
		return error;
	}
	vp = *vpp;

	if (f->qid.type == O9FS_QTDIR)
		vp->v_type = VDIR;
	else
		vp->v_type = VREG;

	vp->v_data = f;
	f->ref++;
	vp->v_flag = flag;

	DRET();
	return error;
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

void *
o9fsrealloc(void *ptr, size_t oldsize, size_t newsize)
{
	void *p;
	
	if (newsize == oldsize)
		return ptr;
	p = malloc(newsize, M_O9FS, M_WAITOK);
	if (ptr)
		bcopy(ptr, p, oldsize);
	ptr = p;
	return p;
}

void
_printvp(struct vnode *vp)
{
	struct o9fid *f;

	if (vp == NULL || VTO92(vp) == NULL)
		return;
	f = VTO92(vp);
	printf("[%p] %p fid %d ref %d qid (%.16llx %lu %d) mode %d iounit %ld\n", vp, f, f->fid, f->ref, f->qid.path, f->qid.vers, f->qid.type, f->mode, f->iounit);
}
