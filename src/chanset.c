/* 
   chanset.c -- handles:
     low-level channel and chanset manipulation
     channel pointers to the userrec cache
     check expired channel stuff

   dprintf'ized, 5feb96
   multi-channel, 6feb96
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
#include <sys/stat.h>
#include "eggdrop.h"
#include "users.h"
#include "chan.h"
#include "proto.h"

extern char botname[];
extern char newbotname[];
extern int serv;
extern int ban_time;
extern char origbotname[];
extern char ver[];
extern int nulluser;

/* data for each channel */
struct chanset_t *chanset=NULL;
/* where channel records are stored */
char chanfile[121]="";
/* time to wait for user to return for net-split */
int wait_split = 300;


/* memory expected to be used by this module */
int expmem_chan()
{
  int tot=0; banlist *b; struct chanset_t *chan=chanset;
  context;
  while (chan!=NULL) {
    tot+=sizeof(struct chanset_t);
    tot+=strlen(chan->channel.key)+1;
    tot+=(sizeof(struct memstruct)*(chan->channel.members+1));
    b=chan->channel.ban; while (b!=NULL) {
      tot+=strlen(b->ban)+1;
      if (b->ban[0]) tot+=strlen(b->who)+1;
      tot+=sizeof(struct banstruct);
      b=b->next;
    }
    chan=chan->next;
  }
  return tot;
}    

/* find a chanset by channel name */
struct chanset_t *findchan PROTO1(char *,name)
{
  struct chanset_t *chan=chanset;
  while (chan!=NULL) {
    if (strcasecmp(chan->name,name)==0) return chan;
    chan=chan->next;
  }
  return NULL;
}

/* get pointer to new chanset */
struct chanset_t *newchanset()
{
  struct chanset_t *c;
  c=(struct chanset_t *)nmalloc(sizeof(struct chanset_t));
  return c;
}

/* add a chanset pointer to the list */
void addchanset PROTO1(struct chanset_t *,chan)
{
  struct chanset_t *c=chanset,*old=NULL;
  chan->next=NULL;
  while (c!=NULL) {
    old=c; c=c->next;
  }
  if (old!=NULL) old->next=chan;
  else chanset=chan;
}

/* destroy a chanset in the list */
/* does NOT free up memory associated with channel data inside the chanset! */
int killchanset PROTO1(char *,name)
{
  struct chanset_t *c=chanset,*old=NULL;
  while (c!=NULL) {
    if (strcasecmp(c->name,name)==0) {
      if (old!=NULL) old->next=c->next;
      else chanset=c->next;
      nfree(c); return 1;
    }
    old=c; c=c->next;
  }
  return 0;
}

/* get channels list */
void getchanlist PROTO2(char *,s,int,maxlen)
{
  struct chanset_t *chan=chanset;
  s[0]=0;
  while (chan!=NULL) {
    if (strlen(s)+strlen(chan->name)+1 > maxlen) return;
    if (s[0]) strcat(s," ");
    strcat(s,chan->name);
    chan=chan->next;
  }
}

/* set the key */
void set_key PROTO2(struct chanset_t *,chan, char *,k)
{
  nfree(chan->channel.key);
  if (k==NULL) {
    chan->channel.key=(char *)nmalloc(1); chan->channel.key[0]=0;
    return;
  }
  chan->channel.key=(char *)nmalloc(strlen(k)+1);
  strcpy(chan->channel.key,k);
}

/* is this channel +s/+p? */
int channel_hidden PROTO1(struct chanset_t *,chan)
{
  return (chan->channel.mode&(CHANPRIV|CHANSEC));
}

/* is this channel +t? */
int channel_optopic PROTO1(struct chanset_t *,chan)
{
  return (chan->channel.mode&CHANTOPIC);
}

int hand_on_chan PROTO2(struct chanset_t *,chan, char *,handle)
{
  char s[UHOSTLEN],h[10];
  memberlist *m=chan->channel.member;
  while (m->nick[0]) {
    sprintf(s,"%s!%s",m->nick,m->userhost);
    get_handle_by_host(h,s);
    if (strcasecmp(h,handle)==0) return 1;
    m=m->next;
  }
  return 0;
}

/* initialize out the channel record */
void init_channel PROTO1(struct chanset_t *,chan)
{
  chan->channel.maxmembers=(-1);  
  chan->channel.mode=0; chan->channel.members=0;
  chan->channel.key=(char *)nmalloc(1); chan->channel.key[0]=0;
  chan->channel.ban=(banlist *)nmalloc(sizeof(banlist));
  chan->channel.ban->ban=(char *)nmalloc(1); chan->channel.ban->ban[0]=0;
  chan->channel.ban->who=NULL;
  chan->channel.ban->next=NULL;
  chan->channel.member=(memberlist *)nmalloc(sizeof(memberlist));
  chan->channel.member->nick[0]=0; chan->channel.member->next=NULL;
  chan->channel.topic[0]=0;
}

/* clear out channel data from memory */
void clear_channel PROTO2(struct chanset_t *,chan, int,reset)
{
  memberlist *m,*m1; banlist *b,*b1;
  nfree(chan->channel.key);
  m=chan->channel.member;
  while (m!=NULL) {
    m1=m->next; nfree(m); m=m1;
  }
  b=chan->channel.ban;
  while (b!=NULL) {
    b1=b->next; if (b->ban[0]) nfree(b->who); nfree(b->ban); nfree(b); b=b1; 
  }
  if (reset) init_channel(chan);
}

/* reset all the channels, as if we just left a server or something */
void clear_channels()
{
  struct chanset_t *chan;
  chan=chanset; while (chan!=NULL) {
    clear_channel(chan,1);
    chan->stat&=~(CHANPEND|CHANACTIVE);
    chan=chan->next;
  }
}

/* shortcut for get_user_by_host -- might have user record in one */
/* of the channel caches */
struct userrec *check_chanlist PROTO1(char *,host)
{
  char nick[NICKLEN],uhost[UHOSTLEN],s1[NICKLEN]; memberlist *m;
  struct chanset_t *chan;
  chan=chanset;
  strncpy(uhost,host,UHOSTLEN); uhost[UHOSTLEN-1]=0;
  strncpy(s1,host,NICKLEN); s1[NICKLEN-1]=0;
  splitnick(nick,s1);
  while (chan!=NULL) {
    m=chan->channel.member;
    while (m->nick[0]) {
      if ((strcasecmp(uhost,m->userhost)==0) && (strcasecmp(nick,m->nick)==0))
	return m->user;
      m=m->next;
    }
    chan=chan->next;
  }
  return NULL;
}

/* shortcut for get_user_by_handle -- might have user record in channels */
struct userrec *check_chanlist_hand PROTO1(char *,hand)
{
  struct chanset_t *chan=chanset; memberlist *m;
  while (chan!=NULL) {
    m=chan->channel.member;
    while (m->nick[0]) {
      if (m->user!=NULL) if (strcasecmp(m->user->handle,hand)==0)
	return m->user;
      m=m->next;
    }
    chan=chan->next;
  }
  return NULL;
}

/* clear the user pointers in the chanlists */
/* (necessary when a hostmask is added/removed or a user is added) */
void clear_chanlist()
{
  memberlist *m; struct chanset_t *chan=chanset;
  while (chan!=NULL) {
    m=chan->channel.member;
    while (m->nick[0]) {
      m->user=NULL;
      m=m->next;
    }
    chan=chan->next;
  }
}

/* if this user@host is in a channel, set it (it was null) */
void set_chanlist PROTO2(char *,host, struct userrec *,rec)
{
  char nick[NICKLEN],uhost[UHOSTLEN]; memberlist *m;
  struct chanset_t *chan=chanset;
  strcpy(uhost,host); splitnick(nick,uhost);
  while (chan!=NULL) {
    m=chan->channel.member;
    while (m->nick[0]) {
      if ((strcasecmp(uhost,m->userhost)==0) && (strcasecmp(nick,m->nick)==0))
	m->user=rec;
      m=m->next;
    }
    chan=chan->next;
  }
}

int defined_channel PROTO1(char *,name)
{
  struct chanset_t *chan;
  chan=findchan(name);
  if (chan==NULL) return 0;
  return 1;
}

int active_channel PROTO1(char *,name)
{
  struct chanset_t *chan;
  chan=findchan(name);
  if (chan==NULL) return 0;
  if (chan->stat&CHANACTIVE) return 1;
  return 0;
}

/* returns a pointer to a new channel member structure */
memberlist *newmember PROTO1(struct chanset_t *,chan)
{
  memberlist *x;
  x=chan->channel.member;
  while (x->nick[0]) x=x->next;
  x->next=(memberlist *)nmalloc(sizeof(memberlist));
  x->next->next=NULL;
  x->next->nick[0]=0;
  x->next->split=0L;
  x->next->last=0L;
  chan->channel.members++;
  return x;
}

/* adds a ban to the list */
void newban PROTO3(struct chanset_t *,chan, char *,s, char *,who)
{
  banlist *b;
  b=chan->channel.ban;
  while ((b->ban[0]) && (strcasecmp(b->ban,s)!=0)) b=b->next;
  if (b->ban[0]) return;  /* already existent ban */
  b->next=(banlist *)nmalloc(sizeof(banlist));
  b->next->next=NULL;
  b->next->ban=(char *)nmalloc(1); b->next->ban[0]=0;
  nfree(b->ban); b->ban=(char *)nmalloc(strlen(s)+1);
  strcpy(b->ban,s);
  b->who=(char *)nmalloc(strlen(who)+1);
  strcpy(b->who,who);
  b->timer=time(NULL);
}

/* removes a nick from the channel member list (returns 1 if successful) */
int killmember PROTO2(struct chanset_t *,chan, char *,nick)
{
  memberlist *x,*old;
  x=chan->channel.member; old=NULL;
  while ((x->nick[0]) && (strcasecmp(x->nick,nick)!=0)) { old=x; x=x->next; }
  if ((x->nick[0]==0) && (!(chan->channel.mode & CHANPEND))) {
    putlog(LOG_MISC,"*","(!) killmember(%s) -> nonexistent",nick);
    return 0;
  }
  if (old==NULL) chan->channel.member=x->next;
  else old->next=x->next;
  nfree(x); chan->channel.members--; return 1;
}

/* removes a ban from the list */
int killban PROTO2(struct chanset_t *,chan, char *,s)
{
  banlist *b,*old;
  b=chan->channel.ban; old=NULL;
  while ((b->ban[0]) && (strcasecmp(b->ban,s)!=0)) { old=b; b=b->next; }
  if (b->ban[0]==0) return 0;
  if (old==NULL) chan->channel.ban=b->next;
  else old->next=b->next;
  nfree(b->ban); nfree(b->who); nfree(b); return 1;
}

/* returns memberfields if the nick is in the member list */
memberlist *ismember PROTO2(struct chanset_t *,chan, char *,nick)
{
  memberlist *x;
  x=chan->channel.member;
  while ((x->nick[0]) && (strcasecmp(x->nick,nick)!=0)) x=x->next;
  if (x->nick[0]==0) return NULL;
  return x;
}

/* boolean form for other modules to use */
int ischanmember PROTO2(char *,chname,char *,nick)
{
  struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) return 0;
  return (ismember(chan,nick)!=NULL);
}

/* am i a chanop? */
int me_op PROTO1(struct chanset_t *,chan)
{
  memberlist *mx=NULL;
  if (newbotname[0]) mx=ismember(chan,newbotname);
  if (mx==NULL) mx=ismember(chan,botname);
  if (mx==NULL) return 0;
  if (mx->flags&CHANOP) return 1; else return 0;
}

/* are there any ops on the channel? */
int any_ops PROTO1(struct chanset_t *,chan)
{
  memberlist *x=chan->channel.member;
  while ((x->nick[0]) && (!(x->flags&CHANOP))) x=x->next;
  if (x->nick[0]==0) return 0;
  return 1;
}

/* returns true if this is one of the channel bans */
int isbanned PROTO2(struct chanset_t *,chan, char *,user)
{
  banlist *b;
  b=chan->channel.ban;
  while ((b->ban[0]) && (strcasecmp(b->ban,user)!=0)) b=b->next;
  if (b->ban[0]==0) return 0;
  return 1;
}

void getchanhost PROTO3(char *,chname,char *,nick,char *,host)
{
  struct chanset_t *chan; memberlist *m;
  host[0]=0;
  chan=findchan(chname); if (chan==NULL) return;
  m=ismember(chan,nick); if (m==NULL) return;
  strcpy(host,m->userhost);
}

int is_split PROTO2(char *,chname,char *,nick)
{
  memberlist *m; struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) return 0;
  m=ismember(chan,nick); if (m==NULL) return 0;
  return (int)(m->split);
}

/* called only if dynamic-bans is on */
void check_expired_chanbans()
{
  banlist *b; time_t now=time(NULL); struct chanset_t *chan;
  chan=chanset; while (chan!=NULL) {
    if ((chan->stat&CHAN_DYNAMICBANS) && (me_op(chan))) {
      b=chan->channel.ban;
      while (b->ban[0]) {
	if ((now - b->timer > 60*ban_time) &&
	    !u_sticky_ban(chan->bans,b->ban) && !sticky_ban(b->ban)) {
	  putlog(LOG_MODES,chan->name,"(%s) Channel ban on %s expired.",
		 chan->name,b->ban);
	  add_mode(chan,'-','b',b->ban);
	  b->timer=time(NULL);  /* reset timer to avoid repititive deban */
	}
	b=b->next;
      }
    }
    chan=chan->next;
  }
}

/* kick anyone off the channel who matches a ban */
void kick_match_ban PROTO2(struct chanset_t *,chan, char *,ban)
{
  memberlist *m; char s[UHOSTLEN];
  if (!(chan->stat&CHAN_ENFORCEBANS)) return;
  m=chan->channel.member; 
  while (m->nick[0]) {
    sprintf(s,"%s!%s",m->nick,m->userhost);
    if ((wild_match(ban,s)) && (strcasecmp(m->nick,botname)!=0))
      mprintf(serv,"KICK %s %s :banned\n",chan->name,m->nick);
    m=m->next;
  }
}

/* kick everyone on the channel with matching 
   hostmask that joined since the time stamp */
void kick_match_since PROTO3(struct chanset_t *,chan, char *,mask, 
			     time_t,timestamp)
{
  char s[UHOSTLEN]; memberlist *m=chan->channel.member; 
  while (m->nick[0]) {
    sprintf(s,"%s!%s",m->nick,m->userhost);
    if ((wild_match(mask,s)) && (m->joined>=timestamp))
      mprintf(serv,"KICK %s %s :lemmingbot\n",chan->name,m->nick);
    m=m->next;
  }
}

/* resets the bans on the channel */
void resetbans PROTO1(struct chanset_t *,chan)
{
  banlist *b=chan->channel.ban;
  if (!me_op(chan)) return;   /* can't do it */
  /* remove bans we didn't put there */
  while (b->ban[0]) {
    if ((!equals_ban(b->ban)) && (!u_equals_ban(chan->bans,b->ban)))
      add_mode(chan,'-','b',b->ban);
    b=b->next;
  }
  /* make sure the intended bans are still there */
  recheck_bans(chan);
}

/* remove any bogus bans */
void kill_bogus_bans PROTO1(struct chanset_t *,chan)
{
  banlist *b=chan->channel.ban; int bogus,i;
  if (!me_op(chan)) return;
  while (b->ban[0]) {
    bogus=0;
    for (i=0; i<strlen(b->ban); i++)
      if ((b->ban[i]<32) || (b->ban[i]>126)) bogus=1;
    if (bogus) add_mode(chan,'-','b',b->ban);
    b=b->next;
  }
}

/* check that this person has ops on all AUTOOP channels */
void recheck_ops PROTO2(char *,nick,char *,hand)
{
  struct chanset_t *chan=chanset; int chatr,atr=get_attr_handle(hand);
  while (chan!=NULL) {
    chatr=get_chanattr_handle(hand,chan->name);
    if ((chan->stat & CHAN_OPONJOIN) &&
	((chatr & CHANUSER_OP) || 
	 ((atr & USER_GLOBAL) && !(chatr&CHANUSER_DEOP))))
      add_mode(chan,'+','o',nick);
    chan=chan->next;
  }
}

/* things to do when i just became a chanop: */
void recheck_channel PROTO1(struct chanset_t *,chan)
{
  memberlist *m; char s[UHOSTLEN],hand[10]; int chatr,atr;
  /* okay, sort through who needs to be deopped. */
  m=chan->channel.member; while (m->nick[0]) {
    sprintf(s,"%s!%s",m->nick,m->userhost);
    get_handle_by_host(hand,s);
    chatr=get_chanattr_handle(hand,chan->name);
    atr=get_attr_handle(hand);
    /* ignore myself */
    if ((newbotname[0]) && (strcasecmp(m->nick,newbotname)==0)) { /* skip */ }
    else if ((!newbotname[0]) && (strcasecmp(m->nick,botname)==0)) {
      /* skip */
    }
    else {
      if ((m->flags&CHANOP) && ((chatr & CHANUSER_DEOP) ||
	  ((atr & USER_DEOP) && !(chatr & CHANUSER_OP))) &&
	  ((chan->stat&CHAN_BITCH) &&
	  (!(chatr & CHANUSER_OP) && 
	   !((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)))))
	add_mode(chan,'-','o',m->nick);
      if ((!(m->flags&CHANOP)) &&
	  ((chatr & CHANUSER_OP) || 
	   ((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))) &&
	  (chan->stat&CHAN_OPONJOIN))
	add_mode(chan,'+','o',m->nick);
      if ((chan->stat&CHAN_ENFORCEBANS) &&
	  ((match_ban(s)) || (u_match_ban(chan->bans,s))))
	refresh_ban_kick(chan,s,m->nick);
      /* ^ will use the ban comment */
      else if ((chatr & CHANUSER_KICK) || (atr & USER_KICK)) {
	quickban(chan,m->userhost);
	get_handle_comment(hand,s);
	if (!s[0])
	  mprintf(serv,"KICK %s %s :...and thank you for playing.\n",
		  chan->name,m->nick);
	else mprintf(serv,"KICK %s %s :%s\n",chan->name,m->nick,s);
      }
    }
    m=m->next;
  }
  recheck_bans(chan);
  recheck_chanmode(chan);
}

/* recheck ALL channels */
void recheck_channels()
{
  struct chanset_t *chan;
  chan=chanset; while (chan!=NULL) {
    recheck_channel(chan); chan=chan->next;
  }
}

void newly_chanop PROTO1(struct chanset_t *,chan)
{
  recheck_channel(chan);
}

/* is user x a chanop? */
int member_op PROTO2(char *,chname,char *,x)
{
  memberlist *mx; struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) return 0;
  mx=ismember(chan,x); if (mx==NULL) return 0;
  if (mx->flags&CHANOP) return 1; else return 0;
}

/* is user x a voice? (+v) */
int member_voice PROTO2(char *,chname,char *,x)
{
  memberlist *mx; struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) return 0;
  mx=ismember(chan,x); if (mx==NULL) return 0;
  if (mx->flags&CHANVOICE) return 1; else return 0;
}

/* reset the channel information */
void reset_chan_info PROTO1(struct chanset_t *,chan)
{
  clear_channel(chan,1);
  chan->stat|=CHANPEND;
  chan->stat&=~CHANACTIVE;
  mprintf(serv,"WHO %s\n",chan->name);
  mprintf(serv,"MODE %s\n",chan->name);
  if (me_op(chan)) newly_chanop(chan);
}

/*  if i'm the only person on the channel, and i'm not op'd,   *
 *  might as well leave and rejoin. If i'm NOT the only person *
 *  on the channel, but i'm still not op'd, demand ops         */
void check_lonely_channel PROTO1(struct chanset_t *,chan)
{
  memberlist *m; char s[UHOSTLEN]; int i=0; static int whined=0;
  context;
  if (chan->stat&CHANPEND) return;
  m=chan->channel.member;
  if ((chan->stat&CHANACTIVE) && (!me_op(chan))) {
    /* count non-split channel members */
    while (m->nick[0]) {
      if (m->split==0L) i++;
      m=m->next;
    }
    if (i==1) {
      putlog(LOG_MISC,"*","Trying to cycle %s to regain ops.",chan->name);
      mprintf(serv,"PART %s\n",chan->name);
      mprintf(serv,"JOIN %s %s\n",chan->name,chan->key_prot);
      whined=0;
    }
    else if (any_ops(chan)) {
      whined=0;
      if (chan->need_op[0]) do_tcl("need-op",chan->need_op);
    }
    else {
      /* other people here, but none are ops */
      /* are there other bots?  make them LEAVE. */
      int ok=1;
      if (!whined) {
	putlog(LOG_MISC,"*","%s is active but has no ops :(",chan->name);
	whined=1;
      }
      m=chan->channel.member;
      while (m->nick[0]) {
	sprintf(s,"%s!%s",m->nick,m->userhost);
	if ((strcasecmp(m->nick,botname)!=0) &&
	    (!(get_attr_host(s) & USER_BOT))) ok=0;
	m=m->next;
      }
      if (ok) {
	/* ALL bots!  make them LEAVE!!! */
	m=chan->channel.member; while (m->nick[0]) {
	  mprintf(serv,"PRIVMSG %s :go %s\n",m->nick,chan->name);
	  m=m->next;
	}
      }
      else {
	/* some humans on channel, but still op-less */
	if (chan->need_op[0]) do_tcl("need-op",chan->need_op);
      }
    }
  }
}

void check_lonely_channels()
{
  struct chanset_t *chan;
  chan=chanset; while (chan!=NULL) {
    check_lonely_channel(chan);
    chan=chan->next;
  }
}

/* check for expired netsplit people */
void check_expired_splits()
{
  memberlist *m,*n; time_t now; char s[UHOSTLEN],hand[10];
  struct chanset_t *chan;
  now=time(NULL);
  chan=chanset; while (chan!=NULL) {
    m=chan->channel.member; while (m->nick[0]) {
      if (m->split) {
	n=m->next;
	if (!(chan->stat&CHANACTIVE)) killmember(chan,m->nick);
	else if (now-(m->split) > wait_split) {
	  sprintf(s,"%s!%s",m->nick,m->userhost);
	  get_handle_by_host(hand,s);
	  check_tcl_sign(m->nick,m->userhost,hand,chan->name,
			 "lost in the netsplit");
	  putlog(LOG_JOIN,chan->name,"%s (%s) got lost in the net-split.",
		 m->nick,m->userhost);
	  killmember(chan,m->nick);
	}
	m=n;
      }
      else m=m->next;
    }
    chan=chan->next;
  }
}

/* called once a minute to kick idlers */
void check_idle_kick()
{
  memberlist *m; time_t now; char s[UHOSTLEN]; int atr,chatr;
  struct chanset_t *chan;
  now=time(NULL); chan=chanset;
  while (chan!=NULL) {
    if ((chan->stat&CHANACTIVE) && (chan->idle_kick)) {
      m=chan->channel.member;
      while (m->nick[0]) {
	if ((now-(m->last)>=chan->idle_kick*60) &&
	    (strcmp(m->nick,botname)!=0) &&
	    (strcmp(m->nick,newbotname)!=0)) {
	  sprintf(s,"%s!%s",m->nick,m->userhost);
	  atr=get_attr_host(s);
	  chatr=get_chanattr_host(s,chan->name);
	  if (!(atr & (USER_MASTER|USER_FRIEND|USER_BOT|USER_GLOBAL)) &&
	      !(chatr & (CHANUSER_MASTER|CHANUSER_FRIEND|CHANUSER_OP)))
	    mprintf(serv,"KICK %s %s :idle %d min\n",chan->name,m->nick,
		    chan->idle_kick);
	}
	m=m->next;
      }
    }
    chan=chan->next;
  }
}

void update_idle(char *chname,char *nick)
{
  memberlist *m; struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) return;
  m=ismember(chan,nick);
  if (m==NULL) return;
  m->last=time(NULL);
}

/* write channel's local banlist to a file */
int write_chanbans PROTO1(FILE *,f)
{
  struct chanset_t *chan; struct eggqueue *q;
  chan=chanset; while (chan!=NULL) {
    if (fprintf(f,"::%s bans\n",chan->name)==EOF) return 0;
    q=chan->bans->host; while (q!=NULL) {
      if (strcmp(q->item,"none")!=0)
	if (fprintf(f,"- %s\n",q->item)==EOF) return 0;
      q=q->next;
    }
    chan=chan->next;
  }
  return 1;
}

/* channel ban loaded from user file */
void restore_chanban PROTO2(char *,chname,char *,host)
{
  struct chanset_t *chan;
  chan=findchan(chname); if (chan==NULL) {
    putlog(LOG_MISC,"*","* Trying to load ban to nonexistent channel '%s'!",
	   chname);
    return;
  }
  addhost_by_handle2(chan->bans,"null",host);
}

void write_channels () {
  FILE *f; char s[121],w[1024];
  struct chanset_t *chan; time_t now;
  context;
  sprintf(s,"%s~new",chanfile);
  f=fopen(s,"w");
  chmod(s,0600); 
  if (f==NULL) {
    putlog(LOG_MISC,"*","ERROR writing channel file.");
    return;
  }
  putlog(LOG_MISC,"*","Writing channel file ...");
  now=time(NULL); 
  fprintf(f,"#Dynamic Channel File for %s (%s) -- written %s\n",origbotname,ver,
	  ctime(&now));
  chan=chanset;
  while (chan!=NULL) {
    if (!(chan->stat & CHANSTATIC)) { /* no need to write out config channels */
      fprintf(f,"channel add %s {\n",chan->name);
      get_mode_protect(chan,w);
      if (w[0]) fprintf(f,"   chanmode \"%s\"\n",w);
      if (chan->idle_kick)
        fprintf(f,"   idle-kick %d\n",chan->idle_kick);
      else fprintf(f,"   dont-idle-kick\n");
      if (chan->need_op[0]) 
        fprintf(f,"   need-op  {%s}\n",chan->need_op);
      if (chan->need_invite[0])
        fprintf(f,"   need-invite {%s}\n",chan->need_invite);
      if (chan->need_key[0])
        fprintf(f,"   need-key {%s}\n",chan->need_key);
      if (chan->need_unban[0]) 
        fprintf(f,"   need-unban {%s}\n",chan->need_unban);
      if (chan->need_limit[0])
        fprintf(f,"   need-limit {%s}\n",chan->need_limit);
      fprintf(f,"   %cclearbans\n",
	      (chan->stat & CHAN_CLEARBANS)? '+' : '-');
      fprintf(f,"   %cenforcebans\n",
	      (chan->stat & CHAN_ENFORCEBANS) ? '+' : '-');
      fprintf(f,"   %cdynamicbans\n",
	      (chan->stat & CHAN_DYNAMICBANS) ? '+' : '-');
      fprintf(f,"   %cuserbans\n",
	      (chan->stat & CHAN_NOUSERBANS) ? '-' : '+');
      fprintf(f,"   %cautoop\n",
	      (chan->stat & CHAN_OPONJOIN) ? '+' : '-');
      fprintf(f,"   %cbitch\n",
	      (chan->stat & CHAN_BITCH) ? '+' : '-');
      fprintf(f,"   %cgreet\n",
	      (chan->stat & CHAN_GREET) ? '+' : '-');
      fprintf(f,"   %cprotectops\n",
	      (chan->stat & CHAN_PROTECTOPS) ? '+' : '-');
      fprintf(f,"   %cstatuslog\n",
	      (chan->stat & CHAN_LOGSTATUS) ? '+' : '-');
      fprintf(f,"   %cstopnethack\n",
	      (chan->stat & CHAN_STOPNETHACK) ? '+' : '-');
      fprintf(f,"   %crevenge\n",
	      (chan->stat & CHAN_REVENGE) ? '+' : '-');
      fprintf(f,"   %csecret\n",
	      (chan->stat&CHAN_SECRET) ? '+' : '-');
      if (fprintf(f,"}\n")==EOF) {
        putlog(LOG_MISC,"*","ERROR writing channel file.");
        fclose(f);
        return;
      }
    }
    chan=chan->next;
  }
  fclose(f); 
  unlink(chanfile);
#ifdef RENAME
  rename(s,chanfile);
#else
  movefile(s,chanfile);
#endif
}

void read_channels () {
  struct chanset_t *chan;
  if (!chanfile[0]) return;
  chan=chanset; while (chan!=NULL) {
    if (!(chan->stat & CHANSTATIC)) chan->stat |= CHANFLAGGED;
    chan=chan->next;
  }
  nulluser=1; /* let's not share out a null user when adding channels */
  if (!readtclprog(chanfile))
    fatal("CHANNEL FILE DEFINED, BUT DOESNT EXIST!",0);
  nulluser=0; /* should be safe now */
  chan=chanset; while (chan!=NULL) {
    if (chan->stat & CHANFLAGGED) {
      putlog(LOG_MISC,"*","No longer supporting channel %s",chan->name);
      if (serv>=0) mprintf(serv,"PART %s\n",chan->name);
      clear_channel(chan,0);
      freeuser(chan->bans);
      killchanset(chan->name);
      chan=chanset;
    }
    else chan=chan->next;
  }
}
