#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

static
void
o9fs_debugqid(struct o9fsqid *qid)
{
	printf("(%lld %d ", qid->path, qid->vers);
	if (qid->type & O9FS_QTDIR)
		printf("d");
	if (qid->type & O9FS_QTAPPEND)
		printf("a");
	if (qid->type & O9FS_QTEXCL)
		printf("x");
	if (qid->type & O9FS_QTAUTH)
		printf("A");
	if (qid->type & O9FS_QTTMP)
		printf("t");
	printf(")");
}

static
void
o9fs_debugperm(int perm)
{
	if (perm & O9FS_DMDIR)
		printf("d");
	if (perm & O9FS_DMAPPEND)
		printf("a");
	if (perm & O9FS_DMAUTH)
		printf("A");
	if (perm & O9FS_DMEXCL)
		printf("l");
	printf("%03o", perm&0777); /* XXX or 077 */
}

static
void
o9fs_debugdata(char *data, int count)
{
	int i;
	
	i = 0;
	while (i < count) {
		printf("%02x", data[i]);
		/* from v9fs */
		if (i%4 == 3)
			printf(" ");
		if (i%32 == 31)
			printf("\n");
		i++;
	}
	printf("\n");
}

static
void
o9fs_debugstat(struct o9fsstat *stat)
{
	printf("'%s' '%s' '%s' '%s' q ", stat->name, stat->uid, stat->gid, stat->muid);
	o9fs_debugqid(&stat->qid);
	printf(" m ");
	o9fs_debugperm(stat->mode);
	printf(" at %d mt %d l %lld", stat->atime, stat->mtime, stat->length);
}

void
o9fs_debugfcall(struct o9fsfcall *fcall)
{
	int i;
	struct o9fsmsg *m;
	struct o9fsstat *stat;

	switch (fcall->type) {
	case O9FS_TVERSION:
		printf("Tversion tag %u msize %u version '%s'\n",
			fcall->tag, fcall->msize, fcall->version);
		break;
	case O9FS_RVERSION:
		printf("Rversion tag %u msize %u version '%s'\n",
			fcall->tag, fcall->msize, fcall->version);
		break;
	case O9FS_TAUTH:
		printf("Tauth tag %u afid %d uname %s aname %s\n",
			fcall->tag, fcall->afid, fcall->uname, fcall->aname);
		break;
	case O9FS_RAUTH:
		printf("Rauth tag %u qid ", fcall->tag);
		o9fs_debugqid(&fcall->qid);
		break;
	case O9FS_TATTACH:
		printf("Tattach tag %u fid %d afid %d uname %s aname %s\n",
			fcall->tag, fcall->fid, fcall->afid, fcall->uname, fcall->aname);
		break;
	case O9FS_RATTACH:
		printf("Rattach tag %u qid ", fcall->tag);
		o9fs_debugqid(&fcall->qid);
		printf("\n");
		break;
	case O9FS_TCLUNK:
		printf("Tclunk tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RCLUNK:
		printf("Rclunk tag %u\n", fcall->tag);
		break;
	case O9FS_TREMOVE:
		printf("Tremove tag %u fid %u\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RREMOVE:
		break;
	case O9FS_TSTAT:
		printf("Tstat tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	case O9FS_RSTAT:
		printf("Rstat tag %u ", fcall->tag);
		m = o9fs_msg(fcall->stat, fcall->nstat, O9FS_MLOAD);
		stat = malloc(sizeof(struct o9fsstat) + fcall->nstat, 
				M_O9FS, M_WAITOK | M_ZERO);
		o9fs_msgstat(m, stat);
		o9fs_debugstat(stat);
		printf("\n");
		o9fs_freestat(stat);
		break;
	case O9FS_RERROR:
		printf("Rerror tag %u ename %s\n", fcall->tag, fcall->ename);
		break;
	case O9FS_TWALK:
		printf("Twalk tag %u fid %d newfid %d nwname", 
			fcall->tag, fcall->fid, fcall->newfid, fcall->nwname);
		for (i = 0; i < fcall->nwname; i++)
			printf("%d: %s\n", i, fcall->wname[i]);
		break;
	case O9FS_RWALK:
		printf("Rwalk tag %u nwqid %d", fcall->tag, fcall->nwqid);
		for (i = 0; i < fcall->nwqid; i++)
			o9fs_debugqid(&fcall->wqid[i]);
		break;
	case O9FS_TOPEN:
		printf("Topen tag %u fid %d mode %d", fcall->tag, fcall->fid, fcall->mode);
		break;
	case O9FS_ROPEN:
		printf("Ropen tag %u", fcall->tag);
		o9fs_debugqid(&fcall->qid);
		printf(" iounit %d\n", fcall->iounit);
		break;
	case O9FS_TCREATE:
		printf("Tcreate tag %u fid %d name %s", fcall->tag, fcall->fid, fcall->name);
		o9fs_debugperm(fcall->perm);
		printf(" mode %d\n", fcall->mode);
		break;
	case O9FS_RCREATE:
		printf("Rcreate tag %u", fcall->tag);
		o9fs_debugqid(&fcall->qid);
		printf(" iounit %d\n", fcall->iounit);
		break;
	case O9FS_TREAD:
		printf("Tread tag %u fid %d offset %ld count %d\n",
			fcall->tag, fcall->fid, fcall->offset, fcall->count);
		break;
	case O9FS_RREAD:
		printf("Rread tag %u count %u data ", fcall->tag, fcall->count);
		o9fs_debugdata(fcall->data, fcall->count);
		break;
	case O9FS_TWRITE:
		printf("Twrite tag %u fid %d offset %lld count %u data ",
			fcall->tag, fcall->fid, fcall->offset, fcall->count);
		o9fs_debugdata(fcall->data, fcall->count);
		break;
	case O9FS_RWRITE:
		printf("Rwrite tag %u fid %d\n", fcall->tag, fcall->fid);
		break;
	default:
		break;
	}
}
