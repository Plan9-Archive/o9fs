#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include <miscfs/o9fs/o9fs.h>
#include <miscfs/o9fs/o9fs_extern.h>

/* based on the nice libixp implementation of the convertions */

struct o9fsmsg *
o9fs_msg(u_char *data, u_int len, enum o9fs_msgmode mode)
{
	struct o9fsmsg *m;

	m = (struct o9fsmsg *) malloc(sizeof(struct o9fsmsg), M_O9FS, M_WAITOK);
	m->m_base = m->m_cur = data;
	m->m_end = m->m_base + len;
	m->m_size = len;
	m->m_mode = mode;
	return m;
}

int
o9fs_msgoverflow(struct o9fsmsg *m, size_t size)
{
	return m->m_cur + size > m->m_end;
}

/* generic uint packing to a message buffer */
void
o9fs_msguint(struct o9fsmsg *m, size_t size, u_int *n)
{
	int v;

	if(o9fs_msgoverflow(m, size)) {
		printf("o9fs_msguint overflowed:\n"
			"base=%p cur=%p end=%p size=%p\n",
			m->m_base, m->m_cur, m->m_end, size);
		return;
	}

	switch(m->m_mode) {
	case O9FS_MSTORE:
		v = *n;
		switch(size) {
		case O9FS_DWORD:
			m->m_cur[3] = v>>24;
			m->m_cur[2] = v>>16;
		case O9FS_WORD:
			m->m_cur[1] = v>>8;
		case O9FS_BYTE:
			m->m_cur[0] = v;
			break;
		}
	case O9FS_MLOAD:
		v = 0;
		switch(size) {
		case O9FS_DWORD:
			v |= m->m_cur[3]<<24;
			v |= m->m_cur[2]<<16;
		case O9FS_WORD:
			v |= m->m_cur[1]<<8;
		case O9FS_BYTE:
			v |= m->m_cur[0];
			break;
		}
		*n = v;
	}
	m->m_cur += size;
}


void
o9fs_msgbyte(struct o9fsmsg *m, u_char *n)
{
	u_int v;

	v = *n;
	o9fs_msguint(m, O9FS_BYTE, &v);
	*n = (u_char)v;	
}

void
o9fs_msgword(struct o9fsmsg *m, u_short *n)
{
	u_int v;
	
	v = *n;
	o9fs_msguint(m, O9FS_WORD, &v);
	*n = (u_short)v;
}

void
o9fs_msgdword(struct o9fsmsg *m, u_int *n)
{
	o9fs_msguint(m, O9FS_DWORD, n);
}

void
o9fs_msgqword(struct o9fsmsg *m, uint64_t *n)
{
	u_int vl, vh;
	
	vl = (u_int)*n;
	vh = (u_int)(*n>>32);

	o9fs_msguint(m, O9FS_DWORD, &vl);
	o9fs_msguint(m, O9FS_DWORD, &vh);

	*n = vl | ((uint64_t)vh<<32);
}

void
o9fs_msgstring(struct o9fsmsg *m, char **s)
{
	u_short len;
	
	if (m->m_mode == O9FS_MSTORE)
		len = strlen(*s);
	o9fs_msgword(m, &len);
	
	if (!o9fs_msgoverflow(m, len)) {
		if (m->m_mode == O9FS_MLOAD) {
			*s = (char *)malloc(len + 1, M_O9FS, M_WAITOK);
			memcpy(*s, m->m_cur, len);
			(*s)[len] = '\0';
		} else
			memcpy(m->m_cur, *s, len);
		m->m_cur += len;
	}
}

void
o9fs_msgstrings(struct o9fsmsg *m, u_short *n, char *strings[])
{
	u_char *s;
	u_int i, size;
	u_short len;

	o9fs_msgword(m, n);
	if (*n > O9FS_MAXWELEM) {
		m->m_cur = m->m_end+1;
		return;
	}

	if (m->m_mode == O9FS_MLOAD) {
		s = m->m_cur;
		size = 0;
		for (i = 0; i < *n; i++) {
			o9fs_msgword(m, &len);
			m->m_cur += len;
			size += len;
			if (m->m_cur > m->m_end)
				return;
		}
		m->m_cur = s;
		size += *n;
		s = malloc(size, M_O9FS, M_WAITOK);
	}
	
	for (i = 0; i < *n; i++) {
		if (m->m_mode == O9FS_MSTORE)
			len = strlen(strings[i]);
		o9fs_msgword(m, &len);

		if (m->m_mode == O9FS_MLOAD) {
			memcpy(s, m->m_cur, len);
			strings[i] = (char *)s;
			s += len;
			m->m_cur += len;
			*s++ = '\0';
		} else
			o9fs_msgdata(m, &strings[i], len);
	}
}

void
o9fs_msgdata(struct o9fsmsg *m, char **data, size_t len)
{
	if (!o9fs_msgoverflow(m, len)) {
		if (m->m_mode == O9FS_MLOAD) {
			*data = malloc(len, M_O9FS, M_WAITOK);
			memcpy(*data, m->m_cur, len);
		} else
			memcpy(m->m_cur, *data, len);
		m->m_cur += len;
	}
//	m->m_cur += len; //XXX
}

void
o9fs_msgqid(struct o9fsmsg *m, struct o9fsqid *qid)
{
	o9fs_msgbyte(m, &qid->type);
	o9fs_msgdword(m, (u_int *)&qid->vers); /* XXX */
	o9fs_msgqword(m, &qid->path);
}

void
o9fs_msgqids(struct o9fsmsg *m, u_short *n, struct o9fsqid qid[])
{
	int i;

	o9fs_msgword(m, n);
	if (*n > O9FS_MAXWELEM) {
		m->m_cur = m->m_end+1;
		return;
	}
	
	for (i = 0; i < *n; i++)
		o9fs_msgqid(m, &qid[i]);
}

size_t
o9fs_sizeof_stat(struct o9fsstat *stat)
{
	return (
		O9FS_WORD + O9FS_WORD + O9FS_DWORD + 
		(O9FS_BYTE + O9FS_DWORD + O9FS_QWORD) +
		O9FS_DWORD + O9FS_DWORD + O9FS_DWORD + 
		O9FS_QWORD + O9FS_WORD + 
		O9FS_WORD + strlen(stat->name) +
		O9FS_WORD + strlen(stat->uid) +
		O9FS_WORD + strlen(stat->gid) +
		O9FS_WORD + strlen(stat->muid));
}

void
o9fs_msgstat(struct o9fsmsg *m, struct o9fsstat *stat)
{
	u_short size;

	if (m->m_mode == O9FS_MSTORE)
		size = o9fs_sizeof_stat(stat) - 2;
	
	o9fs_msgword(m, &size);
	o9fs_msgword(m, &stat->type);
	o9fs_msgdword(m, &stat->dev);
	o9fs_msgqid(m, &stat->qid);
	o9fs_msgdword(m, &stat->mode);
	o9fs_msgdword(m, &stat->atime);
	o9fs_msgdword(m, &stat->mtime);
	o9fs_msgqword(m, &stat->length);
	o9fs_msgstring(m, &stat->name);
	o9fs_msgstring(m, &stat->uid);
	o9fs_msgstring(m, &stat->gid);
	o9fs_msgstring(m, &stat->muid);
}
