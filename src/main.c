/* 
   main.c -- handles:
     changing nicknames when the desired nick is in use
     flood detection
     signal handling
     telnet code translation
     command line arguments
     connecting to a server (bot and helpbot)
     interpreting server responses (main loop)

   dprintf'ized, 15nov95
*/
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author (Robey Pointer) can be reached at:  robey@netcom.com
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#ifdef STOP_UAC    /* osf/1 complains a lot */
# include <sys/sysinfo.h>
# define UAC_NOPRINT    0x00000001       /* Don't report unaligned fixups */
#endif
/* some systems have a working sys/wait.h even though configure will */
/* decide it's not bsd compatable.  oh well. */
#include "eggdrop.h"
#include "chan.h"
#include "tclegg.h"
#ifdef MODULES
#include "modules.h"
#else
#ifndef NO_FILE_SYSTEM
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "../lush.h"
#include "files.h"
#endif
#endif

/* number of seconds to wait between transmitting queued lines to the server */
/* lower this value at your own risk.  ircd is known to start flood control */
/* at 512 bytes/2 seconds */
#define msgrate 2

/* solaris needs this */
#define _POSIX_SOURCE 1

extern char botname[];
extern char origbotname[];
extern int dcc_total;
extern struct dcc_t dcc[];
extern char dccdir[];
extern char dccin[];
extern char admin[];
extern char notefile[];
extern char newserver[];
extern char newserverpass[];
extern int newserverport;
extern int lastsock;
extern int conmask;
extern struct userrec *userlist;
extern int cache_hit,cache_miss;
extern char userfile[];
extern struct chanset_t *chanset;
extern int ban_time;
extern int ignore_time;
extern char botnetnick[];
extern log_t logs[];
extern char ctcp_finger[];
extern char ctcp_userinfo[];
extern char ctcp_version[];
extern Tcl_Interp *interp;
extern int default_port;

#ifndef MODULES
extern char tempdir[];
#include "mod/filedb.c"
#include "mod/files.c"
#include "mod/fileq.c"
#endif
/*
   Please use the PATCH macro instead of directly altering the version
   string from now on (it makes it much easier to maintain patches).
   Also please read the README file regarding your rights to distribute
   modified versions of this bot.

   Note: Loading more than 10 patches could make your patch level "roll
   over" to the next release!  So try not to do that. :)
*/
char egg_version[1024]="1.1.4";
int egg_numver=1010400;
int min_share =1010300; /* minimum version I will share with */

/* socket that the server is on */
int serv=(-1);
/* run in the background? */
#ifdef __CYGWIN32__
int backgrd=0;
#else
int backgrd=1;
#endif
/* successful connect yet? */
int online=0;
/* foreground: constantly display channel stats? */
int con_chan=0;
/* foreground: use the terminal as a party line? */
int term_z=0;
/* trying to connect to a server right now? */
time_t trying_server=0L;
/* how lagged (in seconds) is the server? */
int server_lag=0;
/* bot's username */
char botuser[21];
/* bot's real name field */
char botrealname[121];
/* our current host */
char bothost[121];
/* our server */
char botserver[121];
/* port # to connect to */
int botserverport=6667;
/* name of the config file */
char configfile[121]="egg.config";
/* possible alternate nickname to use */
char altnick[10]="";
/* temporary thing for nick changes */
char newbotname[10];
/* current position in server list: */
int curserv=0;
/* directory of help files (if used) */
char helpdir[121];
/* directory for text files that get dumped */
char textdir[121]="";
/* MSG flood */
int flood_thr=5;
int flood_time=60;
/* PUBLIC flood */
int flood_pub_thr=10;
int flood_pub_time=60;
/* JOIN flood */
int flood_join_thr=5;
int flood_join_time=60;
/* CTCP flood */
int flood_ctcp_thr=3;
int flood_ctcp_time=60;
/* what, if anything, to send to the server on connection */
char initserver[121];
/* never erase logfiles, no matter how old they are? */
int keep_all_logs=0;
/* context storage for fatal crashes */
#ifdef EBUG
char cx_file[16][30]; int cx_line[16]; int cx_ptr=0;
#else
char cx_file[30]; int cx_line;
#endif
/* unix-time that the bot loaded up */
time_t online_since;
/* bot's user@host (refreshed whenever the bot joins a channel) */
/* may not be correct user@host BUT it's how the server sees it */
char botuserhost[121];
/* using bot in make-userfile mode?  (first user to 'hello' becomes master) */
int make_userfile=0;
/* never give up when connecting to servers? */
int never_give_up=0;
/* permanent owner(s) of the bot */
char owner[121]="";
/* keep trying to regain my intended nickname? */
int keepnick=1;
/* set when i unidle myself, cleared when i get the response */
int waiting_for_awake=0;
/* name of the file for the pid to be stored in */
char pid_file[40];
/* how many minutes past the hour to save the userfile? */
int save_users_at=0;
/* how many minutes past the hour to notify users of notes? */
int notify_users_at=0;
/* when (military time) to switch logfiles */
int switch_logfiles_at=300;
/* version info (long form) */
char version[81];
/* version info (short form) */
char ver[41];
/* patch info */
char egg_xtra[1024];
/* server connection time */
time_t server_online=0L;
/* send stuff to stderr instead of logfiles? */
int use_stderr=1;
/* .restart has been called, restart a.s.a.p. */
int do_restart = 0;

void fatal PROTO2(char *,s,int,recoverable)
{
  int i;
  putlog(LOG_MISC,"*","* %s",s);
  flushlogs();
  if (serv>=0) killsock(serv);
  for (i=0; i<dcc_total; i++) killsock(dcc[i].sock);
  unlink(pid_file);
  exit(1);
}

/* for mem.c : calculate memory we SHOULD be using */
int expected_memory()
{
  int tot;
  context;
  tot=expmem_chan()+expmem_chanprog()+expmem_misc()+expmem_users()+
    expmem_dccutil()+expmem_botnet()+expmem_tcl()+expmem_tclhash()+
    expmem_net();
#ifndef MODULES
  tot+=expmem_assoc()+expmem_blowfish();
#ifndef NO_FILE_SYSTEM
  tot+=expmem_fileq();
#endif
#else
  tot+=expmem_modules(0);
#endif
  return tot;
}

/* fix the last parameter... if it starts with ':' then accept all of it,
   otherwise return only the first word */
void fixcolon PROTO1(char *,s)
{
  if (s[0]==':') strcpy(s,&s[1]);
  else split(s,s);
}

#ifndef NO_IRC

/* given <in> (raw stuff from server), pull off who it's from & the code */
void parsemsg PROTO4(char *,in,char *,from,char *,code,char *,params)
{
  char *p;
  from[0]=0;
  if (in[0]==':') {
    strcpy(in,&in[1]);
    p=strchr(in,' ');
    if (p==NULL) { from[0]=params[0]=0; strcpy(code,in); return; }
    strcpy(params,p+1);
    *p=0; strcpy(from,in); *p=' ';
    p++; strcpy(in,p);
  }
  p=strchr(in,' ');
  if (p==NULL) { strcpy(code,in); params[0]=0; return; }
  *p=0; strcpy(code,in); *p=' ';
  strcpy(params,p+1);
}

/* ping from server */
void gotpong PROTO2(char *,from,char *,msg)
{
  split(NULL,msg); fixcolon(msg);   /* scrap server name */
  waiting_for_awake=0;
  server_lag=time(NULL)-my_atoul(msg);
  if (server_lag > 99999) {
    /* bogus */
    server_lag=(-1);
  }
}

/* 302 : USERHOST to be used at a later date in a tcl command mebbe */
void got302 PROTO2(char *,from,char *,msg)
{
/*  char userhost[UHOSTLEN],nick[NICKLEN];
    int i,oper=0,away=0;
    context;
    split(NULL,msg); fixcolon(msg); strcpy(s,msg);
    * now we have to interpret this shit *
    * <nick>['*']'='<'+'|'-'><hostname> *
    for (i=0; i<strlen(s); i++) {
    if (s[i]==61) s[i]=' ';
    if (s[i]==42) oper=1;
    if (s[i]==43) away=1;
    }
    split(nick,s); strcpy(userhost,&s[1]);
    if (oper) nick[strlen(nick)-1]=0;
  
    I had specific uses for what I put here but I 
    thought I would throw the above in for starters
*/
}

/* trace failed! meaning my nick is not in use!
   206 (undernet)
   401 (other non-efnet)
   402 (Efnet)
*/
void trace_fail PROTO2(char *,from,char *,msg)
{
  debug1("%s\n",msg);
  if (strcasecmp(botname,origbotname)==0) return;
  putlog(LOG_MISC,"*","Switching back to nick %s",origbotname);
  strcpy(newbotname,botname);  /* save, just in case */
  strcpy(botname,origbotname);
  tprintf(serv,"NICK %s\n",botname);
}

/* 432 : bad nickname */
void got432 PROTO2(char *,from,char *,msg)
{
  putlog(LOG_MISC,"*","Server says my nickname is invalid.");
  /* make random nick. */ 
  if (!newbotname[0]) { /* if it's due to an attempt to change nicks ..*/
    makepass(botname);
    tprintf(serv,"NICK %s\n",botname);
  } else {              /* go back to old nick */
    strcpy(botname,newbotname);
    newbotname[0]=0;
  }
}

/* 433 : nickname in use */
/* change nicks till we're acceptable or we give up */
void got433 PROTO2(char *,from,char *,msg)
{
  char c,*oknicks="^-_\\[]`",*p;
  /* could be futile attempt to regain nick: */
  if (newbotname[0]) {
    tprintf(serv,"NICK %s\n",newbotname);
    strcpy(botname,newbotname); newbotname[0]=0; return;
  }
  /* alternate nickname defined? */
  if ((altnick[0]) && (strcasecmp(altnick,botname)!=0)) {
    strcpy(botname,altnick);
  }
  /* if alt nickname failed, drop thru to here */
  else {
    c=botname[strlen(botname)-1];
    p=strchr(oknicks,c);
    if (((c>='0') && (c<='9')) || (p!=NULL)) {
      if (p==NULL) {
	if (c=='9') botname[strlen(botname)-1]=oknicks[0];
	else botname[strlen(botname)-1]=c+1;
      }
      else {
	p++; if (!*p) botname[strlen(botname)-1]='a'+random()%26;
	else botname[strlen(botname)-1]=(*p);
      }
    }
    else {
      if (strlen(botname)==9) botname[strlen(botname)-1]='0';
      else {
	botname[strlen(botname)+1]=0;
	botname[strlen(botname)]='0';
      }
    }
  }
  putlog(LOG_MISC,"*","NICK IN USE: Trying '%s'",botname);
  tprintf(serv,"NICK %s\n",botname);
}

/* 437 : nickname juped (Euronet) */
void got437 PROTO2(char *,from,char *,msg)
{
  char s[512];
  struct chanset_t *chan;
  split(NULL,msg); split(s,msg);
  if (strchr("#&+",s[0])!=NULL) {
    chan=findchan(s); if (chan!=NULL) {
      if (chan->stat&CHANACTIVE) {
	putlog(LOG_MISC,"*","Can't change nickname on %s.  Is my nickname banned?",s);
	got433(from,NULL);
      } else {
	putlog(LOG_MISC,"*","Channel %s is juped. :(",s);
      }
    }
  } else {
    putlog(LOG_MISC,"*","Nickname has been juped.");
    got433(from,NULL);
  }
}

/* 451 : Not registered */
void got451 PROTO2(char *,from,char *,msg)
{
/* usually if we get this then we really fucked up somewhere
   or this is a non-standard server, so we log it and kill the socket
   hoping the next server will work :) -poptix */
   putlog(LOG_MISC,"*","%s says I'm not registered, trying next one.",from);
   tprintf(serv,"QUIT :You have a fucked up server.\n");
   if (serv>=0) killsock(serv); serv=(-1);
}

#endif /* !NO_IRC */

int lastmsgs[5]={ 0,0,0,0,0 };
char lastmsghost[5][81]={ "","","","","" };
time_t lastmsgtime[5]={ 0L,0L,0L,0L,0L };

/* do on NICK, PRIVMSG, and NOTICE -- and JOIN */
int detect_flood PROTO4(char *,from,struct chanset_t *,chan,int,which,
			int,tochan)
{
  char *p; time_t t; char h[UHOSTLEN],floodnick[NICKLEN],handle[10],ftype[10];
  int thr=0,lapse=0; memberlist *m;
  if (get_attr_host(from) & (USER_BOT|USER_MASTER|USER_FRIEND)) return 0;
  if ((tochan) && (get_chanattr_host(from,chan->name) & 
		   (CHANUSER_FRIEND|CHANUSER_MASTER)))
    return 0;
  /* determine how many are necessary to make a flood */
  switch(which) {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
      if (tochan) {
	thr=flood_pub_thr;
	lapse=flood_pub_time;
	strcpy(ftype,"pub");
      }
      else {
	thr=flood_thr;
	lapse=flood_time;
	strcpy(ftype,"msg");
      }
      break;
    case FLOOD_JOIN:
    case FLOOD_NICK:
      thr=flood_join_thr;
      lapse=flood_join_time;
      if (which==FLOOD_JOIN) strcpy(ftype,"join");
      else strcpy(ftype,"nick");
      break;
    case FLOOD_CTCP:
      thr=flood_ctcp_thr;
      lapse=flood_ctcp_time;
      strcpy(ftype,"ctcp");
      break;
  }
  if (thr==0) return 0;    /* no flood protection */
  /* okay, make sure i'm not flood-checking myself */
  strcpy(h,from); splitnick(floodnick,h);
  if (newbotname[0]) {
    if (strcasecmp(floodnick,newbotname)==0) return 0;
  }
  else if (strcasecmp(floodnick,botname)==0) return 0;
  if (strcasecmp(h,botuserhost)==0) return 0;   /* my user@host (?) */
  p=strchr(from,'!');
  if (p!=NULL) { p++; p=strchr(p,'@'); }
  if (p!=NULL) {
    p++; t=time(NULL);
    if (strcasecmp(lastmsghost[which],p)!=0) {   /* new */
      strcpy(lastmsghost[which],p); lastmsgtime[which]=t;
      lastmsgs[which]=0;
      return 0;
    }
  }
  if (p==NULL) return 0;   /* uh... whatever. */
  if (lastmsgtime[which] < t-lapse) {
    /* flood timer expired, reset it */
    lastmsgtime[which]=t; lastmsgs[which]=0;
    return 0;
  }
  lastmsgs[which]++;
  if (lastmsgs[which]>=thr) {   /* FLOOD */
    /* reset counters */
    lastmsgs[which]=0; lastmsgtime[which]=0; lastmsghost[which][0]=0;
    get_handle_by_host(handle,from);
    if (tochan) {
      if (check_tcl_flud(floodnick,from,handle,ftype,chan->name)) return 0;
    }
    else if (check_tcl_flud(floodnick,from,handle,ftype,"*")) return 0;
    if (((which==FLOOD_PRIVMSG) || (which==FLOOD_NOTICE)) && (!tochan)) {
      /* private msg */
      sprintf(h,"*!*@%s",p);
      putlog(LOG_MISC,"*","Flood from @%s!  Placing on ignore!",p);
      addignore(h,origbotname,"msg/notice flood",time(NULL)+(60*ignore_time));
#ifdef SILENCE
      /* attempt to use ircdu's SILENCE command */
      mprintf(serv,"SILENCE *@%s\n",p);
#endif
      return 1;
    }
    else if ((which==FLOOD_CTCP) && (!tochan)) {   /* ctcp flood (off-chan) */
      sprintf(h,"*!*@%s",p);
      putlog(LOG_MISC,"*","CTCP flood from @%s!  Placing on ignore!",p);
      addignore(h,origbotname,"ctcp flood",time(NULL)+(60*ignore_time));
#ifdef SILENCE
      /* attempt to use ircdu's SILENCE command */
      mprintf(serv,"SILENCE *@%s\n",p);
#endif
      return 1;
    }
    else if (which==FLOOD_JOIN) {   /* join flood */
      char sx[21];
      sprintf(h,"*!*@%s",p); strcpy(sx,"join flood");
      if (!isbanned(chan,h)) {
	add_mode(chan,'+','b',h); flush_mode(chan,QUICK);
      }
      if ((match_ban(from)) || (u_match_ban(chan->bans,from)))
	return 1;  /* already banned */
      putlog(LOG_MISC|LOG_JOIN,chan->name,"Join flood from @%s!  Banning.",p);
      u_addban(chan->bans,h,origbotname,sx,time(NULL)+(60*ban_time));
      if (!(chan->stat&CHAN_ENFORCEBANS))
	kick_match_since(chan,h,lastmsgtime[which]);
      return 1;
    }
    else if (which==FLOOD_NICK) {   /* nick flood */
      char sx[21]; struct chanset_t *ch;
      p=strchr(from,'!'); if (p!=NULL) {
	*p=0; sprintf(h,"*!%s",p+1); strcpy(sx,"nick flood");
	ch=chanset; while (ch!=NULL) {
	  m=ismember(ch,from); if (m!=NULL) {
	    if (!(m->flags & SENTKICK)) {
	      mprintf(serv,"KICK %s %s :nick flood\n",ch->name,from);
	      add_mode(ch,'+','b',h);
	      m->flags |= SENTKICK;
	    }
	  }
	  ch=ch->next;
	}
	addban(h,origbotname,sx,time(NULL)+(60*ban_time));
	*p='!';
      }
    }
    else {
      /* flooding chan! either by public or notice */
      p=strchr(from,'!'); if (p!=NULL) {
	putlog(LOG_MODES,chan->name,"Channel flood from %s -- kicking",
	       from);
	*p=0; mprintf(serv,"KICK %s %s :flood\n",chan->name,from);
	*p='!'; return 1;
      }
    }
  }
  return 0;
}

void write_debug()
{
  int x; time_t now=time(NULL); char s[80];
#ifdef EBUG
  int y;
#endif
  static int nested_debug=0;
  if (nested_debug) {
    /* yoicks, if we have this there's serious trouble */
    /* all of these are pretty reliable, so we'll try these */
    x=creat("DEBUG.DEBUG",0644); 
    setsock(x,SOCK_NONSOCK);
    if (x>=0) { 
      strcpy(s,ctime(&now));
      tprintf(x,"Debug (%s) written %s",ver,s);
      tprintf(x,"Full Patch List: %s\n",egg_xtra);
#ifdef EBUG
      tprintf(x,"Context: ");
      for (y=((cx_ptr+1)&15); y!=cx_ptr; y=((y+1)&15)) {
        tprintf(x,"%s/%d,\n         ",cx_file[y],cx_line[y]);
      }
      tprintf(x,"%s/%d\n\n",cx_file[y],cx_line[y]);
#else
      tprintf(x,"Context: %s/%d\n",cx_file,cx_line);
#endif
      killsock(x); close(x);
    }
    return;
  } 
  else nested_debug=1;
#ifdef EBUG
  putlog(LOG_MISC,"*","* Last context: %s/%d",cx_file[cx_ptr],cx_line[cx_ptr]);
#else
  putlog(LOG_MISC,"*","* Last context: %s/%d",cx_file,cx_line);
#endif
  x=creat("DEBUG",0644); setsock(x,SOCK_NONSOCK);
  if (x<0) { putlog(LOG_MISC,"*","* Failed to write DEBUG"); }
  else {
    strcpy(s,ctime(&now));
    tprintf(x,"Debug (%s) written %s",ver,s);
    tprintf(x,"Full Patch List: %s\n",egg_xtra);
#ifdef EBUG
    tprintf(x,"Context: ");
    for (y=((cx_ptr+1)&15); y!=cx_ptr; y=((y+1)&15)) {
       tprintf(x,"%s/%d,\n         ",cx_file[y],cx_line[y]);
    }
    tprintf(x,"%s/%d\n\n",cx_file[cx_ptr],cx_line[cx_ptr]);
#else
    tprintf(x,"Context: %s/%d\n",cx_file,cx_line);
#endif
    tell_dcc(-x); tprintf(x,"\n");
    debug_mem_to_dcc(-x);
    killsock(x); close(x);
    putlog(LOG_MISC,"*","* Wrote DEBUG");
  }
}

void got_bus PROTO1(int,z)
{
  write_debug();
  fatal("BUS ERROR -- CRASHING!",1);
}

void got_segv PROTO1(int,z)
{
  write_debug();
  fatal("SEGMENT VIOLATION -- CRASHING!",1);
}

void got_fpe PROTO1(int,z)
{
  write_debug();
  fatal("FLOATING POINT ERROR -- CRASHING!",1);
}

void got_term PROTO1(int,z)
{
#ifdef DIE_ON_SIGTERM
  write_userfile();
  tprintf(serv,"QUIT :terminate signal\n");
  fatal("TERMINATE SIGNAL -- SIGNING OFF",0);
#else
  putlog(LOG_MISC,"*","RECEIVED TERMINATE SIGNAL (IGNORING)");
  write_userfile();
  return;
#endif
}

void got_quit PROTO1(int,z)
{
  putlog(LOG_MISC,"*","RECEIVED QUIT SIGNAL (IGNORING)");
  return;
}

void got_hup PROTO1(int,z)
{
#ifdef DIE_ON_SIGHUP
  write_userfile();
  tprintf(serv,"QUIT :hangup signal\n");
  fatal("HANGUP SIGNAL -- SIGNING OFF",0);
#else
  putlog(LOG_MISC,"*","Received HUP signal: rehashing...");
  rehash();
  return;
#endif
}

void got_alarm PROTO1(int,z)
{
  /* connection to a server was ended prematurely */
  return;
}

void got_usr1 PROTO1(int,z)
{
  int i;
  putlog(LOG_MISC,"*","* USER1 SIGNAL: Debugging sockets");
  write_debug();
  if (fcntl(serv,F_GETFD,0)<0) {
    putlog(LOG_MISC,"*","* Server socket expired -- pfft");
    killsock(serv); serv=(-1);
  }
  for (i=0; i<dcc_total; i++) {
    if ((dcc[i].type!=DCC_FORK) && (dcc[i].type!=DCC_LOST))
      if (fcntl(dcc[i].sock,F_GETFD,0)<0) {
	putlog(LOG_MISC,"*",
	       "* DCC socket %d (type %d, nick '%s') expired -- pfft",
	       dcc[i].sock);
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
  }
  putlog(LOG_MISC,"*","* Finished test.");
}

/* got USR2 signal -- crash */
void got_usr2 PROTO1(int,z)
{
  putlog(LOG_MISC,"*","* Last context: %s/%d",cx_file,cx_line);
  write_debug();
  fatal("USR2 SIGNAL -- CRASHING!",0);
}

/* got ILL signal -- log context and continue */
void got_ill PROTO1(int,z)
{
  putlog(LOG_MISC,"*","* Context: %s/%d",cx_file,cx_line);
}

/* for relays: swallow all codes as if they don't exist */
void swallow_telnet_codes PROTO1(char *,buf)
{
  unsigned char *p=(unsigned char *)buf; int mark;
  while (*p!=0) {
    while ((*p!=255) && (*p!=0)) p++;   /* search for IAC */
    if (*p==255) {
      mark=2;
      if (!*(p+1)) mark=1;  /* bogus */
      if ((*(p+1)>=251) && (*(p+1)<=254)) {
	mark=3;
	if (!*(p+2)) mark=2;  /* bogus */
      }
      strcpy((char *)p,(char *)(p+mark));
    }
  }
}

void strip_telnet PROTO3(int,sock,char *,buf,int *,len)
{
  unsigned char *p=(unsigned char *)buf; int mark;
  while (*p!=0) {
    while ((*p!=255) && (*p!=0)) p++;   /* search for IAC */
    if (*p==255) {
      p++; mark=2;
      if (!*p) mark=1;  /* bogus */
      if ((*p>=251) && (*p<=254)) {
	mark=3;
	if (!*(p+1)) mark=2;  /* bogus */
      }
      if (*p==251) {
	/* WILL X -> response: DONT X */
	/* except WILL ECHO which we just smile and ignore */
	if (!(*(p+1)==1)) {
	  write(sock,"\377\376",2); write(sock,p+1,1);
	}
      }
      if (*p==253) {
	/* DO X -> response: WONT X */
	/* except DO ECHO which we just smile and ignore */
	if (!(*(p+1)==1)) {
	  write(sock,"\377\374",2); write(sock,p+1,1);
	}
      }
      if (*p==246) {
	/* "are you there?" */
	/* response is: "hell yes!" */
	write(sock,"\r\nHell, yes!\r\n",14);
      }
      /* anything else can probably be ignored */
      p--; strcpy((char *)p,(char *)(p+mark));  /* wipe code from buffer */
      *len=*len - mark;
    }
  }
}

void do_arg PROTO1(char *,s)
{
  int i;
  if (s[0]=='-') for (i=1; i < strlen(s); i++) {
    if (s[i]=='n') backgrd=0;
    if (s[i]=='c') { con_chan=1; term_z=0; }
    if (s[i]=='t') { con_chan=0; term_z=1; }
    if (s[i]=='m') make_userfile=1;
    if (s[i]=='v') {
      char x[256],z[81];
      strcpy(x,egg_version);
      nsplit(z,x); nsplit(z,x);
      printf("%s\n",version);
      if (x[0]) printf("  (patches: %s)\n",x);
      exit(0);
    }
    if (s[i]=='h') {
      printf("\n%s\n\n",version);
      printf("Command line arguments:\n");
      printf("  -h   help\n");
      printf("  -v   print version and exit\n");
      printf("  -n   don't go into the background\n");
      printf("  -c   (with -n) display channel stats every 10 seconds\n");
      printf("  -t   (with -n) use terminal to simulate dcc-chat\n");
      printf("  -m   userfile creation mode\n");
      printf("  optional config filename (default 'egg.config')\n");
      printf("\n"); exit(0);
    }
  }
  else strcpy(configfile,s);
}

/* hook up to a server */
/* works a little differently now... async i/o is your friend */
void connect_server()
{
  char s[121],pass[121]; static int oldserv=(-1);
  waiting_for_awake=0; trying_server=time(NULL); empty_msgq();
  /* start up the counter (always reset it if "never-give-up" is on) */
  if ((oldserv<0) || (never_give_up)) oldserv=curserv;
  if (newserverport) {   /* jump to specified server */
    curserv=(-1);  /* reset server list */
    strcpy(botserver,newserver); botserverport=newserverport;
    strcpy(pass,newserverpass);
    newserver[0]=0; newserverport=0; newserverpass[0]=0; oldserv=(-1);
  }
  next_server(&curserv,botserver,&botserverport,pass);
  putlog(LOG_SERV,"*","Trying server %s:%d",botserver,
	 botserverport);
  serv=open_telnet(botserver,botserverport);
  if (serv<0) {
    if (serv==(-2)) strcpy(s,"DNS lookup failed");
    else neterror(s);
    putlog(LOG_SERV,"*","Failed connect to %s (%s)",botserver,s);
    if ((oldserv==curserv) && !(never_give_up))
      fatal("NO SERVERS WILL ACCEPT MY CONNECTION.",1);
  }
  else {
    /* queue standard login */
    tprintf(serv,"NICK %s\n",botname);
    if (pass[0]) tprintf(serv,"PASS %s\n",pass);
    tprintf(serv,"USER %s %s %s :%s\n",botuser,bothost,botserver,botrealname);
    /* We join channels AFTER getting the 001 -Wild */
    /* wait for async result now */
  }
}

void backup_userfile()
{
  char s[150];
  putlog(LOG_MISC,"*","Backing up user file...");
  strcpy(s,userfile); strcat(s,"~bak");
  copyfile(userfile,s);
}

/* timer info: */
static int cnt=0,timecnt=0,fivemin=0,midnite=0,hourly=0,hourli=0;
static int switched=0,lastmin=99,miltime;
static time_t then,now;
static struct tm *nowtm;

void periodic_timers()
{
  time_t now; int i,j,k,l; char s[520];
#ifndef NO_IRC
  char s1[UHOSTLEN],hand[10];
  struct chanset_t *chan; memberlist *m;
#endif
  /* ONCE A SECOND */
  now=time(NULL); if (now!=then) {    /* once a second */
    context;
    random(); /* woop, lest really jumble things */
    timecnt++;  /* time to dequeue a msg? */
    if (timecnt==msgrate) { deq_msg(); timecnt=0; }
    check_utimers();  /* secondly timers */
    cnt++;
    if (cnt>=10) {    /* every 10 seconds */
      cnt=0;
      if ((con_chan) && (!backgrd)) {
	tprintf(STDOUT,"\033[2J\033[1;1H");
	tell_verbose_status(DP_STDOUT,0);
	tell_mem_status_dcc(DP_STDOUT);
      }
    }
    if ((!online) && (now-online_since>=60)) online=1;
  }
  nowtm=localtime(&now); then=now;
  if ((online) && (nowtm->tm_min!=lastmin)) {
    /* once a minute */
    context;
    lastmin=(lastmin+1)%60;
    check_tcl_time(nowtm);
#ifndef NO_IRC
    check_lonely_channels();
    if (keepnick) {
      if (strcmp(botname,origbotname)!=0) {
	/* see if my nickname is in use and if if my nick is right */
	tprintf(serv,"TRACE %s\n",origbotname);
	/* will return 206(undernet), 401(other),
	   or 402(efnet) numeric if not online */
      } /* if no nick change after a minute...assumin it succeeded???? 
	 * seems a bit silly to me
      else if (newbotname[0]) {
	newbotname[0]=0; waiting_for_awake=0;
	putlog(LOG_MISC,"*","Regained nickname '%s'.  (?)",botname);
      } */
    }
    check_idle_kick();
    /* join any channels that aren't active or pending */
    chan=chanset; while (chan!=NULL) {
      if (!(chan->stat&(CHANACTIVE|CHANPEND)))
	mprintf(serv,"JOIN %s\n",chan->name);
      chan=chan->next;
    }
    check_expired_splits();
    check_expired_ignores();
    check_expired_bans();
    check_for_split();  /* am *I* split? */
    check_expired_chanbans();
#endif   /* ifndef NO_IRC */
    check_expired_dcc();
    check_expired_tbufs();
    autolink_cycle(NULL); /* attempt autolinks */
    check_timers();
    /* in case for some reason more than 1 min has passed: */
    i=0;
    while (nowtm->tm_min!=lastmin) {
      /* timer drift, dammit */
      debug2("timer: drift (lastmin=%d, now=%d)",lastmin,nowtm->tm_min);
      check_timers(); i++;
      lastmin=(lastmin+1)%60;
      check_tcl_time(nowtm);
    }
    if (i>1) putlog(LOG_MISC,"*","(!) timer drift -- spun %d minutes",i);
  }
  context;
  if (((int)(nowtm->tm_min/5)*5)==(nowtm->tm_min))  {  /* 5 min */
    if (!fivemin) {
      fivemin=1; 
#ifndef NO_IRC
      log_chans();
#ifdef CHECK_STONED
      if (waiting_for_awake) {
	/* uh oh!  never got pong from last time, five minutes ago! */
	/* server is probably stoned */
	killsock(serv); serv=(-1);  /* will reconnect about 50 lines down */
	putlog(LOG_SERV,"*","Server got stoned; jumping...");
      }
      else {
	/* check for server being stoned */
	if ((serv>=0) && !trying_server) {
	  tprintf(serv,"PING :%lu\n",(unsigned long)time(NULL));
	  waiting_for_awake=1;
	}
      }
#endif /* CHECK_STONED */
#endif /* !NO_IRC */
      check_botnet_pings();
      flushlogs();
    }
  }
  else fivemin=0;
  context;
  miltime=(nowtm->tm_hour*100)+(nowtm->tm_min);
  if (miltime==0) {   /* at midnight */
    if (!midnite) {
      midnite=1;
      strcpy(s,ctime(&now));
      s[strlen(s)-1]=0; strcpy(&s[11],&s[20]);
      putlog(LOG_MISC,"*","--- %s",s);
      backup_userfile();
      for (j=0; j<MAXLOGS; j++) {
        if (logs[j].filename!=NULL && logs[j].f!=NULL) {
	  fclose(logs[j].f); logs[j].f=NULL;
        }
      }
    }
  }
  else midnite=0;
  if (miltime==switch_logfiles_at) {
    if (!switched) {
      switched=1;
      expire_notes();
      if (!keep_all_logs) {
	putlog(LOG_MISC,"*","Switching logfiles...");
	for (i=0; i<MAXLOGS; i++) if (logs[i].filename!=NULL) {
	  if (logs[i].f!=NULL) { fclose(logs[i].f); logs[i].f=NULL; }
	  sprintf(s,"%s.yesterday",logs[i].filename); unlink(s);
#ifdef RENAME
	  rename(logs[i].filename,s);
#else
	  movefile(logs[i].filename,s);
#endif
	}
      }
    }
  }
  else switched=0;
  if (nowtm->tm_min==save_users_at) {    /* hourly! */
    if (!hourly) {
      hourly=1;
      write_userfile();
    }
  }
  else hourly=0;
  if (nowtm->tm_min==notify_users_at) {    /* hourli! */
    context;
    if (!hourli) {
      hourli=1;
#ifndef NO_IRC
      chan=chanset; while (chan!=NULL) {
	m=chan->channel.member;  while (m->nick[0]) {
	  sprintf(s1,"%s!%s",m->nick,m->userhost);
	  get_handle_by_host(hand,s1);
	  k=num_notes(hand);
	  for (l=0; l<dcc_total; l++) {
	    if ((dcc[l].type==DCC_CHAT) && (strcasecmp(dcc[l].nick,hand)==0))
	      k=0;    /* they already know they have notes */
	  }
	  if (k) {
	    hprintf(serv,"NOTICE %s :You have %d note%s waiting on %s.\n",
		    m->nick,k,k==1?"":"s",botname);
	    hprintf(serv,"NOTICE %s :For a list, /MSG %s NOTES [pass] INDEX\n",
		    m->nick,botname);
	  }
	  m=m->next;
	}
	chan=chan->next;
      }
#endif
      for (l=0; l<dcc_total; l++) {
	k=num_notes(dcc[l].nick);
	if (k>0 && dcc[l].type==DCC_CHAT) {
	  dprintf(l,"### You have %d note%s waiting.\n",k,k==1?"":"s");
	  dprintf(l,"### Use '.notes read' to read them.\n");
	}
      }
    }
  }
  else hourli=0;
}

int main PROTO2(int,argc,char **,argv)
{
  int xx,i; char buf[520],s[520]; FILE *f;
  struct sigaction sv; int modecnt=0;
#ifndef NO_IRC
  char from[121],code[520],msg[520]; struct chanset_t *chan; int j;
#endif
  /* initialise context list */
  for (i=0;i<16;i++) { context; }
   
  /* --- PATCH INFO GOES HERE --- */
   
#ifdef MODULES
   PATCH("modules");
#endif
   
  /* --- END OF PATCH INFO --- */

  /* version info! */
  sprintf(ver,"eggdrop v%s",egg_version);
  sprintf(version,"Eggdrop v%s  (c)1997 Robey Pointer",egg_version);
  /* now add on the patchlevel (for Tcl) */
  sprintf(&egg_version[strlen(egg_version)]," %08u",egg_numver);
  strcat(egg_version,egg_xtra);
  strncpy(ctcp_version,ver,160);
  ctcp_version[160] = 0;
  strncpy(ctcp_finger,ver,160);
  ctcp_finger[160] = 0;
  strncpy(ctcp_userinfo,ver,160);
  ctcp_userinfo[160] = 0;
  context;
#ifdef STOP_UAC
  {
    int nvpair[2];
    nvpair[0]=SSIN_UACPROC;
    nvpair[1]=UAC_NOPRINT;
    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif
  if (argc>1) for (i=1; i<argc; i++) do_arg(argv[i]);
  printf("\n%s\n",version);
  /* set up error traps: */
  sv.sa_handler=got_bus; sigemptyset(&sv.sa_mask);
  sv.sa_flags=0; sigaction(SIGBUS,&sv,NULL);
  sv.sa_handler=got_segv; sigaction(SIGSEGV,&sv,NULL);
  sv.sa_handler=got_fpe; sigaction(SIGFPE,&sv,NULL);
  sv.sa_handler=got_term; sigaction(SIGTERM,&sv,NULL);
  sv.sa_handler=got_hup; sigaction(SIGHUP,&sv,NULL);
  sv.sa_handler=got_quit; sigaction(SIGQUIT,&sv,NULL);
/*  sv.sa_handler=got_pipe; sigaction(SIGPIPE,&sv,NULL); */
  sv.sa_handler=SIG_IGN; sigaction(SIGPIPE,&sv,NULL);
  sv.sa_handler=got_usr1; sigaction(SIGUSR1,&sv,NULL);
  sv.sa_handler=got_usr2; sigaction(SIGUSR2,&sv,NULL);
  sv.sa_handler=got_ill; sigaction(SIGILL,&sv,NULL);
  sv.sa_handler=got_alarm; sigaction(SIGALRM,&sv,NULL);
/*  sv.sa_handler=got_child; sigaction(SIGCHLD,&sv,NULL);  */

  /* initialize variables and stuff */
  now=time(NULL);
  botname[0]=0; botserver[0]=0; newbotname[0]=0; chanset=NULL;
  nowtm=localtime(&now); lastmin=nowtm->tm_min;
  srandom(time(NULL));
  init_mem(); init_misc(); init_bots(); init_net(); 
#ifdef MODULES
  init_modules();
#else
  init_blowfish();
#endif
  init_tcl(); chanprog();
  context;
#ifdef MODULES
  if (encrypt_pass==0) {
     printf("You have installed modules but have not selected an encryption\n");
     printf("module, please consult the default config file for info.\n");
     exit(1);
  }
#endif
  cache_miss=0; cache_hit=0;
  getmyhostname(bothost);
  context;
  sprintf(botuserhost,"%s@%s",botuser,bothost);  /* wishful thinking */
  get_first_server();
  context;
  sprintf(pid_file,"pid.%s",botnetnick);
  context;
  /* check for pre-existing eggdrop! */
  f=fopen(pid_file,"r"); if (f!=NULL) {
    fgets(s,10,f); xx=atoi(s);
    kill(xx,SIGCHLD);   /* meaningless kill to determine if pid is used */
    if (errno!=ESRCH) {
      printf("I detect %s already running from this directory.\n",origbotname);
      printf("If this is incorrect, erase the '%s' file.\n\n",pid_file);
      exit(1);
    }
  }
  context;
  i=count_users(userlist); use_stderr=0;
#ifndef NO_IRC
  j=0; chan=chanset; while (chan!=NULL) {
    j++; chan=chan->next;
  }
#endif
  strcpy(s,ctime(&now)); s[strlen(s)-1]=0; strcpy(&s[11],&s[20]);
  putlog(LOG_ALL,"*","");
  putlog(LOG_ALL,"*","--- Loading %s (%s)",ver,s);
#ifdef NO_IRC
  putlog(LOG_ALL,"*","=== %s: in limbo, %d users.",botname,i);
  use_stderr=1;
  printf("\n%s: in limbo, %d users.\n",botname,i);
#else
  putlog(LOG_ALL,"*","=== %s: %d channel%s, %d users.",botname,j,j==1?"":"s",
	 i);
  use_stderr=1;
  printf("\n%s: %d channel%s, %d users.\n",botname,j,j==1?"":"s",i);
#endif
  /* move into background? */
  if (backgrd) {
    xx=fork();
    if (xx==-1) fatal("CANNOT FORK PROCESS.",0);
    if (xx!=0) {
      printf("Launched into the background  (pid: %d)\n\n",xx);
#if HAVE_SETPGID
      setpgid(xx,xx);
#endif
      exit(0);
    }
  }
  use_stderr=0;   /* stop writing to stderr now */
  xx=getpid();
  if (xx!=0) {
    FILE *fp;
    /* write pid to file */
    unlink(pid_file);
    fp=fopen(pid_file,"w"); if (fp!=NULL) {
      fprintf(fp,"%u\n",xx);
      fclose(fp);
    }
    else printf("* Warning!  Could not write %s file!\n",pid_file);
  }
  if (backgrd) {
    /* ok, try to disassociate from controlling terminal */
    /* (finger cross) */
#if HAVE_SETPGID
    setpgid(0,0);
#endif
    /* close out stdin/out/err */
    freopen("/dev/null","r",stdin);
    freopen("/dev/null","w",stdout);
    freopen("/dev/null","w",stderr);
    /* tcl wants those file handles kept open */
/*    close(0); close(1); close(2);  */
  }
  /* terminal emulating dcc chat */
  if ((!backgrd) && (term_z)) {
    int n=dcc_total;
    dcc[n].addr=iptolong(getmyip());
    dcc[n].port=0;
    dcc[n].sock=STDOUT;
    dcc[n].type=DCC_CHAT;
    set_chat(n);
    dcc[n].u.chat->away=NULL;
    dcc[n].u.chat->status=0;
    dcc[n].u.chat->timer=time(NULL);
    dcc[n].u.chat->msgs_per_sec=0;
    dcc[n].u.chat->con_flags=conmask;
    dcc[n].u.chat->channel=0;
    dcc[n].u.chat->buffer=NULL;
    dcc[n].u.chat->max_line=0;   
    dcc[n].u.chat->line_count=0;
    dcc[n].u.chat->current_lines=0;
    strcpy(dcc[n].nick,"HQ");
    strcpy(dcc[n].host,"local console");
    setsock(STDOUT,0);  /* entry in net table */
    dprintf(n,"\n### ENTERING DCC CHAT SIMULATION ###\n\n");
    dcc_total++;
    dcc_chatter(n);
  }
  debug0("main: initialization done, server connect");
#ifndef NO_IRC
  /* now connect to a server: */
  newserver[0]=0; newserverport=0;
  connect_server();
#else
  /* initialize irc things so they won't bother us */
  serv=(-1); dccdir[0]=0;
  strcpy(botname,origbotname);
#endif
  then=time(NULL); online_since=time(NULL);
  autolink_cycle(NULL);   /* hurry and connect to tandem bots */
  debug0("main: entering loop");
  while(1) {
    periodic_timers();
#ifndef NO_IRC
    if (serv<0) { clear_channels(); connect_server(); }
#endif
    /* clean up sockets that were just left for dead */
    for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_LOST) {
      dcc[i].type=dcc[i].sock; lostdcc(i); i--;
    }
    /* check for server or dcc activity */
    dequeue_sockets();
    /* new net routines: help me mary! */
    xx=sockgets(buf,&i);
    if (xx>=0) {  /* non-error */
#ifdef NO_IRC
      dcc_activity(xx,buf,i);
#else
      if (xx!=serv) dcc_activity(xx,buf,i);
      else {   /* SERVER ACTIVITY */
	if (trying_server) {
	  putlog(LOG_SERV,"*","Connected to %s",botserver);
	  server_online=time(NULL);
	  trying_server=0; waiting_for_awake=0;
        }
        strcpy(s,buf);
        parsemsg(buf,from,code,msg); fixfrom(from);
#ifdef USE_CONSOLE_R
	if ((strcmp(code,"PRIVMSG")==0) || (strcmp(code,"NOTICE")==0)) {
          if (!match_ignore(from)) putlog(LOG_RAW,"*","[@] %s",s);
	}
	else putlog(LOG_RAW,"*","[@] %s",s);
#endif
	context;
#ifdef RAW_BINDS
        if (check_tcl_raw(from,code,msg)) { /* nothing */ }
        else if (strcmp(code,"PRIVMSG")==0) gotmsg(from,msg,match_ignore(from));
#else
	if (strcmp(code,"PRIVMSG")==0) gotmsg(from,msg,match_ignore(from));
#endif
	else if (strcmp(code,"NOTICE")==0) gotnotice(from,msg,match_ignore(from));
	else if (strcmp(code,"MODE")==0) gotmode(from,msg);
	else if (strcmp(code,"JOIN")==0) gotjoin(from,msg);
	else if (strcmp(code,"PART")==0) gotpart(from,msg);
	else if (strcmp(code,"ERROR")==0) goterror(from,msg);
	else if (strcmp(code,"PONG")==0) gotpong(from,msg);
	else if (strcmp(code,"WALLOPS")==0) gotwall(from,msg);
	else if (strcmp(code,"001")==0) got001(from,msg);
	else if (strcmp(code,"206")==0) trace_fail(from,msg); /* undernet */
	else if (strcmp(code,"251")==0) got251(from,msg);
	else if (strcmp(code,"302")==0) got302(from,msg);
	else if (strcmp(code,"315")==0) got315(from,msg);
	else if (strcmp(code,"324")==0) got324(from,msg);
	else if (strcmp(code,"331")==0) got331(from,msg);
	else if (strcmp(code,"332")==0) got332(from,msg);
	else if (strcmp(code,"352")==0) got352(from,msg);
	else if (strcmp(code,"367")==0) got367(from,msg);
	else if (strcmp(code,"368")==0) got368(from,msg);
	else if (strcmp(code,"401")==0) trace_fail(from,msg); /* other */
	else if (strcmp(code,"402")==0) trace_fail(from,msg); /* efnet */
	else if (strcmp(code,"405")==0) got405(from,msg);
	else if (strcmp(code,"432")==0) got432(from,msg);
	else if (strcmp(code,"433")==0) got433(from,msg);
	else if (strcmp(code,"437")==0) got437(from,msg);    
	else if (strcmp(code,"442")==0) got442(from,msg);
	else if (strcmp(code,"451")==0) got451(from,msg);
	else if (strcmp(code,"471")==0) got471(from,msg);
	else if (strcmp(code,"473")==0) got473(from,msg);
	else if (strcmp(code,"474")==0) got474(from,msg);
	else if (strcmp(code,"475")==0) got475(from,msg);
	else if (strcmp(code,"QUIT")==0) gotquit(from,msg);
	else if (strcmp(code,"NICK")==0) {
          detect_flood(from,NULL,FLOOD_NICK,0);
          gotnick(from,msg);
	}
	else if (strcmp(code,"KICK")==0) gotkick(from,msg);
	else if (strcmp(code,"INVITE")==0) gotinvite(from,msg);
	else if ((strcmp(code,"375")==0) || (strcmp(code,"376")==0) ||
		 (strcmp(code,"372")==0))  ;  /* ignore motd */
	else if (strcmp(code,"TOPIC")==0) gottopic(from,msg);
	else if (strcmp(code,"PING")==0) {
	  fixcolon(msg); tprintf(serv,"PONG :%s\n",msg);
	}
      }
      /* in periods of high traffic (sockgets always returns info), only */
      /* flush the stacked modes every 5th time -- in calmer times (when */
      /* sockgets sometimes spends a whole second with no input) dump out */
      /* any pending modes */
      context;
      if (modecnt==4) flush_modes();  /* dump all mode changes */
      modecnt=(modecnt+1)%5;
#endif  /* NO_IRC */
    }
    else if (xx==-1) {   /* EOF from someone */
      context;
#ifdef NO_IRC
      if ((i!=STDOUT) || backgrd) eof_dcc(i);
      else fatal("EOF ON TERMINAL",1);
#else
      if (i==serv) {
	/* we lost this server, dammit */
	putlog(LOG_SERV,"*","Disconnected from %s",botserver);
	clear_channels();    /* we're not on any channels any more */
	killsock(serv);
	connect_server();
      }
      else if ((i!=STDOUT) || backgrd) eof_dcc(i);
      else fatal("END OF FILE ON TERMINAL",1);
#endif
    }
    else if ((xx==-2) && (errno!=EINTR)) {      /* select() error */
      context;
      putlog(LOG_MISC,"*","* Socket error #%d; recovering.",errno);
#ifndef NO_IRC
      if (fcntl(serv,F_GETFD,0)==(-1)) {
	putlog(LOG_MISC,"*","Server socket expired -- pfft");
	killsock(serv); serv=(-1);
      }
#endif
      for (i=0; i<dcc_total; i++) {
	if ((dcc[i].type!=DCC_LOST) && (dcc[i].type!=DCC_FORK))
	  if (fcntl(dcc[i].sock,F_GETFD,0)==(-1)) {
	    putlog(LOG_MISC,"*",
		   "DCC socket %d (type %d, name '%s') expired -- pfft",
		   dcc[i].sock,dcc[i].type,dcc[i].nick);
	    killsock(dcc[i].sock); lostdcc(i); i--;
	  }
      }
    }
    else if (xx==(-3)) {
      /* nothing happened */
      if (modecnt) {
	flush_modes(); modecnt=0;
      }
    }
    if (do_restart) {
       Tcl_DeleteInterp(interp);
       init_tcl();
       rehash();
       do_restart = 0;
    }
  }
}
