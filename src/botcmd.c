
/*
   botcmd.c -- handles:
     commands that comes across the botnet
     userfile transfer and update commands from sharebots

   dprintf'ized, 10nov95
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
#include "eggdrop.h"
#include "tandem.h"
#include "users.h"
#include "chan.h"
#include "tclegg.h"
#ifdef MODULES
#include "modules.h"
#endif

extern tand_t tand[];
extern int tands;
extern char botnetnick[];
extern int dcc_total;
extern struct dcc_t dcc[];
extern int noshare;
extern int passive;
extern int share_users;
extern char motdfile[];
extern struct userrec *userlist;
extern char ver[];
extern char os[];
extern struct chanset_t *chanset;
extern int serv;
extern char ban_time;
extern char ignore_time;
extern char network[];
extern int min_share;

/* static buffer for goofy bot stuff */
char TBUF[1024];

/* used for 1.0 compatibility: if a join message arrives with no sock#, */
/* i'll just grab the next "fakesock" # (incrementing to assure uniqueness) */
int fakesock=2300;

void fake_alert PROTO1(int,idx)
{
  tprintf(dcc[idx].sock,"chat %s NOTICE: Fake message rejected.\n",
	  botnetnick);
}

/* chan <from> <chan> <text> */
void bot_chan PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*s=TBUF+512,*p; int i,chan;
  nsplit(from,par); chan=atoi(par); nsplit(NULL,par);
  /* strip annoying control chars */
  for (p=from; *p; ) {
    if ((*p<32) || (*p==127)) strcpy(p,p+1);
    else p++;
  }
  p=strchr(from,'@');
  if (p==NULL) { sprintf(s,"*** (%s) %s",from,par); p=from; }
  else {
    sprintf(s,"<%s> %s",from,par);
    *p=0; partyidle(p+1,from); *p='@'; p++;
  }
  i=nextbot(p);
  if (i!=idx) {
    fake_alert(idx);
    return;
  }
  tandout_but(idx,"chan %s %d %s\n",from,chan,par);
  chanout(chan,"%s\n",s);
  if (strchr(from,'@')!=NULL) check_tcl_chat(from,chan,par);
  else check_tcl_bcst(from,chan,par);
}

/* chat <from> <notice>  -- only from bots */
void bot_chat PROTO2(int,idx,char *,par)
{
  char *from=TBUF; int i;
  nsplit(from,par);
  if (strchr(from,'@')!=NULL) {
    fake_alert(idx);
    return;
  }
  /* make sure the bot is valid */
  i=nextbot(from); if (i!=idx) {
    fake_alert(idx);
    return;
  }
  chatout("*** (%s) %s\n",from,par);
  tandout_but(idx,"chat %s %s\n",from,par);
}

/* actchan <from> <chan> <text> */
void bot_actchan PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*p; int i,chan;
  nsplit(from,par); p=strchr(from,'@');
  if (p==NULL) {
    /* how can a bot do an action? */
    fake_alert(idx); return;
  }
  *p=0; partyidle(p+1,from); *p='@';
  p++; i=nextbot(p); if (i!=idx) {
    fake_alert(idx); return;
  }
  chan=atoi(par); nsplit(NULL,par);
  chanout(chan,"* %s %s\n",from,par);
  check_tcl_act(from,chan,par);
  tandout_but(idx,"actchan %s %d %s\n",from,chan,par);
}

/* priv <from> <to> <message> */
void bot_priv PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*p,*to=TBUF+600,*tobot=TBUF+512; int i;
  nsplit(from,par); nsplit(tobot,par); tobot[40]=0; splitc(to,tobot,'@');
  p=strchr(from,'@'); if (p!=NULL) p++; else p=from;
  i=nextbot(p); if ((i!=idx) || (!to[0])) {
    fake_alert(idx); return;
  }
  if (strcasecmp(tobot,botnetnick)==0) {  /* for me! */
    if (p==from) add_note(to,from,par,-2,0);
    else {
      i=add_note(to,from,par,-1,0);
      switch(i) {
      case NOTE_ERROR:
	tprintf(dcc[idx].sock,"priv %s %s No such user %s.\n",botnetnick,
		from,to);
	break;
      case NOTE_STORED:
	tprintf(dcc[idx].sock,"priv %s %s Not online; note stored.\n",
		botnetnick,from);
	break;
      case NOTE_FULL:
	tprintf(dcc[idx].sock,"priv %s %s Notebox is full, sorry.\n",
		botnetnick,from);
	break;
      case NOTE_AWAY:
	tprintf(dcc[idx].sock,"priv %s %s %s is away; note stored.\n",
		botnetnick,from,to);
	break;
      case NOTE_TCL:
	break;  /* do nothing */
      case NOTE_OK:
	tprintf(dcc[idx].sock,"priv %s %s Note sent to %s.\n",botnetnick,
		from,to);
	break;
      }
    }
  }
  else {   /* pass it on */
    i=nextbot(tobot); if (i>=0)
      tprintf(dcc[i].sock,"priv %s %s@%s %s\n",from,to,tobot,par);
  }
}

void bot_bye PROTO2(int,idx,char *,par)
{
  putlog(LOG_BOTS,"*","Disconnected from: %s",dcc[idx].nick);
  chatout("*** Disconnected from: %s\n",dcc[idx].nick);
  tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
  tandout_but(idx,"chat %s Disconnected from: %s\n",botnetnick,dcc[idx].nick);
  cancel_user_xfer(idx);
  tprintf(dcc[idx].sock,"*bye\n"); killsock(dcc[idx].sock); lostdcc(idx);
}

/* who <from@bot> <tobot> <chan#> */
void bot_who PROTO2(int,idx,char *,par)
{
  char *p; char *from=TBUF,*to=TBUF+512; int i;
  nsplit(from,par); p=strchr(from,'@'); if (p==NULL) {
    sprintf(to,"%s@%s",from,dcc[idx].nick); strcpy(from,to);
  }
  nsplit(to,par);
  if (strcasecmp(to,botnetnick)==0) to[0]=0;  /* (for me) */
  if (to[0]) {     /* pass it on */
    i=nextbot(to); if (i>=0)
      tprintf(dcc[i].sock,"who %s %s %s\n",from,to,par);
  }
  else {
    remote_tell_who(dcc[idx].sock,from,atoi(par));
  }
}

void bot_version PROTO2(int,idx,char *,par)
{
  if ((par[0] >= '0') && (par[0] <= '9')) {
    char work[600];
    nsplit(work,par);
    dcc[idx].u.bot->numver=atoi(work);
  } else
    dcc[idx].u.bot->numver=0;
  strcpy(dcc[idx].u.bot->version,par);
  if (dcc[idx].u.bot->numver >= min_share) {
    if ((share_users) && (get_attr_handle(dcc[idx].nick)&BOT_SHARE)) {
      if (passive) {
        if (dcc[idx].u.bot->status&STAT_CALLED) {
  	  if (can_resync(dcc[idx].nick)) tprintf(dcc[idx].sock,"resync?\n");
	  else tprintf(dcc[idx].sock,"userfile?\n");
	  dcc[idx].u.bot->status|=STAT_OFFERED;
        }
      }
      else {
        if (can_resync(dcc[idx].nick)) tprintf(dcc[idx].sock,"resync?\n");
        else tprintf(dcc[idx].sock,"userfile?\n");
        dcc[idx].u.bot->status|=STAT_OFFERED;
      }
    }
  }
}

/* who? <from@bot> <chan>  ->  whom <to@bot> <attr><nick> <bot> <host> */
void bot_whoq PROTO2(int,idx,char *,par)
{
  /* ignore old-style 'whom' request */
  putlog(LOG_BOTS,"*","Outdated 'whom' request from %s (ignoring)",
	 dcc[idx].nick);
}

/* info? <from@bot>   -> send priv */
void bot_infoq PROTO2(int,idx,char *,par)
{
#ifndef NO_IRC
  char s[161]; struct chanset_t *chan;
  chan=chanset; s[0]=0;
  while (chan!=NULL) {
    if (!(chan->stat & CHAN_SECRET)) {
      strcat(s,chan->name); strcat(s,", ");
    }
    chan=chan->next;
  }
  if (s[0]) {
    s[strlen(s)-2]=0;
    tprintf(dcc[idx].sock,"priv %s %s %s <%s> (%s)\n",botnetnick,par,ver,
	    network,s);
  }
  else
    tprintf(dcc[idx].sock,"priv %s %s %s <%s> (no channels)\n",botnetnick,par,
	    ver,network);
#else
  tprintf(dcc[idx].sock,"priv %s %s %s\n",botnetnick,par,ver);
#endif
  tandout_but(idx,"info? %s\n",par);
}

/* whom <to@bot> <attr><nick> <bot> <etc> */
void bot_whom PROTO2(int,idx,char *,par)
{
  /* scrap it */
  putlog(LOG_BOTS,"*","Outdated 'whom' request from %s (ignoring)",
	 dcc[idx].nick);
}

void bot_ping PROTO2(int,idx,char *,par)
{
  tprintf(dcc[idx].sock,"pong\n");
}

void bot_pong PROTO2(int,idx,char *,par)
{
  dcc[idx].u.bot->status&=~STAT_PINGED;
}

/* link <from@bot> <who> <to-whom> */
void bot_link PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*bot=TBUF+512,*rfrom=TBUF+41; int i;
  nsplit(from,par); nsplit(bot,par); from[40]=0;
  if (strcasecmp(bot,botnetnick)==0) {
    strcpy(rfrom,from); splitc(NULL,rfrom,':');
    putlog(LOG_CMDS,"*","#%s# link %s",rfrom,par);
    if (botlink(from,-2,par))
      tprintf(dcc[idx].sock,"priv %s %s Attempting link to %s ...\n",
	      botnetnick,from,par);
    else tprintf(dcc[idx].sock,"priv %s %s Can't link there.\n",botnetnick,
		 from);
  }
  else {
    i=nextbot(bot); if (i>=0)
      tprintf(dcc[i].sock,"link %s %s %s\n",from,bot,par);
  }
}

/* unlink <from@bot> <linking-bot> <undesired-bot> <reason> */
void bot_unlink PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*bot=TBUF+512,*rfrom=TBUF+41;int i; char *p;
  char *undes=TBUF+550;
  nsplit(from,par); nsplit(bot,par);
  nsplit(undes,par);
  from[40]=0;
  if (strcasecmp(bot,botnetnick)==0) {
    strcpy(rfrom,from); splitc(NULL,rfrom,':');
    putlog(LOG_CMDS,"*","#%s# unlink %s (%s)",rfrom,undes,par[0]?par:
	   "No reason");
    if (botunlink(-2,undes,par[0]?par:NULL)) {
      p=strchr(from,'@'); if (p!=NULL) {
	/* idx will change after unlink -- get new idx */
	i=nextbot(p+1);
	if (i>=0) tprintf(dcc[i].sock,"priv %s %s Unlinked %s.\n",botnetnick,
			  from,undes);
      }
    }
    else {
      tandout("unlinked %s\n",undes);   /* just to clear trash from link lists */
      p=strchr(from,'@'); if (p!=NULL) {
	/* ditto above, about idx */
	i=nextbot(p+1);
	if (i>=0) tprintf(dcc[i].sock,"priv %s %s Can't unlink %s.\n",
			  botnetnick,from,undes);
      }
    }
  }
  else {
    i=nextbot(bot); if (i>=0)
      tprintf(dcc[i].sock,"unlink %s %s %s %s\n",from,bot,undes,par);
  }
}

void bot_ufno PROTO2(int,idx,char *,par)
{
  putlog(LOG_BOTS,"*","User file rejected by %s: %s",dcc[idx].nick,par);
  dcc[idx].u.bot->status&=~STAT_OFFERED;
  if (!(dcc[idx].u.bot->status&STAT_GETTING))
    dcc[idx].u.bot->status&=~STAT_SHARE;
}

void bot_ufobsolete PROTO2(int,idx,char *,par)
{
  putlog(LOG_BOTS,"*","Can't send userfile to %s -- doesn't recognize new format",
	 dcc[idx].nick);
  dcc[idx].u.bot->status&=~STAT_OFFERED;
  if (!(dcc[idx].u.bot->status&STAT_GETTING))
    dcc[idx].u.bot->status&=~STAT_SHARE;
  /* drop the bot */
  tprintf(dcc[idx].sock,"error Please upgrade; I can no longer send a userfile to you.\n");
  tprintf(dcc[idx].sock,"bye\n");
  tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
  tandout_but(idx,"chat %s Disconnected %s (incompatible userfile transfer)\n",
	      botnetnick,dcc[idx].nick);
  chatout("*** Disconnected %s (incompatible userfile transfer)\n",
	  dcc[idx].nick);
  unvia(idx,dcc[idx].nick);
  killsock(dcc[idx].sock); lostdcc(idx);
}

void bot_ufyes3 PROTO2(int,idx,char *,par)
{
  if (dcc[idx].u.bot->status&STAT_OFFERED) {
    dcc[idx].u.bot->status&=~STAT_OFFERED;
    dcc[idx].u.bot->status|=STAT_SHARE;
    dcc[idx].u.bot->status|=STAT_SENDING;
    start_sending_users(idx);
    putlog(LOG_BOTS,"*","Sending user file to %s",dcc[idx].nick);
  }
}

void bot_userfileq PROTO2(int,idx,char *,par)
{
  int ok=1,i;
  flush_tbuf(dcc[idx].nick);
  if (!share_users) tprintf(dcc[idx].sock,"uf-no Not sharing userfile.\n");
#ifdef MODULES
  else if (find_module("filesys",0,0)==NULL)
    tprintf(dcc[idx].sock,"uf-no No file system intalled.\n");
#endif
  else if (!passive) tprintf(dcc[idx].sock,"uf-no Aggressive mode active.\n");
  else if (!(get_attr_handle(dcc[idx].nick) & BOT_SHARE))
    tprintf(dcc[idx].sock,"uf-no You are not +s for me.\n");
  else if (min_share > dcc[idx].u.bot->numver) 
    tprintf(dcc[idx].sock,
	    "uf-no Your version is not high enough, need v%d.%d.%d\n",
	    (min_share/1000000),(min_share/10000)%100,(min_share/100)%100);
  else {
    for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_BOT)
      if ((dcc[i].u.bot->status&STAT_SHARE) &&
	  (dcc[i].u.bot->status&STAT_GETTING)) ok=0;
    if (!ok) tprintf(dcc[idx].sock,"uf-no Already downloading a userfile.\n");
    else {
      tprintf(dcc[idx].sock,"uf-yes3\n");
      /* set stat-getting to avoid race condition (robey 23jun96) */
      dcc[idx].u.bot->status|=STAT_SHARE|STAT_GETTING;
      putlog(LOG_BOTS,"*","Downloading user file from %s",dcc[idx].nick);
    }
  }
}

/* ufsend <ip> <port> <length> */
void bot_ufsend PROTO2(int,idx,char *,par)
{
  char *ip=TBUF,*port=TBUF+512; int i;
  if (!(dcc[idx].u.bot->status & STAT_SHARE)) {
    tprintf(dcc[idx].sock,"error You didn't ask; you just started sending.\n");
    tprintf(dcc[idx].sock,"error Ask before sending the userfile.\n");
    zapfbot(idx);
    return;
  }
  if (dcc_total==MAXDCC) {
    putlog(LOG_MISC,"*","NO MORE DCC CONNECTIONS -- can't grab userfile");
    tprintf(dcc[idx].sock,"error I can't open a DCC to you; I'm full.\n");
    zapfbot(idx);
    return;
  }
  nsplit(ip,par); nsplit(port,par);
  i=dcc_total;
  dcc[i].addr=my_atoul(ip);
  dcc[i].port=atoi(port);
  dcc[i].sock=(-1);
  dcc[i].type=DCC_FORK;
  strcpy(dcc[i].nick,"*users");
  strcpy(dcc[i].host,dcc[idx].nick);
  dcc[i].u.other=NULL; set_fork(i);
  get_xfer_ptr(&(dcc[i].u.fork->u.xfer));
  strcpy(dcc[i].u.fork->u.xfer->filename,".share.users");
  dcc[i].u.fork->u.xfer->dir[0]=0;  /* this dir */
  dcc[i].u.fork->u.xfer->length=atol(par);
  dcc[i].u.fork->u.xfer->sent=0;
  dcc[i].u.fork->u.xfer->sofar=0;
  dcc[i].u.fork->u.xfer->f=fopen(".share.users","w");
  if (dcc[i].u.fork->u.xfer->f==NULL) {
    putlog(LOG_MISC,"*","CAN'T WRITE USERFILE DOWNLOAD FILE!");
    nfree(dcc[i].u.fork->u.xfer);
    nfree(dcc[i].u.fork);
    zapfbot(idx);
    return;
  }
  dcc[idx].u.bot->status|=STAT_GETTING;
  dcc[i].u.fork->type=DCC_SEND;
  dcc_total++;
  /* don't buffer this */
  dcc[i].sock=getsock(SOCK_BINARY);
  if (open_telnet_dcc(dcc[i].sock,ip,port) < 0) {
    putlog(LOG_MISC,"*","Asynchronous connection failed!");
    tprintf(dcc[idx].sock,"error Can't connect to you!\n");
    lostdcc(i);
    zapfbot(idx);
  }
}

void bot_resyncq PROTO2(int,idx,char *,par)
{
#ifndef ALLOW_RESYNC
  tprintf(dcc[idx].sock,"resync-no Not permitting resync.\n");
  return;
#else
  if (!share_users) tprintf(dcc[idx].sock,"resync-no Not sharing userfile.\n");
  else if (!(get_attr_handle(dcc[idx].nick) & BOT_SHARE))
    tprintf(dcc[idx].sock,"resync-no You are not +s for me.\n");
  else if (can_resync(dcc[idx].nick)) {
    tprintf(dcc[idx].sock,"resync!\n");
    dump_resync(dcc[idx].sock,dcc[idx].nick);
    dcc[idx].u.bot->status&=~STAT_OFFERED;
    dcc[idx].u.bot->status|=STAT_SHARE;
    putlog(LOG_BOTS,"*","Resync'd user file with %s",dcc[idx].nick);
  }
  else if (passive) {
    tprintf(dcc[idx].sock,"resync!\n");
    dcc[idx].u.bot->status&=~STAT_OFFERED;
    dcc[idx].u.bot->status|=STAT_SHARE;
    putlog(LOG_BOTS,"*","Resyncing user file from %s",dcc[idx].nick);
  }
  else tprintf(dcc[idx].sock,"resync-no No resync buffer.\n");
#endif
}

void bot_resync PROTO2(int,idx,char *,par)
{
  if (dcc[idx].u.bot->status&STAT_OFFERED) {
    if (can_resync(dcc[idx].nick)) {
      dump_resync(dcc[idx].sock,dcc[idx].nick);
      dcc[idx].u.bot->status&=~STAT_OFFERED;
      dcc[idx].u.bot->status|=STAT_SHARE;
      putlog(LOG_BOTS,"*","Resync'd user file with %s",dcc[idx].nick);
    }
  }
}

void bot_resync_no PROTO2(int,idx,char *,par)
{
  putlog(LOG_BOTS,"*","Resync refused by %s: %s",dcc[idx].nick,par);
  flush_tbuf(dcc[idx].nick);
  tprintf(dcc[idx].sock,"userfile?\n");
}

#define CHKSEND if (!(dcc[idx].u.bot->status&STAT_SENDING)) return
#define CHKGET if (!(dcc[idx].u.bot->status&STAT_GETTING)) return
#define CHKSHARE if (!(dcc[idx].u.bot->status&STAT_SHARE)) return

void bot_chpass PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chpass %s\n",par);
  noshare=1; nsplit(hand,par); change_pass_by_handle(hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: newpass %s",dcc[idx].nick,hand);
}

void bot_chhand PROTO2(int,idx,char *,par)
{
  char *hand=TBUF; int i;
  CHKSHARE;
  shareout_but(idx,"chhand %s\n",par);
  noshare=1; nsplit(hand,par); change_handle(hand,par); 
  notes_change(-1,hand,par);
  noshare=0;
  putlog(LOG_CMDS,"*","%s: handle %s->%s",dcc[idx].nick,hand,par);
  for (i=0; i<dcc_total; i++) if (strcasecmp(hand,dcc[i].nick)==0) {
    if ((dcc[i].type==DCC_CHAT) && (dcc[i].u.chat->channel>=0)) {
      chanout2(dcc[i].u.chat->channel,"Nick change: %s -> %s\n",
               dcc[i].nick,par);
      context;
      if (dcc[i].u.chat->channel < 100000) {
        tandout("part %s %s %d\n",botnetnick,dcc[i].nick,dcc[i].sock);
        tandout("join %s %s %d %c%d %s\n",botnetnick,par,dcc[i].u.chat->channel,
  	        geticon(i),dcc[i].sock,dcc[i].host);
      }
    }
    strcpy(dcc[i].nick,par);
  }
}

void bot_chattr PROTO2(int,idx,char *,par)
{
  char *hand=TBUF,*atr=TBUF+50,*s=TBUF+512; int oatr,natr;struct chanset_t *cst;
  CHKSHARE;
  shareout_but(idx,"chattr %s\n",par);
  noshare=1; nsplit(hand,par);
   /* If user is a bot with +s, ignore command */
  if (get_attr_handle(hand) & BOT_SHARE) {
    noshare=0;
    return;
  }
  nsplit(atr,par);
  if (par[0]) {
    if (defined_channel(par)) {
      cst=findchan(par);
      natr=str2chflags(atr);
      if (cst->stat&CHAN_SHARED) {
        set_chanattr_handle(hand,par,natr);
        noshare=0; chflags2str(get_chanattr_handle(hand,par),s);
        putlog(LOG_CMDS,"*","%s: chattr %s %s %s",dcc[idx].nick,hand,s,par);
      }
      else putlog(LOG_CMDS,"*","Rejected info for unshared channel %s from %s",
		  par,dcc[idx].nick);
    }
    return;
  }
  /* don't let bot flags be altered */
  oatr=(get_attr_handle(hand) & ~BOT_MASK);
  natr=str2flags(atr);
#ifdef PRIVATE_OWNER
  oatr|=(get_attr_handle(hand) & USER_OWNER);
  natr&=(~USER_OWNER);
#endif
  set_attr_handle(hand,(natr & BOT_MASK)|oatr);
  noshare=0; flags2str(get_attr_handle(hand),s);
  putlog(LOG_CMDS,"*","%s: chattr %s %s",dcc[idx].nick,hand,s);
}

void bot_newuser PROTO2(int,idx,char *,par)
{
  char *etc=TBUF,*etc2=TBUF+41,*etc3=TBUF+121;
  CHKSHARE;
  shareout_but(idx,"newuser %s\n",par);
  noshare=1; nsplit(etc,par); etc[40]=0;
   /* If user already exists, ignore command */
  if(is_user(etc)) {
    noshare=0;
    return;
  }
  nsplit(etc2,par); etc2[80]=0; nsplit(etc3,par);
  userlist=adduser(userlist,etc,etc2,etc3,atoi(par)); noshare=0;
  putlog(LOG_CMDS,"*","%s: newuser %s",dcc[idx].nick,etc);
}

void bot_killuser PROTO2(int,idx,char *,par)
{
  CHKSHARE;
  noshare=1;
  /* If user is a share bot, ignore command */
  if(get_attr_handle(par) & BOT_SHARE) {
    noshare=0;
    return;
  }
  if (deluser(par)) {
    noshare=0;
    shareout_but(idx,"killuser %s\n",par);
    putlog(LOG_CMDS,"*","%s: killuser %s",dcc[idx].nick,par);
  } 
  else noshare=0;
}

void bot_pls_upload PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"+upload %s\n",par);
  noshare=1; nsplit(hand,par); stats_add_upload(hand,atol(par)); noshare=0;
  /* no point logging this */
}

void bot_pls_dnload PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"+dnload %s\n",par);
  noshare=1; nsplit(hand,par); stats_add_dnload(hand,atol(par)); noshare=0;
  /* no point logging this either */
}

void bot_pls_host PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"+host %s\n",par);
  noshare=1; nsplit(hand,par); addhost_by_handle(hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: +host %s %s",dcc[idx].nick,hand,par);
}

void bot_pls_bothost PROTO2(int,idx,char *,par)
{
  char *hand=TBUF, *p=TBUF+512;
  CHKSHARE;
  shareout_but(idx,"+bothost %s\n",par);
  noshare=1; nsplit(hand,par);
  /* add bot to userlist if not there */
  if (is_user(hand)) {
    if (!(get_attr_handle(hand) & USER_BOT)) {
      noshare=0; return;   /* ignore */
    }
    addhost_by_handle(hand,par);
  }
  else {
    makepass(p);
    userlist=adduser(userlist,hand,par,p,USER_BOT);
  }
  noshare=0;
  putlog(LOG_CMDS,"*","%s: +host %s %s",dcc[idx].nick,hand,par);
}

void bot_mns_host PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"-host %s\n",par);
  noshare=1; nsplit(hand,par);
    /* If user is a share bot, ignore command */
  if (get_attr_handle(hand) & BOT_SHARE) {
    noshare=0;
    return;
  }
  delhost_by_handle(hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: -host %s %s",dcc[idx].nick,hand,par);
}

void bot_chemail PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chemail %s\n",par);
  noshare=1; nsplit(hand,par); set_handle_email(userlist,hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: change email %s",dcc[idx].nick,hand);
}

void bot_chdccdir PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chdccdir %s\n",par);
  noshare=1; nsplit(hand,par); set_handle_dccdir(userlist,hand,par); noshare=0;
}

void bot_chcomment PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chcomment %s\n",par);
  noshare=1; nsplit(hand,par); set_handle_comment(userlist,hand,par);
  noshare=0;
  putlog(LOG_CMDS,"*","%s: change comment %s",dcc[idx].nick,hand);
}

void bot_chinfo PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chinfo %s\n",par);
  noshare=1; nsplit(hand,par); set_handle_info(userlist,hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: change info %s",dcc[idx].nick,hand);
}

void bot_chchinfo PROTO2(int,idx,char *,par)
{
  char *hand=TBUF; char *chan=TBUF+512;struct chanset_t *cst;
  CHKSHARE;
  shareout_but(idx,"chchinfo %s\n",par);
  noshare=1; nsplit(hand,par); nsplit(chan,par);
  cst=findchan(chan);
  if (cst->stat&CHAN_SHARED) {
    set_handle_chaninfo(userlist,hand,chan,par);
    noshare=0;
    putlog(LOG_CMDS,"*","%s: change info %s %s",dcc[idx].nick,chan,hand);
  } 
  else putlog(LOG_CMDS,"*","Info line change from %s denied.  Channel %s not shared.",
	      dcc[idx].nick,chan);
}

void bot_chaddr PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"chaddr %s\n",par);
  noshare=1; nsplit(hand,par);
  /* add bot to userlist if not there */
  if (!is_user(hand)) userlist=adduser(userlist,hand,"none","-",USER_BOT);
  if (!(get_attr_handle(hand) & USER_BOT)) {
    noshare=0; return;   /* ignore */
  }
  set_handle_info(userlist,hand,par); noshare=0;
  putlog(LOG_CMDS,"*","%s: change address %s",dcc[idx].nick,hand);
}

void bot_clrxtra PROTO2(int,idx,char *,par)
{
  CHKSHARE;
  shareout_but(idx,"clrxtra %s\n",par);
  noshare=1; set_handle_xtra(userlist,par,""); noshare=0;
}

void bot_addxtra PROTO2(int,idx,char *,par)
{
  char *hand=TBUF;
  CHKSHARE;
  shareout_but(idx,"addxtra %s\n",par);
  noshare=1; nsplit(hand,par); add_handle_xtra(userlist,hand,par); noshare=0;
}

void bot_mns_ban PROTO2(int,idx,char *,par)
{
  CHKSHARE;
  shareout_but(idx,"-ban %s\n",par);
  putlog(LOG_CMDS,"*","%s: cancel ban %s",dcc[idx].nick,par);
  noshare=1; delban(par); noshare=0;
}

void bot_mns_banchan PROTO2(int,idx,char *,par)
{
  char *chname=TBUF; struct chanset_t *chan;
  CHKSHARE;
  shareout_but(idx,"-banchan %s\n",par);
  nsplit(chname,par);
  chan=findchan(chname); if (chan==NULL) return;
  putlog(LOG_CMDS,"*","%s: cancel ban %s on %s",dcc[idx].nick,par,chname);
  noshare=1; u_delban(chan->bans,par); noshare=0;
}

void bot_mns_ignore PROTO2(int,idx,char *,par)
{
  CHKSHARE;
  shareout_but(idx,"-ignore %s\n",par);
  putlog(LOG_CMDS,"*","%s: cancel ignore %s",dcc[idx].nick,par);
  noshare=1; delignore(par); noshare=0;
}

void bot_pls_ban PROTO2(int,idx,char *,par)
{
  time_t now=time(NULL),expire_time;
  char *ban=TBUF,*tm=TBUF+512,*from=TBUF+532;
  CHKSHARE;
  shareout_but(idx,"+ban %s\n",par);
  noshare=1; nsplit(ban,par); nsplit(tm,par); nsplit(from,par);
  if (from[strlen(from)-1]==':') from[strlen(from)-1]=0;
  putlog(LOG_CMDS,"*","%s: ban %s (%s: %s)",dcc[idx].nick,ban,from,par);
  /* new format? */
  if (tm[0]=='+') {
    /* time left */
    strcpy(tm,&tm[1]);
    expire_time=(time_t)atol(tm);
    if (expire_time!=0L) expire_time+=now;
  }
  else {
    expire_time=(time_t)atol(tm);
    if (expire_time!=0L) expire_time=(now-expire_time);
  }
  addban(ban,from,par,expire_time);
  noshare=0;
}

void bot_pls_banchan PROTO2(int,idx,char *,par)
{
  time_t now=time(NULL),expire_time; struct chanset_t *chan;
  char *ban=TBUF,*tm=TBUF+512,*chname=TBUF+600,*from=TBUF+700;
  CHKSHARE;
  shareout_but(idx,"+banchan %s\n",par);
  nsplit(ban,par); nsplit(tm,par); nsplit(chname,par); nsplit(from,par);
  if (from[strlen(from)-1]==':') from[strlen(from)-1]=0;
  chan=findchan(chname); if (chan==NULL) return;
  putlog(LOG_CMDS,"*","%s: ban %s on %s (%s:%s)",dcc[idx].nick,ban,chname,
	 from,par);
  noshare=1;
  /* new format? */
  if (tm[0]=='+') {
    /* time left */
    strcpy(tm,&tm[1]);
    expire_time=(time_t)atol(tm);
    if (expire_time!=0L) expire_time+=now;
  }
  else {
    expire_time=(time_t)atol(tm);
    if (expire_time!=0L) expire_time=(now-expire_time)+(60*ban_time);
  }
  u_addban(chan->bans,ban,from,par,expire_time);
  noshare=0;
}

/* +ignore <host> +<seconds-left> <from> <note> */
void bot_pls_ignore PROTO2(int,idx,char *,par)
{
  time_t now=time(NULL),expire_time;
  char *ign=TBUF,*from=TBUF+256,*ts=TBUF+512;
  CHKSHARE;
  shareout_but(idx,"+ignore %s\n",par);
  noshare=1; nsplit(ign,par);
  if (par[0]=='+') {
    /* new-style */
    nsplit(ts,par); strcpy(ts,&ts[1]);
    if (atoi(ts)==0) expire_time=0L;
    else expire_time=now+atoi(ts);
    nsplit(from,par); from[10]=0; par[65]=0;
  }
  else {
    if (atoi(par)==0) expire_time=0L;
    else expire_time=now+(60*ignore_time)-atoi(par);
    strcpy(from,dcc[idx].nick); strcpy(par,"-");
  }
  putlog(LOG_CMDS,"*","%s: ignore %s (%s: %s)",dcc[idx].nick,ign,from,par);
  addignore(ign,from,par,expire_time);
  noshare=0;
}

void bot_nlinked PROTO2(int,idx,char *,par)
{
  char *newbot=TBUF,*next=TBUF+512,*p; int reject=0,bogus=0,atr,i;
  nsplit(newbot,par); nsplit(next,par);
  if (strlen(newbot)>9) newbot[9]=0;
  if (strlen(next)>9) next[9]=0;
  if (!next[0]) {
    putlog(LOG_BOTS,"*","Invalid eggnet protocol from %s (zapfing)",
	   dcc[idx].nick);
    chatout("*** Disconnected %s (invalid bot)\n",dcc[idx].nick);
    tandout_but(idx,"chat %s Disconnected %s (invalid bot)\n",botnetnick,
		dcc[idx].nick);
    tprintf(dcc[idx].sock,"error invalid eggnet protocol for 'nlinked'\n");
    reject=1;
  }
  else if ((in_chain(newbot)) || (strcasecmp(newbot,botnetnick)==0)) {
    /* loop! */
    putlog(LOG_BOTS,"*","Detected loop: disconnecting %s (mutual: %s)",
	   dcc[idx].nick,newbot);
    chatout("*** Loop (%s): disconnected %s\n",newbot,dcc[idx].nick);
    tandout_but(idx,"chat %s Loop (%s): disconnected %s\n",botnetnick,newbot,
		dcc[idx].nick);
    tprintf(dcc[idx].sock,"error Loop (%s)\n",newbot);
    reject=1;
  }
  if (!reject) {
    for (p=newbot; *p; p++) if ((*p<32) || (*p==127)) bogus=1;
    i=nextbot(next); if (i!=idx) bogus=1;
  }
  if (bogus) {
    putlog(LOG_BOTS,"*","Bogus link notice from %s!  (%s -> %s)",dcc[idx].nick,
	   next,newbot);
    chatout("*** Bogus link notice: disconnecting %s\n",dcc[idx].nick);
    tandout_but(idx,"chat %s Bogus link notice: disconnecting %s\n",
		botnetnick,dcc[idx].nick);
    tprintf(dcc[idx].sock,"error Bogus link notice (%s -> %s)\n",next,newbot);
    reject=1;
  }
  atr=get_attr_handle(dcc[idx].nick);
  if (atr & BOT_LEAF) {
    putlog(LOG_BOTS,"*","Disconnecting leaf %s  (linked to %s)",dcc[idx].nick,
	   newbot);
    chatout("*** Illegal link by leaf %s (to %s): disconnecting\n",
	    dcc[idx].nick,newbot);
    tandout_but(idx,"chat %s Illegal link by leaf %s (to %s): disconnecting\n",
		botnetnick,dcc[idx].nick,newbot);
    tprintf(dcc[idx].sock,"error You are supposed to be a leaf!\n");
    reject=1;
  }
  if (reject) {
    tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
    cancel_user_xfer(idx);
    tprintf(dcc[idx].sock,"bye\n");
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  addbot(newbot,dcc[idx].nick,next);
  tandout_but(idx,"nlinked %s %s %s\n",newbot,next,par);
  check_tcl_link(newbot,next);
  if (get_attr_handle(newbot) & BOT_REJECT) {
    tprintf(dcc[idx].sock,"reject %s %s\n",botnetnick,newbot);
    putlog(LOG_BOTS,"*","Rejecting bot %s from %s",newbot,dcc[idx].nick);
  }
}

void bot_linked PROTO2(int,idx,char *,par)
{
  putlog(LOG_BOTS,"*","Older bot detected (unsupported)");
  chatout("*** Disconnected %s (outdated)\n",dcc[idx].nick);
  tandout_but(idx,"chat %s Disconnected %s (outdated)\n",botnetnick,
	      dcc[idx].nick);
  tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
  cancel_user_xfer(idx); 
  killsock(dcc[idx].sock); lostdcc(idx);
}

void bot_unlinked PROTO2(int,idx,char *,par)
{
  int i;
  char bot[512];
  nsplit(bot,par);
  i=nextbot(bot);
  if ((i>=0) && (i!=idx)) /* bot is NOT downstream along idx, so BOGUS! */
    fake_alert(idx);
  else if (i>=0) { /* valid bot downstream of idx */
    rembot(bot,dcc[idx].nick);
    unvia(idx,bot);
    tandout_but(idx,"unlinked %s\n",bot);
  } /* otherwise it's not even a valid bot, so just ignore! */
}

void bot_trace PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*dest=TBUF+512; int i;
  /* trace <from@bot> <dest> <chain:chain..> */
  nsplit(from,par); nsplit(dest,par);
  if (strcasecmp(dest,botnetnick)==0) {
    tprintf(dcc[idx].sock,"traced %s %s:%s\n",from,par,botnetnick);
  }
  else {
    i=nextbot(dest); if (i>=0)
      tprintf(dcc[i].sock,"trace %s %s %s:%s\n",from,dest,par,botnetnick);
  }
}

void bot_traced PROTO2(int,idx,char *,par)
{
  char *to=TBUF,*ss=TBUF+512,*p; int i,sock;
  /* traced <to@bot> <chain:chain..> */
  nsplit(to,par); p=strchr(to,'@');
  if (p==NULL) p=to; else { *p=0; p++; }
  if (strcasecmp(p,botnetnick)==0) {
    splitc(ss,to,':');
    if (ss[0]) sock=atoi(ss); else sock=(-1);
    for (i=0; i<dcc_total; i++)
      if ((dcc[i].type==DCC_CHAT) && (strcasecmp(dcc[i].nick,to)==0) &&
	  ((sock==(-1)) || (sock==dcc[i].sock)))
	dprintf(i,"Trace result -> %s\n",par);
  }
  else {
    i=nextbot(p); if (i>=0)
      tprintf(dcc[i].sock,"traced %s@%s %s\n",to,p,par);
  }
}

/* reject <from> <bot> */
void bot_reject PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*who=TBUF+81,*destbot=TBUF+41,*p; int i;
  nsplit(from,par); from[40]=0;
  p=strchr(from,'@'); if (p==NULL) p=from; else p++;
  i=nextbot(p); if (i!=idx) {
    fake_alert(idx);
    return;
  }
  if (strchr(par,'@')==NULL) {
    /* rejecting a bot */
    i=nextbot(par); if (i<0) {
      tprintf(dcc[idx].sock,"priv %s %s Can't reject %s (doesn't exist)\n",
	      botnetnick,from,par);
    }
    else if (strcasecmp(dcc[i].nick,par)==0) {
      /* i'm the connection to the rejected bot */
      putlog(LOG_BOTS,"*","%s rejected %s",from,dcc[i].nick);
      tprintf(dcc[i].sock,"bye\n");
      tandout_but(i,"unlinked %s\n",dcc[i].nick);
      tandout_but(i,"chat %s Disconnected %s (rejected by %s)\n",botnetnick,
		  dcc[i].nick,from);
      chatout("*** Disconnected %s (rejected by %s)\n",dcc[i].nick,from);
      cancel_user_xfer(i);
      killsock(dcc[i].sock); lostdcc(i);
    }
    else {
      if (i<0) tandout_but(idx,"reject %s %s\n",from,par);
      else tprintf(dcc[i].sock,"reject %s %s\n",from,par);
    }
  }
  else {    /* rejecting user */
    nsplit(destbot,par); destbot[40]=0;
    splitc(who,destbot,'@');
    if (strcasecmp(destbot,botnetnick)==0) {
      /* kick someone here! */
      int ok=0;
#ifdef SHAREBOT_BOOTS
      p=strchr(from,'@'); if (p==NULL) p=from; else p++;
      if (!(get_attr_handle(p) & BOT_SHARE)) {
	add_note(from,botnetnick,"Remote boots are not allowed.",-1,0);
	return;
      }
#endif
#ifdef REMOTE_BOOTS
      for (i=0; (i<dcc_total) && (!ok); i++) 
	if ((strcasecmp(who,dcc[i].nick)==0) && (dcc[i].type==DCC_CHAT)) {
#ifdef OWNER
	  int atr=get_attr_handle(dcc[i].nick);
	  if (atr&USER_OWNER) {
	    add_note(from,botnetnick,"Can't boot the bot owner.",-1,0);
	    return;
	  }
#endif
	  do_boot(i,from,par); ok=1;
 	  putlog(LOG_CMDS,"*","#%s# boot %s (%s)",from,dcc[i].nick,par);
	}
#else
      tprintf(dcc[idx].sock,"priv %s %s Remote boots are not allowed.\n",
	      botnetnick,from);
      ok=ok;
#endif
    }
    else {
      i=nextbot(destbot);
      if (i<0) tandout_but(idx,"reject %s %s@%s %s\n",from,who,destbot,par);
      else tprintf(dcc[i].sock,"reject %s %s@%s %s\n",from,who,destbot,par);
    }
  }
}

void bot_thisbot PROTO2(int,idx,char *,par)
{
  if (strcasecmp(par,dcc[idx].nick)!=0) {
    putlog(LOG_BOTS,"*","Wrong bot: wanted %s, got %s",dcc[idx].nick,par);
    tprintf(dcc[idx].sock,"bye\n");
    tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
    tandout_but(idx,"chat %s Disconnected %s (imposter)\n",botnetnick,
		dcc[idx].nick);
    chatout("*** Disconnected %s (imposter)\n",dcc[idx].nick);
    unvia(idx,dcc[idx].nick);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  if (get_attr_handle(par) & BOT_LEAF)
    dcc[idx].u.bot->status|=STAT_LEAF;
  /* set capitalization the way they want it */
  noshare=1; change_handle(dcc[idx].nick,par); noshare=0;
  strcpy(dcc[idx].nick,par);
}

void bot_handshake PROTO2(int,idx,char *,par)
{
  change_pass_by_handle(dcc[idx].nick,par);
  if ((dcc[idx].u.bot->status&STAT_SHARE) &&
      (!(dcc[idx].u.bot->status&STAT_GETTING)) &&
      (!(dcc[idx].u.bot->status&STAT_SENDING))) {
    noshare=1; change_pass_by_handle(botnetnick,par); noshare=0;
  /* i have no idea what that is supposed to accomplish ^ robey 18dec96 */
  }
}

void bot_trying PROTO2(int,idx,char *,par)
{
  tandout_but(idx,"trying %s\n",par);
  /* currently ignore */
}

void bot_end_trying PROTO2(int,idx,char *,par)
{
  tandout_but(idx,"*trying %s\n",par);
  /* currently ignore */
}

/* used to send a direct msg from Tcl on one bot to Tcl on another */
/* zapf <frombot> <tobot> <code [param]> */
void bot_zapf PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*to=TBUF+512; int i;
  nsplit(from,par); nsplit(to,par);
  i=nextbot(from); if (i!=idx) {
    fake_alert(idx);
    return;
  }
  if (strcasecmp(to,botnetnick)==0) {
    /* for me! */
    char opcode[512];
    nsplit(opcode,par);
    check_tcl_bot(from,opcode,par);
    return;
  }
  i=nextbot(to); if (i>=0)
    tprintf(dcc[i].sock,"zapf %s %s %s\n",from,to,par);
}

/* used to send a global msg from Tcl on one bot to every other bot */
/* zapf-broad <frombot> <code [param]> */
void bot_zapfbroad PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*opcode=TBUF+512; int i;
  nsplit(from,par);
  nsplit(opcode,par);
  i=nextbot(from); if (i!=idx) {
    fake_alert(idx); return;
  }
  check_tcl_bot(from,opcode,par);
  tandout_but(idx,"zapf-broad %s %s %s\n",from,opcode,par);
}

/* show motd to someone */
void bot_motd PROTO2(int,idx,char *,par)
{
  FILE *vv; char *s=TBUF,*who=TBUF+512,*p; int i;
  nsplit(who,par);
  if ((!par[0]) || (strcasecmp(par,botnetnick)==0)) {
    p=strchr(who,':'); if (p==NULL) p=who; else p++;
    putlog(LOG_CMDS,"*","#%s# motd",p);
    vv=fopen(motdfile,"r"); if (vv!=NULL) {
      tprintf(dcc[idx].sock,"priv %s %s --- MOTD file:\n",botnetnick,who);
      help_subst(NULL,NULL,0,1);
      while (!feof(vv)) {
	fgets(s,120,vv);
	if (!feof(vv)) {
	  if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
	  if (!s[0]) strcpy(s," ");
	  help_subst(s,who,USER_BOT,1);
	  if (s[0]) tprintf(dcc[idx].sock,"priv %s %s %s\n",botnetnick,who,s);
	}
      }
      fclose(vv);
    }
    else tprintf(dcc[idx].sock,"priv %s %s No MOTD file. :(\n",botnetnick,
		 who);
  }
  else {
    /* pass it on */
    i=nextbot(par); if (i>=0) tprintf(dcc[i].sock,"motd %s %s\n",who,par);
  }
}

#ifdef MODULES
extern void (*do_bot_assoc) PROTO((int,char *));
/* assoc [link-flag] <chan#> <name> */
/* link-flag is Y if botlinking */
void bot_assoc PROTO2(int,idx,char *,par)
{
  context;
  do_bot_assoc(idx,par);
}
#endif

/* filereject <bot:filepath> <sock:nick@bot> <reason...> */
void bot_filereject PROTO2(int,idx,char *,par)
{
  char *path=TBUF,*tobot=TBUF+512,*to=TBUF+542,*p; int i;
  nsplit(path,par); nsplit(tobot,par); splitc(to,tobot,'@');
  if (strcasecmp(tobot,botnetnick)==0) {  /* for me! */
    p=strchr(to,':'); if (p!=NULL) {
      *p=0;
      for (i=0; i<dcc_total; i++) {
	if (dcc[i].sock==atoi(to))
	  dprintf(i,"FILE TRANSFER REJECTED (%s): %s\n",path,par);
      }
      *p=':';
    }
    /* no ':'? malformed */
    putlog(LOG_FILES,"*","%s rejected: %s",path,par);
  }
  else {   /* pass it on */
    i=nextbot(tobot); if (i>=0)
      tprintf(dcc[i].sock,"filereject %s %s@%s %s\n",path,to,tobot,par);
  }
}

/* filreq <sock:nick@bot> <bot:file> */
void bot_filereq PROTO2(int,idx,char *,par)
{
  char *from=TBUF,*tobot=TBUF+41; int i;
  nsplit(from,par); splitc(tobot,par,':');
  if (strcasecmp(tobot,botnetnick)==0) {  /* for me! */
    /* process this */
#ifdef MODULES
    if (remote_filereq == NULL)
#endif
#if defined(MODULES) || defined(NO_FILE_SYSTEM)
       tprintf(dcc[idx].sock,"priv %s %s I have no file system to grab files from.\n",
	       botnetnick,from);
#endif
#ifdef MODULES
     else 
#endif
#ifndef NO_FILE_SYSTEM
       remote_filereq(idx,from,par);
#endif
  }
  else {   /* pass it on */
    i=nextbot(tobot); if (i>=0)
      tprintf(dcc[i].sock,"filereq %s %s:%s\n",from,tobot,par);
  }
}

/* filesend <bot:path> <sock:nick@bot> <IP#> <port> <size> */
void bot_filesend PROTO2(int,idx,char *,par)
{
  char *botpath=TBUF,*nick=TBUF+512,*tobot=TBUF+552,*sock=TBUF+692; int i;
  char *nfn;
  nsplit(botpath,par); nsplit(tobot,par); splitc(nick,tobot,'@');
  splitc(sock,nick,':');
  if (strcasecmp(tobot,botnetnick)==0) {  /* for me! */
    nfn=strrchr(botpath,'/');
    if (nfn==NULL) {
      nfn=strrchr(botpath,':');
      if (nfn==NULL) nfn=botpath;    /* that's odd. */
      else nfn++;
    }
    else nfn++;
    /* send it to 'nick' as if it's from me */
    mprintf(serv,"PRIVMSG %s :\001DCC SEND %s %s\001\n",nick,nfn,par);
  }
  else {
    i=nextbot(tobot); if (i>=0)
      tprintf(dcc[i].sock,"filesend %s %s:%s@%s %s\n",botpath,sock,nick,
	      tobot,par);
  }
}

void bot_error PROTO2(int,idx,char *,par)
{
  putlog(LOG_MISC|LOG_BOTS,"*","%s: %s",dcc[idx].nick,par);
}

/* join <bot> <nick> <chan> <flag><sock> <from> */
void bot_join PROTO2(int,idx,char *,par)
{
  char *bot=TBUF,*nick=TBUF+20,*x=TBUF+40,*y=TBUF+60,*from=TBUF+70;
  int i,sock;
  nsplit(bot,par); bot[9]=0;
  nsplit(nick,par); nick[9]=0;
  nsplit(x,par); x[10]=0;
  nsplit(y,par); /* only first char matters */
  if (!y[0]) { y[0]='-'; sock=0; }
  else sock=atoi(&y[1]);
  /* 1.1 bots always send a sock#, even on a channel change */
  /* so if sock# is 0, this is from an old bot and we must tread softly */
  /* grab old sock# if there is one, otherwise make up one */
  if (sock==0) sock=partysock(bot,nick);
  if (sock==0) sock=fakesock++;
  strncpy(from,par,40); from[40]=0;
  i=nextbot(bot); if (i!=idx) return;  /* garbage sent by 1.0g bot */
  addparty(bot,nick,atoi(x),y[0],sock,from);
  tandout_but(idx,"join %s %s %d %c%d %s\n",bot,nick,atoi(x),y[0],sock,from);
  check_tcl_chjn(bot,nick,atoi(x),y[0],sock,from);
}

/* part <bot> <nick> <sock> [etc..] */
void bot_part PROTO2(int,idx,char *,par)
{
  char *bot=TBUF,*nick=TBUF+20,*etc=TBUF+40; int sock;
  nsplit(bot,par); bot[9]=0;
  nsplit(nick,par); nick[9]=0;
  nsplit(etc,par); sock=atoi(etc);
  if (sock==0) sock=partysock(bot,nick);
  check_tcl_chpt(bot,nick,sock);
  remparty(bot,sock);
  tandout_but(idx,"part %s %s %d %s\n",bot,nick,sock,par);
}

/* away <bot> <sock> <message> */
void bot_away PROTO2(int,idx,char *,par)
{
  char *bot=TBUF,*etc=TBUF+20; int sock;
  nsplit(bot,par); bot[9]=0;
  nsplit(etc,par); sock=atoi(etc);
  if (sock==0) sock=partysock(bot,etc);
  partystat(bot,sock,PLSTAT_AWAY,0);
  partyaway(bot,sock,par);
  tandout_but(idx,"away %s %d %s\n",bot,sock,par);
}

/* unaway <bot> <sock> */
void bot_unaway PROTO2(int,idx,char *,par)
{
  char *bot=TBUF,*etc=TBUF+20; int sock;
  nsplit(bot,par); bot[9]=0;
  nsplit(etc,par); sock=atoi(etc);
  if (sock==0) sock=partysock(bot,etc);
  partystat(bot,sock,0,PLSTAT_AWAY);
  tandout_but(idx,"unaway %s %d %s\n",bot,sock,par);
}

/* (a courtesy info to help during connect bursts) */
/* idle <bot> <sock> <#secs> */
void bot_idle PROTO2(int,idx,char *,par)
{
  char *bot=TBUF,*etc=TBUF+20; int sock;
  nsplit(bot,par); bot[9]=0;
  nsplit(etc,par); sock=atoi(etc);
  if (sock==0) sock=partysock(bot,etc);
  partysetidle(bot,sock,atoi(par));
  tandout_but(idx,"idle %s %d %s\n",bot,sock,par);
}

void bot_stick(idx,par)
int idx; char *par;
{
   char *host=TBUF,*val=TBUF+512;
   int yn;
   
   nsplit(host,par);
   nsplit(val,par);
   yn = atoi(val);
   noshare = 1;
   if (!par[0]) {/* global ban */
      if (setsticky_ban(par,yn) >0)
	putlog(LOG_CMDS,"*","%s: stick %s %c",dcc[idx].nick,host,yn?'y':'n');
   } else {
      struct chanset_t * chan=findchan(dcc[idx].u.chat->con_chan);
      if (chan != NULL) 
	if (u_setsticky_ban(chan->bans,par,yn)>0)
	  putlog(LOG_CMDS,"*","%s: stick %s %c %s",dcc[idx].nick,host,
		 yn?'y':'n',par);
   }
   noshare = 0;
}
