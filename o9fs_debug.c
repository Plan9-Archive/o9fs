#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

int
o9fs_debugqid(char *data, int len, struct o9fsqid *qid)
{
	int n;
	char b[6];

	n = 0;
	if (qid->type & O9FS_QTDIR)
		b[n++] = 'd';	
	if (qid->type & O9FS_QTAPPEND)
		b[n++] = 'a';
	if (qid->type & O9FS_QTEXCL)
		b[n++] = 'x';
	if (qid->type & O9FS_QTAUTH)
		b[n++] = 'A';
	if (qid->type & O9FS_QTTMP)
		b[n++] = 't';
	b[n] = '\0';
	return snprintf(data, len, "(%.16llx %x %s)", (int64_t)qid->path, qid->vers, b);
}

int
o9fs_debugperm(char *data, int len, int perm)
{
	int n;
	char b[6];

	n = 0;
	if (perm & O9FS_DMDIR)
		b[n++] = 'd';
	if (perm & O9FS_DMAPPEND)
		b[n++] = 'a';
	if (perm & O9FS_DMAUTH)
		b[n++] = 'A';
	if (perm & O9FS_DMEXCL)
		b[n++] = 'l';
	if (n > 0)
		b[n++] = '+';
	b[n] = '\0';
	return snprintf(data, len, "%s%03o", b, perm&0777); /* XXX or 077 */
}

int
o9fs_debugdata(char *buf, int len, uint8_t *data, int count)
{
	int i, printable;
	uint8_t *p;

	printable = 1;
	if (count > 8168)
		count = 8168;

	printf("debugdata: count=%d\n", count);
	for (i = 0; i < count && printable; i++)
		if((data[i] < 32 && data[i] != '\n' && data[i] != '\t') || data[i] > 127)
			printable = 0;
	p = buf;
	if (printable) {
		memcpy(p, data, count);
		p += count;
	} else {
		for (i = 0; i < count; i++) {
			if (i>0 && i%4 == 0)
				*p++ = ' ';
			if (i>0 && i%32 == 0)
				*p++ = '\n';
			snprintf(p, len, "%02x", data[i]);
			p += 2;
		}
	}
	*p = '\0';
	return p - data;
}

int
o9fs_debugstat(char *data, int len, struct o9fsstat *stat)
{
	int n;

	n = snprintf(data, len, "'%s' '%s' '%s' '%s' q ", 
				stat->name, stat->uid, stat->gid, stat->muid);
	n += o9fs_debugqid(data + n, len - n, &stat->qid);
	n += snprintf(data + n, len - n, " m ");
	n += o9fs_debugperm(data + n, len - n, stat->mode);
	n += snprintf(data + n, len - n, " at %d mt %d l %lld", stat->atime, stat->mtime, stat->length);
	return n;
}

void
o9fs_debugfcall(struct o9fsfcall *fcall)
{
	int i, fid, type, tag, n, len;
	struct o9fsmsg *m;
	struct o9fsstat *stat;
	char *buf, *tmp;

	buf = malloc(8192, M_O9FS, M_WAITOK);
	len = 8192;
	tmp = malloc(200, M_O9FS, M_WAITOK);
	n = 0;
	type = fcall->type;
	fid = fcall->fid;
	tag = fcall->tag;

	if (type != O9FS_RREAD)
		return;

	switch (type) {
	case O9FS_TVERSION:
		snprintf(buf, len, "Tversion tag %u msize %u version '%s'\n",
			fcall->tag, fcall->msize, fcall->version);
		break;
	case O9FS_RVERSION:
		snprintf(buf, len, "Rversion tag %u msize %u version '%s'\n",
			fcall->tag, fcall->msize, fcall->version);
		break;
	case O9FS_TAUTH:
		snprintf(buf, len, "Tauth tag %u afid %d uname %s aname %s\n",
			fcall->tag, fcall->afid, fcall->uname, fcall->aname);
		break;
	case O9FS_RAUTH:
		n += snprintf(buf, len, "Rauth tag %u qid ", fcall->tag);
		o9fs_debugqid(buf + n, len - n, &fcall->qid);
		break;
	case O9FS_TATTACH:
		snprintf(buf, len, "Tattach tag %u fid %d afid %d uname %s aname %s\n",
			fcall->tag, fcall->fid, fcall->afid, fcall->uname, fcall->aname);
		break;
	case O9FS_RATTACH:
		n += snprintf(buf, len, "Rattach tag %u qid ", fcall->tag);
		o9fs_debugqid(buf + n, len - n, &fcall->qid);
		break;
	case O9FS_TCLUNK:
		snprintf(buf, len, "Tclunk tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RCLUNK:
		snprintf(buf, len, "Rclunk tag %u\n", fcall->tag);
		break;
	case O9FS_TREMOVE:
		snprintf(buf, len, "Tremove tag %u fid %u\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RREMOVE:
		break;
	case O9FS_TSTAT:
		snprintf(buf, len, "Tstat tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RSTAT:
		n += snprintf(buf, len, "Rstat tag %u ", fcall->tag);
		m = o9fs_msg(fcall->stat, fcall->nstat, O9FS_MLOAD);
		stat = malloc(sizeof(struct o9fsstat) + fcall->nstat, 
				M_O9FS, M_WAITOK);
		o9fs_msgstat(m, stat);
		o9fs_debugstat(buf + n, len - n, stat);
		o9fs_freestat(stat);
		break;
	case O9FS_RERROR:
		snprintf(buf, len, "Rerror tag %u ename %s\n", fcall->tag, fcall->ename);
		break;
	case O9FS_TWALK:
		n += snprintf(buf, len, "Twalk tag %u fid %d newfid %d nwname ", 
			fcall->tag, fcall->fid, fcall->newfid, fcall->nwname);
		for (i = 0; i < fcall->nwname; i++)
			n += snprintf(buf + n, len - n, "%d: %s\n", i, fcall->wname[i]);
		break;
	case O9FS_RWALK:
		n += snprintf(buf, len, "Rwalk tag %u nwqid %d", fcall->tag, fcall->nwqid);
		for (i = 0; i < fcall->nwqid; i++)
			n += o9fs_debugqid(buf + n, len - n, &fcall->wqid[i]);
		break;
	case O9FS_TOPEN:
		snprintf(buf, len, "Topen tag %u fid %d mode %d\n", fcall->tag, fcall->fid, fcall->mode);
		break;
	case O9FS_ROPEN:
		n += snprintf(buf, len, "Ropen tag %u", fcall->tag);
		n += o9fs_debugqid(buf + n, len - n, &fcall->qid);
		snprintf(buf + n, len - n, " iounit %d\n", fcall->iounit);
		break;
	case O9FS_TCREATE:
		n += snprintf(buf, len, "Tcreate tag %u fid %d name %s", fcall->tag, fcall->fid, fcall->name);
		n += o9fs_debugperm(buf + n, len - n, fcall->perm);
		snprintf(buf + n, len -n, " mode %d\n", fcall->mode);
		break;
	case O9FS_RCREATE:
		n += snprintf(buf, len, "Rcreate tag %u", fcall->tag);
		n += o9fs_debugqid(buf + n, len - n, &fcall->qid);
		snprintf(buf + n, len - n, " iounit %d\n", fcall->iounit);
		break;
	case O9FS_TREAD:
		snprintf(buf, len, "Tread tag %u fid %d offset %ld count %d\n",
			fcall->tag, fcall->fid, fcall->offset, fcall->count);
		break;
	case O9FS_RREAD:
		n += snprintf(buf, len, "Rread tag %u count %u data\n", fcall->tag, fcall->count);
		o9fs_debugdata(buf + n, len - n, fcall->data, fcall->count);
		break;
	case O9FS_TWRITE:
		n += snprintf(buf, len, "Twrite tag %u fid %d offset %lld count %u data ",
			fcall->tag, fcall->fid, fcall->offset, fcall->count);
		o9fs_debugdata(buf + n, len - n, fcall->data, fcall->count);
		break;
	case O9FS_RWRITE:
		snprintf(buf, len, "Rwrite tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	default:
		break;
	}
	printf("%s\n", buf);
}

