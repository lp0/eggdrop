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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <varargs.h>
#include <errno.h>
#include "eggdrop.h"
#include "chan.h"
#include "proto.h"
#ifdef MODULES
#include "modules.h"
#else
extern int wait_dcc_xfer;
#endif

extern struct dcc_t dcc[];
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

/* file where the motd for dcc chat is stored */
char motdfile[121]="motd";
/* how long to wait before a server connection times out */
int server_timeout=15;
/* how long to wait before a telnet connection times out */
int connect_timeout=15;

int expmem_dccutil()
{
  int tot,i,j;
  context;
  tot=0;
  for (i=0; i<dcc_total; i++) {
    j=dcc[i].type;
    if ((j==DCC_CHAT) || (j==DCC_CHAT_PASS) || (j==DCC_TELNET_ID) ||
	(j==DCC_TELNET_NEW) || (j==DCC_TELNET_PW)) {
      tot+=sizeof(struct chat_info);
      if (dcc[i].u.chat->away!=NULL) tot+=strlen(dcc[i].u.chat->away)+1;
      if (dcc[i].u.chat->buffer) {
	struct eggqueue *p=dcc[i].u.chat->buffer;
	while (p!=NULL) {
	  tot+=sizeof(struct eggqueue);
	  tot+=strlen(p->item);
	  p=p->next;
	}
      }
    }
    if ((j==DCC_FILES) || (j==DCC_FILES_PASS)) {
      tot+=sizeof(struct file_info)+sizeof(struct chat_info);
      if (dcc[i].u.file->chat->away!=NULL)
        tot+=strlen(dcc[i].u.file->chat->away)+1;
    }
    if ((j==DCC_SEND) || (j==DCC_GET) || (j==DCC_GET_PENDING))
      tot+=sizeof(struct xfer_info);
    if ((j==DCC_BOT) || (j==DCC_BOT_NEW))
      tot+=sizeof(struct bot_info);
    if ((j==DCC_RELAY) || (j==DCC_RELAYING)) {
      tot+=sizeof(struct relay_info)+sizeof(struct chat_info);
      if (dcc[i].u.relay->chat->away!=NULL)
        tot+=strlen(dcc[i].u.relay->chat->away)+1;
    }
    if (j==DCC_FORK) {
      tot+=sizeof(struct fork_info);
      switch(dcc[i].u.fork->type) {
      case DCC_FORK:
      case DCC_CHAT: tot+=sizeof(struct chat_info); break;
      case DCC_FILES: tot+=sizeof(struct file_info)+sizeof(struct chat_info);
	break;
      case DCC_SEND: tot+=sizeof(struct xfer_info); break;
      case DCC_BOT: tot+=sizeof(struct bot_info); break;
      case DCC_RELAY: tot+=sizeof(struct relay_info)+sizeof(struct chat_info);
	break;
      case DCC_RELAYING: tot+=sizeof(struct relay_info)+sizeof(struct chat_info);
	break;
      }
    }
    if (j==DCC_SCRIPT) {
      tot+=sizeof(struct script_info);
      switch(dcc[i].u.script->type) {
      case DCC_CHAT: tot+=sizeof(struct chat_info); break;
      case DCC_FILES: tot+=sizeof(struct file_info)+sizeof(struct chat_info);
	break;
      }
    }
  }
  return tot;
}

/* Remove the color control codes that mIRC,pIRCh etc use to make    *
 * their client seem so fecking cool! (Sorry, Khaled, you are a nice *
 * guy, but when you added this feature you forced people to either  *
 * use your *SHAREWARE* client or face screenfulls of crap!)	     */

void strip_mirc_codes PROTO2(int,flags, char *,text)
{
  char *vv;
  while (*text) {
    switch (*text) {
    case 2:				/* Bold text */
      if (flags&STRIP_BOLD) strcpy(text,text+1);
      else text++;
      break;
    case 3:				/* mIRC colors? */
      if (flags&STRIP_COLOR) {
        if (isdigit(text[1])) {		/* Is the first char a number? */
          vv=text + 2;			/* Skip over the ^C and the first digit */
          if (isdigit(*vv)) vv++;       /* Is this a double digit number? */
          if (*vv==',') {		/* Do we have a background color next? */
            if (isdigit(vv[1])) vv+=2;  /* Skip over the first background digit */
            if (isdigit(*vv)) vv++;     /* Is it a double digit? */
          }
          strcpy(text,vv);		/* Thrap all over the text!! */
        }
        else strcpy(text,text+1);       /* Not a real color code? kill it anyway! */
      }
      else text++;
      break;
    case 0x16:				/* Reverse video */
      if (flags&STRIP_REV) strcpy(text,text+1);
      else text++;
      break;
    case 0x1f:				/* Underlined text */
      if (flags&STRIP_UNDER) strcpy(text,text+1);
      else text++;
      break;
    case 033:
       context;
      if (flags&STRIP_ANSI) {
       context;
	vv=text+1;
       context;
	if (*vv == '[') {
       context;
	  vv++; 
       context;
	  while ((*vv == ';') || ((*vv >= '0') && (*vv <= '9')))
	    vv++;
       context;
	  if (*vv)
	    vv++; /* also kill the following char */
       context;
	}
       context;
	strcpy(text,vv);
       context;
      } else 
	text++;
      break;
    default:
      text++;				/* Move on to the next char */
    }
  }
}

/* better than tprintf, it can differentiate between socket	*
 * types since you give it a dcc index instead of a socket	*
 * number. In the future, more and more things will call this	*
 * INSTEAD of tprintf. Yes, it's slower and more cpu intensive,	*
 * but it does linefeeds correctly for telnet users.		*/

static char SBUF[1024];
void dprintf(va_alist)
va_dcl
{
  char *format,*p,*q; int idx,cr; va_list va;
  va_start(va);
  idx=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(SBUF,format,va);
  va_end(va);
  cr=0;  /* telnet people get a special linefeed type -- !!!!UGH!!!! */
  if (idx<0) {
     tputs(-idx,SBUF,strlen(SBUF));
  } else if (idx > 0x7FF0) {
     switch (idx) {
      case DP_LOG:
	putlog(LOG_MISC,"*","%s",SBUF);
	break;
      case DP_STDOUT:
	tputs(STDOUT,SBUF,strlen(SBUF));
	break;
      case DP_SERVER:
	mprintf(serv,"%s",SBUF);
	break;
      case DP_HELP:
	hprintf(serv,"%s",SBUF);
	break;
     }
     return;
  } else switch (dcc[idx].type) {
  case DCC_CHAT:
    strip_mirc_codes(dcc[idx].u.chat->strip_flags,SBUF);
  case DCC_CHAT_PASS:
  case DCC_TELNET_ID:
  case DCC_TELNET_NEW:
  case DCC_TELNET_PW:
    if (dcc[idx].u.chat->status&STAT_TELNET) cr=1;
    break;
  case DCC_FILES:
  case DCC_FILES_PASS:
    if (dcc[idx].u.file->chat->status&STAT_TELNET) cr=1;
    break;
  case DCC_RELAYING:
    if (dcc[idx].u.relay->chat->status&STAT_TELNET) cr=1;
    break;
  case DCC_SCRIPT:
    if (dcc[idx].u.script->type==DCC_FILES) {
      if (dcc[idx].u.script->u.file->chat->status&STAT_TELNET) cr=1;
    }
    else if (dcc[idx].u.script->type==DCC_CHAT) {
      if (dcc[idx].u.script->u.chat->status&STAT_TELNET) cr=1;
    }
    break;
  case DCC_BOT:
    tputs(dcc[idx].sock,SBUF,strlen(SBUF));
    idx = -1;
    break;
  }
  if (idx >= 0) {
     if (cr) {
	/* replace \n with \r\n */
	for (p=SBUF; *p!=0; p++) if (*p=='\n') {
	   *p++='\r'; q=nmalloc(strlen(p)+1);
	   strcpy(q,p); *p++='\n';
	   strcpy(p,q); nfree(q); p--;
	}
     }
     if (strlen(SBUF)>500) {    /* truncate to fit */
	SBUF[500]=0;
	if (cr) strcat(SBUF,"\r\n"); 
	else strcat(SBUF,"\n");
     }
     /* dummy sentinel for STDOUT */
     if ((dcc[idx].type==DCC_CHAT) && 
	 (dcc[idx].u.chat->status & STAT_PAGE)) {
	append_line(idx,SBUF);
	return;
     }
     tputs(dcc[idx].sock,SBUF,strlen(SBUF));
  }
}

void qprintf(va_alist)
va_dcl
{
  char *format; int idx; va_list va;
  va_start(va);
  idx=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(SBUF,format,va);
  if (strlen(SBUF)>500) SBUF[500]=0;
  /* dummy sentinel for STDOUT */
  if (idx==DP_STDOUT) tputs(STDOUT,SBUF,strlen(SBUF));
  else if (idx>=0) tputs(dcc[idx].sock,SBUF,strlen(SBUF));
  va_end(va);
}

void chatout(va_alist)
va_dcl
{
  int i; va_list va; char *format; char s[601];
  va_start(va); format=va_arg(va,char *); vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel>=0) dprintf(i,"%s",s);
  va_end(va);
}

void tandout(va_alist)
va_dcl
{
  int i; va_list va; char *format; char s[601];
  context;
  va_start(va); format=va_arg(va,char *); vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_BOT)
    tputs(dcc[i].sock,s,strlen(s));
  va_end(va);
}

void chanout(va_alist)
va_dcl
{
  int i; va_list va; char *format; int chan; char s[601];
  va_start(va); chan=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel==chan) dprintf(i,"%s",s);
  va_end(va);
}

/* send notice to channel and other bots */
void chanout2(va_alist)
va_dcl
{
  int i; va_list va; char *format; int chan; char s[601],s1[601];
  va_start(va); chan=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel==chan) dprintf(i,"*** %s",s);
  sprintf(s1,"chan %s %d %s",botnetnick,chan,s);
  check_tcl_bcst(botnetnick,chan,s);
  context;
  if (chan<100000) 
  for (i=0; i<dcc_total; i++) {
    if (dcc[i].type==DCC_BOT) {
      tputs(dcc[i].sock,s1,strlen(s1));
      context;
    }
  }
  va_end(va);
}
    
void shareout(va_alist)
va_dcl
{
  int i; va_list va; char *format; char s[601];
  va_start(va); format=va_arg(va,char *); vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) 
    if ((dcc[i].type==DCC_BOT) && 
	(dcc[i].u.bot->status&STAT_SHARE) &&
	(!(dcc[i].u.bot->status&STAT_GETTING)) &&
	(!(dcc[i].u.bot->status&STAT_SENDING))) 
      tputs(dcc[i].sock,s,strlen(s));
  q_resync(s);
  va_end(va);
}

void shareout_but(va_alist)
va_dcl
{
  int i,x; va_list va; char *format; char s[601];
  va_start(va); x=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_BOT) && (i!=x) &&
	(dcc[i].u.bot->status&STAT_SHARE) &&
	(!(dcc[i].u.bot->status&STAT_GETTING)) &&
	(!(dcc[i].u.bot->status&STAT_SENDING)))
      tputs(dcc[i].sock,s,strlen(s));
  q_resync(s);
  va_end(va);
}

/* print to all but one */
void chatout_but(va_alist)
va_dcl
{
  int i,x; va_list va; char *format; char s[601];
  va_start(va); x=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) 
    if ((dcc[i].type==DCC_CHAT) && (i!=x)) dprintf(i,"%s",s);
  va_end(va);
}

/* print to all on this channel but one */
void chanout_but(va_alist)
va_dcl
{
  int i,x,chan; va_list va; char *format; char s[601];
  va_start(va); x=va_arg(va,int); chan=va_arg(va,int);
  format=va_arg(va,char *); vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) 
    if ((dcc[i].type==DCC_CHAT) && (i!=x))
      if (dcc[i].u.chat->channel==chan) dprintf(i,"%s",s);
  va_end(va);
}

/* ditto for tandem bots */
void tandout_but(va_alist)
va_dcl
{
  int i,x; va_list va; char *format; char s[601];
  va_start(va); x=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) 
    if ((dcc[i].type==DCC_BOT) && (i!=x)) tputs(dcc[i].sock,s,strlen(s));
  va_end(va);
}

/* send notice to channel and other bots */
void chanout2_but(va_alist)
va_dcl
{
  int i,x; va_list va; char *format; int chan; char s[601],s1[601];
  va_start(va); x=va_arg(va,int); chan=va_arg(va,int);
  format=va_arg(va,char *); vsprintf(s,format,va);
  for (i=0; i<dcc_total; i++) if ((dcc[i].type==DCC_CHAT) && (i!=x))
    if (dcc[i].u.chat->channel==chan) dprintf(i,"*** %s",s);
  sprintf(s1,"chan %s %d %s",botnetnick,chan,s);
  check_tcl_bcst(botnetnick,chan,s);
  if (chan<100000)
    for (i=0; i<dcc_total; i++)
      if ((dcc[i].type==DCC_BOT) && (i!=x)) tputs(dcc[i].sock,s1,strlen(s1));
  va_end(va);
}
    
void remote_tell_info PROTO2(int,z,char *,nick)
{
  char s[256],*p,*q; struct chanset_t *chan;
  p=(char *)nmalloc(11); strcpy(p,"Channels: ");
  chan=chanset; while (chan!=NULL) {
    if (!(chan->stat & CHAN_SECRET)) {
      strcpy(s,chan->name); strcat(s,", ");
      q=(char *)nmalloc(strlen(p)+strlen(s)+1);
      strcpy(q,p); strcat(q,s); nfree(p); p=q;
    }
    chan=chan->next;
  }
  if (strlen(p)>10) {
    p[strlen(p)-2]=0;
    tprintf(z,"priv %s %s %s  (%s)\n",botnetnick,nick,p,ver);
  }
  else tprintf(z,"priv %s %s No channels.  (%s)\n",botnetnick,nick,ver);
  nfree(p);
  if (admin[0])
    tprintf(z,"priv %s %s Admin: %s\n",botnetnick,nick,admin);
}

void remote_tell_who PROTO3(int,z,char *,nick,int,chan)
{
  int i,k,ok=0; char s[121]; time_t tt;
  tt=time(NULL);
  remote_tell_info(z,nick);
  if (chan==0)
    tprintf(z,"priv %s %s Party line members:  (* = owner, + = master, @ = op)\n",
	    botnetnick,nick);
  else {
    char *cname=get_assoc_name(chan);
    if (cname==NULL)
      tprintf(z,"priv %s %s People on channel %s%d:  (* = owner, + = master, @ = op)\n",
	      botnetnick,nick,(chan<100000)?"":"*",chan%100000);
    else
      tprintf(z,"priv %s %s People on channel '%s' (%s%d):  (* = owner, + = master, @ = op)\n",
	      botnetnick,nick,cname,(chan<100000)?"":"*",chan%100000);
  }
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel==chan) {
      sprintf(s,"  %c%-10s %s",(geticon(i)=='-'?' ':geticon(i)),
	      dcc[i].nick,dcc[i].host);
      if (tt - dcc[i].u.chat->timer > 300) {
	unsigned long days,hrs,mins;
	days=(tt-dcc[i].u.chat->timer)/86400;
	hrs=((tt-dcc[i].u.chat->timer) - (days*86400))/3600;
	mins=((tt-dcc[i].u.chat->timer) - (hrs*3600))/60;
	if (days>0) sprintf(&s[strlen(s)]," (idle %lud%luh)",days,hrs);
	else if (hrs>0) sprintf(&s[strlen(s)]," (idle %luh%lum)",hrs,mins);
	else sprintf(&s[strlen(s)]," (idle %lum)",mins);
      }
      tprintf(z,"priv %s %s %s\n",botnetnick,nick,s);
      if (dcc[i].u.chat->away!=NULL) 
	tprintf(z,"priv %s %s       AWAY: %s\n",botnetnick,nick,
		dcc[i].u.chat->away);
    }
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_BOT) {
    if (!ok) {
      ok=1;
      tprintf(z,"priv %s %s Bots connected:\n",botnetnick,nick);
    }
    tprintf(z,"priv %s %s   %s%c%-10s %s\n",botnetnick,nick,
	    dcc[i].u.bot->status&STAT_CALLED?"<-":"->",
	    dcc[i].u.bot->status&STAT_SHARE?'+':' ',
	    dcc[i].nick,dcc[i].u.bot->version);
  }
  ok=0;
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel!=chan) {
      if (!ok) {
	ok=1;
	tprintf(z,"priv %s %s Other people on the bot:\n",botnetnick,nick);
      }
      sprintf(s,"  %c%-10s %s",(geticon(i)=='-'?' ':geticon(i)),
	      dcc[i].nick,dcc[i].host);
      if (tt - dcc[i].u.chat->timer > 300) {
	k=(tt-dcc[i].u.chat->timer)/60;
	if (k<60) sprintf(&s[strlen(s)]," (idle %dm)",k);
	else sprintf(&s[strlen(s)]," (idle %dh%dm)",k/60,k%60);
      }
      tprintf(z,"priv %s %s %s\n",botnetnick,nick,s);
      if (dcc[i].u.chat->away!=NULL)
	tprintf(z,"priv %s %s       AWAY: %s\n",botnetnick,nick,
		dcc[i].u.chat->away);
    }
}

void tell_who PROTO2(int,idx,int,chan)
{
  int i,k,ok=0,atr; char s[121]; time_t tt;
  atr=get_attr_handle(dcc[idx].nick);
  tt=time(NULL);
  if (chan==0)
    dprintf(idx,"Party line members:  (* = owner, + = master, @ = op)\n");
  else {
    char *cname=get_assoc_name(chan);
    if (cname==NULL)
      dprintf(idx,"People on channel %s%d:  (* = owner, + = master, @ = op)\n",
	      (chan<100000)?"":"*",chan%100000);
    else
      dprintf(idx,"People on channel '%s' (%s%d):  (* = owner, + = master, @ = op)\n",
	      cname,(chan<100000)?"":"*",chan%100000);
  }
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_CHAT)
    if (dcc[i].u.chat->channel==chan) {
      sprintf(s,"  %c%-10s %s",(geticon(i)=='-'?' ':geticon(i)),
	      dcc[i].nick,dcc[i].host);
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->con_flags)
	  sprintf(&s[strlen(s)]," (con:%s)",masktype(dcc[i].u.chat->con_flags));
      }
      if (tt - dcc[i].u.chat->timer > 300) {
	unsigned long days,hrs,mins;
	days=(tt-dcc[i].u.chat->timer)/86400;
	hrs=((tt-dcc[i].u.chat->timer) - (days*86400))/3600;
	mins=((tt-dcc[i].u.chat->timer) - (hrs*3600))/60;
	if (days>0) sprintf(&s[strlen(s)]," (idle %lud%luh)",days,hrs);
	else if (hrs>0) sprintf(&s[strlen(s)]," (idle %luh%lum)",hrs,mins);
	else sprintf(&s[strlen(s)]," (idle %lum)",mins);
      }
      dprintf(idx,"%s\n",s);
      if (dcc[i].u.chat->away!=NULL)
	dprintf(idx,"      AWAY: %s\n",dcc[i].u.chat->away);
    }
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_BOT) {
    if (!ok) {
      ok=1; dprintf(idx,"Bots connected:\n");
    }
    strcpy(s,ctime(&dcc[i].u.bot->timer)); strcpy(s,&s[1]); s[9]=0;
    strcpy(s,&s[7]); s[2]=' '; strcpy(&s[7],&s[10]); s[12]=0;
    dprintf(idx,"  %s%c%-10s (%s) %s\n",dcc[i].u.bot->status&STAT_CALLED?
	    "<-":"->",dcc[i].u.bot->status&STAT_SHARE?'+':' ',dcc[i].nick,
	    s,dcc[i].u.bot->version);
  }
  ok=0;
  for (i=0; i<dcc_total; i++) {
    if ((dcc[i].type==DCC_CHAT) && (dcc[i].u.chat->channel!=chan)) {
      if (!ok) {
	ok=1; dprintf(idx,"Other people on the bot:\n");
      }
      sprintf(s,"  %c%-10s ",(geticon(i)=='-'?' ':geticon(i)),dcc[i].nick);
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->channel<0)
	  strcat(s,"(-OFF-) ");
	else if (dcc[i].u.chat->channel==0)
	  strcat(s,"(party) ");
	else sprintf(&s[strlen(s)],"(%5d) ",dcc[i].u.chat->channel);
      }
      strcat(s,dcc[i].host);
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->con_flags)
	  sprintf(&s[strlen(s)]," (con:%s)",masktype(dcc[i].u.chat->con_flags));
      }
      if (tt - dcc[i].u.chat->timer > 300) {
	k=(tt-dcc[i].u.chat->timer)/60;
	if (k<60) sprintf(&s[strlen(s)]," (idle %dm)",k);
	else sprintf(&s[strlen(s)]," (idle %dh%dm)",k/60,k%60);
      }
      dprintf(idx,"%s\n",s);
      if (dcc[i].u.chat->away!=NULL)
	dprintf(idx,"      AWAY: %s\n",dcc[i].u.chat->away);
    }
    if ((atr & USER_MASTER) && (dcc[i].type==DCC_FILES)) {
      if (!ok) {
	ok=1; dprintf(idx,"Other people on the bot:\n");
      }
      sprintf(s,"  %c%-10s (files) %s",dcc[i].u.file->chat->status&STAT_CHAT?
	      '+':' ',dcc[i].nick,dcc[i].host);    
      dprintf(idx,"%s\n",s);
    }
  }
}

void dcc_chatter PROTO1(int,idx)
{
  int i,j;
#ifndef NO_IRC
  int atr=get_attr_handle(dcc[idx].nick),chatr,found=0,find=0;
  struct chanset_t *chan; chan=chanset;
#endif
  dprintf(idx,"Connected to %s, running %s\n",botname,version);
  show_motd(idx);
/*  tell_who  (idx); */
  dprintf(idx,"Commands start with '.' (like '.quit' or '.help')\n");
  dprintf(idx,"Everything else goes out to the party line.\n\n");
  i=dcc[idx].u.chat->channel; dcc[idx].u.chat->channel=234567;
  j=dcc[idx].sock;
#ifndef NO_IRC
  if (atr & (USER_GLOBAL|USER_MASTER|USER_OWNER)) found=1;
  if (owner_anywhere(dcc[idx].nick)) find=CHANUSER_OWNER;
  else if (master_anywhere(dcc[idx].nick)) find=CHANUSER_MASTER;
  else find=CHANUSER_OP;
  while (chan!=NULL && found==0) {
    chatr=get_chanattr_handle(dcc[idx].nick,chan->name);
    if (chatr & find) found=1;
    else chan=chan->next;
  }
  if (chan==NULL) chan=chanset;
  strcpy(dcc[idx].u.chat->con_chan,chan->name);
#endif
  check_tcl_chon(dcc[idx].nick,dcc[idx].sock);
  /* still there? */
  if ((idx>=dcc_total) || (dcc[idx].sock!=j)) return;  /* nope */
  /* tcl script may have taken control */
  if (dcc[idx].type==DCC_CHAT) {
    if (dcc[idx].u.chat->channel==234567) dcc[idx].u.chat->channel=i;
    if (dcc[idx].u.chat->channel==0) {
      chanout2(0,"%s joined the party line.\n",dcc[idx].nick);
      context;
    }
    else if (dcc[idx].u.chat->channel>0) {
      chanout2(dcc[idx].u.chat->channel,"%s joined the channel.\n", 
	       dcc[idx].nick);
    }
    if (dcc[idx].u.chat->channel >= 0) {
      context;
      if (dcc[idx].u.chat->channel<100000) {
        context;
        tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[idx].nick,
	        dcc[idx].u.chat->channel,geticon(idx),dcc[idx].sock,
	        dcc[idx].host);
      }
    }
    check_tcl_chjn(botnetnick,dcc[idx].nick,dcc[idx].u.chat->channel,
		    geticon(idx),dcc[idx].sock,dcc[idx].host);
    context;
    notes_read(dcc[idx].nick,"",-1,idx);
  }
}

/* remove entry from dcc list */
void lostdcc PROTO1(int,n)
{
  switch(dcc[n].type) {
  case DCC_CHAT:
    if (dcc[n].u.chat->buffer) {
      struct eggqueue * p = dcc[n].u.chat->buffer,*q;
      while (p) {
	q = p->next;
	nfree(p->item);
	nfree(p);
        p = q;
      }
    }
  case DCC_TELNET_ID:
  case DCC_CHAT_PASS:
  case DCC_TELNET_NEW:
  case DCC_TELNET_PW:
    if (dcc[n].u.chat->away!=NULL) nfree(dcc[n].u.chat->away);
    nfree(dcc[n].u.chat); break;
  case DCC_FILES_PASS:
  case DCC_FILES:
    nfree(dcc[n].u.file->chat);
    nfree(dcc[n].u.file); break;
  case DCC_SEND:
  case DCC_GET:
  case DCC_GET_PENDING:
    nfree(dcc[n].u.xfer); break;
  case DCC_BOT:
  case DCC_BOT_NEW:
    nfree(dcc[n].u.bot); break;
  case DCC_RELAY:
  case DCC_RELAYING:
    nfree(dcc[n].u.relay->chat);
    nfree(dcc[n].u.relay); break;
  case DCC_FORK:
    switch (dcc[n].u.fork->type) {
    case DCC_FORK:
    case DCC_CHAT:
      nfree(dcc[n].u.fork->u.chat); break;
    case DCC_FILES:
      nfree(dcc[n].u.fork->u.file->chat);
      nfree(dcc[n].u.fork->u.file); break;
    case DCC_BOT:
      nfree(dcc[n].u.fork->u.bot); break;
    case DCC_RELAY:
      nfree(dcc[n].u.fork->u.relay->chat);
      nfree(dcc[n].u.fork->u.relay); break;
    case DCC_SEND:
      nfree(dcc[n].u.fork->u.xfer); break;
    }
    nfree(dcc[n].u.fork); break;
  case DCC_SCRIPT:
    switch(dcc[n].u.script->type) {
    case DCC_CHAT:
      nfree(dcc[n].u.script->u.chat); break;
    case DCC_FILES:
      nfree(dcc[n].u.script->u.file->chat);
      nfree(dcc[n].u.script->u.file); break;
    }
    nfree(dcc[n].u.script); break;
  }
  dcc_total--;
  if (n < dcc_total) {
    strcpy(dcc[n].nick,dcc[dcc_total].nick);
    strcpy(dcc[n].host,dcc[dcc_total].host);
    dcc[n].sock=dcc[dcc_total].sock;
    dcc[n].addr=dcc[dcc_total].addr;
    dcc[n].port=dcc[dcc_total].port;
    dcc[n].type=dcc[dcc_total].type;
    dcc[n].u.other=dcc[dcc_total].u.other;
  }
}

char *stat_str PROTO1(int,st)
{
  static char s[10];
  s[0]=st&STAT_CHAT?'C':'c';
  s[1]=st&STAT_PARTY?'P':'p';
  s[2]=st&STAT_TELNET?'T':'t';
  s[3]=st&STAT_ECHO?'E':'e';
  s[4]=st&STAT_PAGE?'P':'p';
  s[5]=0; return s;
}

char *stat_str2 PROTO1(int,st)
{
  static char s[10];
  s[0]=st&STAT_PINGED?'P':'p';
  s[1]=st&STAT_SHARE?'U':'u';
  s[2]=st&STAT_CALLED?'C':'c';
  s[3]=st&STAT_OFFERED?'O':'o';
  s[4]=st&STAT_SENDING?'S':'s';
  s[5]=st&STAT_GETTING?'G':'g';
  s[6]=st&STAT_WARNED?'W':'w';
  s[7]=st&STAT_LEAF?'L':'l';
  s[8]=0; return s;
}

/* show list of current dcc's to a dcc-chatter */
/* positive value: idx given -- negative value: sock given */
void tell_dcc PROTO1(int,zidx)
{
  int i; char s[20],x[20],other[40]; time_t now=time(NULL);
  if (zidx<0) {
    tprintf(-zidx,"SOCK ADDR     PORT  NICK      HOST              TYPE\n");
    tprintf(-zidx,"---- -------- ----- --------- ----------------- ----\n");
  }
  else {
    dprintf(zidx,"SOCK ADDR     PORT  NICK      HOST              TYPE\n");
    dprintf(zidx,"---- -------- ----- --------- ----------------- ----\n");
  }
  /* show server */
  if (strlen(botserver)>17) strcpy(s,&botserver[strlen(botserver)-17]);
  else strcpy(s,botserver);
  if (zidx<0) {
    tprintf(-zidx,"%-4d 00000000 %5d (server)  %-17s %s\n",serv,botserverport,
	    s,trying_server?"conn":"serv");
  } 
  else {
    dprintf(zidx,"%-4d 00000000 %5d (server)  %-17s %s\n",serv,botserverport,
	    s,trying_server?"conn":"serv");
  }
  for (i=0; i<dcc_total; i++) {
    if (strlen(dcc[i].host)>17) strcpy(s,&dcc[i].host[strlen(dcc[i].host)-17]);
    else strcpy(s,dcc[i].host);
    switch (dcc[i].type) {
    case DCC_CHAT: 
      strcpy(x,"chat");
      sprintf(other,"flags: %s/%d",stat_str(dcc[i].u.chat->status),
	      dcc[i].u.chat->channel);
      break;
    case DCC_CHAT_PASS: 
      strcpy(x,"pass");
      sprintf(other,"waited %lus",now-dcc[i].u.chat->timer);
      break;
    case DCC_SEND:
      strcpy(x,"send");
      sprintf(other,"%lu/%lu",dcc[i].u.xfer->sent,dcc[i].u.xfer->length);
      break;
    case DCC_GET:
      strcpy(x,"get ");
      if (dcc[i].u.xfer->sent==dcc[i].u.xfer->length) {
        /* on the home stretch -- display acked amount instead */
        sprintf(other,"(%lu)/%lu",dcc[i].u.xfer->acked,dcc[i].u.xfer->length);
      }
      else sprintf(other,"%lu/%lu",dcc[i].u.xfer->sent,dcc[i].u.xfer->length);
      break;
    case DCC_GET_PENDING:
      strcpy(x,"getp"); 
      sprintf(other,"waited %lus",now-dcc[i].u.xfer->pending);
      break;
    case DCC_TELNET: 
      strcpy(x,"lstn");
      other[0]=0;
      break;
    case DCC_TELNET_ID:
      strcpy(x,"t-in");
      sprintf(other,"waited %lus",now-dcc[i].u.chat->timer);
      break;
    case DCC_FILES:
      strcpy(x,"file");
      sprintf(other,"flags: %s",stat_str(dcc[i].u.file->chat->status));
      break;
    case DCC_BOT:
      strcpy(x,"bot");
      sprintf(other,"flags: %s",stat_str2(dcc[i].u.bot->status));
      break;
    case DCC_BOT_NEW:
      strcpy(x,"bot*"); 
      sprintf(other,"waited %lus",now-dcc[i].u.bot->timer);
      break;
    case DCC_RELAY:
      strcpy(x,"rela");
      sprintf(other,"-> sock %d",dcc[i].u.relay->sock);
      break;
    case DCC_RELAYING:
      strcpy(x,">rly");
      sprintf(other,"-> sock %d",dcc[i].u.relay->sock);
      break;
    case DCC_FORK:
      strcpy(x,"conn");
      switch(dcc[i].u.fork->type) {
      case DCC_CHAT: strcpy(other,"chat"); break;
      case DCC_FILES: strcpy(other,"file"); break;
      case DCC_BOT: strcpy(other,"bot"); break;
      case DCC_RELAY: strcpy(other,"rela"); break;
      case DCC_RELAYING: strcpy(other,">rly"); break;
      case DCC_SEND: strcpy(other,"send"); break;
      }
      break;
    case DCC_FILES_PASS: 
      strcpy(x,"fpas"); 
      sprintf(other,"waited %lus",now-dcc[i].u.file->chat->timer);
      break;
    case DCC_TELNET_NEW:
      strcpy(x,"new ");
      sprintf(other,"waited %lus",now-dcc[i].u.chat->timer);
      break;
    case DCC_TELNET_PW:
      strcpy(x,"newp");
      sprintf(other,"waited %lus",now-dcc[i].u.chat->timer);
      break;
    case DCC_SCRIPT:
      strcpy(x,"scri");
      strcpy(other,dcc[i].u.script->command);
      break;
    case DCC_LOST:
      strcpy(x,"LOST");
      sprintf(other,"(zombie)");
      break;
    case DCC_SOCKET:
      strcpy(x,"sock");
      sprintf(other,"(stranded)");
      break;
    default:
      sprintf(x,"?:%02d",dcc[i].type);
      sprintf(other,"!! ERROR !!");
      break;
    }
    if (zidx<0)
      tprintf(-zidx,"%-4d %08X %5d %-9s %-17s %-4s  %s\n",dcc[i].sock,
	      dcc[i].addr,dcc[i].port,dcc[i].nick,s,x,other);
    else
      dprintf(zidx,"%-4d %08X %5d %-9s %-17s %-4s  %s\n",dcc[i].sock,
	      dcc[i].addr,dcc[i].port,dcc[i].nick,s,x,other);
    if ((dcc[i].type==DCC_SEND) || (dcc[i].type==DCC_GET) ||
	(dcc[i].type==DCC_GET_PENDING)) {
      if (zidx<0)
	tprintf(-zidx,"   Filename: %s\n",dcc[i].u.xfer->filename);
      else
	dprintf(zidx,"   Filename: %s\n",dcc[i].u.xfer->filename);
    }
  }
}

/* mark someone on dcc chat as no longer away */
void not_away PROTO1(int,idx)
{
  if (dcc[idx].u.chat->away==NULL) {
    dprintf(idx,"You weren't away!\n");
    return;
  }
  if (dcc[idx].u.chat->channel>=0) {
    chanout2(dcc[idx].u.chat->channel,"%s is no longer away.\n",dcc[idx].nick);
    context;
    if (dcc[idx].u.chat->channel<100000)
      tandout("unaway %s %d\n",botnetnick,dcc[idx].sock);
  }
  dprintf(idx,"You're not away any more.\n");
  nfree(dcc[idx].u.chat->away);
  dcc[idx].u.chat->away=NULL;
  notes_read(dcc[idx].nick,"",-1,idx);
}

void set_away PROTO2(int,idx,char *,s)
{
  if (s==NULL) { not_away(idx); return; }
  if (!s[0]) { not_away(idx); return; }
  if (dcc[idx].u.chat->away!=NULL) nfree(dcc[idx].u.chat->away);
  dcc[idx].u.chat->away=(char *)nmalloc(strlen(s)+1);
  strcpy(dcc[idx].u.chat->away,s);
  if (dcc[idx].u.chat->channel>=0) {
    chanout2(dcc[idx].u.chat->channel,"%s is now away: %s\n",
	     dcc[idx].nick,s);
    context;
    if(dcc[idx].u.chat->channel<100000)
      tandout("away %s %d %s\n",botnetnick,dcc[idx].sock,s);
  }
  dprintf(idx,"You are now away; notes will be stored.\n");
}

/* assumes it was chat type before! */
void set_files PROTO1(int,idx)
{
  struct chat_info *ci;
  ci=dcc[idx].u.chat;
  dcc[idx].u.file=(struct file_info *)nmalloc(sizeof(struct file_info));
  dcc[idx].u.file->chat=ci;
}

void set_tand PROTO1(int,idx)
{
  dcc[idx].u.bot=(struct bot_info *)nmalloc(sizeof(struct bot_info));
}

void set_chat PROTO1(int,idx)
{
  dcc[idx].u.chat=(struct chat_info *)nmalloc(sizeof(struct chat_info));
}

void set_xfer PROTO1(int,idx)
{
  dcc[idx].u.xfer=(struct xfer_info *)nmalloc(sizeof(struct xfer_info));
}

void get_file_ptr PROTO1(struct file_info **,p)
{
  (*p)=(struct file_info *)nmalloc(sizeof(struct file_info));
}

void get_chat_ptr PROTO1(struct chat_info **,p)
{
  (*p)=(struct chat_info *)nmalloc(sizeof(struct chat_info));
}

void get_xfer_ptr PROTO1(struct xfer_info **,p)
{
  (*p)=(struct xfer_info *)nmalloc(sizeof(struct xfer_info));
}

void set_fork PROTO1(int,idx)
{
  void *hold;
  hold=dcc[idx].u.other;
  dcc[idx].u.fork=(struct fork_info *)nmalloc(sizeof(struct fork_info));
  dcc[idx].u.fork->u.other=hold;
  dcc[idx].u.fork->type=dcc[idx].type;
  dcc[idx].type=DCC_FORK;
}

void set_script PROTO1(int,idx)
{
  void *hold;
  hold=dcc[idx].u.other;
  dcc[idx].u.script=(struct script_info *)nmalloc(sizeof(struct script_info));
  dcc[idx].u.script->u.other=hold;
  dcc[idx].u.script->type=dcc[idx].type;
  dcc[idx].type=DCC_SCRIPT;
}

void set_relay PROTO1(int,idx)
{
  dcc[idx].u.relay=(struct relay_info *)nmalloc(sizeof(struct relay_info));
}

void set_new_relay PROTO1(int,idx)
{
  set_relay(idx);
  dcc[idx].u.relay->chat=(struct chat_info *)nmalloc(sizeof(struct chat_info));
}

/* make a password, 10-15 random letters and digits */
void makepass PROTO1(char *,s)
{
  int i,j;
  i=10+(random()%6);
  for (j=0; j<i; j++) {
    if (random()%3==0) s[j]='0'+(random()%10);
    else s[j]='a'+(random()%26);
  }
  s[i]=0;
}

void check_expired_dcc()
{
  int i; time_t now;
#ifndef NO_FILE_SYSTEM
#ifndef MODULES
  char xx[121],*p;
#endif   
#endif
  now=time(NULL);
#ifndef NO_IRC
  /* server connect? */
  if ((trying_server) && (serv>=0)) {
    if (now-trying_server > server_timeout) {
      putlog(LOG_SERV,"*","Timeout: connect to %s",botserver);
      killsock(serv); serv=(-1);
    }
  }
#endif
  for (i=0; i<dcc_total; i++) {
    if (dcc[i].type==DCC_FORK) {
      if (now-dcc[i].u.fork->start > connect_timeout) {
	switch (dcc[i].u.fork->type) {
#ifndef NO_IRC
	case DCC_CHAT:
	case DCC_SEND:
	case DCC_FILES:
	  failed_got_dcc(i); break;
#endif
	case DCC_BOT:
	  failed_link(i); break;
	case DCC_RELAY:
	  failed_tandem_relay(i); break;
	}
      }
    }
#ifndef NO_FILE_SYSTEM
# ifndef MODULES
    if (dcc[i].type==DCC_GET_PENDING) {
      if (now-dcc[i].u.xfer->pending > wait_dcc_xfer) {
	if (strcmp(dcc[i].nick,"*users")==0) {
	  int x,y=0;
	  for (x=0; x<dcc_total; x++)
	    if ((strcasecmp(dcc[x].nick,dcc[i].host)==0) &&
		(dcc[x].type==DCC_BOT))
	      y=x;
	  if (y!=0) {
	    dcc[y].u.bot->status&=~STAT_SENDING;
	    dcc[y].u.bot->status&=~STAT_SHARE;
	  }
	  unlink(dcc[i].u.xfer->filename);
	  flush_tbuf(dcc[y].nick);
	  putlog(LOG_MISC,"*","Timeout on userfile transfer.");
	  xx[0]=0;
	}
	else {
	  strcpy(xx,dcc[i].u.xfer->filename); p=strrchr(xx,'/');
	  mprintf(serv,"NOTICE %s :Timeout during transfer, aborting %s.\n",
		  dcc[i].nick,p?p+1:xx);
	  putlog(LOG_FILES,"*","DCC timeout: GET %s (%s) at %lu/%lu",p?p+1:xx,
		 dcc[i].nick,dcc[i].u.xfer->sent,dcc[i].u.xfer->length);
	  wipe_tmp_file(i); strcpy(xx,dcc[i].nick);
	}
	killsock(dcc[i].sock); lostdcc(i); i--;
	if (!at_limit(xx)) send_next_file(xx);
      }
    }
    else if (dcc[i].type==DCC_SEND) {
      if (now-dcc[i].u.xfer->pending > wait_dcc_xfer) {
	if (strcmp(dcc[i].nick,"*users")==0) {
	  int x,y=0;
	  for (x=0; x<dcc_total; x++)
	    if ((strcasecmp(dcc[x].nick,dcc[i].host)==0) &&
		(dcc[x].type==DCC_BOT))
	      y=x;
	  if (y!=0) {
	    dcc[y].u.bot->status&=~STAT_GETTING;
	    dcc[y].u.bot->status&=~STAT_SHARE;
	  }
	  unlink(dcc[i].u.xfer->filename);
	  putlog(LOG_MISC,"*","Timeout on userfile transfer.");
	}
	else {
	  mprintf(serv,"NOTICE %s :Timeout during transfer, aborting %s.\n",
		  dcc[i].nick,dcc[i].u.xfer->filename);
	  putlog(LOG_FILES,"*","DCC timeout: SEND %s (%s) at %lu/%lu",
		 dcc[i].u.xfer->filename,dcc[i].nick,dcc[i].u.xfer->sent,
		 dcc[i].u.xfer->length);
	  sprintf(xx,"%s%s",tempdir,dcc[i].u.xfer->filename);
	  unlink(xx);
	}
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
    else if (dcc[i].type==DCC_FILES_PASS) {
      if (now-dcc[i].u.file->chat->timer > 180) {
	dprintf(i,"Timeout.\n");
	putlog(LOG_MISC,"*","Password timeout on dcc chat: [%s]%s",dcc[i].nick,
	       dcc[i].host);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
#endif
#endif
    else if (dcc[i].type==DCC_CHAT_PASS) {
      if (now-dcc[i].u.chat->timer > 180) {
	dprintf(i,"Timeout.\n");
	putlog(LOG_MISC,"*","Password timeout on dcc chat: [%s]%s",dcc[i].nick,
	       dcc[i].host);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
    else if (dcc[i].type==DCC_TELNET_ID) {
      if (now-dcc[i].u.chat->timer > 180) {
	dprintf(i,"Timeout.\n");
	putlog(LOG_MISC,"*","Ident timeout on telnet: %s",dcc[i].host);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
    else if (dcc[i].type==DCC_BOT_NEW) {
      if (now-dcc[i].u.bot->timer > 60) {
	putlog(LOG_MISC,"*","Timeout: bot link to %s at %s:%d",dcc[i].nick,
	       dcc[i].host,dcc[i].port);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
    else if (dcc[i].type==DCC_TELNET_NEW) {
      if (now-dcc[i].u.chat->timer > 180) {
	dprintf(i,"Guess you're not there.  Bye.\n");
	putlog(LOG_MISC,"*","Timeout on new telnet user: %s/%d",dcc[i].host,
	       dcc[i].port);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
    else if (dcc[i].type==DCC_TELNET_PW) {
      if (now-dcc[i].u.chat->timer > 180) {
	dprintf(i,"Guess you're not there.  Bye.\n");
	putlog(LOG_MISC,"*","Timeout on new telnet user: [%s]%s/%d",
	       dcc[i].nick,dcc[i].host,dcc[i].port);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
#ifdef MODULES
     else call_hook(HOOK_TIMEOUT,i);
#endif
  }
}

void append_line PROTO2(int,idx, char *,line)
{
  int l=strlen(line);
  struct eggqueue *p,*q; struct chat_info *c=dcc[idx].u.chat;
  if (c->current_lines > 1000) {
    p = c->buffer;
    /* they're probably trying to fill up the bot nuke the sods :) */
    while (p) { /* flush their queue */
      q = p->next;
      nfree(p->item);
      nfree(p);
      p = q;
    }
    c->buffer = 0;
    c->status &= ~STAT_PAGE; 
    do_boot(idx,botname,"too many pages - senq full");
    return;
  }
  if ((c->line_count < c->max_line) && (c->buffer==NULL)) {
    c->line_count++;
    tputs(dcc[idx].sock,line,l);
  } 
  else {
    c->current_lines++;
    if (c->buffer==NULL) q=NULL;
    else {
      q=c->buffer;
      while (q->next!=NULL) q=q->next;
    }
    p=(struct eggqueue *)nmalloc(sizeof(struct eggqueue));
    p->stamp=l;
    p->item=(char *)nmalloc(p->stamp+1);
    p->next=NULL;
    strcpy(p->item,line);
    if (q==NULL) c->buffer=p;
    else q->next=p;
  }
}

void flush_lines PROTO1(int,idx)
{
  int c=dcc[idx].u.chat->line_count;
  struct eggqueue *p=dcc[idx].u.chat->buffer,*o;
  while (p && c < (dcc[idx].u.chat->max_line)) {
    dcc[idx].u.chat->current_lines--;
    tputs(dcc[idx].sock,p->item,p->stamp);
    nfree(p->item);
    o=p->next;
    nfree(p);
    p=o;
    c++;
  }
  if (p!=NULL) {
     tputs(dcc[idx].sock,"[More]: ",8);
  }
  dcc[idx].u.chat->buffer=p;
  dcc[idx].u.chat->line_count=0;
}

int new_dcc PROTO1(int,type) {
   int i = dcc_total;
   if (dcc_total==MAXDCC) 
     return -1;
   dcc_total++;
   memset(&dcc[i],0,sizeof(struct dcc_t));
   dcc[i].type = type;
   switch (type) {
    case DCC_GET_PENDING:
      set_xfer(i);
      break;
    default:
      putlog(LOG_MISC,"*","woops new_dcc on unsupported type %d!\n");
   }
   return i;
}

int new_fork PROTO1(int,type) {
   int i = dcc_total;
   if (dcc_total==MAXDCC) 
     return -1;
   dcc_total++;
   memset(&dcc[i],0,sizeof(struct dcc_t));
   set_fork(i);
   get_xfer_ptr(&(dcc[i].u.fork->u.xfer));
   dcc[i].u.fork->type = type;
   return i;
}
