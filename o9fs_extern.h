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
struct	o9fsfid *o9fs_findfid(struct o9fs *, int);
void		printfidvp(struct o9fsfid *);

/* o9fs_9p.c */
struct	o9fsstat *o9fs_fstat(struct o9fsmount *, struct o9fsfid *);
struct	o9fsfid *o9fs_twalk(struct o9fsmount *, int, char *);
long	o9fs_tread(struct o9fsmount *, struct o9fsfid *, void *, u_long, int64_t);
long	o9fs_twrite(struct o9fsmount *, struct o9fsfid *, void *, long, int64_t);
void	o9fs_fidclunk(struct o9fsmount *, struct o9fsfid *);
struct	o9fsfid *o9fs_fclone(struct o9fsmount *, struct o9fsfid *);
struct	o9fsfid *o9fs_funique(struct o9fsmount *, struct o9fsfid *);

/* o9fs_conv* */
u_char	*pstring(u_char *, char *);
u_char	*pqid(u_char *, struct o9fsqid *);
u_int	stringsz(char *);
u_int	o9fs_sizeS2M(struct o9fsfcall *);
uint	o9fs_convS2M(struct o9fsfcall *, u_char *, u_int);

u_char	*gstring(u_char *, u_char *, char **);
u_char	*gqid(u_char *, u_char *, struct o9fsqid *);
u_int	o9fs_convM2S(u_char *, u_int, struct o9fsfcall *);

int		o9fs_statcheck(u_char *, u_int);
u_int	o9fs_convM2D(u_char *, u_int, struct o9fsstat *, char *);


/* o9fs_rpc.c */
int		o9fs_rpc(struct o9fsmount *, struct o9fsfcall *, struct o9fsfcall *);	

/* o9fs_debug.c */
int		o9fs_debugqid(char *, int, struct o9fsqid *qid);
int		o9fs_debugperm(char *, int, int perm);
int		o9fs_debugdata(char *, int, uint8_t *, int);
int		o9fs_debugstat(char *, int, struct o9fsstat *stat);
void	o9fs_debugfcall(struct o9fsfcall *);

extern int (**o9fs_vnodeop_p)(void *);

