/*
 * assoc.c - the assoc module, moved here mainly from botnet.c for module
 * work
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#define MOD_FILESYS
#define MODULE_NAME "filesys"

#include "../module.h"
#include "filesys.h"
#include <sys/stat.h>
#include "../../tandem.h"
#include "../../files.h"
#include "../../users.h"
#include "../../cmdt.h"
#ifdef HAVE_NAT
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifndef NO_FILE_SYSTEM
#ifdef MODULES
extern char dccdir[];
extern char dccin[];
extern char filedb_path[];
extern int upload_to_cd;
Function *transfer_funcs = NULL;
#else
extern int copy_to_tmp;
#endif
extern int dcc_users;
#ifdef HAVE_NAT
extern char natip[];
#endif
/* maximum allowable file size for dcc send (1M) */
int dcc_maxsize = 1024;

int filesys_activity_hook PROTO((int idx, char *buf, int len));

void tell_file_stats PROTO2(int, idx, char *, hand)
{
   struct userrec *u;
   float fr = (-1.0), kr = (-1.0);
   u = get_user_by_handle(userlist, hand);
   if (u == NULL)
      return;
   modprintf(idx, "  uploads: %4u / %6luk\n", u->uploads, u->upload_k);
   modprintf(idx, "downloads: %4u / %6luk\n", u->dnloads, u->dnload_k);
   if (u->uploads)
      fr = ((float) u->dnloads / (float) u->uploads);
   if (u->upload_k)
      kr = ((float) u->dnload_k / (float) u->upload_k);
   if (fr < 0.0)
      modprintf(idx, "(infinite file leech)\n");
   else
      modprintf(idx, "leech ratio (files): %6.2f\n", fr);
   if (kr < 0.0)
      modprintf(idx, "(infinite size leech)\n");
   else
      modprintf(idx, "leech ratio (size) : %6.2f\n", kr);
}

#ifdef MODULES
static
#endif
int cmd_files PROTO2(int, idx, char *, par)
{
   int atr = get_attr_handle(dcc[idx].nick);
   modcontext;

   if (dccdir[0] == 0)
      modprintf(idx, "There is no file transfer area.\n");
   else if (too_many_filers()) {
      modcontext;
      modprintf(idx, "The maximum of %d people are in the file area right now.\n",
		dcc_users);
      modprintf(idx, "Please try again later.\n");
   } else {
      if (!(atr & (USER_MASTER | USER_XFER)))
	 modprintf(idx, "You don't have access to the file area.\n");
      else {
	 putlog(LOG_CMDS, "*", "#%s# files", dcc[idx].nick);
	 modprintf(idx, "Entering file system...\n");
	 if (dcc[idx].u.chat->channel >= 0) {
	    chanout2(dcc[idx].u.chat->channel, "%s is away: file system\n",
		     dcc[idx].nick);
	    modcontext;
	    if (dcc[idx].u.chat->channel < 100000)
	       tandout("away %s %d file system\n", botnetnick, dcc[idx].sock);
	 }
	 set_files(idx);
	 dcc[idx].type = DCC_FILES;
	 dcc[idx].u.file->chat->status |= STAT_CHAT;
	 if (!welcome_to_files(idx)) {
	    struct chat_info *ci = dcc[idx].u.file->chat;
	    modfree(dcc[idx].u.file);
	    dcc[idx].u.chat = ci;
	    dcc[idx].type = DCC_CHAT;
	    putlog(LOG_FILES, "*", "File system broken.");
	    if (dcc[idx].u.chat->channel >= 0) {
	       chanout2(dcc[idx].u.chat->channel, "%s has returned.\n",
			dcc[idx].nick);
	       modcontext;
	       if (dcc[idx].u.chat->channel < 100000)
		  tandout("unaway %s %d\n", botnetnick, dcc[idx].sock);
	    }
	 }
      }
   }
   modcontext;
   return 0;
}

#ifdef MODULES
static
#endif
int cmd_filestats PROTO2(int, idx, char *, par)
{
   char nick[512];
   modcontext;
   if (!par[0]) {
      modprintf(idx, "Usage: filestats <user>\n");
      return 0;
   }
   nsplit(nick, par);
   putlog(LOG_CMDS, "*", "#%s# filestats %s", dcc[idx].nick, par);
   if (nick[0] == 0)
      tell_file_stats(idx, dcc[idx].nick);
   else if (!is_user(nick))
      modprintf(idx, "No such user.\n");
   else if ((!strcmp(par, "clear")) &&
	    !(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
      set_handle_uploads(userlist, nick, 0, 0);
      set_handle_dnloads(userlist, nick, 0, 0);
   } else
      tell_file_stats(idx, nick);
   return 0;
}

#define DCCSEND_OK     0
#define DCCSEND_FULL   1	/* dcc table is full */
#define DCCSEND_NOSOCK 2	/* can't open a listening socket */
#define DCCSEND_BADFN  3	/* no such file */

int _dcc_send PROTO4(int, idx, char *, filename, char *, nick, char *, dir)
{
   int x;
   char *nfn;
   modcontext;
   x = raw_dcc_send(filename, nick, dcc[idx].nick, dir);
   if (x == DCCSEND_FULL) {
      modprintf(idx, "Sorry, too many DCC connections.  (try again later)\n");
      putlog(LOG_FILES, "*", "DCC connections full: GET %s [%s]", filename,
	     dcc[idx].nick);
      return 0;
   }
   if (x == DCCSEND_NOSOCK) {
      if (reserved_port) {
	 modprintf(idx, "My DCC SEND port is in use.  Try later.\n");
	 putlog(LOG_FILES, "*", "DCC port in use (can't open): GET %s [%s]",
		filename, dcc[idx].nick);
      } else {
	 modprintf(idx, "Unable to listen at a socket.\n");
	 putlog(LOG_FILES, "*", "DCC socket error: GET %s [%s]", filename,
		dcc[idx].nick);
      }
      return 0;
   }
   if (x == DCCSEND_BADFN) {
      modprintf(idx, "File not found (???)\n");
      putlog(LOG_FILES, "*", "DCC file not found: GET %s [%s]", filename,
	     dcc[idx].nick);
      return 0;
   }
   nfn = strrchr(filename, '/');
   if (nfn == NULL)
      nfn = filename;
   else
      nfn++;
   if (strcasecmp(nick, dcc[idx].nick) != 0)
      modprintf(DP_HELP, "NOTICE %s :Here is a file from %s ...\n", nick, dcc[idx].nick);
   modprintf(idx, "Type '/DCC GET %s %s' to receive.\n", botname, nfn);
   modprintf(idx, "Sending: %s to %s\n", nfn, nick);
   return 1;
}

int do_dcc_send PROTO3(int, idx, char *, dir, char *, filename)
{
   char s[161], s1[161], fn[512], nick[512];
   FILE *f;
   int x;

   modcontext;
   /* nickname? */
   strcpy(nick, filename);
   nsplit(fn, nick);
   nick[9] = 0;
   if (dccdir[0] == 0) {
      modprintf(idx, "DCC file transfers not supported.\n");
      putlog(LOG_FILES, "*", "Refused dcc get %s from [%s]", fn, dcc[idx].nick);
      return 0;
   }
   if (strchr(fn, '/') != NULL) {
      modprintf(idx, "Filename cannot have '/' in it...\n");
      putlog(LOG_FILES, "*", "Refused dcc get %s from [%s]", fn, dcc[idx].nick);
      return 0;
   }
   if (dir[0])
      sprintf(s, "%s%s/%s", dccdir, dir, fn);
   else
      sprintf(s, "%s%s", dccdir, fn);
   f = fopen(s, "r");
   if (f == NULL) {
      modprintf(idx, "No such file.\n");
      putlog(LOG_FILES, "*", "Refused dcc get %s from [%s]", fn, dcc[idx].nick);
      return 0;
   }
   fclose(f);
   if (!nick[0])
      strcpy(nick, dcc[idx].nick);
   /* already have too many transfers active for this user?  queue it */
   modcontext;
   if (at_limit(nick)) {
      char xxx[1024];
      sprintf(xxx, "%d*%s%s", strlen(dccdir), dccdir, dir);
      queue_file(xxx, fn, dcc[idx].nick, nick);
      modprintf(idx, "Queued: %s to %s\n", fn, nick);
      return 1;
   }
   modcontext;
   if (copy_to_tmp) {
      /* copy this file to /tmp */
      sprintf(s, "%s%s%s%s", dccdir, dir, dir[0] ? "/" : "", fn);
      sprintf(s1, "%s%s", tempdir, fn);
      if (copyfile(s, s1) != 0) {
	 modprintf(idx, "Can't make temporary copy of file!\n");
	 putlog(LOG_FILES | LOG_MISC, "*", "Refused dcc get %s: copy to %s FAILED!",
		fn, tempdir);
	 return 0;
      }
   } else
      sprintf(s1, "%s%s%s%s", dccdir, dir, dir[0] ? "/" : "", fn);
   modcontext;
   sprintf(s, "%s%s%s", dir, dir[0] ? "/" : "", fn);
   modcontext;
   x = _dcc_send(idx, s1, nick, s);
   modcontext;
   if (x != DCCSEND_OK)
      wipe_tmp_filename(s1, -1);
   modcontext;
   return x;
}
#endif

#ifdef MODULES
static int filesys_timeout PROTO1(int, i)
{
   time_t now = time(NULL);

   modcontext;
   switch (dcc[i].type) {
   case DCC_FILES_PASS:
      if (now - dcc[i].u.file->chat->timer > 180) {
	 modprintf(i, "Timeout.\n");
	 putlog(LOG_MISC, "*", "Password timeout on dcc chat: [%s]%s", dcc[i].nick,
		dcc[i].host);
	 killsock(dcc[i].sock);
	 lostdcc(i);
	 i--;
      }
      return 1;
   }
   return 0;
}

#ifndef NO_IRC
/* received a ctcp-dcc */
static int filesys_gotdcc PROTO4(char *, nick, char *, from, char *, code, char *, msg)
{
   char param[512], ip[512], s1[512], prt[81], nk[10];
   FILE *f;
   int atr, ok = 0, i, j;

   modcontext;
   if ((strcasecmp(code, "send") != 0))
      return 0;
   /* dcc chat or send! */
   nsplit(param, msg);
   nsplit(ip, msg);
   nsplit(prt, msg);
   sprintf(s1, "%s!%s", nick, from);
   atr = get_attr_host(s1);
   get_handle_by_host(nk, s1);
   if (atr & (USER_MASTER | USER_XFER))
      ok = 1;
   if (!ok) {
#ifndef QUIET_REJECTION
      modprintf(DP_HELP, "NOTICE %s :I don't accept files from strangers. :)\n",
		nick);
#endif
      putlog(LOG_FILES, "*", "Refused DCC SEND %s (no access): %s!%s", param,
	     nick, from);
      return 1;
   }
   if ((dccin[0] == 0) && (!upload_to_cd)) {
      modprintf(DP_HELP, "NOTICE %s :DCC file transfers not supported.\n", nick);
      putlog(LOG_FILES, "*", "Refused dcc send %s from %s!%s", param, nick, from);
      return 1;
   }
   if ((strchr(param, '/') != NULL)) {
      modprintf(DP_HELP, "NOTICE %s :Filename cannot have '/' in it...\n", nick);
      putlog(LOG_FILES, "*", "Refused dcc send %s from %s!%s", param, nick, from);
      return 1;
   }
   i = new_fork(DCC_SEND);
   if (i < 0) {
      modprintf(DP_HELP, "NOTICE %s :Sorry, too many DCC connections.\n", nick);
      putlog(LOG_MISC, "*", "DCC connections full: %s %s (%s!%s)", code, param,
	     nick, from);
      return 1;
   }
   dcc[i].addr = my_atoul(ip);
   dcc[i].port = atoi(prt);
   dcc[i].sock = (-1);
   strcpy(dcc[i].nick, nick);
   strcpy(dcc[i].host, from);
   if (param[0] == '.')
      param[0] = '_';
   strncpy(dcc[i].u.fork->u.xfer->filename, param, 120);
   dcc[i].u.fork->u.xfer->filename[120] = 0;
   if (upload_to_cd) {
      get_handle_dccdir(nk, s1);
      sprintf(dcc[i].u.fork->u.xfer->dir, "%s%s/", dccdir, s1);
   } else
      strcpy(dcc[i].u.fork->u.xfer->dir, dccin);
   dcc[i].u.fork->u.xfer->length = atol(msg);
   dcc[i].u.fork->u.xfer->sent = 0;
   dcc[i].u.fork->u.xfer->sofar = 0;
   if (atol(msg) == 0) {
      modprintf(DP_HELP, "NOTICE %s :Sorry, file size info must be included.\n",
		nick);
      putlog(LOG_FILES, "*", "Refused dcc send %s (%s): no file size", param,
	     nick);
      lostdcc(i);
      return 1;
   }
   if (atol(msg) > (dcc_maxsize * 1024)) {
      modprintf(DP_HELP, "NOTICE %s :Sorry, file too large.\n", nick);
      putlog(LOG_FILES, "*", "Refused dcc send %s (%s): file too large", param,
	     nick);
      lostdcc(i);
      return 1;
   }
   sprintf(s1, "%s%s", dcc[i].u.fork->u.xfer->dir, param);
   f = fopen(s1, "r");
   if (f != NULL) {
      fclose(f);
      modprintf(DP_HELP, "NOTICE %s :That file already exists.\n", nick);
      lostdcc(i);
      return 1;
   }
   /* check for dcc-sends in process with the same filename */
   for (j = 0; j < dcc_total; j++)
      if (j != i) {
	 if (dcc[j].type == DCC_SEND) {
	    if (strcmp(param, dcc[j].u.xfer->filename) == 0) {
	       modprintf(DP_HELP, "NOTICE %s :That file is already being sent.\n", nick);
	       lostdcc(i);
	       return 1;
	    }
	 } else if ((dcc[j].type == DCC_FORK) && (dcc[j].u.fork->type == DCC_SEND)) {
	    if (strcmp(param, dcc[j].u.fork->u.xfer->filename) == 0) {
	       modprintf(DP_HELP, "NOTICE %s :That file is already being sent.\n", nick);
	       lostdcc(i);
	       return 1;
	    }
	 }
      }
   /* put uploads in /tmp first */
   sprintf(s1, "%s%s", tempdir, param);
   dcc[i].u.fork->u.xfer->f = fopen(s1, "w");
   if (dcc[i].u.fork->u.xfer->f == NULL) {
      modprintf(DP_HELP, "NOTICE %s :Can't create that file (temp dir error)\n",
		nick);
      lostdcc(i);
   } else {
      dcc[i].u.fork->start = time(NULL);
      dcc[i].sock = getsock(SOCK_BINARY);	/* doh. */
      if (open_telnet_dcc(dcc[i].sock, ip, prt) < 0) {
	 /* can't connect (?) */
	 call_hook_i(HOOK_EOF, i);
      }
   }
   return 1;
}
#endif

static cmd_t mydcc[] =
{
   {"files", '-', cmd_files},
   {"filestats", 'o', cmd_filestats},
   {0, 0, 0}
};

static tcl_strings mystrings[] =
{
   {"files-path", dccdir, 120, STR_DIR | STR_PROTECT},
   {"incoming-path", dccin, 120, STR_DIR | STR_PROTECT},
   {"filedb-path", filedb_path, 120, STR_DIR | STR_PROTECT},
   {0, 0, 0, 0}
};

static tcl_ints myints[] =
{
   {"max-filesize", &dcc_maxsize},
   {"max-file-users", &dcc_users},
   {"upload-to-pwd", &upload_to_cd},
   {0, 0}
};

extern cmd_t myfiles[];
extern tcl_cmds mytcls[];

static char *filesys_close()
{
   int i;

   modcontext;
   putlog(LOG_MISC, "*", "Unloading filesystem, killing all filesystem connections..");
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == DCC_FILES) {
	 do_boot(i, (char *) botnetnick, "file system closing down");
      }
   del_hook(HOOK_ACTIVITY, filesys_activity_hook);
   del_hook(HOOK_TIMEOUT, filesys_timeout);
   del_hook(HOOK_GOT_DCC, filesys_gotdcc);
   rem_tcl_commands(mytcls);
   rem_tcl_strings(mystrings);
   rem_tcl_ints(myints);
   rem_builtins(BUILTIN_DCC, mydcc);
   rem_builtins(BUILTIN_FILES, myfiles);
   module_undepend(MODULE_NAME);
   return NULL;
}

static int filesys_expmem()
{
   return 0;
}

static void filesys_report PROTO1(int, idx)
{
   if (dccdir[0]) {
      modprintf(idx, "   DCC file path: %s", dccdir);
      if (upload_to_cd)
	 modprintf(idx, "\n        incoming: (go to the current dir)\n");
      else if (dccin[0])
	 modprintf(idx, "\n        incoming: %s\n", dccin);
      else
	 modprintf(idx, "    (no uploads)\n");
      if (dcc_users)
	 modprintf(idx, "       max users is %d\n", dcc_users);
      if ((upload_to_cd) || (dccin[0]))
	 modprintf(idx, "   DCC max file size: %dk\n", dcc_maxsize);
   } else
      modprintf(idx, "  (Filesystem module loaded, but no active dcc path.)\n");
}

Function *global = NULL;
char *filesys_start PROTO((Function *));

static Function filesys_table[] =
{
   (Function) filesys_start,
   (Function) filesys_close,
   (Function) filesys_expmem,
   (Function) filesys_report,
   (Function) remote_filereq,
   (Function) add_file,
   (Function) incr_file_gots
};

char *filesys_start PROTO1(Function *, egg_func_table)
{
   global = egg_func_table;
   module_register(MODULE_NAME, filesys_table, 1, 1);
   if (module_depend(MODULE_NAME, "transfer", 1, 0) == 0)
      return "You need the transfer module to user the file system.";
   transfer_funcs = module_find("transfer", 1, 0)->funcs;
   add_hook(HOOK_ACTIVITY, filesys_activity_hook);
   add_hook(HOOK_TIMEOUT, filesys_timeout);
   add_hook(HOOK_GOT_DCC, filesys_gotdcc);
   add_tcl_commands(mytcls);
   add_tcl_strings(mystrings);
   add_tcl_ints(myints);
   add_builtins(BUILTIN_DCC, mydcc);
   add_builtins(BUILTIN_FILES, myfiles);
   return NULL;
}
#endif
