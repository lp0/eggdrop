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
#include "files.h"
#include "../../users.h"
#include "../../cmdt.h"
#include <netinet/in.h>
#include <arpa/inet.h>

p_tcl_hash_list H_fil;
extern char filedb_path[];
extern int upload_to_cd;
Function *transfer_funcs = NULL;
extern int dcc_users;
extern int quiet_reject;

/* root dcc directory */
char dccdir[121] = "";
/* directory to put incoming dcc's into */
char dccin[121] = "";
/* let all uploads go to the user's current directory? */
int upload_to_cd = 0;
/* maximum allowable file size for dcc send (1M) */
int dcc_maxsize = 1024;

static int is_valid();

/* check for tcl-bound file command, return 1 if found */
/* fil: proc-name <handle> <dcc-handle> <args...> */
static int check_tcl_fil (char * cmd, int idx, char * args)
{
   int x;
   char s[5];
   struct flag_record fr = {0,0,0};
   
   modcontext;
   fr.global = get_attr_handle(dcc[idx].nick);
   fr.chan = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.file->chat->con_chan);
   sprintf(s, "%ld", dcc[idx].sock);
   Tcl_SetVar(interp, "_n", dcc[idx].nick, 0);
   Tcl_SetVar(interp, "_i", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   modcontext;
   x = check_tcl_bind(H_fil, cmd, &fr, " $_n $_i $_a",
		      MATCH_PARTIAL | BIND_USE_ATTR | BIND_HAS_BUILTINS);
   modcontext;
   if (x == BIND_AMBIGUOUS) {
      modprintf(idx, "Ambigious command.\n");
      return 0;
   }
   if (x == BIND_NOMATCH) {
      modprintf(idx, "What?  You need 'help'\n");
      return 0;
   }
   if (x == BIND_EXEC_BRK)
      return 1;
   if (x == BIND_EXEC_LOG)
      putlog(LOG_FILES, "*", "#%s# files: %s %s", dcc[idx].nick, cmd, args);
   return 0;
}

static void dcc_files_pass (int idx, char * buf,int x)
{
   if (!x)
     return;
   if (pass_match_by_handle(buf, dcc[idx].nick)) {
      if (too_many_filers()) {
	 modprintf(idx, "Too many people are in the file system right now.\n");
	 modprintf(idx, "Please try again later.\n");
	 putlog(LOG_MISC, "*", "File area full: DCC chat [%s]%s", dcc[idx].nick,
		dcc[idx].host);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
	 return;
      }
      dcc[idx].type = &DCC_FILES;
      if (dcc[idx].u.file->chat->status & STAT_TELNET)
	 modprintf(idx, "\377\374\001\n");	/* turn echo back on */
      putlog(LOG_FILES, "*", "File system: [%s]%s/%d", dcc[idx].nick,
	     dcc[idx].host, dcc[idx].port);
      if (!welcome_to_files(idx)) {
	 putlog(LOG_FILES, "*", "File system broken.");
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
      } else 
	touch_laston_handle(userlist,dcc[idx].nick,"filearea",now);
      return;
   }
   modprintf(idx, "Negative on that, Houston.\n");
   putlog(LOG_MISC, "*", "Bad password: DCC chat [%s]%s", dcc[idx].nick,
	  dcc[idx].host);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

/* hash function for file area commands */
static int got_files_cmd (int idx, char * msg)
{
   char total[512], code[512];
   modcontext;
   strcpy(msg, check_tcl_filt(idx, msg));
   modcontext;
   if (!msg[0])
      return 1;
   if (msg[0] == '.')
      strcpy(msg, &msg[1]);
   strcpy(total, msg);
   rmspace(msg);
   nsplit(code, msg);
   rmspace(msg);
   return check_tcl_fil(code, idx, msg);
}

static void dcc_files (int idx, char * buf,int i)
{
   modcontext;
   if (detect_dcc_flood(&dcc[idx].timeval,dcc[idx].u.file->chat, idx))
      return;
   dcc[idx].timeval = time(NULL);
   modcontext;
   strcpy(buf, check_tcl_filt(idx, buf));
   modcontext;
   if (!buf[0])
      return;
   touch_laston_handle(userlist,dcc[idx].nick,"filearea",now);
   if (buf[0] == ',') {
      for (i = 0; i < dcc_total; i++) {
	 if ((dcc[i].type == &DCC_CHAT) &&
	     (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	     (dcc[i].u.chat->channel >= 0) &&
	     ((i != idx) || (dcc[idx].u.chat->status & STAT_ECHO)))
	    modprintf(i, "-%s- %s\n", dcc[idx].nick, &buf[1]);
	 if ((dcc[i].type == &DCC_FILES) &&
	     (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	     ((i != idx) || (dcc[idx].u.file->chat->status & STAT_ECHO)))
	    modprintf(i, "-%s- %s\n", dcc[idx].nick, &buf[1]);
      }
   } else if (got_files_cmd(idx, buf)) {
      modprintf(idx, "*** Ja mata!\n");
      putlog(LOG_FILES, "*", "DCC user [%s]%s left file system", dcc[idx].nick,
	     dcc[idx].host);
      set_handle_dccdir(userlist, dcc[idx].nick, dcc[idx].u.file->dir);
      if (dcc[idx].u.file->chat->status & STAT_CHAT) {
	 struct chat_info *ci;
	 modprintf(idx, "Returning you to command mode...\n");
	 ci = dcc[idx].u.file->chat;
	 modfree(dcc[idx].u.file);
	 dcc[idx].u.chat = ci;
	 dcc[idx].u.chat->status &= (~STAT_CHAT);
	 dcc[idx].type = &DCC_CHAT;
	 if (dcc[idx].u.chat->channel >= 0) {
	    chanout2(dcc[idx].u.chat->channel, "%s has returned.\n", dcc[idx].nick);
	    modcontext;
	    if (dcc[idx].u.chat->channel < 100000)
	       tandout("unaway %s %d\n", botnetnick, dcc[idx].sock);
	 }
      } else {
	 modprintf(idx, "Dropping connection now.\n");
	 putlog(LOG_FILES, "*", "Left files: [%s]%s/%d", dcc[idx].nick,
		dcc[idx].host, dcc[idx].port);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
      }
   }
}

void tell_file_stats (int idx, char * hand)
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

static int cmd_files (int idx, char * par)
{
   int atr = get_attr_handle(dcc[idx].nick);
   static struct chat_info * ci;
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
	 ci = dcc[idx].u.chat;
	 dcc[idx].u.file = get_data_ptr(sizeof(struct file_info));
	 dcc[idx].u.file->chat = ci;
	 dcc[idx].type = &DCC_FILES;
	 dcc[idx].u.file->chat->status |= STAT_CHAT;
	 if (!welcome_to_files(idx)) {
	    struct chat_info *ci = dcc[idx].u.file->chat;
	    modfree(dcc[idx].u.file);
	    dcc[idx].u.chat = ci;
	    dcc[idx].type = &DCC_CHAT;
	    putlog(LOG_FILES, "*", "File system broken.");
	    if (dcc[idx].u.chat->channel >= 0) {
	       chanout2(dcc[idx].u.chat->channel, "%s has returned.\n",
			dcc[idx].nick);
	       modcontext;
	       if (dcc[idx].u.chat->channel < 100000)
		  tandout("unaway %s %d\n", botnetnick, dcc[idx].sock);
	    }
	 } else 
	   touch_laston_handle(userlist,dcc[idx].nick,"filearea",now);
      }
   }
   modcontext;
   return 0;
}

#define DCCSEND_OK     0
#define DCCSEND_FULL   1	/* dcc table is full */
#define DCCSEND_NOSOCK 2	/* can't open a listening socket */
#define DCCSEND_BADFN  3	/* no such file */

int _dcc_send (int idx, char * filename, char * nick, char * dir)
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

int do_dcc_send (int idx, char * dir, char * filename)
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

static int builtin_fil STDVAR {
   int idx;
   Function F = (Function) cd;
   
   modcontext;
   BADARGS(4, 4, " hand idx param");
   idx = findidx(atoi(argv[2]));
   modcontext;
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   modcontext;
   if (F == CMD_LEAVE) {
      Tcl_AppendResult(irp, "break", NULL);
      return TCL_OK;
   }
   modcontext;
   (F) (idx, argv[3]);
   modcontext;
   Tcl_ResetResult(irp);
   modcontext;
   return TCL_OK;
}

static void tout_dcc_files_pass (int i)
{

   modcontext;
   modprintf(i, "Timeout.\n");
   putlog(LOG_MISC, "*", "Password timeout on dcc chat: [%s]%s", dcc[i].nick,
	  dcc[i].host);
   killsock(dcc[i].sock);
   lostdcc(i);
}

#ifndef NO_IRC
/* received a ctcp-dcc */
static int filesys_gotdcc (char * nick, char * from, char * code, char * msg)
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
      if (!quiet_reject)
	modprintf(DP_HELP, "NOTICE %s :I don't accept files from strangers. :)\n",
		  nick);
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
   i = new_dcc(&DCC_FORK_SEND,sizeof(struct xfer_info));
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
   strncpy(dcc[i].u.xfer->filename, param, 120);
   dcc[i].u.xfer->filename[120] = 0;
   if (upload_to_cd) {
      get_handle_dccdir(nk, s1);
      sprintf(dcc[i].u.xfer->dir, "%s%s/", dccdir, s1);
   } else
      strcpy(dcc[i].u.xfer->dir, dccin);
   dcc[i].u.xfer->length = atol(msg);
   dcc[i].u.xfer->sent = 0;
   dcc[i].u.xfer->sofar = 0;
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
   sprintf(s1, "%s%s", dcc[i].u.xfer->dir, param);
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
	 if ((dcc[j].type == &DCC_SEND) || (dcc[j].type == &DCC_FORK_SEND)) {
	    if (strcmp(param, dcc[j].u.xfer->filename) == 0) {
	       modprintf(DP_HELP, "NOTICE %s :That file is already being sent.\n", nick);
	       lostdcc(i);
	       return 1;
	    }
	 }
      }
   /* put uploads in /tmp first */
   sprintf(s1, "%s%s", tempdir, param);
   dcc[i].u.xfer->f = fopen(s1, "w");
   if (dcc[i].u.xfer->f == NULL) {
      modprintf(DP_HELP, "NOTICE %s :Can't create that file (temp dir error)\n",
		nick);
      lostdcc(i);
   } else {
      dcc[i].timeval = time(NULL);
      dcc[i].sock = getsock(SOCK_BINARY);	/* doh. */
      if (open_telnet_dcc(dcc[i].sock, ip, prt) < 0) {
	 dcc[i].type->eof(i);
      }
   }
   return 1;
}
#endif

static char *stat_str (int st)
{
   static char s[10];
   s[0] = st & STAT_CHAT ? 'C' : 'c';
   s[1] = st & STAT_PARTY ? 'P' : 'p';
   s[2] = st & STAT_TELNET ? 'T' : 't';
   s[3] = st & STAT_ECHO ? 'E' : 'e';
   s[4] = st & STAT_PAGE ? 'P' : 'p';
   s[5] = 0;
   return s;
}

static void disp_dcc_files (int idx,char * buf) {
   sprintf(buf, "file  flags: %s", stat_str(dcc[idx].u.file->chat->status));
}

static void disp_dcc_files_pass (int idx,char * buf) {
   sprintf(buf, "fpas  waited %lus", now - dcc[idx].timeval);
}

static void kill_dcc_files (int idx) {
   modfree(dcc[idx].u.file->chat);
   modfree(dcc[idx].u.file);
}

static int expmem_dcc_files (int idx) {
   int tot = sizeof(struct file_info) + sizeof(struct chat_info);
   if (dcc[idx].u.file->chat->away != NULL)
     tot += strlen(dcc[idx].u.file->chat->away) + 1;
   return tot;
}

static void out_dcc_files (int idx,char * buf) {
   if (dcc[idx].u.file->chat->status & STAT_TELNET)
     add_cr(buf);
   tputs(dcc[idx].sock,buf,strlen(buf));
}

static void dcc_fork_files (int idx,char * buf,int len)
{
   char s1[121];
   sprintf(s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
   get_handle_by_host(dcc[idx].nick, s1);
   dcc[idx].timeval = now;
   if (pass_match_by_handle("-", dcc[idx].nick)) {
      /* no password set -  if you get here, something is WEIRD */
      modprintf(idx, IRC_NOPASS2);
      putlog(LOG_FILES, "*", "File system: [%s]%s/%d", dcc[idx].nick,
	     dcc[idx].host, dcc[idx].port);
      if (!welcome_to_files(idx)) {
	 putlog(LOG_FILES, "*", DCC_FILESYSBROKEN);
	 killsock(dcc[idx].sock);
	 dcc[idx].sock = (int)dcc[idx].type;
	 dcc[idx].type = &DCC_LOST;
      } else {
	 touch_laston_handle(userlist,dcc[idx].nick,"filearea",now);
	 dcc[idx].type = &DCC_FILES;
      }
   } else {
      modprintf(idx, "%s\n", DCC_ENTERPASS);
      dcc[idx].type = &DCC_FILES_PASS;
   }
}

static cmd_t mydcc[] =
{
   {"files", "-", cmd_files, NULL},
   {0, 0, 0}
};

static tcl_strings mystrings[] =
{
   {"files-path", dccdir, 120, STR_DIR | STR_PROTECT},
   {"incoming-path", dccin, 120, STR_DIR | STR_PROTECT},
   {"filedb-path", filedb_path, 120, STR_DIR | STR_PROTECT},
   {NULL, NULL, 0, 0}
};

static tcl_ints myints[] =
{
   {"max-filesize", &dcc_maxsize},
   {"max-file-users", &dcc_users},
   {"upload-to-pwd", &upload_to_cd},
   {NULL, NULL}
};

extern cmd_t myfiles[];
extern tcl_cmds mytcls[];

static char *filesys_close()
{
   int i;
   p_tcl_hash_list H_dcc;

   modcontext;
   putlog(LOG_MISC, "*", "Unloading filesystem, killing all filesystem connections..");
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_FILES) {
	 do_boot(i, (char *) botnetnick, "file system closing down");
      } else if ((dcc[i].type == &DCC_FILES_PASS)
		|| (dcc[i].type == &DCC_FORK_FILES)) {
	 killsock(dcc[i].sock);
	 lostdcc(i);
      }
   rem_tcl_commands(mytcls);
   rem_tcl_strings(mystrings);
   rem_tcl_ints(myints);
   H_dcc = find_hash_table("dcc");
   rem_builtins(H_dcc, mydcc);
   rem_builtins(H_fil, myfiles);
   del_hash_table(H_fil);
   del_hook(HOOK_GOT_DCC,filesys_gotdcc);
   module_undepend(MODULE_NAME);
   /* the following is evil bodging that will hopefully removed once
    * eggdrop is 100% modules */
   DCC_FILES.activity = 0;
   DCC_FILES.eof = 0;
   DCC_FILES.expmem = 0;
   DCC_FILES.kill = 0;
   DCC_FILES.output = 0;
   DCC_FILES.display = 0;
   DCC_FILES_PASS.activity = 0;
   DCC_FILES_PASS.eof = 0;
   DCC_FILES_PASS.expmem = 0;
   DCC_FILES_PASS.kill = 0;
   DCC_FILES_PASS.output = 0;
   DCC_FILES_PASS.display = 0;
   DCC_FILES_PASS.timeout = 0;
   DCC_FORK_FILES.activity = 0;
   return NULL;
}

static int filesys_expmem()
{
   return 0;
}

static void filesys_report (int idx)
{
   if (dccdir[0]) {
      modprintf(idx, "    DCC file path: %s", dccdir);
      if (upload_to_cd)
	 modprintf(idx, "\n        incoming: (go to the current dir)\n");
      else if (dccin[0])
	 modprintf(idx, "\n        incoming: %s\n", dccin);
      else
	 modprintf(idx, "    (no uploads)\n");
      if (dcc_users)
	 modprintf(idx, "       max users is %d\n", dcc_users);
      if ((upload_to_cd) || (dccin[0]))
	 modprintf(idx, "    DCC max file size: %dk\n", dcc_maxsize);
   } else
      modprintf(idx, "  (Filesystem module loaded, but no active dcc path.)\n");
}

Function *global = NULL;
char *filesys_start ();

static Function filesys_table[] =
{
   (Function) filesys_start,
   (Function) filesys_close,
   (Function) filesys_expmem,
   (Function) filesys_report,
   (Function) remote_filereq,
   (Function) add_file,
   (Function) incr_file_gots,
   (Function) is_valid
};

#ifdef STATIC
char * transfer_start();
#endif

char *filesys_start ()
{
   p_tcl_hash_list H_dcc;
   
   module_register(MODULE_NAME, filesys_table, 1, 1);
   if (module_depend(MODULE_NAME, "transfer", 1, 0) == 0)
#ifdef STATIC
     {
	char * p = transfer_start();
	if (p)
	  return p;
	else
	  if (module_depend(MODULE_NAME, "transfer", 1, 0) == 0) 
#endif
      return "You need the transfer module to user the file system.";
#ifdef STATIC
     }
#endif
   add_tcl_commands(mytcls);
   add_tcl_strings(mystrings);
   add_tcl_ints(myints);
   H_fil = add_hash_table("fil",0,builtin_fil);
   H_dcc = find_hash_table("dcc");
   add_builtins(H_dcc, mydcc);
   add_builtins(H_fil, myfiles);
   add_hook(HOOK_GOT_DCC,filesys_gotdcc);
   /* the following is evil bodging that will hopefully removed once
    * eggdrop is 100% modules */
   DCC_FILES.activity = dcc_files;
   DCC_FILES.eof = DCC_CHAT.eof;
   DCC_FILES.expmem = expmem_dcc_files;
   DCC_FILES.kill = kill_dcc_files;
   DCC_FILES.output = out_dcc_files;
   DCC_FILES.display = disp_dcc_files;
   DCC_FILES_PASS.activity = dcc_files_pass;
   DCC_FILES_PASS.eof = DCC_CHAT.eof;
   DCC_FILES_PASS.expmem = expmem_dcc_files;
   DCC_FILES_PASS.kill = kill_dcc_files;
   DCC_FILES_PASS.output = out_dcc_files;
   DCC_FILES_PASS.display = disp_dcc_files_pass;
   DCC_FILES_PASS.timeout = tout_dcc_files_pass;
   DCC_FORK_FILES.activity = dcc_fork_files;
   return NULL;
}

static int is_valid() {
   return dccdir[0];
}
