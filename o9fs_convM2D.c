#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include "o9fs.h"
#include "o9fs_extern.h"

enum{
	Debug = 1,
};

int
o9fs_statcheck(u_char *buf, u_int nbuf)
{
	u_char *ebuf;
	int i, nstr;

	ebuf = buf + nbuf;

	if(nbuf < O9FS_STATFIXLEN || nbuf != O9FS_BIT16SZ + O9FS_GBIT16(buf))
		return (-1);

	buf += O9FS_STATFIXLEN - 4 * O9FS_BIT16SZ;

	nstr = 4;
	for (i = 0; i < nstr; i++) {
		if(buf + O9FS_BIT16SZ > ebuf)
			return (-1);
		buf += O9FS_BIT16SZ + O9FS_GBIT16(buf);
	}

	if (buf != ebuf)
		return (-1);

	return (0);
}

static char nullstring[] = "";

u_int
o9fs_convM2D(u_char *buf, u_int nbuf, struct o9fsstat *d, char *strs)
{
	u_char *p, *ebuf;
	char *sv[5];
	int i, ns, nstr;

	if(nbuf < O9FS_STATFIXLEN)
		return 0; 

	p = buf;
	ebuf = buf + nbuf;

	p += O9FS_BIT16SZ;	/* ignore size */
	d->type = O9FS_GBIT16(p);
	p += O9FS_BIT16SZ;
	d->dev = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	d->qid.type = O9FS_GBIT8(p);
	p += O9FS_BIT8SZ;
	d->qid.vers = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	d->qid.path = O9FS_GBIT64(p);
	p += O9FS_BIT64SZ;
	d->mode = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	d->atime = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	d->mtime = O9FS_GBIT32(p);
	p += O9FS_BIT32SZ;
	d->length = O9FS_GBIT64(p);
	p += O9FS_BIT64SZ;

	nstr = 4;

	for (i = 0; i < nstr; i++) {
		if (p + O9FS_BIT16SZ > ebuf)
			return 0;
		ns = O9FS_GBIT16(p);
		p += O9FS_BIT16SZ;
		if (p + ns > ebuf)
			return 0;
		if (strs) {
			sv[i] = strs;
			memcpy(strs, p, ns);
			strs += ns;
			*strs++ = '\0';
		}
		p += ns;
	}

	if (strs) {
		d->name = sv[0];
		d->uid = sv[1];
		d->gid = sv[2];
		d->muid = sv[3];
	} else {
		d->name = nullstring;
		d->uid = nullstring;
		d->gid = nullstring;
		d->muid = nullstring;
	}
	
	return p - buf;
}
