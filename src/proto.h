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

#ifdef HAVE_DPRINTF 
#define dprintf dprintf_eggdrop
#endif

#ifndef HAVE_BZERO
void bzero(char *, int);
#endif

struct chanset_t; /* keeps the compiler warnings down :) */
struct userrec;
struct flag_record;
#ifndef MAKING_ASSOC
extern char *(*get_assoc_name) (int);
extern int (*get_assoc) (char *);
#endif

/* blowfish.c */
void init_blowfish();
int expmem_blowfish();
void debug_blowfish (int); 
#if !defined(MAKING_MODS)
extern void (*encrypt_pass) (char *, char *);
extern char *(*encrypt_string) (char *, char *);
extern char *(*decrypt_string) (char *, char *);
#endif
/* botcmd.c */

/* botnet.c */
void answer_local_whom (int, int);
void init_bots();
int expmem_botnet();
char *lastbot (char *);
int nextbot (char *);
int in_chain (char *);
void reject_bot (char *);
void tell_bots (int);
void tell_bottree (int,int);
int botlink (char *, int, char *);
int botunlink (int, char *, char *);
void dump_links (int);
void addbot (char *, char *, char *,char *);
void updatebot (char *, char, char *);
void rembot (char *, char *);
void unvia (int, char *);
void check_botnet_pings();
int partysock (char *, char *);
void addparty (char *, char *, int, char, int, char *);
void remparty (char *, int);
void partystat (char *, int, int, int);
void partyidle (char *, char *);
void partysetidle (char *, int, int);
void partyaway (char *, int, char *);
void zapfbot (int);
void tandem_relay (int, char *,int);

/* chan.c */
void log_chans();
void tell_chan_info (char *);
char *getchanmode (struct chanset_t *);
void tell_verbose_chan_info (int, char *);
void tell_verbose_status (int, int);
int kill_chanban (char *, int, int, int);
int kill_chanban_name (char *, int, char *);
void tell_chanbans (struct chanset_t *,int,int, char *);
void user_kickban (int, char *);
void check_for_split();
void server_activity (char *);

/* chanprog.c */
int expmem_chanprog();
char *masktype (int);
char *maskname (int);
void clearq();
void take_revenge (struct chanset_t *, char *, char *);
void wipe_serverlist();
void next_server (int *, char *, int *, char *);
void add_server (char *);
void tell_servers (int);
void tell_settings (int);
int logmodes (char *);
void reaffirm_owners();
void rehash();
void reload();
void chanprog();
void get_first_server();
void check_timers();
void check_utimers();
void check_expired_chanbans();
void rmspace (char *);
void check_timers();

/* chanset.c */
int any_ops (struct chanset_t *);
int expmem_chan();
void clear_channel (struct chanset_t *,int);
void set_key (struct chanset_t *,char *);
void recheck_ops (char *, char *);
void recheck_channel (struct chanset_t *);
void recheck_channels();
void newly_chanop (struct chanset_t *);
int defined_channel (char *);
int active_channel (char *);
int me_op (struct chanset_t *);
int member_op (char *, char *);
int member_voice (char *, char *);
int ischanmember (char *, char *);
int is_split (char *, char *);
int channel_hidden (struct chanset_t *);
int channel_optopic (struct chanset_t *);
void clear_chanlist();
void set_chanlist (char * host, struct userrec *);
void kill_bogus_bans (struct chanset_t *);
void update_idle (char *, char *);
void getchanhost (char *, char *, char *);
int hand_on_chan (struct chanset_t *, char *);
void newban (struct chanset_t *, char *, char *);
int killban (struct chanset_t *, char *);
int isbanned (struct chanset_t *, char *);
void kick_match_since (struct chanset_t *, char *, time_t);
void kick_match_ban (struct chanset_t *,char *);
void reset_chan_info (struct chanset_t *);
int killmember (struct chanset_t *, char *);
void check_lonely_channel (struct chanset_t *);
void check_lonely_channels();
int killchanset (char *);
void addchanset (struct chanset_t *);
void getchanlist (char *, int);
int write_chanbans (FILE *);
void restore_chanban (char *, char *);
void write_channels();
void read_channels();
void resetbans (struct chanset_t *);
void check_idle_kick();
void check_expired_splits();

/* cmds.c */
int check_dcc_attrs (char *,int,int);
int check_dcc_chanattrs (char *,char *,int,int);
int sanity_check (int);
int stripmodes (char *);
char * stripmasktype (int);
			   
/* dcc.c */
void dcc_activity (int,char *,int);
void eof_dcc (int);
void failed_link (int);

/* dccutil.c */
int expmem_dccutil();
void dprintf();
void qprintf();
void tandout();
void chatout();
void chanout();
void chanout2();
extern void (*shareout)();
extern void (*sharein)(int, char *);
void tandout_but();
void chatout_but();
void chanout_but();
void chanout2_but();
void dcc_chatter (int);
void lostdcc (int);
void makepass (char *);
void tell_dcc (int);
void not_away (int);
void set_away (int, char *);
void * get_data_ptr (int);
void flush_lines  (int);
struct dcc_t * find_idx (int);
int new_dcc (struct dcc_table *,int);
void del_dcc (int);
char * add_cr (char *);
void init_dcc_max();

/* gotdcc.c */
void gotdcc (char *, char *, char *);
void do_boot (int, char *, char *);
int detect_dcc_flood (time_t *,struct chat_info *,int);
#if defined(MAKING_MODS)
int raw_dcc_send (char *,char *,char *,char *);
#endif
int do_dcc_send (int, char *, char *);

/* main.c */
void fatal (char *, int);
void fixcolon (char *);
int detect_flood (char *,struct chanset_t *,int,int);
int expected_memory();
void backup_userfile();

/* match.c */
int wild_match (register unsigned char *,register unsigned char *);

/* mem.c */
void init_mem();
void *n_malloc (int, char *,int);
void *n_realloc (void *,int,char *,int);
void n_free (void *,char *,int);
void tell_mem_status (char *);
void tell_mem_status_dcc (int);
void debug_mem_to_dcc (int);

/* misc.c */
void init_misc();
int expmem_misc();
void putlog();
void flushlogs();
void fixfrom (char *);
void maskhost (char *, char *);
char *stristr (char *, char *);
#define split(a,b) splitc(a,b,' ');
#define splitnick(a,b) splitc(a,b,'!');
void splitc (char *,char *,char);
void nsplit (char *, char *);
void stridx (char *,char *,int);
void dumplots (int,char *,char *);
void daysago (time_t,time_t,char *);
void days (time_t,time_t,char *);
void daysdur (time_t,time_t,char *);
void mprintf();
void hprintf();
void deq_msg();
void empty_msgq();
void help_subst (char *, char*, struct flag_record *, int);
void show_motd (int);
void tellhelp (int, char *, struct flag_record *);
void showhelp (char *,char *, struct flag_record *);
void telltext (int, char *, struct flag_record *);
void showtext (char *,char *,struct flag_record *);
int copyfile (char *, char *);
int movefile (char *, char *);

/* mode.c */
void add_mode (struct chanset_t *,char,char,char *);
void flush_mode (struct chanset_t *,int);
void flush_modes();
void recheck_chanmode (struct chanset_t *);
void get_mode_protect (struct chanset_t *,char *);
void set_mode_protect (struct chanset_t *,char *);
void gotmode (char *, char *);
void got_op (struct chanset_t *,char *,char *,char *,int,int);

/* msgnotice.c */
void gotmsg (char *,char *,int);
void gotnotice (char *, char*,int);

/* net.c */
void my_memcpy (char *, char *, int);
void init_net();
int expmem_net();
IP my_atoul (char *);
unsigned long iptolong (IP);
IP getmyip();
void neterror (char *);
void setsock (int, int);
int getsock (int);
void killsock (int);
int answer (int,char *,unsigned long *,unsigned short *,int);
int open_listen (int *);
int open_telnet (char *, int);
int open_telnet_dcc (int,char *,char *);
int open_telnet_raw (int, char *, int);
void tputs (int, char *,unsigned int);
void dequeue_sockets();
void tprintf();
int sockgets (char *,int *);
void tell_netdebug (int);

/* notes.c */
int num_notes (char *);
void notes_change (int,char *,char *);
void expire_notes();
int add_note (char *,char *,char *,int,int);
void notes_read (char *,char *,int,int);
void notes_del (char *,char *,int,int);

/* tcl.c */
void init_tcl();
void protect_tcl();
void unprotect_tcl();
int expmem_tcl();
void do_tcl (char *, char *);
void set_tcl_vars();
int readtclprog (char *);
int findidx (int);

/* userrec.c */
struct eggqueue *del_q (char *,struct eggqueue *,int *);
struct eggqueue *add_q (char *,struct eggqueue *);
void chg_q (struct eggqueue *,char *);
void flags2str (int, char *);
unsigned int str2flags (char *);
void chflags2str (int, char *);
unsigned int str2chflags (char *);
void get_handle_by_host (char *, char *);
struct userrec *adduser (struct userrec *,char *,char *,char *,int);
void addhost_by_handle (char *, char *);
void addhost_by_handle2 (struct userrec *,char *,char *);
int delhost_by_handle (char *, char *);
int ishost_for_handle (char *, char *);
int is_user (char *);
int is_user2 (struct userrec *,char *);
int count_users (struct userrec *);
int deluser (char *);
void freeuser (struct userrec *);
int change_handle (char *, char *);
void correct_handle (char *);
void clear_userlist (struct userrec *);
void get_pass_by_handle (char *,char *);
void change_pass_by_handle (char *,char *);
int pass_match_by_handle (char *,char *);
int pass_match_by_host (char *,char *);
int get_attr_host (char *);
int get_attr_handle (char *);
void get_allattr_handle (char *,struct flag_record *);
int get_chanattr_handle (char *,char *);
int get_chanattr_host (char *,char *);
void change_chanflags (struct userrec *,char *,char *,unsigned int,unsigned int);
void set_attr_handle (char *,unsigned int);
void set_chanattr_handle (char *,char *,unsigned int);
void get_handle_email (char *,char *);
void set_handle_email (struct userrec *,char *,char *);
void get_handle_info (char *,char *);
void set_handle_info (struct userrec *,char *,char *);
void get_handle_comment (char *,char *);
void set_handle_comment (struct userrec *,char *,char *);
void get_handle_dccdir (char *,char *);
void set_handle_dccdir (struct userrec *,char *,char *);
char *get_handle_xtra (char *);
void set_handle_xtra (struct userrec *,char *,char *);
void add_handle_xtra (struct userrec *,char *,char *);
int write_user (struct userrec * u, FILE * f, int shr);
void write_userfile();
void clear_chanrec (struct userrec *);
struct chanuserrec * add_chanrec (struct userrec *,char *,unsigned int,time_t);
void add_chanrec_by_handle (struct userrec *,char *,char *,unsigned int,time_t);
void del_chanrec (struct userrec *,char *);
void del_chanrec_by_handle (struct userrec *,char *,char *);
void set_handle_chaninfo (struct userrec *,char *,char *,char *);
void get_handle_chaninfo (char *,char *,char *);
int op_anywhere (char *);
int master_anywhere (char *);
int owner_anywhere (char *);
char geticon (int);
void set_handle_uploads (struct userrec *,char *,unsigned int,unsigned long);
void set_handle_dnloads (struct userrec *,char *,unsigned int,unsigned long);
void stats_add_upload (char *,unsigned long);
void stats_add_dnload (char *,unsigned long);
struct userrec *check_dcclist_hand (char *);
void touch_laston (struct userrec *,char *,time_t);
void touch_laston_handle (struct userrec *,char *,char *,time_t);

/* users.c */
int expmem_users();
void addban (char *,char *,char *,time_t);
int u_addban (struct userrec *,char *,char *,char *,time_t);
int delban (char *);
int u_delban (struct userrec *,char *);
void tell_bans (int,int,char *);
void addignore (char *,char *,char *,time_t);
int delignore (char *);
void tell_ignores (int, char *);
int equals_ban (char *);
int u_equals_ban (struct userrec *,char *);
int sticky_ban (char *);
int u_sticky_ban (struct userrec *,char *);
int setsticky_ban (char *, int);
int u_setsticky_ban (struct userrec *,char *,int);
int match_ban (char *);
int u_match_ban (struct userrec *,char *);
int match_ignore (char *);
void check_expired_bans();
void check_expired_ignores();
void recheck_bans (struct chanset_t *);
void refresh_ban_kick (struct chanset_t *,char *,char *);
void autolink_cycle (char *);
void showinfo (struct chanset_t *,char *,char *);
void tell_file_stats (int, char *);
void tell_user_ident (int,char *,int);
void tell_users_match (int,char *,int,int,int,char *);
int readuserfile (char *,struct userrec **);
void update_laston (char *,char *);
void get_handle_laston (char *,char *,time_t *);
void set_handle_laston (char *,char *,time_t);
void get_handle_chanlaston (char *,char *);
int is_global_ban (char *);
#endif
