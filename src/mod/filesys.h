#ifdef MODULES
#define raw_dcc_send filesys_raw_dcc_send
#endif
int too_many_filers ();
int welcome_to_files PROTO((int));
void finish_share PROTO((int));
void add_file PROTO((char *,char *,char *));
int at_limit PROTO((char *));
void wipe_tmp_file PROTO((int));
void wipe_tmp_filename PROTO((char *,int));
void incr_file_gots PROTO((char *));
void send_next_file PROTO((char *));
int raw_dcc_send PROTO((char *,char *,char *,char *));
void queue_file PROTO((char *,char *,char *,char *));
void remote_filereq PROTO((int, char*, char*));
int expmem_fileq();
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
void show_queued_files PROTO((int));
void fileq_cancel PROTO((int,char *));
void tell_file_stats PROTO((int, char *));
int do_dcc_send PROTO((int, char *,char *));
void tcl_get_queued PROTO((Tcl_Interp *,char *));
int files_get PROTO((int, char *,char *));
void files_setpwd PROTO((int, char *));
int resolve_dir PROTO((char *,char *,char *,int));
