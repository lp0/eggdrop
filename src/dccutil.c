/*
   dccutil.c -- handles:
   lots of little functions to send formatted text to varying types
   of connections
   '.who', '.whom', and '.dccstat' code
   memory management for dcc structures
   timeout checking for dcc connections

   dprintf'ized, 28aug95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#define _DCCUTIL

#include "main.h"
#include <sys/stat.h>
#include <varargs.h>
#include <errno.h>
#include "chan.h"
#include "modules.h"

extern struct dcc_t * dcc;
extern int dcc_total;
extern char tempdir[];
extern char botname[];
extern char botnetnick[];
extern char ver[];
extern char version[];
extern char os[];
extern char admin[];
extern int serv;
extern struct chanset_t *chanset;
extern time_t trying_server;
extern char botserver[];
extern int botserverport;
extern time_t now;
extern struct userrec * userlist;
extern int max_dcc;

/* file where the motd for dcc chat is stored */
char motdfile[121] = "motd";
/* how long to wait before a server connection times out */
int server_timeout = 15;
/* how long to wait before a telnet connection times out */
int connect_timeout = 15;

extern sock_list * socklist;
extern int MAXSOCKS;

void init_dcc_max () {
   int osock = MAXSOCKS;
   
   if (max_dcc < 1) 
     max_dcc = 1;
   if (dcc) 
      dcc = nrealloc(dcc,sizeof(struct dcc_t) * max_dcc);
   else
      dcc = nmalloc(sizeof(struct dcc_t) * max_dcc);
   MAXSOCKS = max_dcc + 10;
   if (socklist) 
     socklist = (sock_list *)nrealloc((void *)socklist,
				      sizeof(sock_list) * MAXSOCKS);
   else
     socklist = (sock_list *)nmalloc(sizeof(sock_list) * MAXSOCKS);
   for (; osock < MAXSOCKS; osock++)
     socklist[osock].flags = SOCK_UNUSED;
}
 
int expmem_dccutil()
{
   int tot, i;
   context;
   tot = sizeof(struct dcc_t) * max_dcc + sizeof(sock_list) * MAXSOCKS;
   for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && dcc[i].type->expmem)
	tot += dcc[i].type->expmem(i);
   }
   return tot;
}

/* better than tprintf, it can differentiate between socket     *
 * types since you give it a dcc index instead of a socket    *
 * number. In the future, more and more things will call this   *
 * INSTEAD of tprintf. Yes, it's slower and more cpu intensive, *
 * but it does linefeeds correctly for telnet users.            */

static char SBUF[1024];
char * add_cr (char * buf) {	 /* replace \n with \r\n */
static char WBUF[1024];
   char * p,*q;
   for (p = buf, q = WBUF;*p;p++,q++) {
      if (*p == '\n')
	*q++ = '\r';
      *q=*p;
   }
   *q=*p;
   return WBUF;
}

void dprintf(va_alist)
va_dcl
{
   char *format;
   int idx, len;
   va_list va;
   va_start(va);
   idx = va_arg(va, int);
   format = va_arg(va, char *);
   len = vsprintf(SBUF, format, va);
   va_end(va);
   if (idx < 0) {
      tputs(-idx, SBUF, len);
   } else if (idx > 0x7FF0) {
      switch (idx) {
      case DP_LOG:
	 putlog(LOG_MISC, "*", "%s", SBUF);
	 break;
      case DP_STDOUT:
	 tputs(STDOUT, SBUF, len);
	 break;
      case DP_SERVER:
	 mprintf(serv, "%s", SBUF);
	 break;
      case DP_HELP:
	 hprintf(serv, "%s", SBUF);
	 break;
      }
      return;
   } else {
      if (len > 500) {	/* truncate to fit */
	 SBUF[500] = 0;
	 strcat(SBUF+len, "\n");
	 len = 501;
      }
      if (dcc[idx].type && (long)(dcc[idx].type->output) == 1) {
	 char * p = add_cr(SBUF);
	 tputs(dcc[idx].sock, p, strlen(p));
      } else if (dcc[idx].type && dcc[idx].type->output) {
	 dcc[idx].type->output(idx,SBUF);
      } else
	 tputs(dcc[idx].sock, SBUF, len);
   }
}

void qprintf(va_alist)
va_dcl
{
   char *format;
   int idx;
   va_list va;
   va_start(va);
   idx = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(SBUF, format, va);
   if (strlen(SBUF) > 500)
      SBUF[500] = 0;
   /* dummy sentinel for STDOUT */
   if (idx == DP_STDOUT)
      tputs(STDOUT, SBUF, strlen(SBUF));
   else if (idx >= 0)
      tputs(dcc[idx].sock, SBUF, strlen(SBUF));
   va_end(va);
}

void chatout(va_alist)
va_dcl
{
   int i;
   va_list va;
   char *format;
   char s[601];
   va_start(va);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel >= 0)
	    dprintf(i, "%s", s);
   va_end(va);
}

void tandout(va_alist)
va_dcl
{
   int i;
   va_list va;
   char *format;
   char s[601];
   context;
   va_start(va);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT)
	 tputs(dcc[i].sock, s, strlen(s));
   va_end(va);
}

void chanout(va_alist)
va_dcl
{
   int i;
   va_list va;
   char *format;
   int chan;
   char s[601];
   va_start(va);
   chan = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel == chan)
	    dprintf(i, "%s", s);
   va_end(va);
}

/* send notice to channel and other bots */
void chanout2(va_alist)
va_dcl
{
   int i;
   va_list va;
   char *format;
   int chan;
   char s[601], s1[601];
   va_start(va);
   chan = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel == chan)
	    dprintf(i, "*** %s", s);
   sprintf(s1, "chan %s %d %s", botnetnick, chan, s);
   check_tcl_bcst(botnetnick, chan, s);
   context;
   if (chan < 100000)
      for (i = 0; i < dcc_total; i++) {
	 if (dcc[i].type == &DCC_BOT) {
	    tputs(dcc[i].sock, s1, strlen(s1));
	    context;
	 }
      }
   va_end(va);
}

/* print to all but one */
void chatout_but(va_alist)
va_dcl
{
   int i, x;
   va_list va;
   char *format;
   char s[601];
   va_start(va);
   x = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) && (i != x))
	 dprintf(i, "%s", s);
   va_end(va);
}

/* print to all on this channel but one */
void chanout_but(va_alist)
va_dcl
{
   int i, x, chan;
   va_list va;
   char *format;
   char s[601];
   va_start(va);
   x = va_arg(va, int);
   chan = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) && (i != x))
	 if (dcc[i].u.chat->channel == chan)
	    dprintf(i, "%s", s);
   va_end(va);
}

/* ditto for tandem bots */
void tandout_but(va_alist)
va_dcl
{
   int i, x;
   va_list va;
   char *format;
   char s[601];
   va_start(va);
   x = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_BOT) && (i != x))
	 tputs(dcc[i].sock, s, strlen(s));
   va_end(va);
}

/* send notice to channel and other bots */
void chanout2_but(va_alist)
va_dcl
{
   int i, x;
   va_list va;
   char *format;
   int chan;
   char s[601], s1[601];
   va_start(va);
   x = va_arg(va, int);
   chan = va_arg(va, int);
   format = va_arg(va, char *);
   vsprintf(s, format, va);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) && (i != x))
	 if (dcc[i].u.chat->channel == chan)
	    dprintf(i, "*** %s", s);
   sprintf(s1, "chan %s %d %s", botnetnick, chan, s);
   check_tcl_bcst(botnetnick, chan, s);
   if (chan < 100000)
      for (i = 0; i < dcc_total; i++)
	 if ((dcc[i].type == &DCC_BOT) && (i != x))
	    tputs(dcc[i].sock, s1, strlen(s1));
   va_end(va);
}

void dcc_chatter (int idx)
{
   int i, j;
#ifndef NO_IRC
   int atr = get_attr_handle(dcc[idx].nick), chatr, found = 0, find = 0;
   struct chanset_t *chan;
   chan = chanset;
#endif
   dprintf(idx, "Connected to %s, running %s\n", botname, version);
   show_motd(idx);
/*  tell_who  (idx); */
   dprintf(idx, "Commands start with '.' (like '.quit' or '.help')\n");
   dprintf(idx, "Everything else goes out to the party line.\n\n");
   i = dcc[idx].u.chat->channel;
   dcc[idx].u.chat->channel = 234567;
   j = dcc[idx].sock;
#ifndef NO_IRC
   if (atr & (USER_GLOBAL | USER_MASTER | USER_OWNER))
      found = 1;
   if (owner_anywhere(dcc[idx].nick))
      find = CHANUSER_OWNER;
   else if (master_anywhere(dcc[idx].nick))
      find = CHANUSER_MASTER;
   else
      find = CHANUSER_OP;
   while (chan != NULL && found == 0) {
      chatr = get_chanattr_handle(dcc[idx].nick, chan->name);
      if (chatr & find)
	 found = 1;
      else
	 chan = chan->next;
   }
   if (chan == NULL)
      chan = chanset;
   strcpy(dcc[idx].u.chat->con_chan, chan->name);
#endif
   check_tcl_chon(dcc[idx].nick, dcc[idx].sock);
   /* still there? */
   if ((idx >= dcc_total) || (dcc[idx].sock != j))
      return;			/* nope */
   /* tcl script may have taken control */
   if (dcc[idx].type == &DCC_CHAT) {
      if (dcc[idx].u.chat->channel == 234567)
	 dcc[idx].u.chat->channel = i;
      if (dcc[idx].u.chat->channel == 0) {
	 chanout2(0, "%s joined the party line.\n", dcc[idx].nick);
	 context;
      } else if (dcc[idx].u.chat->channel > 0) {
	 chanout2(dcc[idx].u.chat->channel, "%s joined the channel.\n",
		  dcc[idx].nick);
      }
      if (dcc[idx].u.chat->channel >= 0) {
	 context;
	 if (dcc[idx].u.chat->channel < 100000) {
	    context;
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
		    dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock,
		    dcc[idx].host);
	 }
      }
      check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel,
		     geticon(idx), dcc[idx].sock, dcc[idx].host);
      context;
      notes_read(dcc[idx].nick, "", -1, idx);
   }
   touch_laston_handle(userlist,dcc[idx].nick,"partyline",now);
}

/* remove entry from dcc list */
void lostdcc (int n)
{
   if (dcc[n].type && dcc[n].type->kill)
     dcc[n].type->kill(n);
   else if (dcc[n].u.other)
     nfree(dcc[n].u.other);
   dcc_total--;
   if (n < dcc_total) {
      strcpy(dcc[n].nick, dcc[dcc_total].nick);
      strcpy(dcc[n].host, dcc[dcc_total].host);
      dcc[n].sock = dcc[dcc_total].sock;
      dcc[n].addr = dcc[dcc_total].addr;
      dcc[n].port = dcc[dcc_total].port;
      dcc[n].type = dcc[dcc_total].type;
      dcc[n].u.other = dcc[dcc_total].u.other;
   }
}

/* show list of current dcc's to a dcc-chatter */
/* positive value: idx given -- negative value: sock given */
void tell_dcc (int zidx)
{
   int i;
   char s[20], other[160];
   if (zidx < 0) {
      tprintf(-zidx, "SOCK ADDR     PORT  NICK      HOST              TYPE\n");
      tprintf(-zidx, "---- -------- ----- --------- ----------------- ----\n");
   } else {
      dprintf(zidx, "SOCK ADDR     PORT  NICK      HOST              TYPE\n");
      dprintf(zidx, "---- -------- ----- --------- ----------------- ----\n");
   }
   /* show server */
   if (strlen(botserver) > 17)
      strcpy(s, &botserver[strlen(botserver) - 17]);
   else
      strcpy(s, botserver);
   if (zidx < 0) {
      tprintf(-zidx, "%-4d 00000000 %5d (server)  %-17s %s\n", serv, botserverport,
	      s, trying_server ? "conn" : "serv");
   } else {
      dprintf(zidx, "%-4d 00000000 %5d (server)  %-17s %s\n", serv, botserverport,
	      s, trying_server ? "conn" : "serv");
   }
   for (i = 0; i < dcc_total; i++) {
      if (strlen(dcc[i].host) > 17)
	 strcpy(s, &dcc[i].host[strlen(dcc[i].host) - 17]);
      else
	 strcpy(s, dcc[i].host);
      if (dcc[i].type && dcc[i].type->display)
	dcc[i].type->display(i,other);
      else {
	 sprintf(other, "?:%lX  !! ERROR !!",(long)dcc[i].type);
	 break;
      }
      if (zidx < 0)
	 tprintf(-zidx, "%-4d %08X %5d %-9s %-17s %s\n", dcc[i].sock,
		 dcc[i].addr, dcc[i].port, dcc[i].nick, s, other);
      else
	 dprintf(zidx, "%-4d %08X %5d %-9s %-17s %s\n", dcc[i].sock,
		 dcc[i].addr, dcc[i].port, dcc[i].nick, s, other);
   }
}

/* mark someone on dcc chat as no longer away */
void not_away (int idx)
{
   if (dcc[idx].u.chat->away == NULL) {
      dprintf(idx, "You weren't away!\n");
      return;
   }
   if (dcc[idx].u.chat->channel >= 0) {
      chanout2(dcc[idx].u.chat->channel, "%s is no longer away.\n", dcc[idx].nick);
      context;
      if (dcc[idx].u.chat->channel < 100000)
	 tandout("unaway %s %d\n", botnetnick, dcc[idx].sock);
   }
   dprintf(idx, "You're not away any more.\n");
   nfree(dcc[idx].u.chat->away);
   dcc[idx].u.chat->away = NULL;
   notes_read(dcc[idx].nick, "", -1, idx);
}

void set_away (int idx, char * s)
{
   if (s == NULL) {
      not_away(idx);
      return;
   }
   if (!s[0]) {
      not_away(idx);
      return;
   }
   if (dcc[idx].u.chat->away != NULL)
      nfree(dcc[idx].u.chat->away);
   dcc[idx].u.chat->away = (char *) nmalloc(strlen(s) + 1);
   strcpy(dcc[idx].u.chat->away, s);
   if (dcc[idx].u.chat->channel >= 0) {
      chanout2(dcc[idx].u.chat->channel, "%s is now away: %s\n",
	       dcc[idx].nick, s);
      context;
      if (dcc[idx].u.chat->channel < 100000)
	 tandout("away %s %d %s\n", botnetnick, dcc[idx].sock, s);
   }
   dprintf(idx, "You are now away; notes will be stored.\n");
}

/* this helps the memory debugging */
void * get_data_ptr (int size) {
   return nmalloc(size);
}
/* make a password, 10-15 random letters and digits */
void makepass (char * s)
{
   int i, j;
   i = 10 + (random() % 6);
   for (j = 0; j < i; j++) {
      if (random() % 3 == 0)
	 s[j] = '0' + (random() % 10);
      else
	 s[j] = 'a' + (random() % 26);
   }
   s[i] = 0;
}

void flush_lines (int idx)
{
   struct chat_info *ci = (dcc[idx].type == &DCC_CHAT) ? dcc[idx].u.chat :
   dcc[idx].u.file->chat;
   int c = ci->line_count;
   struct eggqueue *p = ci->buffer, *o;
   while (p && c < (ci->max_line)) {
      ci->current_lines--;
      tputs(dcc[idx].sock, p->item, p->stamp);
      nfree(p->item);
      o = p->next;
      nfree(p);
      p = o;
      c++;
   }
   if (p != NULL) {
      if (dcc[idx].u.chat->status & STAT_TELNET)
	tputs(dcc[idx].sock, "[More]: ", 8);
      else
	tputs(dcc[idx].sock, "[More]\n", 7);
   }
   ci->buffer = p;
   ci->line_count = 0;
}

int new_dcc (struct dcc_table * type,int xtra_size)
{
   int i = dcc_total;
   if (dcc_total == max_dcc)
      return -1;
   dcc_total++;
   bzero((char *)&dcc[i], sizeof(struct dcc_t));
   dcc[i].type = type;
   if (xtra_size)
      dcc[i].u.other = nmalloc(xtra_size);
   return i;
}
