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

struct chanset_t; /* keeps the compiler warnings down :) */
struct userrec;

/* blowfish.c */
void init_blowfish();
int expmem_blowfish();
void encrypt_pass(char *, char *);
void debug_blowfish(int); 
char *encrypt_string(char *, char *);
char *decrypt_string(char *,char *);

/* botcmd.c */

/* botnet.c */
void answer_local_whom(int,int);
void init_bots();
int expmem_botnet();
char *lastbot(char *);
int nextbot(char *);
int in_chain(char *);
int get_tands();
char *get_tandbot(int);
char *get_assoc_name(int);
int get_assoc(char *);
void dump_assoc(int);
void kill_assoc(int);
void add_assoc(char *,int);
void reject_bot(char *);
void tell_bots(int);
void tell_bottree(int);
int botlink(char *, int, char *);
int botunlink(int, char *, char *);
void pre_relay(int, char *);
void failed_pre_relay(int);
void tandem_relay(int, char *);
void failed_link(int);
void cont_link(int);
void failed_tandem_relay(int);
void cont_tandem_relay(int);
void dump_links(int);
void addbot(char *, char *, char *);
void rembot(char *, char *);
void unvia(int,char *);
void cancel_user_xfer(int);
void check_botnet_pings();
void drop_alt_bots();
int partysock(char *, char *);
void addparty(char *,char *,int,char,int,char *);
void remparty(char *,int);
void partystat(char *,int,int,int);
void partyidle(char *, char *);
void partysetidle(char *, int, int);
void partyaway(char *, int, char *);
void zapfbot(int);

/* chan.c */
void log_chans();
void tell_chan_info(char *);
char *getchanmode(struct chanset_t * chan);
void tell_verbose_chan_info(int, char *);
void tell_verbose_status(int, int);
char * quickban(struct chanset_t *, char *);
int kill_chanban(char *, int, int, int);
int kill_chanban_name(char *, int, char *);
void tell_chanbans(struct chanset_t *,int,int, char *);
int add_chan_user(char *, int, char *);
int del_chan_user(char *, int);
int add_bot_hostmask(int, char *);
void gotwall(char *, char *);
void gotjoin(char *, char *);
void gotpart(char *, char *);
void got206(char *, char *);
void got251(char *, char *);
void got315(char *, char *);
void got324(char *, char *);
void got331(char *, char *);
void got332(char *, char *);
void got351(char *, char *);
void got352(char *, char *);
void got367(char *, char *);
void got368(char *, char *);
void got405(char *, char *);
void got442(char *, char *);
void got471(char *, char *);
void got473(char *, char *);
void got474(char *, char *);
void got475(char *, char *);
void gotquit(char *, char *);
void gotnick(char *, char *);
void gotkick(char *, char *);
void gotinvite(char *, char *);
void gottopic(char *, char *);
void show_all_info(char *, char *);
void user_kickban(int, char *);
void user_kick(int, char *);
void give_op(char *, struct chanset_t * chan,int);
void give_deop(char *, struct chanset_t * chan,int);
void check_for_split();

/* chanprog.c */
int expmem_chanprog();
char *masktype(int);
char *maskname(int);
void clearq();
void take_revenge(struct chanset_t *, char *, char *);
void wipe_serverlist();
void next_server(int *, char *, int *, char *);
void add_server(char *);
void tell_servers(int);
void tell_settings(int);
int logmodes(char *);
void reaffirm_owners();
void rehash();
void reload();
void chanprog();
void get_first_server();
void got001(char *,char *);
void check_timers();
void check_utimers();
void check_expired_chanbans();
void rmspace(char *);
void check_timers();

/* chanset.c */
int any_ops(struct chanset_t *);
void init_channel(struct chanset_t *);
int expmem_chan();
void clear_channel(struct chanset_t *,int);
void clear_channels();
void set_key(struct chanset_t *,char *);
void recheck_ops(char *, char *);
void recheck_channel(struct chanset_t *);
void recheck_channels();
void newly_chanop(struct chanset_t *);
int defined_channel(char *);
int active_channel(char *);
int me_op(struct chanset_t *);
int member_op(char *, char *);
int member_voice(char *, char *);
int ischanmember(char *, char *);
int is_split(char *, char *);
int channel_hidden(struct chanset_t *);
int channel_optopic(struct chanset_t *);
void clear_chanlist();
void set_chanlist(char * host, struct userrec *);
void kill_bogus_bans(struct chanset_t *);
void update_idle(char *, char *);
void getchanhost(char *, char *, char *);
int hand_on_chan(struct chanset_t *, char *);
void newban(struct chanset_t *, char *, char *);
int killban(struct chanset_t *, char *);
int isbanned(struct chanset_t *, char *);
void kick_match_since(struct chanset_t *, char *, time_t);
void kick_match_ban(struct chanset_t *,char *);
void reset_chan_info(struct chanset_t *);
int killmember(struct chanset_t *, char *);
void check_lonely_channel(struct chanset_t *);
void check_lonely_channels();
int killchanset(char *);
void addchanset(struct chanset_t *);
void getchanlist(char *, int);
int write_chanbans(FILE *);
void restore_chanban(char *, char *);
void write_channels();
void read_channels();
void resetbans(struct chanset_t *);
void check_idle_kick();
void check_expired_splits();

/* cmds.c */
int check_dcc_attrs(char *,int,int);
int check_dcc_chanattrs(char *,char *,int,int);
int sanity_check(int);
int stripmodes(char *);
char *stripmasktype(int);
char *stripmaskname(int);

/* dcc.c */
void dcc_activity(int,char *,int);
void eof_dcc(int);

/* dccutil.c */
int expmem_dccutil();
void dprintf();
void qprintf();
void strip_mirc_codes(int,char *);
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
void tell_who(int,int);
void remote_tell_who(int,char *,int);
void dcc_chatter(int);
void lostdcc(int);
void makepass(char *);
void tell_dcc(int);
void not_away(int);
void set_away(int,char *);
void set_files(int);
void set_fork(int);
void set_tand(int);
void set_chat(int);
void set_xfer(int);
void set_relay(int);
void set_new_relay(int);
void set_script(int);
void get_xfer_ptr(struct xfer_info **);
void get_chat_ptr(struct chat_info **);
void get_file_ptr(struct file_info **);
void check_expired_dcc();
void append_line (int, char *);
void flush_lines (int);

/* filedb.c */
long findempty(FILE *);
FILE *filedb_open(char *);
void filedb_close(FILE *);
void filedb_add(FILE *,char *,char *);
void filedb_ls(FILE *,int,int,char *,int);
void remote_filereq(int, char*, char*);
void filedb_getowner(char *,char *,char *);
void filedb_setowner(char *,char *,char *);
void filedb_getdesc(char *,char *,char *);
void filedb_setdesc(char *,char *,char *);
int filedb_getgots(char *,char *);
void filedb_setlink(char *,char *,char *);
void filedb_getlink(char *,char *,char *);
void filedb_getfiles(Tcl_Interp *,char *);
void filedb_getdirs(Tcl_Interp *,char *);
void filedb_change(char *,char *,int);

/* fileq.c */
int expmem_fileq();
void send_next_file(char *);
void show_queued_files(int);
void fileq_cancel(int,char *);
void queue_file(char *,char *,char *,char *);
void tcl_get_queued(Tcl_Interp *,char *);

/* files.c */
int too_many_filers();
int at_limit(char *);
int welcome_to_files(int);
void add_file(char *,char *,char *);
void incr_file_gots(char *);
int is_file(char *);
int files_get(int, char *,char *);
void files_setpwd(int,char *);

/* gotdcc.c */
void gotdcc(char*,char *,char *);
void failed_got_dcc(int);
void cont_got_dcc(int);
void do_boot(int,char *,char *);
int detect_dcc_flood(struct chat_info *,int);
void wipe_tmp_filename(char *,int);
void wipe_tmp_file(int);
int raw_dcc_send(char *,char *,char *,char *);
int do_dcc_send(int, char *, char *);

/* hash.c */
void gotcmd(char *,char *,char *,int);
int got_dcc_cmd(int, char *);
int got_files_cmd(int, char *);
void dcc_bot(int, char *);
void init_builtins();

/* main.c */
void fatal(char *,int);
void fixcolon(char *);
void parsemsg(char *,char *,char *,char *);
int detect_flood(char *,struct chanset_t *,int,int);
void strip_telnet(int,char *,int *);
void swallow_telnet_codes(char *);
int expected_memory();
void backup_userfile();

/* match.c */
int wild_match(register unsigned char *,register unsigned char *);
int wild_match_per(register unsigned char *,register unsigned char *);
int wild_match_file(register unsigned char *,register unsigned char *);

/* mem.c */
void init_mem();
void *n_malloc(int, char *,int);
void *n_realloc(void *,int,char *,int);
void n_free(void *,char *,int);
void tell_mem_status(char *);
void tell_mem_status_dcc(int);
void debug_mem_to_dcc(int);

/* misc.c */
void init_misc();
int expmem_misc();
void putlog();
void flushlogs();
void fixfrom(char *);
void maskhost(char *,char *);
char *stristr(char *,char *);
void split(char*, char *);
void splitc(char *,char *,char);
void nsplit(char *,char *);
void splitnick(char *,char *);
void stridx(char *,char *,int);
void dumplots(int,char *,char *);
void daysago(time_t,time_t,char *);
void days(time_t,time_t,char *);
void daysdur(time_t,time_t,char *);
void mprintf();
void hprintf();
void deq_msg();
void empty_msgq();
int can_resync(char *);
void q_tbuf(char *,char *);
void q_resync(char *);
void dump_resync(int,char *);
int flush_tbuf(char *);
void new_tbuf(char *);
void check_expired_tbufs();
void status_tbufs(int);
void help_subst(char *, char*, int, int);
void show_motd(int);
void tellhelp(int, char *, int);
void showhelp(char *,char *,int);
void telltext(int, char *, int);
void showtext(char *,char *,int);
int copyfile(char *,char *);
int movefile(char *,char *);

/* mode.c */
void add_mode(struct chanset_t *,char,char,char *);
void flush_mode(struct chanset_t *,int);
void flush_modes();
void recheck_chanmode(struct chanset_t *);
void get_mode_protect(struct chanset_t *,char *);
void set_mode_protect(struct chanset_t *,char *);
void gotmode(char *,char *);
void got_op(struct chanset_t *,char *,char *,char *,int,int);
void got_deop(struct chanset_t *,char *,char *,char *,int,int);
void got_ban(struct chanset_t *,char *,char *,char *,int,int);
void got_unban(struct chanset_t *,char *,char *,char *,int,int);
void getkey(struct chanset_t *,char *,char *,char *,int);

/* msgnotice.c */
void gotmsg(char *,char *,int);
void gotnotice(char *, char*,int);
void goterror(char *,char *);

/* net.c */
void init_net();
int expmem_net();
IP my_atoul(char *);
unsigned long iptolong(IP);
IP getmyip();
void getmyhostname(char *);
void neterror(char *);
void setsock(int,int);
int getsock(int);
void killsock(int);
int answer(int,char *,unsigned long *,unsigned short *,int);
int open_listen(int *);
int open_telnet(char *,int);
int open_telnet_dcc(int,char *,char *);
int open_telnet_raw(int, char *, int);
void my_memcpy(char *,char *,int);
void tputs(int, char *,unsigned int);
void dequeue_sockets();
void tprintf();
int sockgets(char *,int *);
void tell_netdebug(int);

/* notes.c */
int num_notes(char *);
void notes_change(int,char *,char *);
void expire_notes();
int add_note(char *,char *,char *,int,int);
void notes_read(char *,char *,int,int);
void notes_del(char *,char *,int,int);

/* tcl.c */
void init_tcl();
void protect_tcl();
void unprotect_tcl();
int expmem_tcl();
void do_tcl(char *,char *);
void set_tcl_vars();
void tcl_tell_whois(int,char *);
int readtclprog(char *);
int findidx(int);

/* tclhash.c */
void init_hash();
int expmem_tclhash();
void *tclcmd_alloc(int);
int get_bind_type(char *);
int cmd_bind(int,int,char *,char *);
int cmd_unbind(int,int,char *,char *);
int check_tcl_msg(char *,char *,char *,char *,char *);
int check_tcl_dcc(char *,int,char *);
int check_tcl_fil(char *,int,char *);
int check_tcl_pub(char *,char *,char *,char *);
void check_tcl_msgm(char *,char *,char *,char *,char *);
void check_tcl_pubm(char *,char *,char *,char *);
void check_tcl_notc(char *,char *,char *,char *);
void check_tcl_join(char *,char *,char *,char *);
void check_tcl_part(char *,char *,char *,char *);
void check_tcl_sign(char *,char *,char *,char *,char *);
void check_tcl_kick(char *,char *,char *,char *,char *,char *);
void check_tcl_topc(char *,char *,char *,char *,char *);
void check_tcl_mode(char *,char *,char *,char *,char *);
void check_tcl_nick(char *,char *,char *,char *,char *);
void check_tcl_bcst(char *,int,char *);
void check_tcl_chjn(char *,char *,int,char,int,char *);
void check_tcl_chpt(char *,char *,int);
int check_tcl_ctcp(char *,char *,char *,char *,char *,char *);
int check_tcl_ctcr(char *,char *,char *,char *,char *,char *);
#ifdef RAW_BINDS
int check_tcl_raw(char *,char *,char *);
#endif
void check_tcl_bot(char *,char *,char *);
void check_tcl_chon(char *,int);
void check_tcl_chof(char *,int);
void check_tcl_sent(char *,char *,char *);
void check_tcl_rcvd(char *,char *,char *);
void check_tcl_chat(char *,int,char *);
void check_tcl_link(char *,char *);
void check_tcl_disc(char *);
void check_tcl_rejn(char *,char *,char *,char *);
void check_tcl_splt(char *,char *,char *,char *);
char *check_tcl_filt(int,char *);
int check_tcl_flud(char *,char *,char *,char *,char *);
int check_tcl_note(char *,char *,char *);
void check_tcl_act(char *,int,char *);
void check_tcl_listen(char *,int);
int check_tcl_wall(char *,char *);
void tell_binds(int, char *);
int tcl_getbinds(int, char *);
int call_tcl_func(char *,int,char *);
void check_tcl_time(struct tm *);

/* userrec.c */
struct eggqueue *del_q(char *,struct eggqueue *,int *);
struct eggqueue *add_q(char *,struct eggqueue *);
void chg_q(struct eggqueue *,char *);
void flags2str(int, char *);
unsigned int str2flags(char *);
void chflags2str(int, char *);
unsigned int str2chflags(char *);
void get_handle_by_host(char *,char *);
struct userrec *adduser(struct userrec *,char *,char *,char *,int);
void addhost_by_handle(char *,char *);
void addhost_by_handle2(struct userrec *,char *,char *);
int delhost_by_handle(char *,char *);
int ishost_for_handle(char *,char *);
int is_user(char *);
int is_user2(struct userrec *,char *);
int count_users(struct userrec *);
int deluser(char *);
void freeuser(struct userrec *);
int change_handle(char *,char *);
void correct_handle(char *);
void clear_userlist(struct userrec *);
void get_pass_by_handle(char *,char *);
void change_pass_by_handle(char *,char *);
int pass_match_by_handle(char *,char *);
int pass_match_by_host(char *,char *);
int get_attr_host(char *);
int get_attr_handle(char *);
int get_chanattr_handle(char *,char *);
int get_chanattr_host(char *,char *);
void change_chanflags(struct userrec *,char *,char *,unsigned int,unsigned int);
void set_attr_handle(char *,unsigned int);
void set_chanattr_handle(char *,char *,unsigned int);
void get_handle_email(char *,char *);
void set_handle_email(struct userrec *,char *,char *);
void get_handle_info(char *,char *);
void set_handle_info(struct userrec *,char *,char *);
void get_handle_comment(char *,char *);
void set_handle_comment(struct userrec *,char *,char *);
void get_handle_dccdir(char *,char *);
void set_handle_dccdir(struct userrec *,char *,char *);
char *get_handle_xtra(char *);
void set_handle_xtra(struct userrec *,char *,char *);
void add_handle_xtra(struct userrec *,char *,char *);
void write_userfile();
int write_tmp_userfile(char *,struct userrec *);
int flags_ok(int,int);
void clear_chanrec(struct userrec *);
void add_chanrec(struct userrec *,char *,unsigned int,time_t);
void add_chanrec_by_handle(struct userrec *,char *,char *,unsigned int,time_t);
void del_chanrec(struct userrec *,char *);
void del_chanrec_by_handle(struct userrec *,char *,char *);
void set_handle_chaninfo(struct userrec *,char *,char *,char *);
void get_handle_chaninfo(char *,char *,char *);
int op_anywhere(char *);
int master_anywhere(char *);
int owner_anywhere(char *);
char geticon(int);
void set_handle_uploads(struct userrec *,char *,unsigned int,unsigned long);
void set_handle_dnloads(struct userrec *,char *,unsigned int,unsigned long);
void stats_add_upload(char *,unsigned long);
void stats_add_dnload(char *,unsigned long);
struct userrec *check_dcclist_hand(char *);
void touch_laston(struct userrec *,char *,time_t);
void touch_laston_handle(struct userrec *,char *,char *,time_t);

/* users.c */
int expmem_users();
void addban(char *,char *,char *,time_t);
void u_addban(struct userrec *,char *,char *,char *,time_t);
int delban(char *);
int u_delban(struct userrec *,char *);
void tell_bans(int,int,char *);
void addignore(char *,char *,char *,time_t);
int delignore(char *);
void tell_ignores(int,char *);
int equals_ban(char *);
int u_equals_ban(struct userrec *,char *);
int sticky_ban(char *);
int u_sticky_ban(struct userrec *,char *);
int setsticky_ban(char *,int);
int u_setsticky_ban(struct userrec *,char *,int);
int match_ban(char *);
int u_match_ban(struct userrec *,char *);
int match_ignore(char *);
void check_expired_bans();
void check_expired_ignores();
void recheck_bans(struct chanset_t *);
void refresh_ban_kick(struct chanset_t *,char *,char *);
void autolink_cycle(char *);
void start_sending_users(int);
void finish_share(int);
void showinfo(struct chanset_t *,char *,char *);
void tell_file_stats(int,char *);
void tell_user_ident(int,char *,int);
void tell_users_match(int,char *,int,int,int,char *);
int readuserfile(char *,struct userrec **);
void update_laston(char *,char *);
void get_handle_laston(char *,char *,time_t *);
void set_handle_laston(char *,char *,time_t);
void get_handle_chanlaston(char *,char *);
int is_global_ban(char *);

#endif
