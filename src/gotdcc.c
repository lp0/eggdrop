/*
   gotdcc.c -- handles:
   processing of incoming CTCP DCC's (chat, send)
   outgoing dcc files
   flood checking for dcc chat
   booting someone from dcc chat

   dprintf'ized, 10nov95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "main.h"
#include <sys/stat.h>
#include <varargs.h>
#include <errno.h>
#include "chan.h"
#include "modules.h"

extern int serv;
extern struct dcc_t * dcc;
extern int dcc_total;
extern char dccin[];
extern int conmask;
extern char botname[];
extern char botnetnick[];
extern int require_p;
extern char dccdir[];
extern int dcc_flood_thr;
extern int upload_to_cd;
extern char tempdir[];
extern struct chanset_t *chanset;
extern int backgrd;
extern int quiet_reject;
extern time_t now;
extern int connect_timeout;
extern int max_dcc;

int reserved_port = 0;
#ifndef NO_IRC
static void failed_got_dcc (int idx)
{
   char s1[121];
   if (strcmp(dcc[idx].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	 if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
	     (dcc[x].type == &DCC_BOT))
	    y = x;
      if (y != 0) {
	 dcc[y].u.bot->status &= ~STAT_GETTING;
	 dcc[y].u.bot->status &= ~STAT_SHARE;
      }
      putlog(LOG_MISC, "*", USERF_FAILEDXFER);
      fclose(dcc[idx].u.xfer->f);
      unlink(dcc[idx].u.xfer->filename);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   neterror(s1);
   mprintf(serv, "NOTICE %s :%s (%s)\n", dcc[idx].nick, DCC_CONNECTFAILED1, s1);
   putlog(LOG_MISC, "*", "%s: %s %s (%s!%s)", DCC_CONNECTFAILED2,
	  	dcc[idx].type == &DCC_FORK_SEND ? "SEND" : "CHAT", 
		dcc[idx].type == &DCC_FORK_SEND ? 
		    dcc[idx].u.xfer->filename : "", dcc[idx].nick,
	  	dcc[idx].host);
   putlog(LOG_MISC, "*", "    (%s)", s1);
   if (dcc[idx].type == &DCC_FORK_SEND) {
      /* erase the 0-byte file i started */
      fclose(dcc[idx].u.xfer->f);
      sprintf(s1, "%s%s", tempdir, dcc[idx].u.xfer->filename);
      unlink(s1);
   }
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void cont_got_dcc (int idx,char * buf,int len)
{
   char s1[121];
   sprintf(s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
   putlog(LOG_MISC, "*", "DCC connection: CHAT (%s)", 
	  s1);
   get_handle_by_host(dcc[idx].nick, s1);
   dcc[idx].timeval = now;
   if (pass_match_by_host("-", s1)) {
      /* no password set -  if you get here, something is WEIRD */
      dprintf(idx, IRC_NOPASS2);
      dcc[idx].type = &DCC_CHAT;
      if (get_attr_handle(dcc[idx].nick) & USER_MASTER)
	dcc[idx].u.chat->con_flags = conmask;
      dcc_chatter(idx);
   } else {
      dprintf(idx, "%s\n", DCC_ENTERPASS);
      dcc[idx].type = &DCC_CHAT_PASS;
   }
}

static int expmem_fork_chat (int idx) {
   return sizeof(struct chat_info);
}

static void kill_fork_chat (int idx) {
   nfree(dcc[idx].u.chat);
}

static void display_fork_chat (int idx,char * buf) {
   sprintf(buf,"conn  chat");
}

static int expmem_fork_send (int idx) {
   return sizeof(struct xfer_info) + sizeof(struct chat_info);
}

static void kill_fork_send (int idx) {
   nfree(dcc[idx].u.xfer);
}

static void display_fork_send (int idx,char * buf) {
   sprintf(buf,"conn  send");
}

struct dcc_table DCC_FORK_CHAT = {
   failed_got_dcc,
   cont_got_dcc,
   &connect_timeout,
   failed_got_dcc,
   display_fork_chat,
   expmem_fork_chat,
   kill_fork_chat,
   0
};

struct dcc_table DCC_FORK_FILES = {
   failed_got_dcc,
   cont_got_dcc,
   &connect_timeout,
   failed_got_dcc,
   display_fork_send,
   expmem_fork_send,
   kill_fork_send,
   0
};

   

/* received a ctcp-dcc */
void gotdcc (char * nick, char * from, char * msg)
{
   char code[512], param[512], ip[512], s1[512], prt[81], nk[10];
   int i, atr;
   struct dcc_table * z = 0;
   nsplit(code, msg);
   if ((strcasecmp(code, "chat") != 0)) {
      call_hook_cccc(HOOK_GOT_DCC, nick, from, code, msg);
      return;
   }
   /* dcc chat or send! */
   nsplit(param, msg);
   nsplit(ip, msg);
   nsplit(prt, msg);
   sprintf(s1, "%s!%s", nick, from);
   atr = get_attr_host(s1);
   get_handle_by_host(nk, s1);
   if (strcasecmp(code, "chat") == 0) {
      int ok = 0;
      if ((!require_p) && (op_anywhere(nk)))
	 ok = 1;
      if (atr & (USER_MASTER | USER_XFER | USER_PARTY))
	 ok = 1;
      if (!ok) {
        if (quiet_reject == 0) {
	  mprintf(serv, "NOTICE %s :%s\n", nick, DCC_NOSTRANGERS);
        }
	putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED, nick, from);
	return;
      } else {
	 if (atr & USER_BOT) {
	    if (in_chain(nk)) {
	       mprintf(serv, "NOTICE %s :You're already connected.\n", nick);
	       putlog(LOG_BOTS, "*", DCC_REFUSEDTAND, nk);
	       return;
	    }
	 }
      }
   }
   if (dcc_total == max_dcc) {
      mprintf(serv, "NOTICE %s :%s\n", nick, DCC_TOOMANYDCCS1);
      putlog(LOG_MISC, "*", DCC_TOOMANYDCCS1, code, param, nick, from);
      return;
   }
   i = dcc_total;
   dcc[i].addr = my_atoul(ip);
   dcc[i].port = atoi(prt);
   dcc[i].sock = (-1);
   strcpy(dcc[i].nick, nick);
   strcpy(dcc[i].host, from);
   dcc[i].u.other = NULL;
   dcc[i].u.chat = get_data_ptr(sizeof(struct chat_info));
   dcc[i].u.chat->away = NULL;
   dcc[i].u.chat->status = STAT_ECHO;
   dcc[i].timeval = now;
   dcc[i].u.chat->msgs_per_sec = 0;
   dcc[i].u.chat->con_flags = 0;
   dcc[i].u.chat->buffer = NULL;
   dcc[i].u.chat->max_line = 0;
   dcc[i].u.chat->line_count = 0;
   dcc[i].u.chat->current_lines = 0;
   strcpy(dcc[i].u.chat->con_chan, chanset->name);
   dcc[i].u.chat->channel = 0;
   if (atr & USER_MASTER)
     z = &DCC_FORK_CHAT;
   else if (op_anywhere(nk)) {
      if ((!require_p) || (atr & USER_PARTY))
	z = &DCC_FORK_CHAT;
      else if (atr & USER_XFER)
	z = &DCC_FORK_FILES;
      else {
	 if (quiet_reject == 0) {
	    mprintf(serv, "NOTICE %s :%s.\n", nick, DCC_REFUSED2);
	 }
	 putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED, nick, from);
	 return;
      }
   } else if (atr & USER_PARTY) {
      z = &DCC_FORK_CHAT;
      dcc[i].u.chat->status |= STAT_PARTY;
   } else if (atr & USER_XFER) {
      z = &DCC_FORK_FILES;
   } else {
      if (quiet_reject == 0) {
	 mprintf(serv, "NOTICE %s :No access.\n", nick);
      }
      putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED, nick, from);
      return;
   }
   if (pass_match_by_host("-", s1)) {
      mprintf(serv, "NOTICE %s :%s.\n", nick, DCC_REFUSED3);
      putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED4, nick, from);
      return;
   }
   if (z == &DCC_FORK_FILES) {
      struct file_info *fi;
      module_entry * me = module_find("filesys", 1, 0);
      
      if (!me || !me->funcs[FILESYS_ISVALID] 
	  || !(me->funcs[FILESYS_ISVALID])()) {
	 if (quiet_reject == 0) {
	    mprintf(serv, "NOTICE %s :%s.\n", nick, DCC_REFUSED2);
	 }
	 putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED5, nick, from);
	 return;
      }
      /* ARGH: three level nesting */
      fi = get_data_ptr(sizeof(struct file_info));   
      fi->chat = dcc[i].u.chat;
      dcc[i].u.file = fi;
   }
   dcc[i].type = z;	/* store future type */
   dcc[i].timeval = now;
   dcc_total++;
   /* ok, we're satisfied with them now: attempt the connect */
   if ((z == &DCC_FORK_CHAT) || (z == &DCC_FORK_FILES))
     dcc[i].sock = getsock(0);
   else
     dcc[i].sock = getsock(SOCK_BINARY);	/* doh. */
   if (open_telnet_dcc(dcc[i].sock, ip, prt) < 0) {
      /* can't connect (?) */
      failed_got_dcc(i);
      return;
   }
   /* assume dcc chat succeeded, and move on */
   if ((z == &DCC_FORK_CHAT) || (z == &DCC_FORK_FILES))
     z->activity(i,0,0);
      
}
#endif				/* !NO_IRC */

int detect_dcc_flood (time_t * timer,struct chat_info * chat, int idx)
{
   time_t t;
   if (dcc_flood_thr == 0)
      return 0;
   t = now;
   if (*timer != t) {
      *timer = t;
      chat->msgs_per_sec = 0;
   } else {
      chat->msgs_per_sec++;
      if (chat->msgs_per_sec > dcc_flood_thr) {
	 /* FLOOD */
	 dprintf(idx, "*** FLOOD: %s.\n", IRC_GOODBYE);
	 if (dcc[idx].u.chat->channel >= 0) {
	    chanout2_but(idx, dcc[idx].u.chat->channel, DCC_FLOODBOOT,
			 dcc[idx].nick);
	    if (dcc[idx].u.chat->channel < 100000)
	       tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
	 }
	 check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
	 if ((dcc[idx].sock != STDOUT) || backgrd) {
	    killsock(dcc[idx].sock);
	    lostdcc(idx);
	 } else {
	    tprintf(STDOUT, "\n### SIMULATION RESET ###\n\n");
	    dcc_chatter(idx);
	 }
	 return 1;		/* <- flood */
      }
   }
   return 0;
}

/* handle someone being booted from dcc chat */
void do_boot (int idx, char * by, char * reason)
{
   int files = (dcc[idx].type == &DCC_FILES);
   dprintf(idx, DCC_BOOTED1);
   dprintf(idx, DCC_BOOTED2, DCC_BOOTED2_ARGS);
   if ((!files) && (dcc[idx].u.chat->channel >= 0)) {
      chanout2_but(idx, dcc[idx].u.chat->channel,
	     "%s booted %s from the party line%s%s\n", by, dcc[idx].nick,
		   reason[0] ? ": " : ".", reason);
      if (dcc[idx].u.chat->channel < 100000)
	 tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
   }
   check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
   if ((dcc[idx].sock != STDOUT) || backgrd) {
      killsock(dcc[idx].sock);
      dcc[idx].sock = (long)dcc[idx].type;
      dcc[idx].type = &DCC_LOST;
      /* entry must remain in the table so it can be logged by the caller */
   } else {
      tprintf(STDOUT, "\n### SIMULATION RESET\n\n");
      dcc_chatter(idx);
   }
   return;
}
