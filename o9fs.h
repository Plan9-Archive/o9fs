#define DBG(x...) do{\
	if(Debug){\
		printf("%s: ", __FUNCTION__);\
		printf(x);\
	}\
}while(0)

#define DIN() DBG(">>>\n")
#define DRET() DBG("<<<\n")

#define printvp(x) do{\
	if(Debug){\
		printf("%s: ", __FUNCTION__);\
		_printvp((x));\
	}\
}while(0)

struct o9fsqid {
	uint8_t		type;
	uint32_t	vers;
	uint64_t	path;
};

struct o9fsstat {
	uint16_t	type;			/* server type */
	uint32_t	dev;			/* server subtype */
	struct		o9fsqid qid;	/* unique id from server */
	uint32_t	mode;			/* permissions */
	uint32_t	atime;			/* last read time */
	uint32_t	mtime;			/* last write time */
	uint64_t	length;			/* file length */	
	char		*name;			/* last element of path */
	char		*uid;			/* owner name */
	char		*gid;			/* group name */
	char		*muid;			/* last modifier name */
};

/*
 * Many vnodes can refer to the same o9fid and this is accounted for in ref.
 * When ref drops to zero, the o9fid is clunked.
 * Every change in o9fid should be checking in the cloning process in o9fs_9p.c:/^o9fs_walk
 */
struct o9fid {
	int32_t		fid;
	int8_t		mode;			/* open mode */
	uint32_t	iounit;
	struct		o9fsqid	qid;
	uint64_t	offset;

	struct		o9fid *parent;
	int			ref;
	TAILQ_ENTRY(o9fid) next;
};

#define VTO9(vp) ((struct o9fsfid *)(vp)->v_data)
#define VTO92(vp) ((struct o9fid *)(vp)->v_data)
#define VFSTOO9FS(mp) ((struct o9fs *)((mp)->mnt_data))


#define	O9FS_VERSION9P	"9P2000"
#define	O9FS_MAXWELEM	16

enum {
	Offtype	= 4,
	Offtag	= 5,

	Minhd	= Offtag + 2,		/* Minimum 9P header size, independent of message type */
	Maxhd	= 23,						/* Maximum 9P header size */
};

struct o9fs {
	struct	mount *mp;
	struct	vnode *vroot;		/* Local root of the tree */
	struct	file *servfp;		/* File pointing to the server */
	long	msize;				/* Maximum size of our payload */
	
	/* 
     * Buffers for I/O
	 * Both buffers have the client requested msize,
	 * but only the number of bytes in this structure's msize
	 * should be used.
	 */
	u_char	*inbuf;
	u_char	*outbuf;

	TAILQ_HEAD(, o9fid)	activeq;
	TAILQ_HEAD(, o9fid) freeq;
	int	nextfid;
};


/* O9FS_STATFIXLEN includes leading 16-bit count */
/* The count, however, excludes itself; total size is O9FS_BIT16SZ+count */
#define O9FS_STATFIXLEN	(O9FS_BIT16SZ+O9FS_QIDSZ+5*O9FS_BIT16SZ+4*O9FS_BIT32SZ+1*O9FS_BIT64SZ)	/* amount of fixed length data in a stat buffer */

#define	O9FS_NOTAG		(u_short)~0U	/* Dummy tag */
#define	O9FS_NOFID		(uint32_t)~0U	/* Dummy fid */
#define	O9FS_IOHDRSZ	24				/* ample room for Twrite/Rread header (iounit) */

#define O9FS_GBIT8(p)	((p)[0])
#define O9FS_GBIT16(p)	((p)[0]|((p)[1]<<8))
#define O9FS_GBIT32(p)	((uint32_t)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)))
#define O9FS_GBIT64(p)	((uint32_t)((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24)) |\
						((uint64_t)((p)[4]|((p)[5]<<8)|((p)[6]<<16)|((p)[7]<<24)) << 32))
 
#define O9FS_PBIT8(p,v)		(p)[0]=(v)
#define O9FS_PBIT16(p,v)	(p)[0]=(v);(p)[1]=(v)>>8   
#define O9FS_PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24
#define O9FS_PBIT64(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24;\
							(p)[4]=(v)>>32;(p)[5]=(v)>>40;(p)[6]=(v)>>48;(p)[7]=(v)>>56

#define O9FS_BIT8SZ		1
#define O9FS_BIT16SZ	2
#define O9FS_BIT32SZ	4
#define O9FS_BIT64SZ	8
#define O9FS_QIDSZ      (O9FS_BIT8SZ+O9FS_BIT32SZ+O9FS_BIT64SZ)

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
	O9FS_TMAX
};

/* bits in Qid.type */
#define O9FS_QTDIR           0x80            /* type bit for directories */
#define O9FS_QTAPPEND        0x40            /* type bit for append only files */   
#define O9FS_QTEXCL          0x20            /* type bit for exclusive use files */
#define O9FS_QTMOUNT         0x10            /* type bit for mounted channel */
#define O9FS_QTAUTH          0x08            /* type bit for authentication file */
#define O9FS_QTTMP           0x04            /* type bit for non-backed-up file */  
#define O9FS_QTSYMLINK       0x02            /* type bit for symbolic link */
#define O9FS_QTFILE          0x00            /* type bits for plain file */

/* bits in Dir.mode */
#define O9FS_DMDIR           0x80000000      /* mode bit for directories */
#define O9FS_DMAPPEND        0x40000000      /* mode bit for append only files */
#define O9FS_DMEXCL          0x20000000      /* mode bit for exclusive use files */
#define O9FS_DMMOUNT         0x10000000      /* mode bit for mounted channel */
#define O9FS_DMAUTH          0x08000000      /* mode bit for authentication file */
#define O9FS_DMTMP           0x04000000      /* mode bit for non-backed-up file */

#define O9FS_DMREAD          0x4             /* mode bit for read permission */
#define O9FS_DMWRITE         0x2             /* mode bit for write permission */
#define O9FS_DMEXEC          0x1             /* mode bit for execute permission */

/* open modes */
#define O9FS_OREAD	0		/* open for read */
#define O9FS_OWRITE	1		/* write */
#define O9FS_ORDWR	2		/* read and write */
#define O9FS_OEXEC	3		/* execute, == read but check execute permission */
#define O9FS_OTRUNC	16		/* or'ed in (except for exec), truncate file first */
#define O9FS_OCEXEC		32		/* or'ed in, close on exec */
#define O9FS_ORCLOSE	64		/* or'ed in, remove on close */
#define O9FS_ODIRECT	128		/* or'ed in, direct access */
#define O9FS_ONONBLOCK	256		/* or'ed in, non-blocking call */
#define O9FS_OEXCL		0x1000	/* or'ed in, exclusive use (create only) */
#define O9FS_OLOCK		0x2000	/* or'ed in, lock after opening */
#define O9FS_OAPPEND	0x4000	/* or'ed in, append only */


#define O9FS_DIRMAX		(sizeof(struct o9fsstat)+65535U)	/* max length of o9fsstat structure */

/* hack */
#define M_O9FS M_TEMP
#define MOUNT_O9FS "o9fs"
#define VT_O9FS VT_NON
struct o9fs_args {
	char *hostname;
	int fd;
};
