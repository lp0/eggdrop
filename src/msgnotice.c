/*
   msgnotice.c -- handles:
     msgs & notices from user or channel
     simple ctcp requests and replies

   dprintf'ized, 18nov95
*/
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <varargs.h>
#include "eggdrop.h"
#include "chan.h"
#include "proto.h"

/* SED and UTC are a big lie, but they'll never know */
#define CLIENTINFO "SED VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME ACTION DCC UTC PING ECHO  :Use CLIENTINFO <COMMAND> to get more specific information"
#define CLIENTINFO_SED "SED contains simple_encrypted_data"
#define CLIENTINFO_VERSION "VERSION shows client type, version and environment"
#define CLIENTINFO_CLIENTINFO "CLIENTINFO gives information about available CTCP commands"
#define CLIENTINFO_USERINFO "USERINFO returns user settable information"
#define CLIENTINFO_ERRMSG "ERRMSG returns error messages"
#define CLIENTINFO_FINGER "FINGER shows real name, login name and idle time of user"
#define CLIENTINFO_TIME "TIME tells you the time on the user's host"
#define CLIENTINFO_ACTION "ACTION contains action descriptions for atmosphere"
#define CLIENTINFO_DCC "DCC requests a direct_client_connection"
#define CLIENTINFO_UTC "UTC substitutes the local timezone"
#define CLIENTINFO_PING "PING returns the arguments it receives"
#define CLIENTINFO_ECHO "ECHO returns the arguments it receives"

extern int serv;
extern int backgrd;
extern int con_chan;
extern int term_z;
extern char botname[];
extern char version[];
extern struct dcc_t dcc[];
extern int dcc_total;
extern char admin[];
extern int require_p;
extern char origbotname[];
extern int ignore_time;
extern int flood_ctcp_thr;

/* response to ctcp version requests */
char ctcp_version[121]="you suck, v1.0";
/* response to ctcp finger requests */
char ctcp_finger[121]="yer gonna lose that finger if you don't stop.";
/* response to ctcp userinfo requests */
char ctcp_userinfo[121]="i'm cute, hung, and available";

/* no point if there's no irc */
#ifndef NO_IRC

#ifdef ALLOW_LOWERCASE_CTCP
#define ctcpcmp strcasecmp
#else
#define ctcpcmp strcmp
#endif

static char ctcp_reply[512]="";

/* ctcp embedded in a privmsg */
void gotctcp(ffrom,to,msg,ignoring)
char *ffrom,*to,*msg; int ignoring;
{
  char from[UHOSTLEN],nick[NICKLEN],hand[10],code[512],*p;
  strcpy(from,ffrom);
  if (msg[0]==' ') return;
  split(code,msg); splitnick(nick,from);
  if (code[0]==0) { strcpy(code,msg); msg[0]=0; }
  if ((to[0]=='$') ||
      ((strchr(to,'.')!=NULL) && (strchr("&#+",to[0])==NULL))) {
    if (!ignoring)
      putlog(LOG_PUBLIC,to,"CTCP %s: %s from %s (%s) to %s",code,msg,nick,from,
	     to);
    return;  /* don't interpret */
  }
  get_handle_by_host(hand,ffrom);
#ifndef TRIGGER_BINDS_ON_IGNORE
  if (!ignoring)
#endif
  if (check_tcl_ctcp(nick,from,hand,to,code,msg)) return;
  if (ignoring) return;
  if ((ctcpcmp(code,"FINGER")==0) && (ctcp_finger[0]))
    sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001FINGER %s\001",ctcp_finger);
  else if ((ctcpcmp(code,"PING")==0) || (ctcpcmp(code,"ECHO")==0) ||
	   (ctcpcmp(code,"ERRMSG")==0)) {
    if (strlen(msg)<=80)
      sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001%s %s\001",code,msg);
    /* ignore gratuitously long ctcp echo requests */
  }
  else if ((ctcpcmp(code,"VERSION")==0) && (ctcp_version[0]))
    sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001VERSION %s\001",ctcp_version);
  else if ((ctcpcmp(code,"USERINFO")==0) && (ctcp_userinfo[0]))
    sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001USERINFO %s\001",
	    ctcp_userinfo);
  else if (ctcpcmp(code,"CLIENTINFO")==0) {
    p=NULL;
    if (!msg[0]) p=CLIENTINFO;
    else if (ctcpcmp(msg,"sed")==0) p=CLIENTINFO_SED;
    else if (ctcpcmp(msg,"version")==0) p=CLIENTINFO_VERSION;
    else if (ctcpcmp(msg,"clientinfo")==0) p=CLIENTINFO_CLIENTINFO;
    else if (ctcpcmp(msg,"userinfo")==0) p=CLIENTINFO_USERINFO;
    else if (ctcpcmp(msg,"errmsg")==0) p=CLIENTINFO_ERRMSG;
    else if (ctcpcmp(msg,"finger")==0) p=CLIENTINFO_FINGER;
    else if (ctcpcmp(msg,"time")==0) p=CLIENTINFO_TIME;
    else if (ctcpcmp(msg,"action")==0) p=CLIENTINFO_ACTION;
    else if (ctcpcmp(msg,"dcc")==0) p=CLIENTINFO_DCC;
    else if (ctcpcmp(msg,"utc")==0) p=CLIENTINFO_UTC;
    else if (ctcpcmp(msg,"ping")==0) p=CLIENTINFO_PING;
    else if (ctcpcmp(msg,"echo")==0) p=CLIENTINFO_ECHO;
    if (p==NULL) {
      sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001ERRMSG CLIENTINFO: %s is not a valid function\001",
	      msg);
    }
    else sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001CLIENTINFO %s\001",p);
  }
  else if (ctcpcmp(code,"DCC")==0)
    gotdcc(nick,from,msg);
  else if (ctcpcmp(code,"TIME")==0) {
    time_t tm=time(NULL); char tms[81];
    strcpy(tms,ctime(&tm)); tms[strlen(tms)-1]=0;
    sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001TIME %s\001",tms);
  }
  else if (ctcpcmp(code,"CHAT")==0) {
    int atr=get_attr_host(ffrom),i,ix=(-1);
    if ((atr & (USER_MASTER|USER_PARTY|USER_XFER)) || 
	((atr&USER_GLOBAL) && !require_p)) {
      for (i=0; i<dcc_total; i++) {
	if ((dcc[i].type==DCC_TELNET) &&
	    ((strcmp(dcc[i].nick,"(telnet)")==0) ||
	    (strcmp(dcc[i].nick,"(users)")==0))) {
	  ix=i;
          /* do me a favour and don't change this back to a CTCP reply,
             CTCP replies are NOTICE's this has to be a PRIVMSG -poptix 5/1/97 */
          mprintf(serv,"PRIVMSG %s :\001DCC CHAT chat %lu %u\001\n",
                  nick,iptolong(getmyip()),dcc[ix].port);
	}
      }
      if (ix<0)
        sprintf(&ctcp_reply[strlen(ctcp_reply)],"\001ERROR no telnet port\001");
    }
  }
  /* don't log DCC */
  if (ctcpcmp(code,"DCC")!=0) {
    if ((to[0]=='#') || (to[0]=='&') || (to[0]=='+')) {
      if (ctcpcmp(code,"ACTION")==0) {
	putlog(LOG_PUBLIC,to,"Action: %s %s",nick,msg);
      }
      else {
	putlog(LOG_PUBLIC,to,"CTCP %s: %s from %s (%s) to %s",code,msg,nick,
	       from,to);
      }
      update_idle(to,nick);
    }
    else {
      if (ctcpcmp(code,"ACTION")==0) {
	putlog(LOG_MSGS,"*","Action to %s: %s %s",to,nick,msg);
      }
      else {
	putlog(LOG_MSGS,"*","CTCP %s: %s from %s (%s)",code,msg,nick,from);
      }
    }
  }
}

/* ctcp embedded in a notice */
void gotctcpreply(ffrom,to,msg,ignoring)
char *ffrom,*to,*msg; int ignoring;
{
  char from[UHOSTLEN],nick[NICKLEN],hand[10],code[512];
  strcpy(from,ffrom);
  split(code,msg); splitnick(nick,from);
  if (code[0]==0) { strcpy(code,msg); msg[0]=0; }
  if ((to[0]=='$') || ((strchr(to,'.')!=NULL) && 
      (strchr("&#+",to[0])==NULL))) {
    if (!ignoring)
      putlog(LOG_PUBLIC,"*","CTCP reply %s: %s from %s (%s) to %s",code,msg,
	     nick,from,to);
    return;
    /* don't even interpret into tcl */
  }
  get_handle_by_host(hand,ffrom);
#ifndef TRIGGER_BINDS_ON_IGNORE
  if (!ignoring)
#endif
  if (check_tcl_ctcr(nick,from,hand,to,code,msg)) return;
  if (ignoring) return;
  /* who cares? */
  if ((to[0]=='#') || (to[0]=='&') || (to[0]=='+')) {
    putlog(LOG_PUBLIC,to,"CTCP reply %s: %s from %s (%s) to %s",code,msg,nick,
	   from,to);
    update_idle(to,nick);
  }
  else {
    putlog(LOG_MSGS,"*","CTCP reply %s: %s from %s (%s) to %s",code,msg,
	   nick,from,to);
  }
}  

/* public msg on channel */
void gotpublic(from,to,msg,ignoring)
char *from,*to,*msg; int ignoring;
{
  char nick[NICKLEN]; struct chanset_t *chan;
  chan=findchan(to); if (chan==NULL) return;
  if (!ignoring) detect_flood(from,chan,FLOOD_PRIVMSG,1);
  splitnick(nick,from);
#ifndef TRIGGER_BINDS_ON_IGNORE
  if (!ignoring) {
#else
  {
#endif
    if (check_tcl_pub(nick,from,to,msg)) return;
    check_tcl_pubm(nick,from,to,msg);
  }
  if (!ignoring) putlog(LOG_PUBLIC,to,"<%s> %s",nick,msg);
  update_idle(to,nick);
}

/* public notice on channel */
void gotpublicnotice(from,to,msg,ignoring)
char *from,*to,*msg; int ignoring;
{
  char nick[NICKLEN]; struct chanset_t *chan;
  chan=findchan(to); if (chan==NULL) return;
  if (!ignoring) detect_flood(from,chan,FLOOD_NOTICE,1);
  splitnick(nick,from);
  if (!ignoring) putlog(LOG_PUBLIC,to,"-%s:%s- %s",nick,to,msg);
  update_idle(to,nick);
}

/* check for more than 8 control characters in a line */
/* this could indicate:
     beep flood
     CTCP avalanche
*/
int detect_avalanche(msg)
char *msg;
{
  int count=0; unsigned char *p;
  for (p=(unsigned char *)msg; (*p)&&(count<8); p++) 
    if ((*p==7) || (*p==1)) count++;
  if (count>=8) return 1;
  else return 0;
}

/* private message */
void gotmsg(char *from,char *msg,int ignoring)
{
  char to[UHOSTLEN],uhost[UHOSTLEN],nick[NICKLEN],ctcp[512]; char *p,*p1;
  int ctcp_count=0;
  struct chanset_t *chan;
  split(to,msg); fixcolon(msg);
  strcpy(uhost,from); splitnick(nick,uhost);
  /* only check if flood-ctcp is active */
  if ((flood_ctcp_thr) && (detect_avalanche(msg))) {
    /* discard -- kick user if it was to the channel */
    if ((to[0]=='&') || (to[0]=='#')) {
      mprintf(serv,"KICK %s %s :that was fun, let's do it again!\n",
	      to,nick);
    }
    if (!ignoring) {
      putlog(LOG_MODES,"*","Avalanche from %s!%s - ignoring",nick,uhost);
      p=strchr(uhost,'@');
      if (p!=NULL) p++; else p=uhost;
      sprintf(ctcp,"*!*@%s",p);
      addignore(ctcp,origbotname,"ctcp avalanche",time(NULL)+(60*ignore_time));
    }
    return;
  }
  /* check for CTCP: */
  ctcp_reply[0]=0;
  p=strchr(msg,1);
  while ((p!=NULL) && (*p)) {
    p++; p1=p;
    while ((*p!=1) && (*p!=0)) p++;
    if (*p==1) {
      *p=0; strcpy(ctcp,p1);
      strcpy(p1-1,p+1);
      chan=findchan(to);
      if (chan==NULL) { if (!ignoring) detect_flood(from,NULL,FLOOD_CTCP,0); }
      else if (strncmp(ctcp,"ACTION ",7)==0)
	detect_flood(from,chan,FLOOD_PRIVMSG,1);
      else 
	detect_flood(from,chan,FLOOD_CTCP,1);
#ifdef ANSWER_STACKED_CTCP
      /* respond to the first 3 */
      if (ctcp_count<3) gotctcp(from,to,ctcp,ignoring);
#else
      /* for paranoia reasons, only respond to the first one */
      if (!ctcp_count) gotctcp(from,to,ctcp,ignoring);
#endif
      ctcp_count++;
      p=strchr(msg,1);
    }
  }
  /* send out possible ctcp responses */
  if (ctcp_reply[0]) hprintf(serv,"NOTICE %s :%s\n",nick,ctcp_reply);
  if (msg[0]==0) return;  /* oh. no msg. well forget it then! */
  if ((to[0]=='#') || (to[0]=='&') || (to[0]=='+')) {   /* it's a public msg */
    gotpublic(from,to,msg,ignoring);
  } 
  else if ((to[0]=='$') || (strchr(to,'.')!=NULL)) {   /* msg from oper */
    if (!ignoring) detect_flood(from,NULL,FLOOD_PRIVMSG,0); 
    /* do not interpret as command */
    if (!ignoring)
      putlog(LOG_MSGS|LOG_SERV,"*","[%s!%s to %s] %s",nick,uhost,to,msg);
  }
  else {
    if (!ignoring) detect_flood(from,NULL,FLOOD_PRIVMSG,0);
    gotcmd(nick,uhost,msg,ignoring);
  } 
}

/* private notice */
void gotnotice(char *from,char *msg,int ignoring)
{
  char to[UHOSTLEN],hand[10],nick[NICKLEN],ctcp[512]; char *p,*p1;
  split(to,msg); fixcolon(msg);  
  if ((flood_ctcp_thr) && (detect_avalanche(msg))) {
    splitnick(nick,from);
    /* discard -- kick user if it was to the channel */
    if ((to[0]=='&') || (to[0]=='#')) {
      mprintf(serv,"KICK %s %s :that was fun, let's do it again!\n",
	      to,nick);
    }
    if (!ignoring) putlog(LOG_MODES,"*","Avalanche from %s!%s",nick,from);
    return;
  }
  /* check for CTCP: */
  p=strchr(msg,1);
  while ((p!=NULL) && (*p)) {
    p++; p1=p;
    while ((*p!=1) && (*p!=0)) p++;
    if (*p==1) {
      *p=0; strcpy(ctcp,p1);
      strcpy(p1-1,p+1);
      if (!ignoring) detect_flood(from,NULL,FLOOD_CTCP,0);
      gotctcpreply(from,to,ctcp,ignoring);
      p=strchr(msg,1);
    }
  }
  if (msg[0]==0) return;  /* oh. no msg. well forget it then! */
  if ((to[0]=='#') || (to[0]=='&') || (to[0]=='+')) {   /* it's a public msg */
    gotpublicnotice(from,to,msg,ignoring);
  } 
  else if ((to[0]=='$') || (strchr(to,'.')!=NULL)) {   /* msg from oper */
    if (!ignoring) detect_flood(from,NULL,FLOOD_NOTICE,0);
    splitnick(nick,from);
    if (!ignoring)
      putlog(LOG_MSGS|LOG_SERV,"*","-%s (%s) to %s- %s",nick,from,to,msg);
  }
  else {
    detect_flood(from,NULL,FLOOD_NOTICE,0);
    get_handle_by_host(hand,from);
    splitnick(nick,from);
    /* server notice? */
    if ((from[0]==0) || (nick[0]==0)) {
      /* bugger off you fucking 250 numeric in hiding!! */
      if (strncmp(msg,"Highest connection count:",25)!=0)
	putlog(LOG_SERV,"*","-NOTICE- %s",msg);
    }
    else if (!ignoring) {
      check_tcl_notc(nick,from,hand,msg);
      putlog(LOG_MSGS,"*","-%s (%s)- %s",nick,from,msg);
    }
  }
}

/* error notice */
void goterror(char *from,char *msg)
{
  fixcolon(msg);
  putlog(LOG_SERV|LOG_MSGS,"*","-ERROR- %s",msg);
  killsock(serv); serv=(-1);  /* they're gonna disconnect anyway :) */
}

#endif  /* !NO_IRC */
