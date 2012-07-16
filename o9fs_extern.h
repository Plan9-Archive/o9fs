/* o9fs_subr.c */
void	o9fs_dump(u_char *, long);
char	*putstring(char *, char *);
char	*getstring(char *);
int		o9fs_allocvp(struct mount *, struct o9fid *, struct vnode **, u_long);
struct	o9fid *o9fs_xgetfid(struct o9fs *);
void	o9fs_xputfid(struct o9fs *, struct o9fid *);
int		o9fs_ptoumode(int);
int		o9fs_utopmode(int);
int		o9fs_uflags2omode(int);
void	*o9fsrealloc(void *, size_t, size_t);
void	_printvp(struct vnode *);
long	o9fs_mio(struct o9fs *, u_long);
uint16_t	o9fs_tag(void);

/* o9fs_9p.c */
uint32_t	o9fs_rdwr2(struct o9fs *, struct o9fid *, uint8_t, uint32_t, uint64_t);
int		o9fs_opencreate2(struct o9fs *, struct o9fid *, uint8_t, uint8_t, uint32_t, char *);
struct	o9fid *o9fs_walk(struct o9fs *, struct o9fid *, struct o9fid *, char *);
void	o9fs_clunkremove(struct o9fs *, struct o9fid *, uint8_t);
struct	o9fsstat *o9fs_stat(struct o9fs *, struct o9fid *);

/* o9fs_convM2D.c */
int		o9fs_statcheck(u_char *, u_int);
u_int	o9fs_convM2D(u_char *, u_int, struct o9fsstat *, char *);


extern struct vops o9fs_vops;
