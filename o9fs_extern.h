/* o9fs_subr.c */
int		o9fs_allocvp(struct mount *, struct o9fsfid *, struct vnode **, u_long);
void	o9fs_freevp(struct vnode *);
int		o9fs_insertfid(struct o9fsfid *, struct o9fsfid *);
struct o9fsfid *o9fs_fid_lookup(struct o9fsmount *, struct o9fsfid *, char *);
void	o9fs_dumpchildlist(struct o9fsfid *);
u_int	o9fs_tokenize(char **, u_int, char *, char);
struct o9fsfid *o9fs_getfid(struct o9fsmount *);
void	o9fs_putfid(struct o9fsmount *, struct o9fsfid *);
int		o9fs_ptoumode(int);
int		o9fs_utopmode(int);
int		o9fs_uflags2omode(int);
void	o9fs_freefcall(struct o9fsfcall *);
void	o9fs_freestat(struct o9fsstat *);
long	o9fs_dirpackage(u_char *, long, struct o9fsstat **);

/* o9fs_io.c */
int		o9fs_tcp_connect(struct o9fsmount *);
int		o9fs_tcp_write(struct o9fsmount *, struct uio *);
int		o9fs_tcp_read(struct o9fsmount *, struct uio *);
int		o9fs_tcp_close(struct o9fsmount *);

/* o9fs_9p.c */
int		o9fs_tversion(struct o9fsmount *, int, char *);
struct 	o9fsfid *o9fs_tauth(struct o9fsmount *, char *, char *);
struct	o9fsfid *o9fs_tattach(struct o9fsmount *, struct o9fsfid *, char *, char *);
struct	o9fsstat *o9fs_fstat(struct o9fsmount *, struct o9fsfid *);
struct	o9fsfid *o9fs_twalk(struct o9fsmount *, int, char *);
int		o9fs_topen(struct o9fsmount *, struct o9fsfid *, int);
long	o9fs_tread(struct o9fsmount *, struct o9fsfid *, void *, u_long, int64_t);
long	o9fs_twrite(struct o9fsmount *, struct o9fsfid *, void *, long, int64_t);
void	o9fs_fidclunk(struct o9fsmount *, struct o9fsfid *);

/* o9fs_conv.c */
void	o9fs_fcall(struct o9fsmsg *, struct o9fsfcall *);
u_int	o9fs_fcalltomsg(struct o9fsmsg *, struct o9fsfcall *);
u_int	o9fs_msgtofcall(struct o9fsmsg *, struct o9fsfcall *);

/* o9fs_msg.c */
int		o9fs_msgoverflow(struct o9fsmsg *, size_t);
struct o9fsmsg *o9fs_msg(u_char *, u_int, enum o9fs_msgmode);
struct o9fsmsg *o9fs_msgalloc(size_t size);
void	o9fs_msguint(struct o9fsmsg *, size_t, u_int *);
void	o9fs_msgbyte(struct o9fsmsg *, u_char *);
void	o9fs_msgword(struct o9fsmsg *, u_short *);
void	o9fs_msgdword(struct o9fsmsg *, u_int *);
void	o9fs_msgqword(struct o9fsmsg *, uint64_t *);
void	o9fs_msgstring(struct o9fsmsg *, char **);
void	o9fs_msgstrings(struct o9fsmsg *, u_short *, char **);
void	o9fs_msgdata(struct o9fsmsg *, char **, size_t);
void	o9fs_msgqid(struct o9fsmsg *, struct o9fsqid *);
void	o9fs_msgqids(struct o9fsmsg *, u_short *, struct o9fsqid *);
void	o9fs_msgstat(struct o9fsmsg *, struct o9fsstat *);
size_t	o9fs_sizeof_stat(struct o9fsstat *);

/* o9fs_rpc.c */
u_int	o9fs_recvmsg(struct o9fsmount *, struct o9fsmsg *);
u_int	o9fs_sendmsg(struct o9fsmount *, struct o9fsmsg *);
int		o9fs_rpc(struct o9fsmount *, struct o9fsfcall *, struct o9fsfcall *);	

/* o9fs_debug.c */
void	o9fs_debugfcall(struct o9fsfcall *);
extern int (**o9fs_vnodeop_p)(void *);

