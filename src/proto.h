/*
   prototypes!  for every function used outside its own module
   (i guess i'm not very modular, cos there are a LOT of these.)
   
   with full protoyping, some have been moved to other .h files
   because they use structures in those (saves including those
   .h files EVERY time) - Beldin
*/
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#ifndef _H_PROTO
#define _H_PROTO

#include "../lush.h"

#ifdef STDC_HEADERS
#define PROTO(x) x
#define PROTO1(a,b) (a b)
#define PROTO2(a1,b1,a2,b2) (a1 b1, a2 b2) 
#define PROTO3(a1,b1,a2,b2,a3,b3) (a1 b1, a2 b2, a3 b3)
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) \
              (a1 b1, a2 b2, a3 b3, a4 b4)
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5)
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5, a6 b6)
#else
#define PROTO(x) ()
#define PROTO1(a,b) (b) a b;
#define PROTO2(a1,b1,a2,b2) (b1, b2) a1 b1; a2 b2; 
#define PROTO3(a1,b1,a2,b2,a3,b3) (b1, b2, b3) a1 b1; a2 b2; a3 b3;
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) (b1, b2, b3, b4) \
              a1 b1; a2 b2; a3 b3; a4 b4;
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (b1, b2, b3, b4, b5) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5;
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (b1, b2, b3, b4, b5, b6) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5; a6 b6;
#endif

#ifdef HAVE_DPRINTF 
#define dprintf dprintf_eggdrop
#endif

struct chanset_t; /* keeps the compiler warnings down :) */
struct userrec;

#ifndef MAKING_ASSOC
#ifndef MODULES
/* assoc.c */
int expmem_assoc();
char *get_assoc_name PROTO((int));
int get_assoc PROTO((char *));
#else
char *(*get_assoc_name) PROTO((int));
int (*get_assoc) PROTO((char *));
void dump_assoc PROTO((int));
void kill_assoc PROTO((int));
void add_assoc PROTO((char *, int));
#endif
#endif

/* blowfish.c */
void init_blowfish();
int expmem_blowfish();
void debug_blowfish PROTO((int)); 
#if defined(MODULES) && !defined(MAKING_MODS)
void (*encrypt_pass) PROTO((char *, char *));
#endif
#ifndef MODULES
void encrypt_pass PROTO((char *, char *));
char *encrypt_string PROTO((char *, char *));
char *decrypt_string PROTO((char *, char *));
#endif
/* botcmd.c */

/* botnet.c */
void answer_local_whom PROTO((int, int));
void init_bots();
int expmem_botnet();
char *lastbot PROTO((char *));
int nextbot PROTO((char *));
int in_chain PROTO((char *));
int get_tands();
char *get_tandbot PROTO((int));
void reject_bot PROTO((char *));
void tell_bots PROTO((int));
void tell_bottree PROTO((int));
int botlink PROTO((char *, int, char *));
int botunlink PROTO((int, char *, char *));
void pre_relay PROTO((int, char *));
void failed_pre_relay PROTO((int));
void tandem_relay PROTO((int, char *));
void failed_link PROTO((int));
void cont_link PROTO((int));
void failed_tandem_relay PROTO((int));
void cont_tandem_relay PROTO((int));
void dump_links PROTO((int));
void addbot PROTO((char *, char *, char *));
void rembot PROTO((char *, char *));
void unvia PROTO((int, char *));
void cancel_user_xfer PROTO((int));
void check_botnet_pings();
void drop_alt_bots();
int partysock PROTO((char *, char *));
void addparty PROTO((char *, char *, int, char, int, char *));
void remparty PROTO((char *, int));
void partystat PROTO((char *, int, int, int));
void partyidle PROTO((char *, char *));
void partysetidle PROTO((char *, int, int));
void partyaway PROTO((char *, int, char *));
void zapfbot PROTO((int));

/* chan.c */
void log_chans();
void tell_chan_info PROTO((char *));
char *getchanmode PROTO((struct chanset_t *));
void tell_verbose_chan_info PROTO((int, char *));
void tell_verbose_status PROTO((int, int));
char * quickban PROTO((struct chanset_t *, char *));
int kill_chanban PROTO((char *, int, int, int));
int kill_chanban_name PROTO((char *, int, char *));
void tell_chanbans PROTO((struct chanset_t *,int,int, char *));
int add_chan_user PROTO((char *, int, char *));
int del_chan_user PROTO((char *, int));
int add_bot_hostmask PROTO((int, char *));
void gotwall PROTO((char *, char *));
void gotjoin PROTO((char *, char *));
void gotpart PROTO((char *, char *));
void got251 PROTO((char *, char *));
void got315 PROTO((char *, char *));
void got324 PROTO((char *, char *));
void got331 PROTO((char *, char *));
void got332 PROTO((char *, char *));
void got351 PROTO((char *, char *));
void got352 PROTO((char *, char *));
void got367 PROTO((char *, char *));
void got368 PROTO((char *, char *));
void got405 PROTO((char *, char *));
void got442 PROTO((char *, char *));
void got471 PROTO((char *, char *));
void got473 PROTO((char *, char *));
void got474 PROTO((char *, char *));
void got475 PROTO((char *, char *));
void gotquit PROTO((char *, char *));
void gotnick PROTO((char *, char *));
void gotkick PROTO((char *, char *));
void gotinvite PROTO((char *, char *));
void gottopic PROTO((char *, char *));
void show_all_info PROTO((char *, char *));
void user_kickban PROTO((int, char *));
void user_kick PROTO((int, char *));
void give_op PROTO((char *, struct chanset_t * chan,int));
void give_deop PROTO((char *, struct chanset_t * chan,int));
void check_for_split();

/* chanprog.c */
int expmem_chanprog();
char *masktype PROTO((int));
char *maskname PROTO((int));
void clearq();
void take_revenge PROTO((struct chanset_t *, char *, char *));
void wipe_serverlist();
void next_server PROTO((int *, char *, int *, char *));
void add_server PROTO((char *));
void tell_servers PROTO((int));
void tell_settings PROTO((int));
int logmodes PROTO((char *));
void reaffirm_owners();
void rehash();
void reload();
void chanprog();
void get_first_server();
void got001 PROTO((char *, char *));
void check_timers();
void check_utimers();
void check_expired_chanbans();
void rmspace PROTO((char *));
void check_timers();

/* chanset.c */
int any_ops PROTO((struct chanset_t *));
void init_channel PROTO((struct chanset_t *));
int expmem_chan();
void clear_channel PROTO((struct chanset_t *,int));
void clear_channels();
void set_key PROTO((struct chanset_t *,char *));
void recheck_ops PROTO((char *, char *));
void recheck_channel PROTO((struct chanset_t *));
void recheck_channels();
void newly_chanop PROTO((struct chanset_t *));
int defined_channel PROTO((char *));
int active_channel PROTO((char *));
int me_op PROTO((struct chanset_t *));
int member_op PROTO((char *, char *));
int member_voice PROTO((char *, char *));
int ischanmember PROTO((char *, char *));
int is_split PROTO((char *, char *));
int channel_hidden PROTO((struct chanset_t *));
int channel_optopic PROTO((struct chanset_t *));
void clear_chanlist();
void set_chanlist PROTO((char * host, struct userrec *));
void kill_bogus_bans PROTO((struct chanset_t *));
void update_idle PROTO((char *, char *));
void getchanhost PROTO((char *, char *, char *));
int hand_on_chan PROTO((struct chanset_t *, char *));
void newban PROTO((struct chanset_t *, char *, char *));
int killban PROTO((struct chanset_t *, char *));
int isbanned PROTO((struct chanset_t *, char *));
void kick_match_since PROTO((struct chanset_t *, char *, time_t));
void kick_match_ban PROTO((struct chanset_t *,char *));
void reset_chan_info PROTO((struct chanset_t *));
int killmember PROTO((struct chanset_t *, char *));
void check_lonely_channel PROTO((struct chanset_t *));
void check_lonely_channels();
int killchanset PROTO((char *));
void addchanset PROTO((struct chanset_t *));
void getchanlist PROTO((char *, int));
int write_chanbans PROTO((FILE *));
void restore_chanban PROTO((char *, char *));
void write_channels();
void read_channels();
void resetbans PROTO((struct chanset_t *));
void check_idle_kick();
void check_expired_splits();

/* cmds.c */
int check_dcc_attrs PROTO((char *,int,int));
int check_dcc_chanattrs PROTO((char *,char *,int,int));
int sanity_check PROTO((int));
int stripmodes PROTO((char *));
char *stripmasktype PROTO((int));
char *stripmaskname PROTO((int));

/* dcc.c */
void dcc_activity PROTO((int,char *,int));
void eof_dcc PROTO((int));

/* dccutil.c */
int expmem_dccutil();
void dprintf();
void qprintf();
void strip_mirc_codes PROTO((int, char *));
void tandout();
void chatout();
void chanout();
void chanout2();
void shareout();
void tandout_but();
void chatout_but();
void chanout_but();
void chanout2_but();
void shareout_but();
void tell_who PROTO((int, int));
void remote_tell_who PROTO((int, char *,int));
void dcc_chatter PROTO((int));
void lostdcc PROTO((int));
void makepass PROTO((char *));
void tell_dcc PROTO((int));
void not_away PROTO((int));
void set_away PROTO((int, char *));
void set_files PROTO((int));
void set_fork PROTO((int));
void set_tand PROTO((int));
void set_chat PROTO((int));
void set_xfer PROTO((int));
void set_relay PROTO((int));
void set_new_relay PROTO((int));
void set_script PROTO((int));
void get_xfer_ptr PROTO((struct xfer_info **));
void get_chat_ptr PROTO((struct chat_info **));
void get_file_ptr PROTO((struct file_info **));
void check_expired_dcc();
void append_line  PROTO((int, char *));
void flush_lines  PROTO((int));
struct dcc_t * find_idx PROTO((int));
int new_dcc PROTO((int));
void del_dcc PROTO((int));

/* filedb.c */
long findempty PROTO((FILE *));
FILE *filedb_open PROTO((char *));
void filedb_close PROTO((FILE *));
void filedb_add PROTO((FILE *,char *,char *));
void filedb_ls PROTO((FILE *,int,int,char *,int));
#if defined(MODULES) && !defined(MAKING_MODS)
extern void (*remote_filereq) PROTO((int, char*, char*));
#else
void remote_filereq PROTO((int, char*, char*));
#endif
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

/* fileq.c */
int expmem_fileq();
void send_next_file PROTO((char *));
void show_queued_files PROTO((int));
void fileq_cancel PROTO((int, char *));
void queue_file PROTO((char *,char *,char *,char *));
void tcl_get_queued PROTO((Tcl_Interp *,char *));

/* files.c */
int too_many_filers();
int at_limit PROTO((char *));
int welcome_to_files PROTO((int));
void add_file PROTO((char *,char *,char *));
void incr_file_gots PROTO((char *));
int is_file PROTO((char *));
int files_get PROTO((int, char *,char *));
void files_setpwd PROTO((int, char *));
int resolve_dir PROTO((char *,char *,char *,int));

/* gotdcc.c */
void gotdcc PROTO((char *, char *, char *));
void failed_got_dcc PROTO((int));
void cont_got_dcc PROTO((int));
void do_boot PROTO((int, char *, char *));
int detect_dcc_flood PROTO((struct chat_info *,int));
void wipe_tmp_filename PROTO((char *, int));
void wipe_tmp_file PROTO((int));
#if defined(MODULES) && !defined(MAKING_MODS)
int (*raw_dcc_send) PROTO((char *,char *,char *,char *));
#else
int raw_dcc_send PROTO((char *,char *,char *,char *));
#endif
int do_dcc_send PROTO((int, char *, char *));

/* hash.c */
void gotcmd PROTO((char *,char *,char *,int));
int got_dcc_cmd PROTO((int, char *));
#ifndef MODULES
int got_files_cmd PROTO((int, char *));
#endif
void dcc_bot PROTO((int, char *));
void init_builtins();

/* main.c */
void fatal PROTO((char *, int));
void fixcolon PROTO((char *));
void parsemsg PROTO((char *,char *,char *,char *));
int detect_flood PROTO((char *,struct chanset_t *,int,int));
void strip_telnet PROTO((int,char *,int *));
void swallow_telnet_codes PROTO((char *));
int expected_memory();
void backup_userfile();

/* match.c */
int wild_match PROTO((register unsigned char *,register unsigned char *));
int wild_match_per PROTO((register unsigned char *,register unsigned char *));
int wild_match_file PROTO((register unsigned char *,register unsigned char *));

/* mem.c */
void init_mem();
void *n_malloc PROTO((int, char *,int));
void *n_realloc PROTO((void *,int,char *,int));
void n_free PROTO((void *,char *,int));
void tell_mem_status PROTO((char *));
void tell_mem_status_dcc PROTO((int));
void debug_mem_to_dcc PROTO((int));

/* misc.c */
void init_misc();
int expmem_misc();
void putlog();
void flushlogs();
void fixfrom PROTO((char *));
void maskhost PROTO((char *, char *));
char *stristr PROTO((char *, char *));
void split PROTO((char*, char *));
void splitc PROTO((char *,char *,char));
void nsplit PROTO((char *, char *));
void splitnick PROTO((char *, char *));
void stridx PROTO((char *,char *,int));
void dumplots PROTO((int,char *,char *));
void daysago PROTO((time_t,time_t,char *));
void days PROTO((time_t,time_t,char *));
void daysdur PROTO((time_t,time_t,char *));
void mprintf();
void hprintf();
void deq_msg();
void empty_msgq();
int can_resync PROTO((char *));
void q_tbuf PROTO((char *, char *));
void q_resync PROTO((char *));
void dump_resync PROTO((int, char *));
int flush_tbuf PROTO((char *));
void new_tbuf PROTO((char *));
void check_expired_tbufs();
void status_tbufs  PROTO((int));
void help_subst PROTO((char *, char*, int, int));
void show_motd PROTO((int));
void tellhelp PROTO((int, char *, int));
void showhelp PROTO((char *,char *,int));
void telltext PROTO((int, char *, int));
void showtext PROTO((char *,char *,int));
int copyfile PROTO((char *, char *));
int movefile PROTO((char *, char *));

/* mode.c */
void add_mode PROTO((struct chanset_t *,char,char,char *));
void flush_mode PROTO((struct chanset_t *,int));
void flush_modes();
void recheck_chanmode PROTO((struct chanset_t *));
void get_mode_protect PROTO((struct chanset_t *,char *));
void set_mode_protect PROTO((struct chanset_t *,char *));
void gotmode PROTO((char *, char *));
void got_op PROTO((struct chanset_t *,char *,char *,char *,int,int));
void got_deop PROTO((struct chanset_t *,char *,char *,char *,int,int));
void got_ban PROTO((struct chanset_t *,char *,char *,char *,int,int));
void got_unban PROTO((struct chanset_t *,char *,char *,char *,int,int));
void getkey PROTO((struct chanset_t *,char *,char *,char *,int));

/* msgnotice.c */
void gotmsg PROTO((char *,char *,int));
void gotnotice PROTO((char *, char*,int));
void goterror PROTO((char *, char *));

/* net.c */
void init_net();
int expmem_net();
IP my_atoul PROTO((char *));
unsigned long iptolong PROTO((IP));
IP getmyip();
void getmyhostname PROTO((char *));
void neterror PROTO((char *));
void setsock PROTO((int, int));
int getsock PROTO((int));
void killsock PROTO((int));
int answer PROTO((int,char *,unsigned long *,unsigned short *,int));
int open_listen PROTO((int *));
int open_telnet PROTO((char *, int));
int open_telnet_dcc PROTO((int,char *,char *));
int open_telnet_raw PROTO((int, char *, int));
void my_memcpy PROTO((char *,char *,int));
void tputs PROTO((int, char *,unsigned int));
void dequeue_sockets();
void tprintf();
int sockgets PROTO((char *,int *));
void tell_netdebug PROTO((int));

/* notes.c */
int num_notes PROTO((char *));
void notes_change PROTO((int,char *,char *));
void expire_notes();
int add_note PROTO((char *,char *,char *,int,int));
void notes_read PROTO((char *,char *,int,int));
void notes_del PROTO((char *,char *,int,int));

/* tcl.c */
void init_tcl();
void protect_tcl();
void unprotect_tcl();
int expmem_tcl();
void do_tcl PROTO((char *, char *));
void set_tcl_vars();
void tcl_tell_whois PROTO((int, char *));
int readtclprog PROTO((char *));
int findidx PROTO((int));

/* tclhash.c */
void init_hash();
int expmem_tclhash();
void *tclcmd_alloc PROTO((int));
#ifdef MODULES
void tclcmd_free PROTO((void *));
#endif
int check_tcl_bind PROTO((Tcl_HashTable *,char *,int,char *,int));
int get_bind_type PROTO((char *));
int cmd_bind PROTO((int,int,char *,char *));
int cmd_unbind PROTO((int,int,char *,char *));
int check_tcl_msg PROTO((char *,char *,char *,char *,char *));
int check_tcl_dcc PROTO((char *,int,char *));
#ifndef MODULES
int check_tcl_fil PROTO((char *,int,char *));
#endif
int check_tcl_pub PROTO((char *,char *,char *,char *));
void check_tcl_msgm PROTO((char *,char *,char *,char *,char *));
void check_tcl_pubm PROTO((char *,char *,char *,char *));
void check_tcl_notc PROTO((char *,char *,char *,char *));
void check_tcl_join PROTO((char *,char *,char *,char *));
void check_tcl_part PROTO((char *,char *,char *,char *));
void check_tcl_sign PROTO((char *,char *,char *,char *,char *));
void check_tcl_kick PROTO((char *,char *,char *,char *,char *,char *));
void check_tcl_topc PROTO((char *,char *,char *,char *,char *));
void check_tcl_mode PROTO((char *,char *,char *,char *,char *));
void check_tcl_nick PROTO((char *,char *,char *,char *,char *));
void check_tcl_bcst PROTO((char *,int,char *));
void check_tcl_chjn PROTO((char *,char *,int,char,int,char *));
void check_tcl_chpt PROTO((char *,char *,int));
int check_tcl_ctcp PROTO((char *,char *,char *,char *,char *,char *));
int check_tcl_ctcr PROTO((char *,char *,char *,char *,char *,char *));
#ifdef RAW_BINDS
int check_tcl_raw PROTO((char *,char *,char *));
#endif
void check_tcl_bot PROTO((char *,char *,char *));
void check_tcl_chon PROTO((char *, int));
void check_tcl_chof PROTO((char *, int));
#ifndef MODULES
void check_tcl_sent PROTO((char *,char *,char *));
void check_tcl_rcvd PROTO((char *,char *,char *));
#endif
void check_tcl_chat PROTO((char *,int,char *));
void check_tcl_link PROTO((char *, char *));
void check_tcl_disc PROTO((char *));
void check_tcl_rejn PROTO((char *,char *,char *,char *));
void check_tcl_splt PROTO((char *,char *,char *,char *));
char *check_tcl_filt PROTO((int, char *));
int check_tcl_flud PROTO((char *,char *,char *,char *,char *));
int check_tcl_note PROTO((char *,char *,char *));
void check_tcl_act PROTO((char *,int,char *));
void check_tcl_listen PROTO((char *, int));
int check_tcl_wall PROTO((char *, char *));
void tell_binds PROTO((int, char *));
int tcl_getbinds PROTO((int, char *));
int call_tcl_func PROTO((char *,int,char *));
void check_tcl_time PROTO((struct tm *));

/* userrec.c */
struct eggqueue *del_q PROTO((char *,struct eggqueue *,int *));
struct eggqueue *add_q PROTO((char *,struct eggqueue *));
void chg_q PROTO((struct eggqueue *,char *));
void flags2str PROTO((int, char *));
unsigned int str2flags PROTO((char *));
void chflags2str PROTO((int, char *));
unsigned int str2chflags PROTO((char *));
void get_handle_by_host PROTO((char *, char *));
struct userrec *adduser PROTO((struct userrec *,char *,char *,char *,int));
void addhost_by_handle PROTO((char *, char *));
void addhost_by_handle2 PROTO((struct userrec *,char *,char *));
int delhost_by_handle PROTO((char *, char *));
int ishost_for_handle PROTO((char *, char *));
int is_user PROTO((char *));
int is_user2 PROTO((struct userrec *,char *));
int count_users PROTO((struct userrec *));
int deluser PROTO((char *));
void freeuser PROTO((struct userrec *));
int change_handle PROTO((char *, char *));
void correct_handle PROTO((char *));
void clear_userlist PROTO((struct userrec *));
void get_pass_by_handle PROTO((char *,char *));
void change_pass_by_handle PROTO((char *,char *));
int pass_match_by_handle PROTO((char *,char *));
int pass_match_by_host PROTO((char *,char *));
int get_attr_host PROTO((char *));
int get_attr_handle PROTO((char *));
int get_allattr_handle PROTO((char *));
int get_chanattr_handle PROTO((char *,char *));
int get_chanattr_host PROTO((char *,char *));
void change_chanflags PROTO((struct userrec *,char *,char *,unsigned int,unsigned int));
void set_attr_handle PROTO((char *,unsigned int));
void set_chanattr_handle PROTO((char *,char *,unsigned int));
void get_handle_email PROTO((char *,char *));
void set_handle_email PROTO((struct userrec *,char *,char *));
void get_handle_info PROTO((char *,char *));
void set_handle_info PROTO((struct userrec *,char *,char *));
void get_handle_comment PROTO((char *,char *));
void set_handle_comment PROTO((struct userrec *,char *,char *));
void get_handle_dccdir PROTO((char *,char *));
void set_handle_dccdir PROTO((struct userrec *,char *,char *));
char *get_handle_xtra PROTO((char *));
void set_handle_xtra PROTO((struct userrec *,char *,char *));
void add_handle_xtra PROTO((struct userrec *,char *,char *));
void write_userfile();
int write_tmp_userfile PROTO((char *,struct userrec *));
int flags_ok PROTO((int, int));
void clear_chanrec PROTO((struct userrec *));
void add_chanrec PROTO((struct userrec *,char *,unsigned int,time_t));
void add_chanrec_by_handle PROTO((struct userrec *,char *,char *,unsigned int,time_t));
void del_chanrec PROTO((struct userrec *,char *));
void del_chanrec_by_handle PROTO((struct userrec *,char *,char *));
void set_handle_chaninfo PROTO((struct userrec *,char *,char *,char *));
void get_handle_chaninfo PROTO((char *,char *,char *));
int op_anywhere PROTO((char *));
int master_anywhere PROTO((char *));
int owner_anywhere PROTO((char *));
char geticon PROTO((int));
void set_handle_uploads PROTO((struct userrec *,char *,unsigned int,unsigned long));
void set_handle_dnloads PROTO((struct userrec *,char *,unsigned int,unsigned long));
void stats_add_upload PROTO((char *,unsigned long));
void stats_add_dnload PROTO((char *,unsigned long));
struct userrec *check_dcclist_hand PROTO((char *));
void touch_laston PROTO((struct userrec *,char *,time_t));
void touch_laston_handle PROTO((struct userrec *,char *,char *,time_t));

/* users.c */
int expmem_users();
void addban PROTO((char *,char *,char *,time_t));
void u_addban PROTO((struct userrec *,char *,char *,char *,time_t));
int delban PROTO((char *));
int u_delban PROTO((struct userrec *,char *));
void tell_bans PROTO((int,int,char *));
void addignore PROTO((char *,char *,char *,time_t));
int delignore PROTO((char *));
void tell_ignores PROTO((int, char *));
void restore_chandata();
int equals_ban PROTO((char *));
int u_equals_ban PROTO((struct userrec *,char *));
int sticky_ban PROTO((char *));
int u_sticky_ban PROTO((struct userrec *,char *));
int setsticky_ban PROTO((char *, int));
int u_setsticky_ban PROTO((struct userrec *,char *,int));
int match_ban PROTO((char *));
int u_match_ban PROTO((struct userrec *,char *));
int match_ignore PROTO((char *));
void check_expired_bans();
void check_expired_ignores();
void recheck_bans PROTO((struct chanset_t *));
void refresh_ban_kick PROTO((struct chanset_t *,char *,char *));
void autolink_cycle PROTO((char *));
void start_sending_users PROTO((int));
void finish_share PROTO((int));
void showinfo PROTO((struct chanset_t *,char *,char *));
void tell_file_stats PROTO((int, char *));
void tell_user_ident PROTO((int,char *,int));
void tell_users_match PROTO((int,char *,int,int,int,char *));
int readuserfile PROTO((char *,struct userrec **));
void update_laston PROTO((char *,char *));
void get_handle_laston PROTO((char *,char *,time_t *));
void set_handle_laston PROTO((char *,char *,time_t));
void get_handle_chanlaston PROTO((char *,char *));
int is_global_ban PROTO((char *));

#endif
