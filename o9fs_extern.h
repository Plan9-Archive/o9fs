/* o9fs_subr.c */
int		o9fs_allocvp(struct mount *, struct o9fsfid *, struct vnode **, u_long);
int		o9fs_insertfid(struct o9fsfid *, struct o9fsfid *);
int		o9fs_rpc(struct o9fsmount *, struct o9fsfcall *, struct o9fsfcall *);
struct o9fsfid *o9fs_fid_lookup(struct o9fsmount *, struct o9fsfid *, char *);
void	o9fs_dumpchildlist(struct o9fsfid *);
u_int	o9fs_tokenize(char **, u_int, char *, char);
struct o9fsfid *getfid(struct o9fsmount *);
void	putfid(struct o9fsmount *, struct o9fsfid *);

/* o9fs_io.c */
int		o9fs_tcp_connect(struct o9fsmount *);
int		o9fs_tcp_write(struct o9fsmount *, struct uio *);
int		o9fs_tcp_read(struct o9fsmount *, struct uio *);
int		o9fs_tcp_close(struct o9fsmount *);

/* o9fs_9p.c */
int		o9fs_tversion(struct o9fsmount *, int, char *);
struct	o9fsfid *o9fs_tattach(struct o9fsmount *, struct o9fsfid *, char *, char *);
struct	o9fsstat *o9fs_tstat(struct o9fsmount *, char *);
struct	o9fsstat *o9fs_fstat(struct o9fsmount *, struct o9fsfid *);
struct	o9fsfid *o9fs_twalk(struct o9fsmount *, struct o9fsfid *, char *);
int		o9fs_topen(struct o9fsmount *, struct o9fsfid *, int);
int		o9fs_tread(struct o9fsmount *, struct o9fsfid *, int64_t, uint32_t, struct o9fsfcall **);

/* o9fs_conv*.c */
u_int	o9fs_convM2S(u_char*, u_int, struct o9fsfcall*);
u_int	o9fs_convS2M(struct o9fsfcall*, u_char*, u_int);
u_int	o9fs_sizeS2M(struct o9fsfcall*);
int		o9fs_statcheck(u_char *, u_int);
u_int	o9fs_convM2D(u_char *, u_int, struct o9fsstat *, char *);

extern int (**o9fs_vnodeop_p)(void *);

