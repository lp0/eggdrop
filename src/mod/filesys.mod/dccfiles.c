/* 
   dcc.c -- handles:
   activity on a dcc socket
   disconnect on a dcc socket
   ...and that's it!  (but it's a LOT)

   dprintf'ized, 27oct95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "../module.h"
#ifdef MODULES
#define MODULE_NAME "filesys"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "filesys.h"
#else
#include "../../tclegg.h"
#endif

/* root dcc directory */
char dccdir[121] = "";
/* directory to put incoming dcc's into */
char dccin[121] = "";
/* let all uploads go to the user's current directory? */
int upload_to_cd = 0;

#ifndef NO_FILE_SYSTEM
/* check for tcl-bound file command, return 1 if found */
/* fil: proc-name <handle> <dcc-handle> <args...> */
#ifdef MODULES
static
#endif
int check_tcl_fil PROTO3(char *, cmd, int, idx, char *, args)
{
   int atr, chatr, x;
   char s[5];
   modcontext;
   atr = get_attr_handle(dcc[idx].nick);
   chatr = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.file->chat->con_chan);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   sprintf(s, "%d", dcc[idx].sock);
   Tcl_SetVar(interp, "_n", dcc[idx].nick, 0);
   Tcl_SetVar(interp, "_i", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   modcontext;
   x = check_tcl_bind(&H_fil, cmd, atr, " $_n $_i $_a",
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

#ifdef MODULES
static
#endif
void dcc_files_pass PROTO2(int, idx, char *, buf)
{
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
      dcc[idx].type = DCC_FILES;
      if (dcc[idx].u.file->chat->status & STAT_TELNET)
	 modprintf(idx, "\377\374\001\n");	/* turn echo back on */
      putlog(LOG_FILES, "*", "File system: [%s]%s/%d", dcc[idx].nick,
	     dcc[idx].host, dcc[idx].port);
      if (!welcome_to_files(idx)) {
	 putlog(LOG_FILES, "*", "File system broken.");
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
      }
      return;
   }
   modprintf(idx, "Negative on that, Houston.\n");
   putlog(LOG_MISC, "*", "Bad password: DCC chat [%s]%s", dcc[idx].nick,
	  dcc[idx].host);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

/* hash function for file area commands */
#ifdef MODULES
static
#endif
int got_files_cmd PROTO2(int, idx, char *, msg)
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

#ifdef MODULES
static
#endif
void dcc_files PROTO2(int, idx, char *, buf)
{
   int i;

   modcontext;
   if (detect_dcc_flood(dcc[idx].u.file->chat, idx))
      return;
   dcc[idx].u.file->chat->timer = time(NULL);
   modcontext;
   strcpy(buf, check_tcl_filt(idx, buf));
   modcontext;
   if (!buf[0])
      return;
   if (buf[0] == ',') {
      for (i = 0; i < dcc_total; i++) {
	 if ((dcc[i].type == DCC_CHAT) &&
	     (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	     (dcc[i].u.chat->channel >= 0) &&
	     ((i != idx) || (dcc[idx].u.chat->status & STAT_ECHO)))
	    modprintf(i, "-%s- %s\n", dcc[idx].nick, &buf[1]);
	 if ((dcc[i].type == DCC_FILES) &&
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
	 dcc[idx].type = DCC_CHAT;
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


#ifdef MODULES
int filesys_activity_hook PROTO3(int, idx, char *, buf, int, len)
{
   modcontext;
   switch (dcc[idx].type) {
   case DCC_FILES_PASS:
      modcontext;
      dcc_files_pass(idx, buf);
      return 1;
   case DCC_FILES:
      modcontext;
      dcc_files(idx, buf);
      return 1;
   }
   return 0;
}
#endif
#endif				/* !NO_FILE_SYSTEM */
