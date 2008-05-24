#include <sys/param.h>
#include <sys/malloc.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

u_char*
gstring(u_char *p, u_char *ep, char **s)
{
	u_int n;

	if (p + O9FS_BIT16SZ > ep)
		return NULL;
	n = O9FS_GBIT16(p);
	p += O9FS_BIT16SZ - 1;
	if (p+n+1 > ep)
		return NULL;
	/* move it down, on top of count, to make room for '\0' */
	memcpy(p, p+1, n);
	p[n] = '\0';
	*s = (char*)p;
	p += n+1;
	return p;
}

u_char*
gqid(u_char *p, u_char *ep, struct o9fsqid *q)
{
	if (p + O9FS_QIDSZ > ep)
		return NULL;
	q->type = O9FS_GBIT8(p);
	p += O9FS_BIT8SZ;
	q->vers = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	q->path = O9FS_GBIT64(p);
	p += O9FS_BIT64SZ;
	return p;
}

/*
 * no syntactic checks.
 * three causes for error:
 *  1. message size field is incorrect
 *  2. input buffer too short for its own data (counts too long, etc.)
 *  3. too many names or qids
 * gqid() and gstring() return NULL if they would reach beyond buffer.
 * main switch statement checks range and also can fall through
 * to test at end of routine.
 */
u_int
o9fs_convM2S(u_char *ap, u_int nap, struct o9fsfcall *f)
{
	u_char *p, *ep;
	u_int i, size;

	p = ap;
	ep = p + nap;

	if (p + O9FS_BIT32SZ + O9FS_BIT8SZ + O9FS_BIT16SZ > ep)
		return 0;
	size = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;

	if (size > nap)
		return 0;
	if (size < O9FS_BIT32SZ + O9FS_BIT8SZ + O9FS_BIT16SZ)
		return 0;

	f->type = O9FS_GBIT8(p);
	p += O9FS_BIT8SZ;
	f->tag = O9FS_GBIT16(p);
	p += O9FS_BIT16SZ;

	switch(f->type)
	{
	default:
		return 0;

	case O9FS_TVERSION:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->msize = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		p = gstring(p, ep, &f->version);
		break;

	case O9FS_TFLUSH:
		if (p+O9FS_BIT16SZ > ep)
			return 0;
		f->oldtag = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		break;

	case O9FS_TAUTH:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->afid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		p = gstring(p, ep, &f->uname);
		if (p == NULL)
			break;
		p = gstring(p, ep, &f->aname);
		if (p == NULL)
			break;
		break;

	case O9FS_TATTACH:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->afid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		p = gstring(p, ep, &f->uname);
		if (p == NULL)
			break;
		p = gstring(p, ep, &f->aname);
		if (p == NULL)
			break;
		break;

	case O9FS_TWALK:
		if (p + O9FS_BIT32SZ + O9FS_BIT32SZ + O9FS_BIT16SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->newfid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->nwname = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		if (f->nwname > O9FS_MAXWELEM)
			return 0;
		for (i = 0; i < f->nwname; i++) {
			p = gstring(p, ep, &f->wname[i]);
			if (p == NULL)
				break;
		}
		break;

	case O9FS_TOPEN:
		if (p + O9FS_BIT32SZ + O9FS_BIT8SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->mode = O9FS_GBIT8(p);
		p += O9FS_BIT8SZ;
		break;

	case O9FS_TCREATE:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		p = gstring(p, ep, &f->name);
		if (p == NULL)
			break;
		if (p + O9FS_BIT32SZ + O9FS_BIT8SZ > ep)
			return 0;
		f->perm = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->mode = O9FS_GBIT8(p);
		p += O9FS_BIT8SZ;
		break;

	case O9FS_TREAD:
		if (p + O9FS_BIT32SZ + O9FS_BIT64SZ + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->offset = O9FS_GBIT64(p);
		p += O9FS_BIT64SZ;
		f->count = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TWRITE:
		if (p + O9FS_BIT32SZ + O9FS_BIT64SZ + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->offset = O9FS_GBIT64(p);
		p += O9FS_BIT64SZ;

		p += O9FS_BIT32SZ;
		if (p + f->count > ep)
			return 0;
		f->data = (char*)p;
		p += f->count;
		break;

	case O9FS_TCLUNK:
	case O9FS_TREMOVE:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TSTAT:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_TWSTAT:
		if (p + O9FS_BIT32SZ + O9FS_BIT16SZ > ep)
			return 0;
		f->fid = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		f->nstat = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		if (p + f->nstat > ep)
			return 0;
		f->stat = p;
		p += f->nstat;
		break;

/*
 */
	case O9FS_RVERSION:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->msize = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		p = gstring(p, ep, &f->version);
		break;

	case O9FS_RERROR:
		p = gstring(p, ep, &f->ename);
		break;

	case O9FS_RFLUSH:
		break;

	case O9FS_RAUTH:
		p = gqid(p, ep, &f->aqid);
		if (p == NULL)
			break;
		break;

	case O9FS_RATTACH:
		p = gqid(p, ep, &f->qid);
		if (p == NULL)
			break;
		break;

	case O9FS_RWALK:
		if (p + O9FS_BIT16SZ > ep)
			return 0;
		f->nwqid = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		if (f->nwqid > O9FS_MAXWELEM)
			return 0;
		for (i = 0; i < f->nwqid; i++){
			p = gqid(p, ep, &f->wqid[i]);
			if (p == NULL)
				break;
		}
		break;

	case O9FS_ROPEN:
	case O9FS_RCREATE:
		p = gqid(p, ep, &f->qid);
		if (p == NULL)
			break;
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->iounit = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_RREAD:
		if (p + O9FS_BIT32SZ > ep)
			return 0;
		f->count = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		if (p + f->count > ep)
			return 0;
		f->data = (char*)p;
		p += f->count;
		break;

	case O9FS_RWRITE:
		if (p + O9FS_BIT32SZ > ep)
			return (0);
		f->count = O9FS_GBIT32(p);
		p += O9FS_BIT32SZ;
		break;

	case O9FS_RCLUNK:
	case O9FS_RREMOVE:
		break;

	case O9FS_RSTAT:
		if (p + O9FS_BIT16SZ > ep)
			return 0;
		f->nstat = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		if (p + f->nstat > ep)
			return 0;
		f->stat = p;
		p += f->nstat;
		break;

	case O9FS_RWSTAT:
		break;
	}

	if (p == NULL || p > ep)
		return 0;
	if (ap + size == p)
		return size;
	return 0;
}

