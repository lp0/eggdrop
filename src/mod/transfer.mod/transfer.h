int raw_dcc_send (char *,char *,char *,char *);
void wipe_tmp_filename (char *,int);
int at_limit (char *);
void queue_file (char *,char *,char *,char *);
void show_queued_files (int);
void fileq_cancel (int,char *);
int wild_match_file(register unsigned char * m, register unsigned char * n);
extern int copy_to_tmp;
