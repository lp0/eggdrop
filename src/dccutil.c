/*
   dccutil.c -- handles:
   lots of little functions to send formatted text to varyinlg types
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

#include "main.h"
#include <sys/stat.h>
#include <varargs.h>
#include <errno.h>
#include "chan.h"
#include "modules.h"
#include "tandem.h"

extern struct dcc_t * dcc;
extern int dcc_total;
extern char tempdir[];
extern char botname[];
extern char botnetnick[];
extern char ver[], spaces[];
extern char version[];
extern char admin[];
extern int serv;
extern struct chanset_t *chanset;
extern time_t trying_server;extern char botserver[];
extern int botserverport;
extern time_t now;
extern struct userrec * userlist;
extern int max_dcc;
extern int dcc_flood_thr;
extern int backgrd;

/* file where the motd for dcc chat is stored */
char motdfile[121] = "motd";
/* how long to wait before a telnet connection times out */
int connect_timeout = 15;
int reserved_port = 0;

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
	tot += dcc[i].type->expmem(dcc[i].u.other);
   }
   return tot;
}

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

extern void (*qserver)(int, char *, int);
void dprintf(va_alist)
va_dcl
{
   char *format;
   int idx, len;
   
   va_list va;
   va_start(va);
   idx = va_arg(va, int);
   format = va_arg(va, char *);
#ifdef HAVE_VSNPRINTF
   if ((len = vsnprintf(SBUF, 1023, format, va)) < 0)
     SBUF[len = 1023] = 0;
#else
   len = vsprintf(SBUF, format, va);
#endif
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
      case DP_STDERR:
	 tputs(STDERR, SBUF, len);
	 break;
       case DP_SERVER:
       case DP_HELP:
       case DP_MODE:
	 qserver(idx, SBUF,len);
	 break;
      }
      return;
   } else {
      if (len > 500) {	/* truncate to fit */
	 SBUF[500] = 0;
	 strcat(SBUF+len, "\n");
	 len = 501;
      }
      if (dcc[idx].type && ((long)(dcc[idx].type->output) == 1)) {
	 char * p = add_cr(SBUF);
	 tputs(dcc[idx].sock, p, strlen(p));
      } else if (dcc[idx].type && dcc[idx].type->output) {
	 dcc[idx].type->output(idx,SBUF,dcc[idx].u.other);
      } else
	tputs(dcc[idx].sock, SBUF, len);
   }
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
#ifdef HAVE_VSNPRINTF
   if (vsnprintf(s, 511, format, va) < 0)
     s[511] = 0;
#else
   vsprintf(s, format, va);
#endif
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel >= 0)
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
#ifdef HAVE_VSNPRINTF
   if (vsnprintf(s, 511, format, va) < 0)
     s[511] = 0;
#else
   vsprintf(s, format, va);
#endif
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) && (i != x))
	 if (dcc[i].u.chat->channel == chan)
	    dprintf(i, "%s", s);
   va_end(va);
}

void dcc_chatter (int idx)
{
   int i, j;
   struct flag_record fr = {FR_GLOBAL|FR_CHAN|FR_ANYWH,0,0,0,0,0};
   
   get_user_flagrec(dcc[idx].user,&fr,NULL);
   dprintf(idx, "Connected to %s, running %s\n", botnetnick, version);
   show_motd(idx);
   dprintf(idx, "Commands start with '.' (like '.quit' or '.help')\n");
   dprintf(idx, "Everything else goes out to the party line.\n\n");
   i = dcc[idx].u.chat->channel;
   dcc[idx].u.chat->channel = 234567;
   j = dcc[idx].sock;
   strcpy(dcc[idx].u.chat->con_chan,"***");
   check_tcl_chon(dcc[idx].nick, dcc[idx].sock);
   /* still there? */
   if ((idx >= dcc_total) || (dcc[idx].sock != j))
      return;			/* nope */
   /* tcl script may have taken control */
   if (dcc[idx].type == &DCC_CHAT) {
      if (!strcmp(dcc[idx].u.chat->con_chan,"***"))
	strcpy(dcc[idx].u.chat->con_chan,"*");
      if (dcc[idx].u.chat->channel == 234567) {
	 /* if the chat channel has already been altered it's *highly*
	  * probably join/part messages have been broadcast everywhere,
	  * so dont bother sending them */
	 dcc[idx].u.chat->channel = i;
	 if (dcc[idx].u.chat->channel >= 0) {
	    context;
	    if (dcc[idx].u.chat->channel < 100000) {
	       botnet_send_join_idx(idx, -1);
	    }
	 }
	 check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel,
			geticon(idx), dcc[idx].sock, dcc[idx].host);
      }
      /* but *do* bother with sending it locally */
      if (dcc[idx].u.chat->channel == 0) {
	 chanout_but(-1,0, "*** %s joined the party line.\n", dcc[idx].nick);
      } else if (dcc[idx].u.chat->channel > 0) {
	 chanout_but(-1,dcc[idx].u.chat->channel, 
		     "*** %s joined the channel.\n",
		     dcc[idx].nick);
      }
   }
}

/* remove entry from dcc list */
void lostdcc (int n)
{
   if (dcc[n].type && dcc[n].type->kill)
     dcc[n].type->kill(n,dcc[n].u.other);
   else if (dcc[n].u.other)
     nfree(dcc[n].u.other);
   dcc_total--;
   if (n < dcc_total) 
     memcpy(&dcc[n],&dcc[dcc_total],sizeof(struct dcc_t));
}

/* show list of current dcc's to a dcc-chatter */
/* positive value: idx given -- negative value: sock given */
void tell_dcc (int zidx)
{
   int i,j,k;
   char other[160];
   context;
   spaces[HANDLEN-9]=0;
   dprintf(zidx, "SOCK ADDR     PORT  NICK     %s HOST              TYPE\n"
	   ,spaces);
   dprintf(zidx, "---- -------- ----- ---------%s ----------------- ----\n"
	   ,spaces);
   spaces[HANDLEN-9]=' ';
   /* show server */
   for (i = 0; i < dcc_total; i++) {
      j = strlen(dcc[i].host);
      if (j > 17)
	j -= 17;
      else
	j = 0;
      if (dcc[i].type && dcc[i].type->display)
	dcc[i].type->display(i,other);
      else {
	 sprintf(other, "?:%lX  !! ERROR !!",(long)dcc[i].type);
	 break;
      }
      k = HANDLEN-strlen(dcc[i].nick);
      spaces[k]=0;
      dprintf(zidx, "%-4d %08X %5d %s%s %-17s %s\n", dcc[i].sock,
	      dcc[i].addr, dcc[i].port, dcc[i].nick, spaces,
	      dcc[i].host + j, other);
      spaces[k]=' ';
   }
}

/* mark someone on dcc chat as no longer away */
void not_away (int idx)
{
   context;
   if (dcc[idx].u.chat->away == NULL) {
      dprintf(idx, "You weren't away!\n");
      return;
   }
   if (dcc[idx].u.chat->channel >= 0) {
      chanout_but(-1,dcc[idx].u.chat->channel,
		  "*** %s is no longer away.\n", dcc[idx].nick);
      context;
      if (dcc[idx].u.chat->channel < 100000) {
	 botnet_send_away(-1,botnetnick,dcc[idx].sock,NULL,idx);
      }
   }
   dprintf(idx, "You're not away any more.\n");
   nfree(dcc[idx].u.chat->away);
   dcc[idx].u.chat->away = NULL;
   check_tcl_away(botnetnick,dcc[idx].sock,NULL);
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
      chanout_but(-1,dcc[idx].u.chat->channel, 
		  "*** %s is now away: %s\n",
		  dcc[idx].nick, s);
      context;
      if (dcc[idx].u.chat->channel < 100000) {
	 botnet_send_away(-1,botnetnick,dcc[idx].sock,s,idx);
      }
   }
   dprintf(idx, "You are now away.\n");
   check_tcl_away(botnetnick,dcc[idx].sock,s);
}

/* this helps the memory debugging */
void * _get_data_ptr (int size, char * file, int line) {
   char * p;
#ifdef EBUG_MEM
   char x[1024];
   simple_sprintf(x,"dccutil.c:%s",file);
   p = n_malloc(size,x,line);
#else
   p = nmalloc(size);
#endif
   bzero(p,size);
   return p;
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

void flush_lines (int idx,struct chat_info *ci)
{
   int c = ci->line_count;
   struct msgq *p = ci->buffer, *o;
   while (p && c < (ci->max_line)) {
      ci->current_lines--;
      tputs(dcc[idx].sock, p->msg, p->len);
      nfree(p->msg);
      o = p->next;
      nfree(p);
      p = o;
      c++;
   }
   if (p != NULL) {
      if (dcc[idx].status & STAT_TELNET)
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
   if (xtra_size) {
      dcc[i].u.other = nmalloc(xtra_size);
      bzero(dcc[i].u.other,xtra_size);
   }
   return i;

}

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
	 /* evil assumption here that flags&DCT_CHAT implies chat type */
	 if ((dcc[idx].type->flags & DCT_CHAT)
	     && chat && (chat->channel >= 0)) {
	    char x[1024];
	    simple_sprintf(x,DCC_FLOODBOOT, dcc[idx].nick);
	    chanout_but(idx, chat->channel, "*** %s", x);
	    if (chat->channel < 100000)
	      botnet_send_part_idx(idx, x);
	 }
	 check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
	 if ((dcc[idx].sock != STDOUT) || backgrd) {
	    killsock(dcc[idx].sock);
	    lostdcc(idx);
	 } else {
	    dprintf(DP_STDOUT, "\n### SIMULATION RESET ###\n\n");
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
   int files = (dcc[idx].type != &DCC_CHAT);
   dprintf(idx, DCC_BOOTED1);
   dprintf(idx, DCC_BOOTED2, DCC_BOOTED2_ARGS);
   /* if it's a partyliner (chatterer :) */
   /* horrible assumption that DCT_CHAT using structure uses same format
    * as DCC_CHAT */
   if ((dcc[idx].type->flags & DCT_CHAT)
       && (dcc[idx].u.chat->channel >= 0)) {
      char x[1024];
      simple_sprintf(x,"%s booted %s from the party line%s%s",
		     by, dcc[idx].nick,
		     reason[0] ? ": " : "", reason);
      chanout_but(idx, dcc[idx].u.chat->channel, "*** %s.\n", x);
      if (dcc[idx].u.chat->channel < 100000)
	botnet_send_part_idx(idx, x);
   }
   check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
   if ((dcc[idx].sock != STDOUT) || backgrd) {
      killsock(dcc[idx].sock);
      dcc[idx].sock = (long)dcc[idx].type;
      dcc[idx].type = &DCC_LOST;
      /* entry must remain in the table so it can be logged by the caller */
   } else {
      dprintf(DP_STDOUT, "\n### SIMULATION RESET\n\n");
      dcc_chatter(idx);
   }
   return;
}

