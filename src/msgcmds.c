/*
   msgcmds.c -- handles:
     all commands entered via /MSG

   dprintf'ized, 4feb96
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
#include "users.h"
#include "chan.h"
#include "proto.h"

/* let unknown users greet us and become known */
int learn_users=0;
/* new server? */
char newserver[121];
/* new server port? */
int newserverport=0;
/* new server password? */
char newserverpass[121];
/* enable the info subsystem? */
int use_info=1;
/* person to send a note to for new users */
char notify_new[121]="";
/* default user flags for people who say 'hello' */
int default_flags=0;

/* non-irc?  need none of this file then */
#ifndef NO_IRC

extern int serv;
extern char botname[];
extern int use_info;
extern char origbotname[];
extern char botnetnick[];
extern char helpdir[];
extern int make_userfile;
extern char notefile[];
extern int dcc_total;
extern struct dcc_t dcc[];
extern struct userrec *userlist;
extern struct chanset_t *chanset;


int msg_hello(n,nick,h,p)
char *n,*nick,*h,*p;
{
  char host[161],s[161],s1[161]; char *p1; int common=0,atr;
  if ((!learn_users) && (!make_userfile)) return 0;
  if (strcasecmp(nick,botname)==0) return 1;
  atr=get_attr_handle(n);
  if ((n[0]!='*') && !(atr & USER_COMMON)) {
    mprintf(serv,"NOTICE %s :Hi, %s.\n",nick,n);
    return 1;
  }
  if (is_user(nick)) {
    showtext(nick,"badhost",atr);
    return 1;
  }
  sprintf(s,"%s!%s",nick,h);
  if (match_ban(s)) {
    hprintf(serv,"NOTICE %s :You're banned, goober.\n",nick);
    return 1;
  }
  if (strlen(nick)>9) {
    /* crazy dalnet */
    hprintf(serv,"NOTICE %s :Your nick is too long to add right now.\n",
	    nick);
    return 1;
  }
  if (atr & USER_COMMON) {
    maskhost(s,host);
    strcpy(s,host); sprintf(host,"%s!%s",nick,&s[2]);
    userlist=adduser(userlist,nick,host,"-",default_flags);
    putlog(LOG_MISC,"*","Introduced to %s (%s) -- common site",nick,host);
    common=1;
  }
  else {
    maskhost(s,host);
    if (make_userfile)
      userlist=adduser(userlist,nick,host,"-",default_flags|USER_MASTER|
		       USER_OWNER);
    else
      userlist=adduser(userlist,nick,host,"-",default_flags);
    putlog(LOG_MISC,"*","Introduced to %s (%s)",nick,host);
  }
  hprintf(serv,"NOTICE %s :Hi %s!  I'm %s, an eggdrop bot.\n",nick,
	  nick,botname);
  hprintf(serv,"NOTICE %s :I'll recognize you by hostmask '%s'%s\n",
	  nick,host," from now on.");
  if (common) {
    hprintf(serv,"NOTICE %s :Since you come from a common irc site, %s",
	    nick,"this means you should\n");
    hprintf(serv,"NOTICE %s :  always use this nickname when talking %s",
	    nick,"to me.\n");
  }
  if (make_userfile) {
#ifdef OWNER
    hprintf(serv,"NOTICE %s :YOU ARE THE MASTER/OWNER ON THIS BOT NOW\n",
	    nick);
#else
    hprintf(serv,"NOTICE %s :YOU ARE THE MASTER ON THIS BOT NOW\n",
	    nick);
#endif
    showtext(nick,"newbot",default_flags|USER_MASTER|USER_OWNER);
    putlog(LOG_MISC,"*","Bot installation complete, first master is %s",
	   nick);
    make_userfile=0;
    write_userfile();
    add_note(nick,origbotname,"Welcome to eggdrop! :)",-1,0);
  }
  else showtext(nick,"intro",default_flags);
  if (notify_new[0]) {
    sprintf(s,"Introduced to %s from %s",nick,host);
    strcpy(s1,notify_new); while (s1[0]) {
      p1=strchr(s1,','); if (p1!=NULL) { *p1=0; p1++; rmspace(p1); }
      rmspace(s1); add_note(s1,origbotname,s,-1,0);
      if (p1==NULL) s1[0]=0; else strcpy(s1,p1);
    }
  }
  return 1;
}

int msg_pass(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char old[512],new[512];
  if (strcasecmp(nick,botname)==0) return 1;
  if (strcmp(hand,"*")==0) return 1;
  if (get_attr_handle(hand) & (USER_BOT|USER_COMMON)) return 1;
  split(old,par);
  if (!par[0]) {
    mprintf(serv,"NOTICE %s :You %s a password set.\n",nick,
	      pass_match_by_handle("-",hand)?"don't have":"have");
    putlog(LOG_CMDS,"*","(%s!%s) !%s! PASS?",nick,host,hand);
    return 1;
  }
  if ((!pass_match_by_handle("-",hand)) && (!old[0])) {
    mprintf(serv,"NOTICE %s :You already have a password set.\n",nick);
    return 1;
  }
  if (!old[0]) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! PASS...",nick,host,hand);
    nsplit(new,par);
    if (strlen(new)>9) new[9]=0;
    if (strlen(new)<4) {
      mprintf(serv,"NOTICE %s :Please use at least 4 characters.\n",nick);
      return 0;
    }
    change_pass_by_handle(hand,new);
    mprintf(serv,"NOTICE %s :Password set to '%s'\n",nick,new);
    return 1;
  }
  if (!pass_match_by_handle(old,hand)) {
    mprintf(serv,"NOTICE %s :Incorrect password.\n",nick);
    return 1;
  }
  nsplit(new,par);
  putlog(LOG_CMDS,"*","(%s!%s) !%s! PASS...",nick,host,hand);
  if (strlen(new)>9) new[9]=0;
  if (strlen(new)<4) {
    mprintf(serv,"NOTICE %s :Please use at least 4 characters.\n",nick);
    return 0;
  }
  change_pass_by_handle(hand,new);
  mprintf(serv,"NOTICE %s :Password changed to '%s'.\n",nick,new);
  return 1;
}

int msg_ident(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char s[121],s1[121],pass[512],who[NICKLEN];
  if (strcasecmp(nick,botname)==0) return 1;
  if (get_attr_handle(hand) & USER_BOT) return 1;
  if (get_attr_handle(hand) & USER_COMMON) {
    mprintf(serv,"NOTICE %s :You're at a common site; you can't ident.\n",
	    nick);
    return 1;
  }
  nsplit(pass,par);
  if (!is_user(nick)) {
    if (strcmp(hand,"*")!=0) {
      mprintf(serv,"NOTICE %s :You're not %s, you're %s.\n",nick,nick,hand);
      return 1;
    }
    if ((!par[0]) || (!is_user(par))) return 1;   /* dunno you */
  }
  strncpy(who,par,NICKLEN); who[NICKLEN-1]=0;
  if (!par[0]) strcpy(who,nick);
  /* This could be used as detection... */
  if (strcasecmp(who,origbotname)==0) return 1;
  if (pass_match_by_handle("-",who)) {
    mprintf(serv,"NOTICE %s :You have no password set.\n",nick);
    return 1;
  }
  if (!pass_match_by_handle(pass,who)) {
    mprintf(serv,"NOTICE %s :Access denied.\n",nick);
    return 1;
  }
  if (strcasecmp(hand,who)==0) {
    mprintf(serv,"NOTICE %s :I recognize you there.\n",nick);
    return 1;
  }
  if (strcmp(hand,"*")!=0) {
    mprintf(serv,"NOTICE %s :You're not %s, you're %s.\n",nick,who,hand);
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! IDENT %s",nick,host,hand,who);
  sprintf(s,"%s!%s",nick,host); maskhost(s,s1);
  hprintf(serv,"NOTICE %s :Added hostmask: %s\n",nick,s1);
  addhost_by_handle(who,s1);
  recheck_ops(nick,who);
  return 1;
}

int msg_email(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char s[161];
  if (strcasecmp(nick,botname)==0) return 1;
  if (strcasecmp(hand,"*")==0) return 0;
  if (get_attr_handle(hand) & USER_COMMON) return 1;
  if (par[0]) {
    get_handle_email(hand,s);
    if (strcasecmp(par,"none")==0) {
      putlog(LOG_CMDS,"*","(%s!%s) !%s! EMAIL NONE",nick,host,hand);
      par[0]=0; set_handle_email(userlist,hand,par);
      mprintf(serv,"NOTICE %s :Removed your email address.\n",nick);
    }
    else {
      putlog(LOG_CMDS,"*","(%s!%s) !%s! EMAIL...",nick,host,hand);
      set_handle_email(userlist,hand,par);
      mprintf(serv,"NOTICE %s :Now: %s\n",nick,par);
    }
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! EMAIL?",nick,host,hand);
  get_handle_email(hand,s);
  if (s[0]) {
    mprintf(serv,"NOTICE %s :Currently: %s\n",nick,s);
    mprintf(serv,"NOTICE %s :To remove it: /msg %s email none\n",nick,botname);
  }
  else mprintf(serv,"NOTICE %s :You have no email address set.\n",nick);
  return 1;
}

int msg_info(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char s[121],pass[512],chname[512]; int locked=0;
  if (strcasecmp(nick,botname)==0) return 1;
  if (!use_info) return 1;
  if (strcasecmp(hand,"*")==0) return 0;
  if (get_attr_handle(hand) & USER_COMMON) return 1;
  nsplit(pass,par);
  if (!pass_match_by_handle(pass,hand)) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! failed INFO",nick,host,hand);
    return 1;
  }
  if ((par[0]=='#') || (par[0]=='+') || (par[0]=='&')) nsplit(chname,par);
  else chname[0]=0;
  if (par[0]) {
    get_handle_info(hand,s);
    if (s[0]=='@') locked=1;
    if (chname[0]) {
      get_handle_chaninfo(hand,chname,s);
      if (s[0]=='@') locked=1;
    }
    if (locked) {
      mprintf(serv,"NOTICE %s :Your info line is locked.\n",nick);
      return 1;
    }
    if (strcasecmp(par,"none")==0) {
      par[0]=0;
      if (chname[0]) {
	set_handle_chaninfo(userlist,hand,chname,par);
	mprintf(serv,"NOTICE %s :Removed your info line on %s.\n",nick,
		chname);
	putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO %s NONE",nick,host,hand,
	       chname);
      }
      else {
	set_handle_info(userlist,hand,par);
	mprintf(serv,"NOTICE %s :Removed your info line.\n",nick);
	putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO NONE",nick,host,hand);
      }
      return 1;
    }
    if (par[0]=='@') strcpy(par,&par[1]);
    mprintf(serv,"NOTICE %s :Now: %s\n",nick,par);
    if (chname[0]) {
      set_handle_chaninfo(userlist,hand,chname,par);
      putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO %s ...",nick,host,hand,chname);
    }
    else {
      set_handle_info(userlist,hand,par);
      putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO ...",nick,host,hand);
    }
    return 1;
  }
  if (chname[0]) {
    get_handle_chaninfo(hand,chname,s);
    putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO? %s",nick,host,hand,chname);
  }
  else {
    get_handle_info(hand,s);
    putlog(LOG_CMDS,"*","(%s!%s) !%s! INFO?",nick,host,hand);
  }
  if (s[0]) {
    mprintf(serv,"NOTICE %s :Currently: %s\n",nick,s);
    mprintf(serv,"NOTICE %s :To remove it: /msg %s info <pass>%s%s none\n",
	    nick,botname,chname[0]?" ":"",chname);
  }
  else {
    if (chname[0])
      mprintf(serv,"NOTICE %s :You have no info set on %s.\n",nick,chname);
    else mprintf(serv,"NOTICE %s :You have no info set.\n",nick);
  }
  return 1;
}

int msg_who(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  struct chanset_t *chan;
  if (strcasecmp(nick,botname)==0) return 1;
  if (strcasecmp(hand,"*")==0) return 0;
  if (!use_info) return 1;
  if (!par[0]) {
    mprintf(serv,"NOTICE %s :Usage: /msg %s who <channel>\n",nick,
	    botname);
    return 0;
  }
  chan=findchan(par); if (chan==NULL) {
    mprintf(serv,"NOTICE %s :I don't monitor that channel.\n",nick);
    return 0;
  }
  if ((channel_hidden(chan)) && 
      (!hand_on_chan(chan,hand)) &&
      !(get_attr_handle(hand) & USER_MASTER) &&
      !(get_chanattr_handle(hand,chan->name)&(CHANUSER_OP|CHANUSER_FRIEND))) {
    mprintf(serv,"NOTICE %s :Channel is currently hidden.\n",nick);
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! WHO",nick,host,hand);
  show_all_info(chan->name,nick);
  return 1;
}

int msg_whois(hand,nick,host,opar)
char *hand,*nick,*host,*opar;
{
  time_t tt; char s[161],s1[81],par[NICKLEN]; int atr,ok;
  struct chanset_t *chan;
  if (strcasecmp(nick,botname)==0) return 1; 
  if (strcasecmp(hand,"*")==0) return 0;
  strncpy(par,opar,NICKLEN); par[NICKLEN-1]=0;
  putlog(LOG_CMDS,"*","(%s!%s) !%s! WHOIS %s",nick,host,hand,par);
  if (!is_user(par)) {
    /* no such handle -- maybe it's a nickname of someone on a chan? */
    ok=0;
    chan=chanset; while ((chan!=NULL) && (!ok)) {
      if (ischanmember(chan->name,par)) {
	sprintf(s,"%s!",par); getchanhost(chan->name,par,&s[strlen(s)]);
	get_handle_by_host(par,s); ok=1;
	if (par[0]=='*') ok=0;
	else hprintf(serv,"NOTICE %s :[%s] aka '%s':\n",nick,opar,par);
      }
      chan=chan->next;
    }
    if (!ok) {
      hprintf(serv,"NOTICE %s :[%s] No user record.\n",nick,opar);
      return 1;
    }
  }
  atr=get_attr_handle(par);
  get_handle_info(par,s);
  if (s[0]=='@') strcpy(s,&s[1]);
  if ((s[0]) && (!(atr&USER_BOT)))
    hprintf(serv,"NOTICE %s :[%s] %s\n",nick,par,s);
  get_handle_email(par,s);
  if (s[0]) hprintf(serv,"NOTICE %s :[%s] email: %s\n",nick,par,s);
  ok=0; chan=chanset; while (chan!=NULL) {
    if (hand_on_chan(chan,par)) {
      sprintf(s1,"NOTICE %s :[%s] On %s now.",nick,par,chan->name);
      ok=1;
    }
    else {
      get_handle_laston(chan->name,par,&tt);
      if (tt) {
	strcpy(s,ctime(&tt)); strcpy(s,&s[4]); s[12]=0; ok=1;
	sprintf(s1,"NOTICE %s :[%s] Last seen on %s %s",nick,par,chan->name,s);
      }
    }
    chan=chan->next;
  }
  if (!ok) sprintf(s1,"NOTICE %s :[%s] Never joined one of my channels.",
		   nick,par);
  if (atr & USER_GLOBAL) strcat(s1,"  (is a global op)");
  if (atr & USER_BOT) strcat(s1,"  (is a bot)");
  if (atr & USER_MASTER) strcat(s1,"  (is a master)");
  hprintf(serv,"%s\n",s1);
  return 1;
}

int msg_help(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  int atr; char s[121],*p;
  if (strcasecmp(nick,botname)==0) return 1;
  sprintf(s,"%s!%s",nick,host); atr=get_attr_host(s);
  if (strcasecmp(hand,"*")==0) {
#ifndef QUIET_REJECTION
    hprintf(serv,"NOTICE %s :I don't know you; please introduce yourself first.\n",
	    nick);
    hprintf(serv,"NOTICE %s :/MSG %s hello\n",nick,botname);
#endif
    return 0;
  }
  if (helpdir[0]) {
    if (!par[0]) showhelp(nick,"help",atr);
    else {
      for (p=par; *p!=0; p++)
	if ((*p>='A') && (*p<='Z')) *p+=('a'-'A');
      showhelp(nick,par,atr);
    }
  }
  else hprintf(serv,"NOTICE %s :No help.\n",nick);
  return 1;
}

/* i guess just op them on every channel they're on */
int msg_op(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  struct chanset_t *chan; char pass[50], pass2[50];
  if (strcasecmp(nick,botname)==0) return 1;
  nsplit(pass,par);
  if (pass_match_by_handle(pass,hand)) {
    get_pass_by_handle(hand,pass2);
    if (strcmp(pass2,"-")==0) {
      putlog(LOG_CMDS,"*","(%s!%s) !%s! failed OP",nick,host,hand);
      return 1;
      /* Ah, hello!  This is what stops people from doing /msg op with no
         password set to get ops.  Why did we uncomment it?  */
    }
    if (par[0]) {
      if (!active_channel(par)) {
        putlog(LOG_CMDS,"*","(%s!%s) !%s! failed OP",nick,host,hand);
        return 1;
      }
      chan=findchan(par); if (chan==NULL) {
        putlog(LOG_CMDS,"*","(%s!%s) !%s! failed OP",nick,host,hand);
        return 1;
      }
      if ((hand_on_chan(chan,hand)) && (!member_op(chan->name,nick)) &&
	  ((get_chanattr_handle(hand,chan->name) & CHANUSER_OP) ||
	   (get_attr_handle(hand) & USER_GLOBAL))) {
	add_mode(chan,'+','o',nick);
	putlog(LOG_CMDS,"*","(%s!%s) !%s! OP %s",nick,host,hand,par);
      }
      return 1;
    }
    chan=chanset; while (chan!=NULL) {
      if ((hand_on_chan(chan,hand)) && (!member_op(chan->name,nick)) &&
	  ((get_chanattr_handle(hand,chan->name) & CHANUSER_OP) ||
	   (get_attr_handle(hand) & USER_GLOBAL)))
	add_mode(chan,'+','o',nick);
      chan=chan->next;
    }
    putlog(LOG_CMDS,"*","(%s!%s) !%s! OP",nick,host,hand);
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! failed OP",nick,host,hand);
  return 1;
}

int msg_invite(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char pass[512];
  if (strcasecmp(nick,botname)==0) return 1;
  if (strcmp(hand,"*")==0) return 0;
  nsplit(pass,par);
  if (pass_match_by_handle(pass,hand)) {
    if (findchan(par)==NULL) {
      mprintf(serv,"NOTICE %s :Usage: /MSG %s invite <pass> <channel>\n",
	      nick,botname);
      return 1;
    }
    if (!active_channel(par)) {
      mprintf(serv,"NOTICE %s :I'm not on %s right now.\n",nick,par);
      return 1;
    }
    mprintf(serv,"INVITE %s %s\n",nick,par);
    putlog(LOG_CMDS,"*","(%s!%s) !%s! INVITE %s",nick,host,hand,par);
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! failed INVITE %s",nick,host,hand,par);
  return 1;
}

int msg_status(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  if (strcasecmp(nick,botname)==0) return 1;
  putlog(LOG_CMDS,"*","(%s!%s) !%s! STATUS",nick,host,hand);
  tell_chan_info(nick);
  return 1;
}

int msg_memory(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  if (strcasecmp(nick,botname)==0) return 1;
  putlog(LOG_CMDS,"*","(%s!%s) !%s! MEMORY",nick,host,hand);
  tell_mem_status(nick);
  return 1;
}

int msg_die(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char s[121];
  if (strcasecmp(nick,botname)==0) return 1;
  if (pass_match_by_handle(par,hand)) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! DIE",nick,host,hand);
    tprintf(serv,"NOTICE %s :Daisy, Daisssyyy, give meee yourr ansssweerrrrrr dooooooooo....\n",nick);
    chatout("*** BOT SHUTDOWN (authorized by %s)\n",hand);
    tandout("chat %s BOT SHUTDOWN (authorized by %s)\n",botnetnick,hand);
    tandout("bye\n");
    tprintf(serv,"QUIT :%s\n",nick);
    write_userfile();
    sleep(1);   /* give the server time to understand */
    sprintf(s,"DEAD BY REQUEST OF %s!%s",nick,host);
    fatal(s,0);
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! failed DIE",nick,host,hand);
  return 1;
}

int msg_rehash(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  if (strcasecmp(nick,botname)==0) return 1;
  if (pass_match_by_handle(par,hand)) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! REHASH",nick,host,hand);
    mprintf(serv,"NOTICE %s :Rehashing...\n",nick);
    rehash(); return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! failed REHASH",nick,host,hand);
  return 1;
}

int msg_reset(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  struct chanset_t *chan;
  if (strcasecmp(nick,botname)==0) return 1;
  if (par[0]) {
    chan=findchan(par);
    if (chan==NULL) {
      mprintf(serv,"NOTICE %s :I don't monitor channel %s\n",nick,par);
      return 0;
    }
    putlog(LOG_CMDS,"*","(%s!%s) !%s! RESET %s",nick,host,hand,par);
    mprintf(serv,"NOTICE %s :Resetting channel info on %s...\n",nick,par);
    reset_chan_info(chan); return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! RESET ALL",nick,host,hand);
  mprintf(serv,"NOTICE %s :Resetting channel info for all channels...\n",
	  nick);
  chan=chanset; while (chan!=NULL) {
    reset_chan_info(chan); chan=chan->next;
  }
  return 1;
}

int msg_go(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  struct chanset_t *chan; int ok=0;
  if (strcasecmp(nick,botname)==0) return 1;
  if (!op_anywhere(hand)) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! failed GO (not op)",nick,host,hand);
    return 1;
  }
  if (par[0]) {
    /* specific GO */
    chan=findchan(par); if (chan==NULL) return 0;
    if (!(get_chanattr_handle(hand,chan->name) & CHANUSER_OP)) {
      putlog(LOG_CMDS,"*","(%s!%s) !%s! failed GO (not op)",nick,host,hand);
      return 1;
    }
    if (!me_op(chan)) {
      tprintf(serv,"PART %s\n",chan->name);
      putlog(LOG_CMDS,chan->name,"(%s!%s) !%s! GO %s",nick,host,hand,par);
      return 1;
    }
    putlog(LOG_CMDS,chan->name,"(%s!%s) !%s! failed GO %s (i'm chop)",nick,
	   host,hand,par);
    return 1;
  }
  chan=chanset; while (chan!=NULL) {
    if (ischanmember(chan->name,nick)) {
      if (!me_op(chan)) {
	tprintf(serv,"PART %s\n",chan->name); ok=1;
      }
    }
    chan=chan->next;
  }
  if (ok) {
    putlog(LOG_CMDS,"*","(%s!%s) !%s! GO",nick,host,hand);
    return 1;
  }
  putlog(LOG_CMDS,"*","(%s!%s) !%s! failed GO (i'm chop)",nick,host,hand);
  return 1;
}

int msg_jump(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char s[512],port[512];
  if (strcasecmp(nick,botname)==0) return 1;
  nsplit(s,par);  /* password */
  if (pass_match_by_handle(s,hand)) {
    if (par[0]) {
      nsplit(s,par); nsplit(port,par);
      if (!port[0]) sprintf(port,"%d",DEFAULT_PORT);
      putlog(LOG_CMDS,"*","(%s!%s) !%s! JUMP %s %s %s",nick,host,hand,s,port,
	     par);
      strcpy(newserver,s); newserverport=atoi(port);
      strcpy(newserverpass,par);
    }
    else putlog(LOG_CMDS,"*","(%s!%s) !%s! JUMP",nick,host,hand);
    tprintf(serv,"NOTICE %s :Jumping servers...\n",nick);
    tprintf(serv,"QUIT :changing servers\n");
    killsock(serv); serv=(-1);
  }
  else putlog(LOG_CMDS,"*","(%s!%s) !%s! failed JUMP",nick,host,hand);
  return 1;
}

/* notes <pass> <func> */
int msg_notes(hand,nick,host,par)
char *hand,*nick,*host,*par;
{
  char pwd[121],fcn[121];
  if (strcasecmp(nick,botname)==0) return 1;
  if (hand[0]=='*') return 0;
  if (!par[0]) {
    hprintf(serv,"NOTICE %s :Usage: NOTES [pass] INDEX\n",nick);
    hprintf(serv,"NOTICE %s :       NOTES [pass] TO <nick> <msg>\n",nick);
    hprintf(serv,"NOTICE %s :       NOTES [pass] READ <# or ALL>\n",nick);
    hprintf(serv,"NOTICE %s :       NOTES [pass] ERASE <# or ALL>\n",nick);
    return 1;
  }
  if (!pass_match_by_handle("-",hand)) {
    /* they have a password set */
    nsplit(pwd,par); if (!pass_match_by_handle(pwd,hand)) return 0;
  }
  nsplit(fcn,par);
  if (strcasecmp(fcn,"INDEX")==0) notes_read(hand,nick,-1,-1);
  else if (strcasecmp(fcn,"READ")==0) {
    if (strcasecmp(par,"ALL")==0) notes_read(hand,nick,0,-1);
    else notes_read(hand,nick,atoi(par),-1);
  }
  else if (strcasecmp(fcn,"ERASE")==0) {
    if (strcasecmp(par,"ALL")==0) notes_del(hand,nick,0,-1);
    else notes_del(hand,nick,atoi(par),-1);
  }
  else if (strcasecmp(fcn,"TO")==0) {
    char to[514]; int i; FILE *f;
    nsplit(to,par);
    if (!par[0]) {
      hprintf(serv,"NOTICE %s :Usage: NOTES [pass] TO <nick> <message>\n",
	      nick);
      return 0;
    }
    if (!is_user(to)) {
      hprintf(serv,"NOTICE %s :I don't know anyone by that name.\n",nick);
      return 1;
    }
    for (i=0; i<dcc_total; i++) {
      if ((strcasecmp(dcc[i].nick,to)==0) && ((dcc[i].type==DCC_CHAT) ||
					      (dcc[i].type==DCC_FILES))) {
	int aok=1;
	if (dcc[i].type==DCC_CHAT) if (dcc[i].u.chat->away!=NULL) aok=0;
	if (dcc[i].type==DCC_FILES) if (dcc[i].u.file->chat->away!=NULL) aok=0;
	if (aok) {
	  dprintf(i,"\007Outside note [%s]: %s\n",hand,par);
	  hprintf(serv,"NOTICE %s :Note delivered.\n",nick);
	  return 1;
	}
      }
    }
    if (notefile[0]==0) {
      hprintf(serv,"NOTICE %s :Notes are not supported on this bot.\n",
	      nick);
      return 1;
    }
    f=fopen(notefile,"a");
    if (f==NULL) f=fopen(notefile,"w");
    if (f==NULL) {
      hprintf(serv,"NOTICE %s :Can't create notefile.  Sorry.\n",nick);
      putlog(LOG_MISC,"*","* Notefile unreachable!");
      return 1;
    }
    fprintf(f,"%s %s %lu %s\n",to,hand,time(NULL),par);
    fclose(f);
    hprintf(serv,"NOTICE %s :Note delivered.\n",nick);
    return 1;
  }
  else hprintf(serv,"NOTICE %s :NOTES function must be one of INDEX, %s\n",
	       nick,"READ, ERASE, TO");
  putlog(LOG_CMDS,"*","(%s!%s) !%s! NOTES %s %s",nick,host,hand,fcn,
	 par[0]?"...":"");
  return 1;
}

#endif  /* !NO_IRC */
