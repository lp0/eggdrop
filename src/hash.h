/* default bindings to load into the Tcl hash-tables */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#ifndef _H_HASH
#define _H_HASH

/*************************************************************************/
/*  it's a waste of time to edit this file now -- you can do a binding   */
/*  to get the same effect:                                              */
/*   bind dcc p status *dcc:status  <- makes '.status' only require 'p'  */
/*  that's easier to do and doesn't require a recompile.                 */
/*************************************************************************/

#define END_FIELD   { NULL, '*', NULL }

#ifndef NO_IRC
/* stupid function prototypes */
int msg_die(), msg_hello(), msg_help(), msg_ident(), msg_info(), msg_invite(),
  msg_jump(), msg_memory(), msg_op(), msg_pass(), msg_rehash(), msg_reset(),
  msg_status(), msg_who(), msg_whois(), msg_email(), msg_notes(), msg_go();

/* MSG COMMANDS */
/* function call should be:
   int msg_cmd("handle","nick","user@host","params");
   function is responsible for any logging
   (return 1 if successful, 0 if not) */
cmd_t C_msg[]={
  { "die", 'n', msg_die },
  { "email", '-', msg_email },
  { "go", '-', msg_go },
  { "hello", '-', msg_hello },
  { "help", '-', msg_help },
  { "ident", '-', msg_ident },
  { "info", '-', msg_info },
  { "invite", '-', msg_invite },
  { "jump", 'm', msg_jump },
  { "memory", 'm', msg_memory },
  { "notes", '-', msg_notes },
  { "op", '-', msg_op },
  { "pass", '-', msg_pass },
  { "rehash", 'm', msg_rehash },
  { "reset", 'm', msg_reset },
  { "status", 'M', msg_status },
  { "who", '-', msg_who },
  { "whois", '-', msg_whois },
  END_FIELD
};
#endif  /* NO_IRC */

/* function prototypes, take two */
int cmd_pls_ban(), cmd_pls_bot(), cmd_chat(), cmd_pls_host(), cmd_pls_ignore(),
  cmd_pls_user(), cmd_mns_ban(), cmd_mns_host(), cmd_botinfo(), cmd_chinfo(),
  cmd_mns_ignore(), cmd_mns_user(), cmd_addlog(), cmd_away(), cmd_bans(),
  cmd_boot(), cmd_bots(), cmd_chaddr(), cmd_chpass(), cmd_comment(),
  cmd_console(), cmd_dccstat(), cmd_debug(), cmd_die(), cmd_email(), 
  cmd_reload(), cmd_chnick(), cmd_help(), cmd_ignores(), cmd_info(),
  cmd_link(), cmd_match(), cmd_me(), cmd_motd(), cmd_newpass(), cmd_note(),
  cmd_rehash(), cmd_relay(), cmd_reset(), cmd_resetbans(), cmd_save(),
  cmd_set(), cmd_status(), cmd_trace(), cmd_unlink(), cmd_who(),
  cmd_whois(), cmd_whom(), cmd_chemail(), cmd_echo(), cmd_bottree(),
  cmd_notes(), cmd_banner(), cmd_nick(), cmd_chattr(), cmd_stick(),
  cmd_assoc(), cmd_tcl(), cmd_binds(), cmd_flush(), cmd_unstick(),
  cmd_strip(), cmd_pls_chrec(), cmd_mns_chrec(), cmd_fries(), cmd_beer(), 
  cmd_restart(), cmd_fixcodes(), cmd_page(),
#ifdef ENABLE_SIMUL
  cmd_simul(), 
#endif
#ifndef NO_FILE_SYSTEM
  cmd_files(), cmd_filestats(),
#endif
#ifndef NO_IRC
  cmd_say(), cmd_act(), cmd_servers(), cmd_msg(), cmd_channel(), cmd_topic(),
  cmd_op(), cmd_deop(), cmd_dump(), cmd_kick(), cmd_kickban(), cmd_jump(),
  cmd_invite(), cmd_adduser(), cmd_deluser(), cmd_chanadd(), cmd_chandel(),
  cmd_chaninfo(), cmd_chanset(), cmd_chansave(), cmd_chanload(),
#endif
  cmd_su();

/* DCC CHAT COMMANDS */
/* function call should be:
   int cmd_whatever(idx,"parameters");
   as with msg commands, function is responsible for any logging
*/
cmd_t C_dcc[]={
  { "+ban", 'O', cmd_pls_ban },
  { "+bot", 'B', cmd_pls_bot },
  { "+bothost", 'B', cmd_pls_host },
  { "+chrec", 'm', cmd_pls_chrec },
  { "+host", 'M', cmd_pls_host },
  { "+ignore", 'm', cmd_pls_ignore },
  { "+user", 'm', cmd_pls_user },
  { "-ban", 'O', cmd_mns_ban },
  { "-bot", 'B', cmd_mns_user },
  { "-bothost", 'B', cmd_mns_host },
  { "-chrec", 'm', cmd_mns_chrec },
  { "-host", 'M', cmd_mns_host },
  { "-ignore", 'm', cmd_mns_ignore },
  { "-user", 'm', cmd_mns_user },
#ifndef NO_IRC
  { "act", 'O', cmd_act },
#endif
  { "addlog", 'o', cmd_addlog },
#ifndef NO_IRC
  { "adduser", 'M', cmd_adduser },
  { "deluser", 'M', cmd_deluser },
#endif
  { "assoc", 'B', cmd_assoc },
  { "away", '-', cmd_away },
  { "banner", 'm', cmd_banner },
  { "bans", 'O', cmd_bans },
  { "beer", '-', cmd_beer },
  { "binds", 'm', cmd_binds },
  { "boot", 'B', cmd_boot },
  { "botinfo", '-', cmd_botinfo },
  { "bots", '-', cmd_bots },
  { "bottree", '-', cmd_bottree },
  { "chaddr", 'B', cmd_chaddr },
#ifndef NO_IRC
  { "chanadd", 'n', cmd_chanadd },
  { "chandel", 'n', cmd_chandel },
  { "chaninfo", 'M', cmd_chaninfo },
  { "chanload", 'N', cmd_chanload },
  { "channel", 'O', cmd_channel },
  { "chanset", 'N', cmd_chanset },
  { "chansave", 'N', cmd_chansave },
#endif
  { "chat", '-', cmd_chat },
  { "chattr", 'M', cmd_chattr },
  { "chbotattr", 'B', cmd_chattr },
  { "chemail", 'm', cmd_chemail },
  { "chinfo", 'm', cmd_chinfo },
  { "chnick", 'B', cmd_chnick },
  { "chpass", 'B', cmd_chpass },
  { "comment", 'm', cmd_comment },
  { "console", 'O', cmd_console },
  { "dccstat", 'B', cmd_dccstat },
  { "debug", 'm', cmd_debug },
#ifndef NO_IRC
  { "deop", 'O', cmd_deop },
#endif
  { "die", 'n', cmd_die },
#ifndef NO_IRC
  { "dump", 'm', cmd_dump },
#endif
  { "echo", '-', cmd_echo },
  { "email", '-', cmd_email },
#ifndef NO_FILE_SYSTEM
  { "files", '-', cmd_files },
  { "filestats", 'o', cmd_filestats },
#endif
  { "fixcodes", '-', cmd_fixcodes },
  { "flush", 'm', cmd_flush },
  { "fries", '-', cmd_fries },
  { "help", '-', cmd_help },
  { "ignores", 'm', cmd_ignores },
  { "info", '-', cmd_info },
#ifndef NO_IRC
  { "invite", 'O', cmd_invite },
  { "jump", 'm', cmd_jump },
  { "kick", 'O', cmd_kick },
  { "kickban", 'O', cmd_kickban },
#endif
  { "link", 'B', cmd_link },
  { "match", 'o', cmd_match },
  { "me", '-', cmd_me },
  { "motd", '-', cmd_motd },
#ifndef NO_IRC
  { "msg", 'o', cmd_msg },
#endif
  { "newpass", '-', cmd_newpass },
  { "nick", '-', cmd_nick },
  { "note", '-', cmd_note },
  { "notes", '-', cmd_notes },
#ifndef NO_IRC
  { "op", 'O', cmd_op },
#endif
  { "page", '-', cmd_page },
  { "quit", '-', CMD_LEAVE },
  { "rehash", 'm', cmd_rehash },
  { "relay", 'B', cmd_relay },
  { "reload", 'M', cmd_reload },
  { "reset", 'M', cmd_reset },
  { "resetbans", 'O', cmd_resetbans },
  { "restart", 'm', cmd_restart },
  { "save", 'M', cmd_save },
#ifndef NO_IRC
  { "say", 'O', cmd_say },
  { "servers", 'o', cmd_servers },
#endif
#ifdef ENABLE_TCL
  { "set", 'n', cmd_set },
#endif
#ifdef ENABLE_SIMUL
  { "simul", 'n', cmd_simul },
#endif
  { "status", 'M', cmd_status },
  { "stick", 'O', cmd_stick },
  { "strip", '-', cmd_strip },
  { "su", '-', cmd_su },
#ifdef ENABLE_TCL
  { "tcl", 'n', cmd_tcl },
#endif
#ifndef NO_IRC
  { "topic", 'O', cmd_topic },
#endif
  { "trace", '-', cmd_trace },
  { "unlink", 'B', cmd_unlink },
  { "unstick", 'O', cmd_unstick },
  { "who", '-', cmd_who },
  { "whois", 'O', cmd_whois },
  { "whom", '-', cmd_whom },
  END_FIELD
};


#ifndef NO_FILE_SYSTEM
/* bleh. */
int cmd_chdir(), cmd_desc(), cmd_get(), cmd_file_help(), cmd_hide(),
  cmd_unhide(), cmd_ls(), cmd_mv(), cmd_pwd(), cmd_rm(), cmd_cp(),
  cmd_mkdir(), cmd_rmdir(), cmd_pending(), cmd_cancel(), cmd_ln(),
  cmd_share(), cmd_unshare(), cmd_lsa(), cmd_stats();

/* FILE AREA COMMANDS */
/* function call should be:
   int cmd_whatever(idx,"parameters");
   as with msg commands, function is responsible for any logging
*/
cmd_t C_file[]={
  { "cancel", '-', cmd_cancel },
  { "cd", '-', cmd_chdir },
  { "chdir", '-', cmd_chdir },
  { "cp", 'j', cmd_cp },
  { "desc", '-', cmd_desc },
  { "get", '-', cmd_get },
  { "help", '-', cmd_file_help },
  { "hide", 'j', cmd_hide },
  { "ln", '-', cmd_ln },
  { "ls", '-', cmd_ls },
  { "lsa", '-', cmd_lsa },
  { "mkdir", 'j', cmd_mkdir },
  { "mv", 'j', cmd_mv },
  { "note", '-', cmd_note },
  { "pending", '-', cmd_pending },
  { "pwd", '-', cmd_pwd },
  { "quit", '-', CMD_LEAVE },
  { "rm", 'j', cmd_rm },
  { "rmdir", 'j', cmd_rmdir },
  { "share", 'j', cmd_share },
  { "stats", '-', cmd_stats },
  { "unhide", 'j', cmd_unhide },
  { "unshare", 'j', cmd_unshare },
  END_FIELD
};
#endif


/* this sucks!! */
int bot_pls_attr(), bot_pls_ban(), bot_killuser(), bot_link(),
  bot_pls_host(), bot_pls_ignore(), bot_mns_attr(), bot_mns_ban(),
  bot_mns_ignore(), bot_senduf(), bot_handshake(), bot_linked(),
  bot_bye(), bot_chat(), bot_chcomment(), bot_chemail(), bot_zapfbroad(),
  bot_chdccdir(), bot_chinfo(), bot_chhand(), bot_chpass(), bot_trying(),
  bot_newuser(), bot_ping(), bot_pong(), bot_end_trying(), bot_zapf(),
  bot_priv(), bot_reject(), bot_resync(), bot_resync_no(), bot_resyncq(),
  bot_trace(), bot_traced(), bot_ufno(), bot_ufobsolete(), bot_unlink(),
  bot_unlinked(), bot_userfileq(), bot_version(), bot_whoq(),
  bot_whom(), bot_who(), bot_nlinked(), bot_thisbot(), bot_infoq(),
  bot_motd(), bot_chan(), bot_actchan(), bot_chattr(), bot_pls_banchan(),
  bot_clrxtra(), bot_addxtra(), bot_assoc(), bot_mns_banchan(),
  bot_filereq(), bot_filereject(), bot_filesend(), bot_mns_host(),
  bot_ufsend(), bot_error(), bot_ufyes3(), bot_join(), bot_part(),
  bot_away(), bot_unaway(), bot_idle(), bot_pls_bothost(), bot_chaddr(),
  bot_chchinfo(), bot_pls_upload(), bot_pls_dnload();
  
/* BOT COMMANDS */
/* function call should be:
   int bot_whatever(idx,"parameters");
*/
cmd_t C_bot[]={
  { "*trying", 0, bot_end_trying },
  { "+ban", 0, bot_pls_ban },
  { "+banchan", 0, bot_pls_banchan },
  { "+bothost", 0, bot_pls_bothost },
  { "+dnload", 0, bot_pls_dnload },
  { "+host", 0, bot_pls_host },
  { "+ignore", 0, bot_pls_ignore },
  { "+upload", 0, bot_pls_upload },
  { "-ban", 0, bot_mns_ban },
  { "-banchan", 0, bot_mns_banchan },
  { "-host", 0, bot_mns_host },
  { "-ignore", 0, bot_mns_ignore },
  { "actchan", 0, bot_actchan },
  { "addxtra", 0, bot_addxtra },
  { "assoc", 0, bot_assoc },
  { "away", 0, bot_away },
  { "bye", 0, bot_bye },
  { "chaddr", 0, bot_chaddr },
  { "chan", 0, bot_chan },
  { "chat", 0, bot_chat },
  { "chchinfo", 0, bot_chchinfo },
  { "chcomment", 0, bot_chcomment },
  { "chdccdir", 0, bot_chdccdir },
  { "chemail", 0, bot_chemail },
  { "chattr", 0, bot_chattr },
  { "chhand", 0, bot_chhand },
  { "chinfo", 0, bot_chinfo },
  { "chpass", 0, bot_chpass },
  { "clrxtra", 0, bot_clrxtra },
  { "error", 0, bot_error },
  { "filereject", 0, bot_filereject },
  { "filereq", 0, bot_filereq },
  { "filesend", 0, bot_filesend },
  { "handshake", 0, bot_handshake },
  { "idle", 0, bot_idle },
  { "info?", 0, bot_infoq },
  { "join", 0, bot_join },
  { "killuser", 0, bot_killuser },
  { "link", 0, bot_link },
  { "linked", 0, bot_linked },
  { "motd", 0, bot_motd },
  { "newuser", 0, bot_newuser },
  { "nlinked", 0, bot_nlinked },
  { "part", 0, bot_part },
  { "ping", 0, bot_ping },
  { "pong", 0, bot_pong },
  { "priv", 0, bot_priv },
  { "reject", 0, bot_reject },
  { "resync!", 0, bot_resync },
  { "resync-no", 0, bot_resync_no },
  { "resync?", 0, bot_resyncq },
  { "thisbot", 0, bot_thisbot },
  { "trace", 0, bot_trace },
  { "traced", 0, bot_traced },
  { "trying", 0, bot_trying },
  { "uf-no", 0, bot_ufno },
  { "uf-yes", 0, bot_ufobsolete },
  { "uf-yes2", 0, bot_ufobsolete },
  { "uf-yes3", 0, bot_ufyes3 },
  { "ufsend", 0, bot_ufsend },
  { "unaway", 0, bot_unaway },
  { "unlink", 0, bot_unlink },
  { "unlinked", 0, bot_unlinked },
  { "userfile?", 0, bot_userfileq },
  { "version", 0, bot_version },
  { "who", 0, bot_who },
  { "who?", 0, bot_whoq },
  { "whom", 0, bot_whom },
  { "zapf", 0, bot_zapf },
  { "zapf-broad", 0, bot_zapfbroad },
  END_FIELD
};

#endif
