int too_many_filers ();
int welcome_to_files PROTO((int));
void finish_share PROTO((int));
void add_file PROTO((char *,char *,char *));
void wipe_tmp_file PROTO((int));
void incr_file_gots PROTO((char *));
void remote_filereq PROTO((int, char*, char*));
long findempty PROTO((FILE *));
FILE *filedb_open PROTO((char *));
void filedb_close PROTO((FILE *));
void filedb_add PROTO((FILE *,char *,char *));
void filedb_ls PROTO((FILE *,int,int,char *,int));
void filedb_getowner PROTO((char *,char *,char *));
void filedb_setowner PROTO((char *,char *,char *));
void filedb_getdesc PROTO((char *,char *,char *));
void filedb_setdesc PROTO((char *,char *,char *));
int filedb_getgots PROTO((char *, char *));
void filedb_setlink PROTO((char *,char *,char *));
void filedb_getlink PROTO((char *,char *,char *));
void filedb_getfiles PROTO((Tcl_Interp *,char *));
void filedb_getdirs PROTO((Tcl_Interp *,char *));
void filedb_change PROTO((char *,char *,int));
void tell_file_stats PROTO((int, char *));
int do_dcc_send PROTO((int, char *,char *));
int files_get PROTO((int, char *,char *));
void files_setpwd PROTO((int, char *));
int resolve_dir PROTO((char *,char *,char *,int));

#ifdef MAKING_MODS
Function * transfer_funcs;
#define copy_to_tmp (*(int*)(transfer_funcs[TRANSFER_COPYTOTMP]))
#define raw_dcc_send(a,b,c,d) (transfer_funcs[TRANSFER_RAW_DCC])(a,b,c,d)
#define wipe_tmp_filename(a,b) (transfer_funcs[TRANSFER_WIPETMPFILE])(a,b)
#define at_limit(a) (transfer_funcs[TRANSFER_ATLIMIT])(a)
#define queue_file(a,b,c,d) (transfer_funcs[TRANSFER_QUEUEFILE])(a,b,c,d)
#define show_queued_files(a) (transfer_funcs[TRANSFER_SHOWQUEUED])(a)
#define fileq_cancel(a,b) (transfer_funcs[TRANSFER_FILEQCANCEL])(a,b)
#else
int raw_dcc_send PROTO((char *,char *,char *,char *));
void wipe_tmp_filename PROTO((char *,int));
int at_limit PROTO((char *));
void queue_file PROTO((char *,char *,char *,char *));
void show_queued_files PROTO((int));
void fileq_cancel PROTO((int,char *));
#endif
