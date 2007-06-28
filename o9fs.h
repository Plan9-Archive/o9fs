/* based on src/lib9pclient/fsimpl.h and include/fcall.h from p9p */

/* 9p specific info */
struct o9fs {
	char version[7];			/* we only support 9P2000 anyway */
	int msize;
	int ref;
	int fd;
	int nextfid;
	struct o9fsfid *root;
	struct o9fsfid *freefid;

#ifdef KERNEL
	struct simple_lock lock;	/* XXX is this the correct lock? */
#endif
};

struct o9fsmount;

struct o9fs_io {
	int	(*open) (struct o9fsmount *);
	int	(*write) (struct o9fsmount *, struct uio *);
	int	(*read) (struct o9fsmount *, struct uio *);
	int	(*close) (struct o9fsmount *);
};

extern struct o9fs_io io_tcp;

/* o9fs session */
struct o9fsmount {
	struct	mount *om_mp;		/* generic mount info */
	struct	o9fsnode *om_root;	/* local root of the tree */
	struct	o9fs	*om_o9fs;	/* 9P info */

	/* for use by transport routines */
	struct	o9fs_io *io;		/* io routines */
	struct	sockaddr *om_saddr;	/* server address */
	size_t	om_saddrlen;		/* saddr size */
	struct	socket *om_so;		/* socket to server */
	sa_family_t	om_sotype;		/* socket type */
};

/* directory entry */
struct o9fsdirentx {
	TAILQ_ENTRY(o9fsdirentx)		od_entries;
	u_long						od_namelen;
	char						od_name[MNAMELEN];
	struct o9fsnode				*od_node;	/* node dirent refers to */
};

TAILQ_HEAD(o9fsdirx, o9fsdirentx);		/* directory itself */

struct o9fsnode {
	enum vtype		on_type;
	ino_t			on_id;
	struct vnode	*on_vnode;
	u_long			on_flags;
	struct o9fsdirx	on_dir;	/* if dir, here is where we keep the entries */
};

struct o9fsqid {
	u_int64_t path;
	u_long vers;
	u_char type;
};

struct o9fsdir {
	/* system-modified data */
	u_short	type;   /* server type */
	u_int	dev;    /* server subtype */
	/* file data */
	struct	o9fsqid qid;    /* unique id from server */
	u_long	mode;   /* permissions */
	u_long	atime;  /* last read time */
	u_long	mtime;  /* last write time */
	int64_t	length; /* file length */	
	char	*name;  /* last element of path */
	char	*uid;   /* owner name */
	char	*gid;   /* group name */
	char	*muid;  /* last modifier name */
};


/* 9p file */
struct o9fsfid {
	int fid;
	int mode;
	struct o9fsqid qid;
	int64_t	offset;		/* rw offset */
	struct o9fs *fs;	/* our fs session */
#ifdef KERNEL
	struct simple_lock lock; /* XXX is this the correct lock? */
#endif
	struct o9fsfid *next;
};


#define VTO9(vp) ((struct o9fsnode *)(vp)->v_data)
#define VFSTOO9FS(mp) ((struct o9fsmount *)((mp)->mnt_data))

#define	O9FS_VERSION9P	"9P2000"
#define	O9FS_MAXWELEM	16

struct o9fsfcall
{
	u_char			type;
	uint32_t		fid;
	u_short			tag;
	uint32_t		msize;		/* Tversion, Rversion */
	char			*version;	/* Tversion, Rversion */

	uint32_t		oldtag;		/* Tflush */

	char			*ename;		/* Rerror */

	struct o9fsqid	qid;		/* Rattach, Ropen, Rcreate */
	uint32_t		iounit;		/* Ropen, Rcreate */
	struct o9fsqid	aqid;		/* Rauth */
	uint32_t		afid;		/* Tauth, Tattach */
	char			*uname;		/* Tauth, Tattach */
	char			*aname;		/* Tauth, Tattach */
	uint32_t		perm;		/* Tcreate */ 
	char			*name;		/* Tcreate */
	u_char			mode;		/* Tcreate, Topen */
	uint32_t		newfid;		/* Twalk */
	u_short			nwname;		/* Twalk */
	char			*wname[O9FS_MAXWELEM];	/* Twalk */
	u_short			nwqid;		/* Rwalk */
	struct o9fsqid	wqid[O9FS_MAXWELEM];	/* Rwalk */
	int64_t	offset;				/* Tread, Twrite */
	uint32_t count;				/* Tread, Twrite, Rread */
	char  *data;				/* Twrite, Rread */
	u_short	nstat;				/* Twstat, Rstat */
	u_char *stat;				/* Twstat, Rstat */
};


#define	O9FS_GBIT8(p)	((p)[0])
#define	O9FS_GBIT16(p)	((p)[0]|((p)[1]<<8))
#define	O9FS_GBIT32(p)	((uint32_t)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)))
#define	O9FS_GBIT64(p)	((uint32_t)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)) |\
				((uint64_t)((p)[4]|((p)[5]<<8)|((p)[6]<<16)|((p)[7]<<24)) << 32))

#define	O9FS_PBIT8(p,v)	(p)[0]=(v)
#define	O9FS_PBIT16(p,v)	(p)[0]=(v);(p)[1]=(v)>>8
#define	O9FS_PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24
#define	O9FS_PBIT64(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24;\
			(p)[4]=(v)>>32;(p)[5]=(v)>>40;(p)[6]=(v)>>48;(p)[7]=(v)>>56

#define	O9FS_BIT8SZ			1
#define	O9FS_BIT16SZ		2
#define	O9FS_BIT32SZ		4
#define	O9FS_BIT64SZ		8
#define	O9FS_QIDSZ	(O9FS_BIT8SZ+O9FS_BIT32SZ+O9FS_BIT64SZ)

/* O9FS_STATFIXLEN includes leading 16-bit count */
/* The count, however, excludes itself; total size is O9FS_BIT16SZ+count */
#define O9FS_STATFIXLEN	(O9FS_BIT16SZ+O9FS_QIDSZ+5*O9FS_BIT16SZ+4*O9FS_BIT32SZ+1*O9FS_BIT64SZ)	/* amount of fixed length data in a stat buffer */

#define	O9FS_NOTAG		(u_short)~0U	/* Dummy tag */
#define	O9FS_NOFID		(uint32_t)~0U	/* Dummy fid */
#define	O9FS_IOHDRSZ		24	/* ample room for Twrite/Rread header (iounit) */

enum
{
	O9FS_TVERSION	= 100,
	O9FS_RVERSION,
	O9FS_TAUTH		= 102,
	O9FS_RAUTH,
	O9FS_TATTACH	= 104,
	O9FS_RATTACH,
	O9FS_TERROR		= 106,	/* illegal */
	O9FS_RERROR,
	O9FS_TFLUSH		= 108,
	O9FS_RFLUSH,
	O9FS_TWALK 		= 110,
	O9FS_RWALK,
	O9FS_TOPEN		= 112,
	O9FS_ROPEN,
	O9FS_TCREATE	= 114,
	O9FS_RCREATE,
	O9FS_TREAD		= 116,
	O9FS_RREAD,
	O9FS_TWRITE		= 118,
	O9FS_RWRITE,
	O9FS_TCLUNK		= 120,
	O9FS_RCLUNK,
	O9FS_TREMOVE	= 122,
	O9FS_RREMOVE,
	O9FS_TSTAT 		= 124,
	O9FS_RSTAT,
	O9FS_TWSTAT		= 126,
	O9FS_RWSTAT,
	O9FS_TMAX,

	Topenfd = 	98,
	Ropenfd
};
