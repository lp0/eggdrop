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

#include "main.h"
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include "chan.h"
#include "modules.h"

extern int serv;
extern char ver[];
extern char version[];
extern char origbotname[];
extern char botname[];
extern char realbotname[];
extern char botnetnick[];
extern char notify_new[];
extern int conmask;
extern int default_flags;
extern int online;
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern int backgrd;
extern int make_userfile;
extern int egg_numver;
extern int connect_timeout;
extern Tcl_Interp * interp;
extern int max_dcc;

/* dcc list */
struct dcc_t * dcc = 0;
/* total dcc's */
int dcc_total = 0;
/* temporary directory (default: current dir) */
char tempdir[121] = "";
/* require 'p' access to get on the party line? */
int require_p = 0;
/* allow people to introduce themselves via telnet */
int allow_new_telnets = 0;
/* name of the IRC network you're on */
char network[41] = "unknown-net";
/* time to wait for a password from a user */
int password_timeout = 180;
/* global time variable */
extern time_t now;
/* bot timeout value */
int bot_timeout = 60;

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

static char *stat_str2 (int st)
{
   static char s[10];
   s[0] = st & STAT_PINGED ? 'P' : 'p';
   s[1] = st & STAT_SHARE ? 'U' : 'u';
   s[2] = st & STAT_CALLED ? 'C' : 'c';
   s[3] = st & STAT_OFFERED ? 'O' : 'o';
   s[4] = st & STAT_SENDING ? 'S' : 's';
   s[5] = st & STAT_GETTING ? 'G' : 'g';
   s[6] = st & STAT_WARNED ? 'W' : 'w';
   s[7] = st & STAT_LEAF ? 'L' : 'l';
   s[8] = 0;
   return s;
}

static void strip_telnet (int sock, char * buf, int * len)
{
   unsigned char *p = (unsigned char *) buf, *o = (unsigned char *)buf;
   int mark;
   while (*p != 0) {
      while ((*p != 255) && (*p != 0))
	 *o++ = *p++;
      if (*p == 255) {
	 p++;
	 mark = 2;
	 if (!*p)
	    mark = 1;		/* bogus */
	 if ((*p >= 251) && (*p <= 254)) {
	    mark = 3;
	    if (!*(p + 1))
	       mark = 2;	/* bogus */
	 }
	 if (*p == 251) {
	    /* WILL X -> response: DONT X */
	    /* except WILL ECHO which we just smile and ignore */
	    if (!(*(p + 1) == 1)) {
	       write(sock, "\377\376", 2);
	       write(sock, p + 1, 1);
	    }
	 }
	 if (*p == 253) {
	    /* DO X -> response: WONT X */
	    /* except DO ECHO which we just smile and ignore */
	    if (!(*(p + 1) == 1)) {
	       write(sock, "\377\374", 2);
	       write(sock, p + 1, 1);
	    }
	 }
	 if (*p == 246) {
	    /* "are you there?" */
	    /* response is: "hell yes!" */
	    write(sock, "\r\nHell, yes!\r\n", 14);
	 }
	 /* anything else can probably be ignored */
	 p += mark-1;
	 *len = *len - mark;
      }
   }
   *o = *p;
}

static void stop_auto (char * nick)
{
   int i;
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_FORK_BOT) {
	 killsock(dcc[i].sock);
	 dcc[i].sock = (long)dcc[i].type;
	 dcc[i].type = &DCC_LOST;
      }
}

static void greet_new_bot (int idx)
{
   int atr = get_attr_handle(dcc[idx].nick);
   stop_auto(dcc[idx].nick);
   dcc[idx].timeval = now;
   dcc[idx].u.bot->version[0] = 0;
   dcc[idx].u.bot->numver = 0;
   if (atr & BOT_REJECT) {
      putlog(LOG_BOTS, "*", "Rejecting link from %s", dcc[idx].nick);
      tprintf(dcc[idx].sock, "error You are being rejected.\n");
      tprintf(dcc[idx].sock, "bye\n");
      killsock(dcc[idx].sock);
      dcc[idx].sock = (long)dcc[idx].type;
      dcc[idx].type = &DCC_LOST;
      return;
   }
   if (atr & BOT_LEAF)
      dcc[idx].u.bot->status |= STAT_LEAF;
   tprintf(dcc[idx].sock, "version %d %s <%s>\n", egg_numver, ver, network);
   tprintf(dcc[idx].sock, "thisbot %s\n", botnetnick);
   putlog(LOG_BOTS, "*", "Linked to %s", dcc[idx].nick);
   chatout("*** Linked to %s\n", dcc[idx].nick);
   tandout_but(idx, "nlinked %s %s\n", dcc[idx].nick, botnetnick);
   tandout_but(idx, "chat %s Linked to %s\n", botnetnick, dcc[idx].nick);
   dump_links(idx);
   addbot(dcc[idx].nick, dcc[idx].nick, botnetnick, "-");
   check_tcl_link(dcc[idx].nick, botnetnick);
}

void failed_link (int idx)
{
   char s[81];
   if (dcc[idx].port >= dcc[idx].u.bot->port + 3) {
      if (dcc[idx].u.bot->linker[0]) {
	 sprintf(s, "Couldn't link to %s.", dcc[idx].nick);
	 add_note(dcc[idx].u.bot->linker, botnetnick, s, -2, 0);
      }
      if (dcc[idx].u.bot->x != (-1))
	 putlog(LOG_BOTS, "*", "Failed link to %s.", dcc[idx].nick);
      killsock(dcc[idx].sock);
      dcc[idx].sock = (long)dcc[idx].type;
      dcc[idx].type = &DCC_LOST;
      autolink_cycle(dcc[idx].nick);	/* check for more auto-connections */
      return;
   }
   /* try next port */
   killsock(dcc[idx].sock);
   dcc[idx].sock = getsock(SOCK_STRONGCONN);
   dcc[idx].port++;
   dcc[idx].timeval = time(NULL);
   if (open_telnet_raw(dcc[idx].sock, dcc[idx].host, dcc[idx].port) < 0) {
      failed_link(idx);
   }
}

/* disconnect all +a bots because we just got a hub */
static void drop_alt_bots()
{
   int atr, i;
   for (i = 0; i < dcc_total; i++) {
      atr = get_attr_handle(dcc[i].nick);
      if (atr & BOT_ALT) {
	 if (dcc[i].type == &DCC_FORK_BOT) {
	    killsock(dcc[i].sock);
	    dcc[i].sock = (long)dcc[i].type;
	    dcc[i].type = &DCC_LOST;
	 } else if (dcc[i].type == &DCC_BOT_NEW) {
	    killsock(dcc[i].sock);
	    dcc[i].sock = (long)dcc[i].type;
	    dcc[i].type = &DCC_LOST;
	 }
      }
   }
}

static void cont_link (int idx,char * buf,int i)
{
   char s[81];
   context;
   if (get_attr_handle(dcc[idx].nick) & BOT_HUB) {
      drop_alt_bots();		/* just those currently in the process of linking */
      if (in_chain(dcc[idx].nick)) {
	 i = nextbot(dcc[idx].nick);
	 if (i > 0) {
	    if (flags_eq(BOT_SHARE | BOT_HUB, get_attr_handle(dcc[i].nick))) {
	       if (flags_eq(BOT_SHARE | BOT_HUB, get_attr_handle(dcc[idx].nick))) {
		  chatout("*** Bringing sharebot %s to me...\n", dcc[idx].nick);
		  tandout("chat %s Bringing sharebot %s to me...\n",
			  botnetnick, dcc[idx].nick);
		  tprintf(dcc[i].sock, "unlink %s %s %s Sharebot Restructure\n",
		      botnetnick, lastbot(dcc[idx].nick), dcc[idx].nick);
	       } else {
		  failed_link(idx);
		  return;
	       }
	    } else {
	       chatout("*** Unlinked %s (restructure)\n", dcc[i].nick);
	       tandout_but(i, "chat %s Unlinked %s (restructure)\n", botnetnick,
			   dcc[i].nick);
	       tandout_but(i, "unlinked %s\n", dcc[i].nick);
	       tprintf(dcc[i].sock, "bye\n");
	       killsock(dcc[i].sock);
	       dcc[i].sock = (long)dcc[i].type;
	       dcc[i].type = &DCC_LOST;
	    }
	 }
      }
   }
   dcc[idx].type = &DCC_BOT_NEW;
   get_pass_by_handle(dcc[idx].nick, s);
   if (strcasecmp(s, "-") == 0)
      tprintf(dcc[idx].sock, "%s\n", botnetnick);
   else
      tprintf(dcc[idx].sock, "%s\n%s\n", botnetnick, s);
   context;
   return;
}

static void dcc_bot_new (int idx, char * buf,int x)
{
   strip_telnet(dcc[idx].sock, buf, &x);
   if (strcasecmp(buf, "*hello!") == 0) {
      dcc[idx].type = &DCC_BOT;
      greet_new_bot(idx);
   }
   if (strcasecmp(buf, "badpass") == 0) {
      /* we entered the wrong password */
      putlog(LOG_BOTS, "*", "Bad password on connect attempt to %s.",
	     dcc[idx].nick);
   }
   if (strcasecmp(buf, "passreq") == 0) {
      if (pass_match_by_handle("-", dcc[idx].nick)) {
	 putlog(LOG_BOTS, "*", "Password required for connection to %s.",
		dcc[idx].nick);
	 tprintf(dcc[idx].sock, "-\n");
      }
   }
   if (strncmp(buf, "error", 5) == 0) {
      split(NULL, buf);
      putlog(LOG_BOTS, "*", "ERROR linking %s: %s", dcc[idx].nick, buf);
   }
   /* ignore otherwise */
}

static void eof_dcc_bot_new (int idx) {
   putlog(LOG_BOTS, "*", "Lost bot: %s", dcc[idx].nick, dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void timeout_dcc_bot_new (int idx) {
   putlog(LOG_MISC, "*", "Timeout: bot link to %s at %s:%d", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_bot_new (int idx,char * buf) {
   sprintf(buf,"bot*  waited %lus",now-dcc[idx].timeval);
}
	 
static int expmem_dcc_bot_ (int n) {
   return sizeof(struct bot_info);
}

static void free_dcc_bot_ (int n) {
   unvia(n, dcc[n].nick);
   nfree(dcc[n].u.bot);
}
	 
struct dcc_table DCC_BOT_NEW = {
   eof_dcc_bot_new,
   dcc_bot_new,
   &bot_timeout,
   timeout_dcc_bot_new,
   display_dcc_bot_new,
   expmem_dcc_bot_,
   free_dcc_bot_,
   0
};

/* hash function for tandem bot commands */
extern botcmd_t C_bot[];

static void dcc_bot (int idx, char * msg,int i) {
   char total[512], code[512];
   int f;
   context;
   strip_telnet(dcc[idx].sock, msg, &i);
   strcpy(total, msg);
   nsplit(code, msg);
   f = 0;
   i = 0;
   while ((C_bot[i].name != NULL) && (!f)) {
      int y = strcasecmp(code, C_bot[i].name);
      
      if (y == 0) {
	 /* found a match */
	 (C_bot[i].func) (idx, msg);
	 f = 1;
      } else if (y < 0)
	   return;
      i++;
   }
}

static void eof_dcc_bot (int idx) {
   putlog(LOG_BOTS, "*", "Lost bot: %s", dcc[idx].nick);
   chatout("*** Lost bot: %s\n", dcc[idx].nick);
   tandout_but(idx, "chat %s Lost bot: %s\n", botnetnick, dcc[idx].nick);
   tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}
   
static void display_dcc_bot (int idx,char * buf) {
   sprintf(buf, "bot   flags: %s", stat_str2(dcc[idx].u.bot->status));
}

static void display_dcc_fork_bot (int idx,char * buf) {
   sprintf(buf, "conn  bot");
}

struct dcc_table DCC_BOT = {
   eof_dcc_bot,
   dcc_bot,
   0,
   0,
   display_dcc_bot,
   expmem_dcc_bot_,
   free_dcc_bot_,
   0
};

struct dcc_table DCC_FORK_BOT = {
   failed_link,
   cont_link,
   &connect_timeout,
   failed_link,
   display_dcc_fork_bot,
   expmem_dcc_bot_,
   free_dcc_bot_,
   0
};

static void dcc_chat_pass (int idx, char * buf,int atr)
{
   if (!atr)
     return;
   strip_telnet(dcc[idx].sock, buf, &atr);
   atr = get_attr_handle(dcc[idx].nick);
   if (pass_match_by_handle(buf, dcc[idx].nick)) {
      if (atr & USER_BOT) {
	 nfree(dcc[idx].u.chat);
	 dcc[idx].type = &DCC_BOT;
	 dcc[idx].u.bot = get_data_ptr(sizeof(struct bot_info));
	 dcc[idx].u.bot->status = STAT_CALLED;
	 tprintf(dcc[idx].sock, "*hello!\n");
	 greet_new_bot(idx);
      } else {
	 if (dcc[idx].u.chat->away != NULL) {
	    nfree(dcc[idx].u.chat->away);
	    dcc[idx].u.chat->away = NULL;
	 }
	 dcc[idx].type = &DCC_CHAT;
	 dcc[idx].u.chat->status &= ~STAT_CHAT;
	 if (atr & USER_MASTER)
	    dcc[idx].u.chat->con_flags = conmask;
	 if (dcc[idx].u.chat->status & STAT_TELNET)
	    tprintf(dcc[idx].sock, "\377\374\001\n");	/* turn echo back on */
	 dcc_chatter(idx);
      }
   } else {
      if (get_attr_handle(dcc[idx].nick) & USER_BOT)
	 tprintf(dcc[idx].sock, "badpass\n");
      else
	 dprintf(idx, "Negative on that, Houston.\n");
      putlog(LOG_MISC, "*", "Bad password: DCC chat [%s]%s", dcc[idx].nick,
	     dcc[idx].host);
      if (dcc[idx].u.chat->away != NULL) {	/* su from a dumb user */
	 strcpy(dcc[idx].nick, dcc[idx].u.chat->away);
	 nfree(dcc[idx].u.chat->away);
	 dcc[idx].u.chat->away = NULL;
	 dcc[idx].type = &DCC_CHAT;
	 if (dcc[idx].u.chat->channel < 100000) {
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
		    dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock,
		    dcc[idx].host);
	 }
	 chanout2(dcc[idx].u.chat->channel, "%s has re-joined the party line.\n", dcc[idx].nick);
      } else {
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
      }
   }
}

static void eof_dcc_general (int idx) {
   putlog(LOG_MISC, "*", "Lost dcc connection to %s (%s/%d)", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void tout_dcc_chat_pass (int idx) {
   dprintf(idx, "Timeout.\n");
   putlog(LOG_MISC, "*", "Password timeout on dcc chat: [%s]%s", dcc[idx].nick,
	  dcc[idx].host);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_chat_pass (int idx,char * buf) {
   sprintf(buf,"pass  waited %lus",now - dcc[idx].timeval);
}

static int expmem_dcc_general (int idx) {
   int tot = sizeof(struct chat_info);
   if (dcc[idx].u.chat->away != NULL)
     tot += strlen(dcc[idx].u.chat->away) + 1;
   return tot;
}

static void kill_dcc_general (int idx) {
   if (dcc[idx].u.chat->away != NULL)
     nfree(dcc[idx].u.chat->away);
   nfree(dcc[idx].u.chat);
}

static void out_dcc_general (int idx,char * buf) {
   char * p = buf;
   if (dcc[idx].u.chat->status & STAT_TELNET)
     p = add_cr(buf);
   tputs(dcc[idx].sock,buf,strlen(buf));
}

struct dcc_table DCC_CHAT_PASS = {
   eof_dcc_general,
   dcc_chat_pass,
   &password_timeout,
   tout_dcc_chat_pass,
   display_dcc_chat_pass,
   expmem_dcc_general,
   kill_dcc_general,
   out_dcc_general   
};

/* make sure ansi code is just for color-changing */
static int check_ansi (char * v)
{
   int count = 2;
   if (*v++ != '\033')
      return 1;
   if (*v++ != '[')
      return 1;
   while (*v) {
      if (*v == 'm')
	 return 0;
      if ((*v != ';') && ((*v < '0') || (*v > '9')))
	 return count;
      v++;
      count++;
   }
   return count;
}

static void eof_dcc_chat (int idx) {
   dcc[idx].u.chat->con_flags = 0;
   putlog(LOG_MISC, "*", "Lost dcc connection to %s (%s/%d)", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   if (dcc[idx].u.chat->channel >= 0) {
      chanout2_but(idx, dcc[idx].u.chat->channel, "%s lost dcc link.\n",
		   dcc[idx].nick);
      context;
      if (dcc[idx].u.chat->channel < 100000)
	tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
   }
   check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock);
   check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

/* for dcc commands -- hash the function */
static int got_dcc_cmd (int idx, char * msg)
{
   char total[512], code[512];
   strcpy(total, msg);
   rmspace(msg);
   nsplit(code, msg);
   rmspace(msg);
   return check_tcl_dcc(code, idx, msg);
}

static void dcc_chat (int idx, char * buf,int i)
{
   int nathan = 0, doron = 0, fixed = 0;
   char *v = buf;
   context;
   strip_telnet(dcc[idx].sock, buf, &i);
   if (detect_dcc_flood(&dcc[idx].timeval,dcc[idx].u.chat, idx))
      return;
   dcc[idx].timeval = now;
   touch_laston_handle(userlist,dcc[idx].nick,"partyline",now);
   if (buf[0])
      strcpy(buf, check_tcl_filt(idx, buf));
   if (buf[0]) {
      /* check for beeps and cancel annoying ones */
      v = buf;
      while (*v)
	 switch (*v) {
	 case 7:		/* beep - no more than 3 */
	    nathan++;
	    if (nathan > 3)
	       strcpy(v, v + 1);
	    else
	       v++;
	    break;
	 case 8:		/* backspace - for lame telnet's :) */
	    if (v > buf) {
	       v--;
	       strcpy(v, v + 2);
	    } else
	       strcpy(v, v + 1);
	    break;
	 case 27:		/* ESC - ansi code? */
	    doron = check_ansi(v);
	    /* if it's valid, append a return-to-normal code at the end */
	    if (!doron) {
	       if (!fixed)
		  strcat(buf, "\033[0m");
	       v++;
	       fixed = 1;
	    } else
	       strcpy(v, v + doron);
	    break;
	 case '\r':		/* weird pseudo-linefeed */
	    strcpy(v, v + 1);
	    break;
	 default:
	    v++;
	 }
      if (buf[0]) {		/* nothing to say - maybe paging... */
	 if ((buf[0] == '.') || (dcc[idx].u.chat->channel < 0)) {
	    if (buf[0] == '.')
	       strcpy(buf, &buf[1]);
	    if (got_dcc_cmd(idx, buf)) {
	       check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock);
	       check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
	       dprintf(idx, "*** Ja mata!\n");
	       flush_lines(idx);
	       putlog(LOG_MISC, "*", "DCC connection closed (%s!%s)", dcc[idx].nick,
		      dcc[idx].host);
	       if (dcc[idx].u.chat->channel >= 0) {
		  chanout2(dcc[idx].u.chat->channel, "%s left the party line%s%s\n",
			   dcc[idx].nick, buf[0] ? ": " : ".", buf);
		  context;
		  if (dcc[idx].u.chat->channel < 100000)
		     tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
	       }
	       if ((dcc[idx].sock != STDOUT) || backgrd) {
		  killsock(dcc[idx].sock);
		  lostdcc(idx);
		  return;
	       } else {
		  tprintf(STDOUT, "\n### SIMULATION RESET\n\n");
		  dcc_chatter(idx);
		  return;
	       }
	    }
	 } else if (buf[0] == ',') {
	    for (i = 0; i < dcc_total; i++) {
	       if ((dcc[i].type == &DCC_CHAT) &&
		   (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
		   (dcc[i].u.chat->channel >= 0) &&
		   ((i != idx) || (dcc[idx].u.chat->status & STAT_ECHO)))
		  dprintf(i, "-%s- %s\n", dcc[idx].nick, &buf[1]);
	       if ((dcc[i].type == &DCC_FILES) &&
		   (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
		   ((i != idx) || (dcc[idx].u.file->chat->status & STAT_ECHO)))
		  dprintf(i, "-%s- %s\n", dcc[idx].nick, &buf[1]);
	    }
	 } else {
	    if (dcc[idx].u.chat->away != NULL)
	       not_away(idx);
	    if (dcc[idx].u.chat->status & STAT_ECHO)
	       chanout(dcc[idx].u.chat->channel, "<%s> %s\n", dcc[idx].nick, buf);
	    else
	       chanout_but(idx, dcc[idx].u.chat->channel, "<%s> %s\n",
			   dcc[idx].nick, buf);
	    if (dcc[idx].u.chat->channel < 100000)
	       tandout("chan %s@%s %d %s\n", dcc[idx].nick, botnetnick,
		       dcc[idx].u.chat->channel, buf);
	    check_tcl_chat(dcc[idx].nick, dcc[idx].u.chat->channel, buf);
	 }
      }
   }
   if (dcc[idx].type == &DCC_CHAT)	/* could have change to files */
      if (dcc[idx].u.chat->status & STAT_PAGE)
	 flush_lines(idx);
}

static void display_dcc_chat (int idx,char * buf) {
   sprintf(buf, "chat  flags: %s/%d", stat_str(dcc[idx].u.chat->status),
	   dcc[idx].u.chat->channel);
}

static int expmem_dcc_chat (int idx) {
   int tot = sizeof(struct chat_info);
   if (dcc[idx].u.chat->away != NULL)
     tot += strlen(dcc[idx].u.chat->away) + 1;
   if (dcc[idx].u.chat->buffer) {
      struct eggqueue *p = dcc[idx].u.chat->buffer;
      while (p != NULL) {
	 tot += sizeof(struct eggqueue);
	 tot += strlen(p->item);
	 p = p->next;
      }
   }
   return tot;
}

static void kill_dcc_chat (int idx) {
   if (dcc[idx].u.chat->buffer) {
      struct eggqueue *p = dcc[idx].u.chat->buffer, *q;
      while (p) {
	 q = p->next;
	 nfree(p->item);
	 nfree(p);
	 p = q;
      }
   }
   kill_dcc_general(idx);
}

/* Remove the color control codes that mIRC,pIRCh etc use to make    *
 * their client seem so fecking cool! (Sorry, Khaled, you are a nice *
 * guy, but when you added this feature you forced people to either  *
 * use your *SHAREWARE* client or face screenfulls of crap!)         */

static void strip_mirc_codes (int flags, char * text)
{
   char *dd=text;
   while (*text) {
      switch (*text) {
      case 2:			/* Bold text */
	 if (flags & STRIP_BOLD) {
	    text++;
	    continue;
	 }
	 break;
      case 3:			/* mIRC colors? */
	 if (flags & STRIP_COLOR) {
	    if (isdigit(text[1])) {	/* Is the first char a number? */
	       text += + 2;	/* Skip over the ^C and the first digit */
	       if (isdigit(*text))
		  text++;		/* Is this a double digit number? */
	       if (*text == ',') {	/* Do we have a background color next? */
		  if (isdigit(text[1]))
		     text += 2;	/* Skip over the first background digit */
		  if (isdigit(*text))
		     text++;	/* Is it a double digit? */
	       }
	    }
	    continue;
	 } 
	 break;
      case 0x16:		/* Reverse video */
	 if (flags & STRIP_REV) {
	    text++;
	    continue;
	 }
	 break;
      case 0x1f:		/* Underlined text */
	 if (flags & STRIP_UNDER) {
	    text++;
	    continue;
	 }
	 break;
      case 033:
	 if (flags & STRIP_ANSI) {
	    text ++;
	    if (*text == '[') {
	       text++;
	       while ((*text == ';') || isdigit(*text)) 
		  text++;
	       if (*text)
		  text++;		/* also kill the following char */
	    }
	    continue;
	 }
	 break;
      }
      *dd++ = *text++;		/* Move on to the next char */
   }
}


static void append_line (int idx, char * line)
{
   int l = strlen(line);
   struct eggqueue *p, *q;
   struct chat_info *c = (dcc[idx].type == &DCC_CHAT) ? dcc[idx].u.chat :
   dcc[idx].u.file->chat;
   if (c->current_lines > 1000) {
      p = c->buffer;
      /* they're probably trying to fill up the bot nuke the sods :) */
      while (p) {		/* flush their queue */
	 q = p->next;
	 nfree(p->item);
	 nfree(p);
	 p = q;
      }
      c->buffer = 0;
      c->status &= ~STAT_PAGE;
      do_boot(idx, botname, "too many pages - senq full");
      return;
   }
   if ((c->line_count < c->max_line) && (c->buffer == NULL)) {
      c->line_count++;
      tputs(dcc[idx].sock, line, l);
   } else {
      c->current_lines++;
      if (c->buffer == NULL)
	 q = NULL;
      else {
	 q = c->buffer;
	 while (q->next != NULL)
	    q = q->next;
      }
      p = (struct eggqueue *) nmalloc(sizeof(struct eggqueue));
      p->stamp = l;
      p->item = (char *) nmalloc(p->stamp + 1);
      p->next = NULL;
      strcpy(p->item, line);
      if (q == NULL)
	 c->buffer = p;
      else
	 q->next = p;
   }
}

static void out_dcc_chat (int idx,char * buf) {
   strip_mirc_codes(dcc[idx].u.chat->strip_flags,buf);
   if (dcc[idx].u.chat->status & STAT_TELNET)
     add_cr(buf);
   if (dcc[idx].u.chat->status & STAT_PAGE) 
     append_line(idx,buf);
   else
     tputs(dcc[idx].sock,buf,strlen(buf));
}
      
struct dcc_table DCC_CHAT = {
   eof_dcc_chat,
   dcc_chat,
   0,
   0,
   display_dcc_chat,
   expmem_dcc_chat,
   kill_dcc_chat,
   out_dcc_chat  
};

static void dcc_telnet (int idx, char * buf,int i)
{
   unsigned long ip;
   unsigned short port;
   int j;
   char s[121], s1[81];
   if (dcc_total + 1 > max_dcc) {
      j = answer(dcc[idx].sock, s, &ip, &port, 0);
      if (j != -1) {
	 tprintf(j, "Sorry, too many connections already.\r\n");
	 killsock(j);
      }
      return;
   }
   i = dcc_total;
   dcc[i].sock = answer(dcc[idx].sock, s, &ip, &port, 0);
   while ((dcc[i].sock == (-1)) && (errno == EAGAIN))
      dcc[i].sock = answer(dcc[idx].sock, s, &ip, &port, 0);
   if (dcc[i].sock < 0) {
      neterror(s1);
      putlog(LOG_MISC, "*", "Failed TELNET incoming (%s)", s1);
      killsock(dcc[i].sock);
      return;
   }
   sprintf(dcc[i].host, "telnet!telnet@%s", s);
   if (match_ignore(dcc[i].host)) {
/*    tprintf(dcc[i].sock,"\r\nSorry, your site is being ignored.\r\n\n"); */
      killsock(dcc[i].sock);
      return;
   }
   if (dcc[idx].host[0] == '@') {
      /* restrict by hostname */
      if (!wild_match(&dcc[idx].host[1], s)) {
/*      tprintf(dcc[i].sock,"\r\nSorry, this port is busy.\r\n");  */
	 putlog(LOG_BOTS, "*", "Refused %s (bad hostname)", s);
	 killsock(dcc[i].sock);
	 return;
      }
   }
   dcc[i].addr = ip;
   dcc[i].port = port;
   sprintf(dcc[i].host, "telnet:%s", s);
   /* script? */
   if (strcmp(dcc[idx].nick, "(script)") == 0) {
      strcpy(dcc[i].nick, "*");
      dcc[i].type = &DCC_SOCKET;
      dcc[i].u.other = NULL;
      dcc_total++;
      check_tcl_listen(dcc[idx].host, dcc[i].sock);
      return;
   }
   dcc[i].type = &DCC_TELNET_ID;
   dcc[i].u.chat = get_data_ptr(sizeof(struct chat_info));
   /* copy acceptable-nick/host mask */
   strncpy(dcc[i].nick, dcc[idx].host, 9);
   dcc[i].nick[9] = 0;
   dcc[i].u.chat->away = NULL;
   dcc[i].u.chat->status = STAT_TELNET | STAT_ECHO;
   if (strcmp(dcc[idx].nick, "(bots)") == 0)
      dcc[i].u.chat->status |= STAT_BOTONLY;
   if (strcmp(dcc[idx].nick, "(users)") == 0)
      dcc[i].u.chat->status |= STAT_USRONLY;
   dcc[i].timeval = now;
   dcc[i].u.chat->msgs_per_sec = 0;
   dcc[i].u.chat->con_flags = 0;
   dcc[i].u.chat->buffer = NULL;
   dcc[i].u.chat->max_line = 0;
   dcc[i].u.chat->line_count = 0;
   dcc[i].u.chat->current_lines = 0;
#ifdef NO_IRC
   if (chanset == NULL)
      strcpy(dcc[i].u.chat->con_chan, "*");
   else
      strcpy(dcc[i].u.chat->con_chan, chanset->name);
#else
   strcpy(dcc[i].u.chat->con_chan, chanset->name);
#endif
   dcc[i].u.chat->channel = 0;	/* party line */
   dcc_total++;
   tprintf(dcc[i].sock, "\r\n\r\n");
   telltext(i, "banner", 0);
   if (allow_new_telnets)
      tprintf(dcc[i].sock, "(If you are new, enter 'NEW' here.)\r\n");
   putlog(LOG_MISC, "*", "Telnet connection: %s/%d", s, port);
}

static void eof_dcc_telnet (int idx) {
   putlog(LOG_MISC, "*", "(!) Listening port %d abruptly died.", dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_telnet (int idx,char * buf) {
   strcpy(buf, "lstn");
}

struct dcc_table DCC_TELNET = {
   eof_dcc_telnet,
   dcc_telnet,
   0,
   0,
   display_telnet,
   0,
   0,
   0
};

static void dcc_telnet_id (int idx, char * buf,int atr)
{
   int ok = 0;
   strip_telnet(dcc[idx].sock, buf, &atr);
   buf[10] = 0;
   /* toss out bad nicknames */
   if ((dcc[idx].nick[0] != '@') && (!wild_match(dcc[idx].nick, buf))) {
      tprintf(dcc[idx].sock, "Sorry, this port is busy.\r\n");
      putlog(LOG_BOTS, "*", "Refused %s (bad nick)", buf);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   atr = get_attr_handle(buf);
   /* make sure users-only/bots-only connects are honored */
   if ((dcc[idx].u.chat->status & STAT_BOTONLY) && !(atr & USER_BOT)) {
      tprintf(dcc[idx].sock, "This telnet port is for bots only.\r\n");
      putlog(LOG_BOTS, "*", "Refused %s (non-bot)", buf);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   if ((dcc[idx].u.chat->status & STAT_USRONLY) && (atr & USER_BOT)) {
      tprintf(dcc[idx].sock, "error Only users may connect at this port.\n");
      putlog(LOG_BOTS, "*", "Refused %s (non-user)", buf);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   dcc[idx].u.chat->status &= ~(STAT_BOTONLY | STAT_USRONLY);
   if (op_anywhere(buf)) {
      if (!require_p)
	 ok = 1;
   }
   if (atr & (USER_MASTER | USER_BOTMAST | USER_BOT | USER_PARTY))
      ok = 1;
   if (atr & USER_XFER) {
      module_entry * me = module_find("filesys", 1, 0);
      if (me && me->funcs[FILESYS_ISVALID] && (me->funcs[FILESYS_ISVALID])())
	ok = 1;
   }
#ifdef NO_IRC
   if ((strcasecmp(buf, "NEW") == 0) && ((allow_new_telnets) || (make_userfile))) {
#else
   if ((strcasecmp(buf, "NEW") == 0) && (allow_new_telnets)) {
#endif
      dcc[idx].type = &DCC_TELNET_NEW;
      dcc[idx].timeval = now;
      tprintf(dcc[idx].sock, "\r\n");
      telltext(idx, "newuser", 0);
      tprintf(dcc[idx].sock, "\r\nEnter the nickname you would like to use.\r\n");
      return;
   }
   if (!ok) {
      tprintf(dcc[idx].sock, "You don't have access.\r\n");
      putlog(LOG_BOTS, "*", "Refused %s (invalid handle: %s)",
	     dcc[idx].host, buf);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   if (atr & USER_BOT) {
      if (in_chain(buf)) {
	 tprintf(dcc[idx].sock, "error Already connected.\n");
	 putlog(LOG_BOTS, "*", "Refused telnet connection from %s (duplicate)", buf);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
	 return;
      }
      if (!online) {
	 tprintf(dcc[idx].sock, "error Not accepting links yet.\n");
	 putlog(LOG_BOTS, "*", "Refused telnet connection from %s (premature)", buf);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
	 return;
      }
   }
   /* no password set? */
   if (pass_match_by_handle("-", buf)) {
      if (atr & USER_BOT) {
	 char ps[20];
	 makepass(ps);
	 change_pass_by_handle(buf, ps);
	 correct_handle(buf);
	 strcpy(dcc[idx].nick, buf);
	 nfree(dcc[idx].u.chat);
	 dcc[idx].u.bot = get_data_ptr(sizeof(struct bot_info));
	 dcc[idx].type = &DCC_BOT;
	 dcc[idx].u.bot->status = STAT_CALLED;
	 tprintf(dcc[idx].sock, "*hello!\n");
	 tprintf(dcc[idx].sock, "handshake %s\n", ps);
	 greet_new_bot(idx);
	 return;
      }
      tprintf(dcc[idx].sock, "Can't telnet until you have a password set.\r\n");
      putlog(LOG_MISC, "*", "Refused [%s]%s (no password)", buf, dcc[idx].host);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   ok = 0;
   dcc[idx].type = &DCC_CHAT_PASS;
   dcc[idx].timeval = now;
   if (atr & (USER_MASTER | USER_BOTMAST))
      ok = 1;
   else if (op_anywhere(dcc[idx].nick)) {
      if (!require_p)
	 ok = 1;
      else if (atr & USER_PARTY)
	 ok = 1;
   } else if (atr & USER_PARTY) {
      ok = 1;
      dcc[idx].u.chat->status |= STAT_PARTY;
   }
   if (atr & USER_BOT)
      ok = 1;
   if (!ok) {
      struct chat_info *ci;
      ci = dcc[idx].u.chat;
      dcc[idx].u.file = get_data_ptr(sizeof(struct file_info));
      dcc[idx].u.file->chat = ci;
   }
   correct_handle(buf);
   strcpy(dcc[idx].nick, buf);
   if (atr & USER_BOT)
      tprintf(dcc[idx].sock, "passreq\n");
   else {
      dprintf(idx, "\nEnter your password.\n\377\373\001");
      /* turn off remote telnet echo: IAC WILL ECHO */
   }
}

static void eof_dcc_telnet_id (int idx) {
   putlog(LOG_MISC, "*", "Lost telnet connection to %s/%d", dcc[idx].host,
	  dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}
   
static void timeout_dcc_telnet_id (int idx) {
   dprintf(idx, "Timeout.\n");
   putlog(LOG_MISC, "*", "Ident timeout on telnet: %s", dcc[idx].host);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_telnet_id (int idx,char * buf) {
   sprintf(buf, "t-in  waited %lus", now - dcc[idx].timeval);
}

struct dcc_table DCC_TELNET_ID = {
   eof_dcc_telnet_id,
   dcc_telnet_id,
   &password_timeout,
   timeout_dcc_telnet_id,
   display_dcc_telnet_id,
   expmem_dcc_general,
   kill_dcc_general,
   out_dcc_general
};
   
static void dcc_telnet_new (int idx, char * buf, int x)
{
   int ok = 1;
   buf[9] = 0;
   strip_telnet(dcc[idx].sock, buf, &x);
   strcpy(dcc[idx].nick, buf);
   dcc[idx].timeval = now;
   for (x = 0; x < strlen(buf); x++)
      if ((buf[x] <= 32) || (buf[x] >= 127))
	 ok = 0;
   if (!ok) {
      dprintf(idx, "\nYou can't use weird symbols in your nick.\n");
      dprintf(idx, "Try another one please:\n");
      return;
   }
   if (strchr("-,+*=:!.@#;$", buf[0]) != NULL) {
      dprintf(idx, "\nYou can't start your nick with the character '%c'\n", buf[0]);
      dprintf(idx, "Try another one please:\n");
      return;
   }
   if (is_user(buf)) {
      dprintf(idx, "\nSorry, that nickname is taken already.\n");
      dprintf(idx, "Try another one please:\n");
      return;
   }
   if ((strcasecmp(buf, origbotname) == 0) || (strcasecmp(buf, botnetnick) == 0)) {
      dprintf(idx, "Sorry, can't use my name for a nick.\n");
      return;
   }
#ifdef NO_IRC
   if (make_userfile)
      userlist = adduser(userlist, buf, "none", "-", default_flags | USER_PARTY |
			 USER_MASTER | USER_OWNER);
   else
      userlist = adduser(userlist, buf, "none", "-", USER_PARTY | default_flags);
#else
   userlist = adduser(userlist, buf, "none", "-", USER_PARTY | default_flags);
#endif
   dcc[idx].u.chat->status = STAT_ECHO;
   dcc[idx].type = &DCC_CHAT;	/* just so next line will work */
   check_dcc_attrs(buf, USER_PARTY | default_flags, USER_PARTY | default_flags);
   dcc[idx].type = &DCC_TELNET_PW;
#ifdef NO_IRC
   if (make_userfile) {
      dprintf(idx, "\nYOU ARE THE MASTER/OWNER ON THIS BOT NOW\n");
      telltext(idx, "newbot-limbo", 0);
      putlog(LOG_MISC, "*", "Bot installation complete, first master is %s", buf);
      make_userfile = 0;
      write_userfile();
      add_note(buf, botnetnick, "Welcome to eggdrop! :)", -1, 0);
   }
#endif				/* NO_IRC */
   dprintf(idx, "\nOkay, now choose and enter a password:\n");
   dprintf(idx, "(Only the first 9 letters are significant.)\n");
}

static void dcc_telnet_pw (int idx, char * buf,int x)
{
   char newpass[20];
   int ok;
   strip_telnet(dcc[idx].sock, buf, &x);
   buf[16] = 0;
   ok = 1;
   if (strlen(buf) < 4) {
      dprintf(idx, "\nTry to use at least 4 characters in your password.\n");
      dprintf(idx, "Choose and enter a password:\n");
      return;
   }
   for (x = 0; x < strlen(buf); x++)
      if ((buf[x] <= 32) || (buf[x] == 127))
	 ok = 0;
   if (!ok) {
      dprintf(idx, "\nYou can't use weird symbols in your password.\n");
      dprintf(idx, "Try another one please:\n");
      return;
   }
   putlog(LOG_MISC, "*", "New user via telnet: [%s]%s/%d", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   if (notify_new[0]) {
      char s[121], s1[121], *p1;
      sprintf(s, "Introduced to %s, %s", dcc[idx].nick, dcc[idx].host);
      strcpy(s1, notify_new);
      while (s1[0]) {
	 p1 = strchr(s1, ',');
	 if (p1 != NULL) {
	    *p1 = 0;
	    p1++;
	    rmspace(p1);
	 }
	 rmspace(s1);
	 add_note(s1, botnetnick, s, -1, 0);
	 if (p1 == NULL)
	    s1[0] = 0;
	 else
	    strcpy(s1, p1);
      }
   }
   nsplit(newpass, buf);
   change_pass_by_handle(dcc[idx].nick, newpass);
   dprintf(idx, "\nRemember that!  You'll need it next time you log in.\n");
   dprintf(idx, "You now have an account on %s...\n\n\n", botnetnick);
   dcc[idx].type = &DCC_CHAT;
   dcc_chatter(idx);
}

static void eof_dcc_telnet_new (int idx) {
   putlog(LOG_MISC, "*", "Lost new telnet user (%s/%d)", dcc[idx].host,
	  dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}
   
static void eof_dcc_telnet_pw (int idx) {
   putlog(LOG_MISC, "*", "Lost new telnet user %s (%s/%d)", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   deluser(dcc[idx].nick);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void tout_dcc_telnet_new (int idx) {
   dprintf(idx, "Guess you're not there.  Bye.\n");
   putlog(LOG_MISC, "*", "Timeout on new telnet user: %s/%d", dcc[idx].host,
		   dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void tout_dcc_telnet_pw (int idx) {
   dprintf(idx, "Guess you're not there.  Bye.\n");
   putlog(LOG_MISC, "*", "Timeout on new telnet user: [%s]%s/%d",
	  dcc[idx].nick, dcc[idx].host, dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_telnet_new (int idx,char * buf) {
   sprintf(buf, "new   waited %lus", now - dcc[idx].timeval);
}

static void display_dcc_telnet_pw (int idx,char * buf) {
   sprintf(buf, "newp  waited %lus", now - dcc[idx].timeval);
}

struct dcc_table DCC_TELNET_NEW = {
   eof_dcc_telnet_new,
   dcc_telnet_new,
   &password_timeout,
   tout_dcc_telnet_new,
   display_dcc_telnet_new,
   expmem_dcc_general,
   kill_dcc_general,
   out_dcc_general
};

struct dcc_table DCC_TELNET_PW = {
   eof_dcc_telnet_pw,
   dcc_telnet_pw,
   &password_timeout,
   tout_dcc_telnet_pw,
   display_dcc_telnet_pw,
   expmem_dcc_general,
   kill_dcc_general,
   out_dcc_general
};

static int call_tcl_func (char * name, int idx, char * args)
{
   char s[11];
   set_tcl_vars();
   sprintf(s, "%d", idx);
   Tcl_SetVar(interp, "_n", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   if (Tcl_VarEval(interp, name, " $_n $_a", NULL) == TCL_ERROR) {
      putlog(LOG_MISC, "*", "Tcl error [%s]: %s", name, interp->result);
      return -1;
   }
   return (atoi(interp->result));
}

static void dcc_script (int idx, char * buf,int len)
{
   void *old;
   strip_telnet(dcc[idx].sock, buf, &len);
   if (!len)
      return;
   dcc[idx].timeval = now;
   set_tcl_vars();
   if (call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, buf)) {
      old = dcc[idx].u.script->u.other;
      dcc[idx].type = dcc[idx].u.script->type;
      nfree(dcc[idx].u.script);
      dcc[idx].u.other = old;
      if (dcc[idx].type == &DCC_SOCKET) {
	 /* kill the whole thing off */
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
	 return;
      }
      notes_read(dcc[idx].nick, "", -1, idx);
      if ((dcc[idx].type == &DCC_CHAT) && (dcc[idx].u.chat->channel >= 0)) {
	 chanout2(dcc[idx].u.chat->channel, "%s has joined the party line.\n",
		  dcc[idx].nick);
	 context;
	 if (dcc[idx].u.chat->channel < 10000)
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
		    dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock,
		    dcc[idx].host);
	 check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel,
			geticon(idx), dcc[idx].sock, dcc[idx].host);
      }
   }
}

static void eof_dcc_script (int idx) {
   void *old;
   /* tell the script they're gone: */
   call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, "");
   old = dcc[idx].u.script->u.other;
   dcc[idx].type = dcc[idx].u.script->type;
   nfree(dcc[idx].u.script);
   dcc[idx].u.other = old;
   /* then let it fall thru to the real one */
   eof_dcc(dcc[idx].sock);
}

static void display_dcc_script (int idx, char * buf) {
   sprintf(buf,"scri  %s",dcc[idx].u.script->command);
}

static int expmem_dcc_script (int idx) {
   int tot = sizeof(struct script_info);
   if (dcc[idx].u.script->type == &DCC_CHAT) 
      tot += sizeof(struct chat_info);
   else if (dcc[idx].u.script->type == &DCC_FILES) 
     tot += sizeof(struct file_info) + sizeof(struct chat_info);
   return tot;
}

static void kill_dcc_script (int idx) {
   if (dcc[idx].u.script->type == &DCC_CHAT)
      nfree(dcc[idx].u.script->u.chat);
   else if (dcc[idx].u.script->type == &DCC_FILES) {
      nfree(dcc[idx].u.script->u.file->chat);
      nfree(dcc[idx].u.script->u.file);
   }
   nfree(dcc[idx].u.script);
}

static void out_dcc_script (int idx,char * buf) {
   char * p = buf;
   if (dcc[idx].u.script->type == &DCC_CHAT) {
      if (dcc[idx].u.script->u.chat->status & STAT_TELNET)
	p = add_cr(buf);
   } else if (dcc[idx].u.script->type == &DCC_FILES)
	if (dcc[idx].u.script->u.file->chat->status & STAT_TELNET)
	  p = add_cr(buf);
   tputs(dcc[idx].sock,buf,strlen(buf));
}
   
struct dcc_table DCC_SCRIPT = {   
   eof_dcc_script,
   dcc_script,
   0,
   0,
   display_dcc_script,
   expmem_dcc_script,
   kill_dcc_script,
   out_dcc_script
};
   
static void dcc_socket (int idx,char * buf,int len) {
}
   
static void eof_dcc_socket (int idx) {
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_socket (int idx,char * buf) {
   strcpy(buf,"sock  (stranded)");
}
   
struct dcc_table DCC_SOCKET = {
   eof_dcc_socket,
   dcc_socket,
   0,
   0,
   display_dcc_socket,
   0,
   0,
   0
};

static void display_dcc_lost (int idx,char * buf) {
   strcpy(buf,"lost");
}

struct dcc_table DCC_LOST = {
   0,
   dcc_socket,
   0,
   0,
   display_dcc_lost,
   0,
   0,
   0
};
   

/**********************************************************************/

/* main loop calls here when activity is found on a dcc socket */
void dcc_activity (int z, char * buf, int len)
{
   int idx;
   context;
   for (idx = 0; (dcc[idx].sock != z) && (idx < dcc_total); idx++);
   if (idx >= dcc_total)
      return;
   context;
   if (dcc[idx].type && dcc[idx].type->activity)
     dcc[idx].type->activity(idx,buf,len);
   else 
     putlog(LOG_MISC, "*", "!!! untrapped dcc activity: type %x, sock %d",
	    dcc[idx].type, dcc[idx].sock);
}

/* eof from dcc goes here from I/O... */
void eof_dcc (int z)
{
   int idx;
   for (idx = 0; (dcc[idx].sock != z) || (dcc[idx].type == &DCC_LOST); idx++);
   if (idx >= dcc_total) {
      putlog(LOG_MISC, "*", "(@) EOF socket %d, not a dcc socket, not anything.",
	     z);
      close(z);
      killsock(z);
      return;
   }
   if (dcc[idx].type && dcc[idx].type->eof)
     dcc[idx].type->eof(idx);
   else {
      putlog(LOG_MISC, "*", "*** ATTENTION: DEAD SOCKET (%d) OF TYPE %08X UNTRAPPED",
	     z, dcc[idx].type);
      killsock(z);
      lostdcc(idx);
   }
}

struct dcc_table DCC_FILES = {0,0,0,0,0,0,0,0};
struct dcc_table DCC_FILES_PASS = {0,0,&password_timeout,0,0,0,0,0};
struct dcc_table DCC_FORK_SEND = {0,0,0,0,0,0,0,0};
struct dcc_table DCC_SEND = {0,0,0,0,0,0,0,0};
struct dcc_table DCC_GET = {0,0,0,0,0,0,0,0};
struct dcc_table DCC_GET_PENDING = {0,0,0,0,0,0,0,0};
   
