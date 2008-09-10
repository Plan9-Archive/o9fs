#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include "o9fs.h"
#include "o9fs_extern.h"

u_char*
pstring(u_char *p, char *s)
{
	u_int n;

	if (s == NULL) {
		O9FS_PBIT16(p, 0);
		p += O9FS_BIT16SZ;
		return p;
	}

	n = strlen(s);
	O9FS_PBIT16(p, n);
	p += O9FS_BIT16SZ;
	memcpy(p, s, n);
	p += n;
	return p;
}

u_char*
pqid(u_char *p, struct o9fsqid *q)
{
	O9FS_PBIT8(p, q->type);
	p += O9FS_BIT8SZ;
	O9FS_PBIT32(p, q->vers);
	p += O9FS_BIT32SZ;
	O9FS_PBIT64(p, q->path);
	p += O9FS_BIT64SZ;
	return p;
}

u_int
stringsz(char *s)
{
	return O9FS_BIT16SZ + strlen(s);
}

u_int
o9fs_sizeS2M(struct o9fsfcall *f)
{
	u_int n;
	int i;

	n = 0;
	n += O9FS_BIT32SZ;	/* size */
	n += O9FS_BIT8SZ;	/* type */
	n += O9FS_BIT16SZ;	/* tag */

	switch(f->type)
	{
	default:
		return 0;

	case O9FS_TVERSION:
		n += O9FS_BIT32SZ;
		n += stringsz(f->version);
		break;

	case O9FS_TFLUSH:
		n += O9FS_BIT16SZ;
		break;

	case O9FS_TAUTH:
		n += O9FS_BIT32SZ;
		n += stringsz(f->uname);
		n += stringsz(f->aname);
		break;

	case O9FS_TATTACH:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT32SZ;
		n += stringsz(f->uname);
		n += stringsz(f->aname);
		break;

	case O9FS_TWALK:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT32SZ;
		n += O9FS_BIT16SZ;
		for (i = 0; i < f->nwname; i++)
			n += stringsz(f->wname[i]);
		break;

	case O9FS_TOPEN:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT8SZ;
		break;

	case O9FS_TCREATE:
		n += O9FS_BIT32SZ;
		n += stringsz(f->name);
		n += O9FS_BIT32SZ;
		n += O9FS_BIT8SZ;
		break;

	case O9FS_TREAD:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT64SZ;
		n += O9FS_BIT32SZ;
		break;

	case O9FS_TWRITE:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT64SZ;
		n += O9FS_BIT32SZ;
		n += f->count;
		break;

	case O9FS_TCLUNK:
	case O9FS_TREMOVE:
		n += O9FS_BIT32SZ;
		break;

	case O9FS_TSTAT:
		n += O9FS_BIT32SZ;
		break;

	case O9FS_TWSTAT:
		n += O9FS_BIT32SZ;
		n += O9FS_BIT16SZ;
		n += f->nstat;
		break;
/*
 */

	case O9FS_RVERSION:
		n += O9FS_BIT32SZ;
		n += stringsz(f->version);
		break;

	case O9FS_RERROR:
		n += stringsz(f->ename);
		break;

	case O9FS_RFLUSH:
		break;

	case O9FS_RAUTH:
		n += O9FS_QIDSZ;
		break;

	case O9FS_RATTACH:
		n += O9FS_QIDSZ;
		break;

	case O9FS_RWALK:
		n += O9FS_BIT16SZ;
		n += f->nwqid * O9FS_QIDSZ;
		break;

	case O9FS_ROPEN:
	case O9FS_RCREATE:
		n += O9FS_QIDSZ;
		n += O9FS_BIT32SZ;
		break;

	case O9FS_RREAD:
		n += O9FS_BIT32SZ;
		n += f->count;
		break;

	case O9FS_RWRITE:
		n += O9FS_BIT32SZ;
		break;

	case O9FS_RCLUNK:
		break;

	case O9FS_RREMOVE:
		break;

	case O9FS_RSTAT:
		n += O9FS_BIT16SZ;
		n += f->nstat;
		break;

	case O9FS_RWSTAT:
		break;
	}
	return n;
}

u_int
o9fs_convS2M(struct o9fsfcall *f, u_char *ap, u_int nap)
{
	u_char *p;
	u_int i, size;

	size = o9fs_sizeS2M(f);
	if (size == 0)
		return 0;
	if (size > nap)
		return 0;

	p = (u_char*)ap;

	O9FS_PBIT32(p, size);
	p += O9FS_BIT32SZ;
	O9FS_PBIT8(p, f->type);
	p += O9FS_BIT8SZ;
	O9FS_PBIT16(p, f->tag);
	p += O9FS_BIT16SZ;

	switch (f->type)
	{
	default:
		return 0;

	case O9FS_TVERSION:
		O9FS_PBIT32(p, f->msize);
		p += O9FS_BIT32SZ;
		p = pstring(p, f->version);
		break;

	case O9FS_TFLUSH:
		O9FS_PBIT16(p, f->oldtag);
		p += O9FS_BIT16SZ;
		break;

	case O9FS_TAUTH:
		O9FS_PBIT32(p, f->afid);
		p += O9FS_BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		break;

	case O9FS_TATTACH:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT32(p, f->afid);
		p += O9FS_BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		break;

	case O9FS_TWALK:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT32(p, f->newfid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT16(p, f->nwname);
		p += O9FS_BIT16SZ;
		if (f->nwname > O9FS_MAXWELEM)
			return 0;
		for (i = 0; i < f->nwname; i++)
			p = pstring(p, f->wname[i]);
		break;

	case O9FS_TOPEN:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT8(p, f->mode);
		p += O9FS_BIT8SZ;
		break;

	case O9FS_TCREATE:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		p = pstring(p, f->name);
		O9FS_PBIT32(p, f->perm);
		p += O9FS_BIT32SZ;
		O9FS_PBIT8(p, f->mode);
		p += O9FS_BIT8SZ;
		break;

	case O9FS_TREAD:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT64(p, f->offset);
		p += O9FS_BIT64SZ;
		O9FS_PBIT32(p, f->count);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TWRITE:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT64(p, f->offset);
		p += O9FS_BIT64SZ;
		O9FS_PBIT32(p, f->count);
		p += O9FS_BIT32SZ;
		memcpy(p, f->data, f->count);
		printf("twrite: %s\n", p);
		p += f->count;
		break;

	case O9FS_TCLUNK:
	case O9FS_TREMOVE:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TSTAT:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TWSTAT:
		O9FS_PBIT32(p, f->fid);
		p += O9FS_BIT32SZ;
		O9FS_PBIT16(p, f->nstat);
		p += O9FS_BIT16SZ;
		memcpy(p, f->stat, f->nstat);
		p += f->nstat;
		break;
/*
 */

	case O9FS_RVERSION:
		O9FS_PBIT32(p, f->msize);
		p += O9FS_BIT32SZ;
		p = pstring(p, f->version);
		break;

	case O9FS_RERROR:
		p = pstring(p, f->ename);
		break;

	case O9FS_RFLUSH:
		break;

	case O9FS_RAUTH:
		p = pqid(p, &f->aqid);
		break;

	case O9FS_RATTACH:
		p = pqid(p, &f->qid);
		break;

	case O9FS_RWALK:
		O9FS_PBIT16(p, f->nwqid);
		p += O9FS_BIT16SZ;
		if (f->nwqid > O9FS_MAXWELEM)
			return 0;
		for (i = 0; i < f->nwqid; i++)
			p = pqid(p, &f->wqid[i]);
		break;

	case O9FS_ROPEN:
	case O9FS_RCREATE: 
		p = pqid(p, &f->qid);
		O9FS_PBIT32(p, f->iounit);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_RREAD:
		O9FS_PBIT32(p, f->count);
		p += O9FS_BIT32SZ;
		memcpy(p, f->data, f->count);
		printf("rread: %s\n", p);
		p += f->count;
		break;

	case O9FS_RWRITE:
		O9FS_PBIT32(p, f->count);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_RCLUNK:
		break;

	case O9FS_RREMOVE:
		break;

	case O9FS_RSTAT:
		O9FS_PBIT16(p, f->nstat);
		p += O9FS_BIT16SZ;
		memcpy(p, f->stat, f->nstat);
		p += f->nstat;
		break;

	case O9FS_RWSTAT:
		break;
	}
	if(size != p-ap)
		return 0;
	return size;
}
