#ifndef _FILESYS_H_
#define _FILESYS_H_

/* TEMPORARY - will be num.h eventually */
#ifdef MAKING_MODS
#include "../../lang/english.h"
#endif

#include "../transfer.mod/transfer.h"

int too_many_filers ();
int welcome_to_files (int);
void finish_share (int);
void add_file (char *,char *,char *);
void wipe_tmp_file (int);
void incr_file_gots (char *);
void remote_filereq (int, char*, char*);
long findempty (FILE *);
FILE *filedb_open (char *);
FILE *filedb_sortopen (char *);
void filedb_close (FILE *);
void filedb_add (FILE *,char *,char *);
void filedb_ls (FILE *,int,int,char *,int);
void filedb_getowner (char *,char *,char *);
void filedb_setowner (char *,char *,char *);
void filedb_getdesc (char *,char *,char *);
void filedb_setdesc (char *,char *,char *);
int filedb_getgots (char *, char *);
void filedb_setlink (char *,char *,char *);
void filedb_getlink (char *,char *,char *);
void filedb_getfiles (Tcl_Interp *,char *);
void filedb_getdirs (Tcl_Interp *,char *);
void filedb_change (char *,char *,int);
void tell_file_stats (int, char *);
int do_dcc_send (int, char *,char *);
int files_get (int, char *,char *);
void files_setpwd (int, char *);
int resolve_dir (char *,char *,char *,int);
int flags_ok (int, int);

extern p_tcl_hash_list H_fil;
#endif
