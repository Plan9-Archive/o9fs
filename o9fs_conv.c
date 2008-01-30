#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

void
o9fs_fcall(struct o9fsmsg *m, struct o9fsfcall *fcall)
{
	o9fs_msgbyte(m, &fcall->type);
	o9fs_msgword(m, &fcall->tag);

//	printf("fcall mode = %s\n", (m->m_mode == O9FS_MSTORE)? "store":"load");
	switch(fcall->type) {
	case O9FS_TVERSION:
	case O9FS_RVERSION:
		o9fs_msgdword(m, &fcall->msize);
		o9fs_msgstring(m, &fcall->version);
		break;
	case O9FS_TAUTH:
		o9fs_msgdword(m, &fcall->afid);
		o9fs_msgstring(m, &fcall->uname);
		o9fs_msgstring(m, &fcall->aname);
		break;
	case O9FS_RAUTH:
		o9fs_msgqid(m, &fcall->aqid);
		break;
	case O9FS_TATTACH:
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgdword(m, &fcall->afid);
		o9fs_msgstring(m, &fcall->uname);
		o9fs_msgstring(m, &fcall->aname);
		break;
	case O9FS_RATTACH:
		o9fs_msgqid(m, &fcall->qid);
		break;
	case O9FS_TCLUNK:
	case O9FS_RCLUNK:
	case O9FS_TREMOVE:
	case O9FS_TSTAT:
		o9fs_msgdword(m, &fcall->fid);
		break;
	case O9FS_RSTAT:
		o9fs_msgword(m, &fcall->nstat);
		o9fs_msgdata(m, (char **)&fcall->stat, fcall->nstat);
		break;
	case O9FS_RERROR:
		o9fs_msgstring(m, &fcall->ename);
		break;
	case O9FS_TWALK:
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgdword(m, &fcall->newfid);
		o9fs_msgstrings(m, &fcall->nwname, fcall->wname);
		break;
	case O9FS_RWALK:
		o9fs_msgqids(m, &fcall->nwqid, fcall->wqid);
		break;
	case O9FS_TOPEN:
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgbyte(m, &fcall->mode);
		break;
	case O9FS_ROPEN:
	case O9FS_RCREATE:
		o9fs_msgqid(m, &fcall->qid);
		o9fs_msgdword(m, &fcall->iounit);
		break;
	case O9FS_TCREATE:
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgstring(m, &fcall->name);
		o9fs_msgdword(m, &fcall->perm);
		o9fs_msgbyte(m, &fcall->mode);
		break;
	case O9FS_TREAD:
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgqword(m, &fcall->offset);
		o9fs_msgdword(m, &fcall->count);
		break;
	case O9FS_RREAD:
		o9fs_msgdword(m, &fcall->count);
		o9fs_msgdata(m, &fcall->data, fcall->count);
		break;
	case O9FS_TWRITE:
	//	printf("fcall: Twrite\n");
		o9fs_msgdword(m, &fcall->fid);
		o9fs_msgqword(m, &fcall->offset);
		o9fs_msgdword(m, &fcall->count);
		o9fs_msgdata(m, &fcall->data, fcall->count);
		break;
	case O9FS_RWRITE:
		o9fs_msgdword(m, &fcall->fid);
		break;	
	default: 
		printf("fcall: msg type not implemented %d\n", fcall->type);
	}
}

u_int
o9fs_fcalltomsg(struct o9fsmsg *m, struct o9fsfcall *fcall)
{
	u_int size;

	m->m_end = m->m_base + m->m_size;
	m->m_cur = m->m_base + O9FS_DWORD;
	m->m_mode = O9FS_MSTORE;
	o9fs_fcall(m, fcall);

	if (m->m_cur > m->m_end) {
			printf("o9fs_fcalltomsg: overflow\n"
				"cur=%p end=%p\n", m->m_cur, m->m_end);
			return 0;
	}
	
	m->m_end = m->m_cur;
	size = m->m_end - m->m_base;
	
	m->m_cur = m->m_base;
	o9fs_msgdword(m, &size);

	m->m_cur = m->m_base;
	return size;
}

u_int
o9fs_msgtofcall(struct o9fsmsg *m, struct o9fsfcall *fcall)
{
	m->m_cur = m->m_base + O9FS_DWORD;
	m->m_mode = O9FS_MLOAD;
	o9fs_fcall(m, fcall);

	if (m->m_cur > m->m_end)
		return 0;
	
	return m->m_cur - m->m_base;
}
