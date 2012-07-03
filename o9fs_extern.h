/* o9fs_subr.c */
void	o9fs_dump(u_char *, long);
char	*putstring(char *, char *);
char	*getstring(char *);
int		o9fs_allocvp(struct mount *, struct o9fid *, struct vnode **, u_long);
struct	o9fsfid *o9fs_getfid(struct o9fs *);
struct	o9fid *o9fs_xgetfid(struct o9fs *);
void	o9fs_xputfid(struct o9fs *, struct o9fid *);
void	o9fs_putfid(struct o9fs *, struct o9fsfid *);
int		o9fs_ptoumode(int);
int		o9fs_utopmode(int);
int		o9fs_uflags2omode(int);
void	o9fs_freefcall(struct o9fsfcall *);
void	o9fs_freestat(struct o9fsstat *);
void	printvp(struct vnode *);

/* o9fs_9p.c */
struct	o9fsstat *o9fs_fstat(struct o9fs *, struct o9fsfid *);
long	o9fs_rdwr(struct o9fs *, int, struct o9fsfid *, void *, u_long, int64_t);
void	o9fs_fidclunk(struct o9fs *, struct o9fsfid *);
struct	o9fsfid *o9fs_twalk(struct o9fs *, struct o9fsfid *, struct o9fsfid *, char *);
int		o9fs_opencreate(int, struct o9fs *, struct o9fsfid *, int, ulong, char *);
int		o9fs_opencreate2(struct o9fs *, struct o9fid *, uint8_t, uint8_t, uint32_t, char *);
struct	o9fid *o9fs_walk(struct o9fs *, struct o9fid *, struct o9fid *, char *);
void	o9fs_clunk(struct o9fs *, struct o9fid *);
struct	o9fsstat *o9fs_stat(struct o9fs *, struct o9fid *);

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
int		o9fs_rpc(struct o9fs *, struct o9fsfcall *, struct o9fsfcall *);
long	o9fs_mio(struct o9fs *, u_long);

extern struct vops o9fs_vops;
