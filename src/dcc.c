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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "eggdrop.h"
#include "chan.h"
#include "proto.h"

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

/* dcc list */
struct dcc_t dcc[MAXDCC];
/* total dcc's */
int dcc_total=0;
/* root dcc directory */
char dccdir[121]="";
/* directory to put incoming dcc's into */
char dccin[121]="";
/* temporary directory (default: current dir) */
char tempdir[121]="";
/* require 'p' access to get on the party line? */
int require_p=0;
/* let all uploads go to the user's current directory? */
int upload_to_cd=0;
/* allow people to introduce themselves via telnet */
int allow_new_telnets=0;
/* how many bytes should we send at once? */
int dcc_block=1024;
/* name of the IRC network you're on */
char network[41]="unknown-net";


void stop_auto(nick)
char *nick;
{
  int i;
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_FORK) && (dcc[i].u.fork->type==DCC_BOT)) {
      killsock(dcc[i].sock);
      dcc[i].sock=dcc[i].type; dcc[i].type=DCC_LOST;
    }
}

void greet_new_bot(idx)
int idx;
{
  int atr=get_attr_handle(dcc[idx].nick);
  stop_auto(dcc[idx].nick);
  dcc[idx].u.bot->timer=time(NULL);
  dcc[idx].u.bot->version[0]=0;
  if (atr & BOT_REJECT) {
    putlog(LOG_BOTS,"*","Rejecting link from %s",dcc[idx].nick);
    tprintf(dcc[idx].sock,"error You are being rejected.\n");
    tprintf(dcc[idx].sock,"bye\n");
    killsock(dcc[idx].sock);
    dcc[idx].sock=dcc[idx].type; dcc[idx].type=DCC_LOST;
    return;
  }
  if (atr & BOT_LEAF) dcc[idx].u.bot->status|=STAT_LEAF;
  tprintf(dcc[idx].sock,"version %s %s\n",ver,network);
  tprintf(dcc[idx].sock,"thisbot %s\n",botnetnick);
  putlog(LOG_BOTS,"*","Linked to %s",dcc[idx].nick);
  chatout("*** Linked to %s\n",dcc[idx].nick);
  tandout_but(idx,"nlinked %s %s\n",dcc[idx].nick,botnetnick);
  tandout_but(idx,"chat %s Linked to %s\n",botnetnick,dcc[idx].nick);
  dump_links(dcc[idx].sock); addbot(dcc[idx].nick,dcc[idx].nick,botnetnick);
  check_tcl_link(dcc[idx].nick,botnetnick);
}

void dcc_chat_pass(idx,buf)
int idx; char *buf;
{
  int atr=get_attr_handle(dcc[idx].nick);
  if (pass_match_by_handle(buf,dcc[idx].nick)) {
    if (atr & USER_BOT) {
      nfree(dcc[idx].u.chat);
      dcc[idx].type=DCC_BOT;
      set_tand(idx);
      dcc[idx].u.bot->status=STAT_CALLED;
      tprintf(dcc[idx].sock,"*hello!\n");
      greet_new_bot(idx);
    }
    else {
      dcc[idx].type=DCC_CHAT;
      dcc[idx].u.chat->status&=~STAT_CHAT;
      if (atr & USER_MASTER) dcc[idx].u.chat->con_flags=conmask;
      if (dcc[idx].u.chat->status&STAT_TELNET)
	tprintf(dcc[idx].sock,"\377\374\001\n");  /* turn echo back on */
      dcc_chatter(idx);
    }
  }
  else {
    if (get_attr_handle(dcc[idx].nick) & USER_BOT)
      tprintf(dcc[idx].sock,"badpass\n");
    else dprintf(idx,"Negative on that, Houston.\n");
    putlog(LOG_MISC,"*","Bad password: DCC chat [%s]%s",dcc[idx].nick,
	   dcc[idx].host);
    killsock(dcc[idx].sock); lostdcc(idx);
  }
}

void dcc_bot_new(idx,buf)
int idx; char *buf;
{
  if (strcasecmp(buf,"*hello!")==0) {
    dcc[idx].type=DCC_BOT;
    greet_new_bot(idx);
  }
  if (strcasecmp(buf,"badpass")==0) {
    /* we entered the wrong password */
    putlog(LOG_BOTS,"*","Bad password on connect attempt to %s.",
	   dcc[idx].nick);
  }
  if (strcasecmp(buf,"passreq")==0) {
    if (pass_match_by_handle("-",dcc[idx].nick)) {
      putlog(LOG_BOTS,"*","Password required for connection to %s.",
	     dcc[idx].nick);
      tprintf(dcc[idx].sock,"-\n");
    }
  }
  if (strncmp(buf,"error",5)==0) {
    split(NULL,buf);
    putlog(LOG_BOTS,"*","ERROR linking %s: %s",dcc[idx].nick,buf);
  }
  /* ignore otherwise */
}

#ifndef NO_FILE_SYSTEM
void dcc_files_pass(idx,buf)
int idx; char *buf;
{
  if (pass_match_by_handle(buf,dcc[idx].nick)) {
    if (too_many_filers()) {
      dprintf(idx,"Too many people are in the file system right now.\n");
      dprintf(idx,"Please try again later.\n");
      putlog(LOG_MISC,"*","File area full: DCC chat [%s]%s",dcc[idx].nick,
	     dcc[idx].host);
      killsock(dcc[idx].sock); lostdcc(idx);  
      return;
    }
    dcc[idx].type=DCC_FILES;
    if (dcc[idx].u.file->chat->status&STAT_TELNET)
      tprintf(dcc[idx].sock,"\377\374\001\n");  /* turn echo back on */
    putlog(LOG_FILES,"*","File system: [%s]%s/%d",dcc[idx].nick,
	   dcc[idx].host,dcc[idx].port);
    if (!welcome_to_files(idx)) {
      putlog(LOG_FILES,"*","File system broken.");
      killsock(dcc[idx].sock); lostdcc(idx);
    }
    return;
  }
  dprintf(idx,"Negative on that, Houston.\n");
  putlog(LOG_MISC,"*","Bad password: DCC chat [%s]%s",dcc[idx].nick,
	 dcc[idx].host);
  killsock(dcc[idx].sock); lostdcc(idx);
}
#endif

void dcc_fork(idx,buf)
int idx; char *buf;
{
  switch(dcc[idx].u.fork->type) {
#ifndef NO_IRC
  case DCC_FILES:
  case DCC_CHAT:
#endif
  case DCC_SEND:
    cont_got_dcc(idx); break;
  case DCC_BOT:
    cont_link(idx); break;
  case DCC_RELAY:
    cont_tandem_relay(idx); break;
  case DCC_RELAYING:
    pre_relay(idx,buf); break;
  default:
    putlog(LOG_MISC,"*","!!! unresolved fork type %d",dcc[idx].u.fork->type);
  }
}

/* ie, connect failed. :) */
void eof_dcc_fork(idx)
int idx;
{
  switch(dcc[idx].u.fork->type) {
#ifndef NO_IRC
  case DCC_SEND:
  case DCC_FILES:
  case DCC_CHAT:
    failed_got_dcc(idx); break;
#endif
  case DCC_BOT:
    failed_link(idx); break;
  case DCC_RELAY:
    failed_tandem_relay(idx); break;
  case DCC_RELAYING:
    failed_pre_relay(idx); break;
  }
}

#ifndef NO_FILE_SYSTEM
void eof_dcc_send(idx)
int idx;
{
  int ok,j; char ofn[121],nfn[121],hand[41],s[161];
  context;
  if (dcc[idx].u.xfer->length==dcc[idx].u.xfer->sent) {
    /* success */
    ok=0;
    fclose(dcc[idx].u.xfer->f);
    if (strcmp(dcc[idx].nick,"*users")==0) {
      finish_share(idx);
      killsock(dcc[idx].sock); lostdcc(idx);
      return;
    }
    putlog(LOG_FILES,"*","Completed dcc send %s from %s!%s",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    sprintf(s,"%s!%s",dcc[idx].nick,dcc[idx].host);
    get_handle_by_host(hand,s);
    /* move the file from /tmp */
    sprintf(ofn,"%s%s",tempdir,dcc[idx].u.xfer->filename);
    sprintf(nfn,"%s%s",dcc[idx].u.xfer->dir,dcc[idx].u.xfer->filename);
    if (movefile(ofn,nfn))
      putlog(LOG_MISC|LOG_FILES,"*","FAILED move %s from %s ! File lost!",
	     dcc[idx].u.xfer->filename,tempdir);
    else {
      /* add to file database */
      add_file(dcc[idx].u.xfer->dir,dcc[idx].u.xfer->filename,hand);
      stats_add_upload(hand,dcc[idx].u.xfer->length);
      check_tcl_rcvd(hand,dcc[idx].nick,nfn);
    }
    for (j=0; j<dcc_total; j++)
      if ((!ok) && ((dcc[j].type==DCC_CHAT) || (dcc[j].type==DCC_FILES)) &&
	  (strcasecmp(dcc[j].nick,hand)==0)) {
	ok=1;
	dprintf(j,"Thanks for the file!\n");
      }
    if (!ok) mprintf(serv,"NOTICE %s :Thanks for the file!\n",
		     dcc[idx].nick);
    killsock(dcc[idx].sock); lostdcc(idx);
    return;
  }
  /* failure :( */
  fclose(dcc[idx].u.xfer->f);
  if (strcmp(dcc[idx].nick,"*users")==0) {
    int x,y=0;
    for (x=0; x<dcc_total; x++)
      if ((strcasecmp(dcc[x].nick,dcc[idx].host)==0) &&
	  (dcc[x].type==DCC_BOT))
	y=x;
    putlog(LOG_MISC,"*","Lost userfile transfer to %s; aborting.",
	   dcc[y].nick);
    unlink(dcc[idx].u.xfer->filename);
    /* drop that bot */
    tprintf(dcc[y].sock,"bye\n");
    tandout_but(y,"unlinked %s\n",dcc[y].nick);
    tandout_but(y,"chat %s Disconnected %s (aborted userfile transfer)\n",
		botnetnick,dcc[y].nick);
    chatout("*** Disconnected %s (aborted userfile transfer)\n",dcc[y].nick);
    cancel_user_xfer(y);
    rembot(dcc[y].nick,dcc[y].nick); unvia(y,dcc[y].nick);
    killsock(dcc[y].sock); dcc[y].sock=dcc[y].type; dcc[y].type=DCC_LOST;
  }
  else {
    putlog(LOG_FILES,"*","Lost dcc send %s from %s!%s (%lu/%lu)",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host,
	   dcc[idx].u.xfer->sent,dcc[idx].u.xfer->length);
    sprintf(s,"%s%s",tempdir,dcc[idx].u.xfer->filename); unlink(s);
  }
  killsock(dcc[idx].sock); lostdcc(idx);
}
#endif

/* make sure ansi code is just for color-changing */
int check_ansi(v)
char *v;
{
  int count=2;
  if (*v++!='\033') return 1;
  if (*v++!='[') return 1;
  while (*v) {
    if (*v=='m') return 0;
    if ((*v!=';') && ((*v<'0') || (*v>'9'))) return count;
    v++; count++;
  }
  return count;
}

void dcc_chat(idx,buf)
int idx; char *buf;
{
  int i,nathan=0,doron=0,fixed=0; char *v=buf;
  context;
  if (detect_dcc_flood(dcc[idx].u.chat,idx)) return;
  dcc[idx].u.chat->timer=time(NULL);
  if (buf[0]) strcpy(buf,check_tcl_filt(idx,buf));
  if (buf[0]) {
    /* check for beeps and cancel annoying ones */
    for(v=buf; *v; v++)
    switch(*v) {
    case 7:  /* beep - no more than 3 */
      nathan++;
      if (nathan>3) strcpy(v,v+1);
      else v++;
      break;
    case 8: /* backspace - for lame telnet's :) */
      if (v > buf) {
	 v--;
	 strcpy(v,v+2);
      }
      break;
    case 27:  /* ESC - ansi code? */
      doron=check_ansi(v);
      /* if it's valid, append a return-to-normal code at the end */
      if (!doron) {
        if (!fixed) strcat(buf,"\033[0m");
        v++; fixed=1;
      }  
      else strcpy(v,v+doron);
      break;
    case '\r':   /* weird pseudo-linefeed */
      strcpy(v,v+1); break;
    }
    if (buf[0]) { /* nothing to say - maybe paging...*/
      if ((buf[0]=='.') || (dcc[idx].u.chat->channel<0)) {
        if (buf[0]=='.') strcpy(buf,&buf[1]);
        if (got_dcc_cmd(idx,buf)) {
          check_tcl_chpt(botnetnick,dcc[idx].nick,dcc[idx].sock);
          check_tcl_chof(dcc[idx].nick,dcc[idx].sock);
          dprintf(idx,"*** Ja mata!\n");
	  flush_lines(idx);
          putlog(LOG_MISC,"*","DCC connection closed (%s!%s)",dcc[idx].nick,
     	         dcc[idx].host);
          if (dcc[idx].u.chat->channel>=0) {
      	    chanout2(dcc[idx].u.chat->channel,"%s left the party line%s%s\n",
	 	     dcc[idx].nick,buf[0]?": ":".",buf);
	    context;
            if (dcc[idx].u.chat->channel<100000)
   	      tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
          }
          if ((dcc[idx].sock!=STDOUT) || backgrd) {
   	    killsock(dcc[idx].sock); lostdcc(idx);
	    return;
          }
          else {
  	    tprintf(STDOUT,"\n### SIMULATION RESET\n\n");
   	    dcc_chatter(idx);
	    return;
          }
        }
      } 
      else if (buf[0]==',') {
        for (i=0; i<dcc_total; i++) {
          if ((dcc[i].type==DCC_CHAT) &&
	      (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	      (dcc[i].u.chat->channel>=0) &&
	      ((i!=idx) || (dcc[idx].u.chat->status&STAT_ECHO)))
            dprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
          if ((dcc[i].type==DCC_FILES) &&
	      (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	      ((i!=idx) || (dcc[idx].u.file->chat->status&STAT_ECHO)))
            dprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
        }
      } 
      else {
        if (dcc[idx].u.chat->away!=NULL) not_away(idx);
        if (dcc[idx].u.chat->status&STAT_ECHO)
          chanout(dcc[idx].u.chat->channel,"<%s> %s\n",dcc[idx].nick,buf);
        else chanout_but(idx,dcc[idx].u.chat->channel,"<%s> %s\n",
			 dcc[idx].nick,buf);
        if (dcc[idx].u.chat->channel<100000)
        tandout("chan %s@%s %d %s\n",dcc[idx].nick,botnetnick,
	        dcc[idx].u.chat->channel,buf);
        check_tcl_chat(dcc[idx].nick,dcc[idx].u.chat->channel,buf);
      }
    }
  }
  if (dcc[idx].type==DCC_CHAT) /* could have change to files */
    if (dcc[idx].u.chat->status & STAT_PAGE)
      flush_lines(idx);
}

#ifndef NO_FILE_SYSTEM
void dcc_files(idx,buf)
int idx; char *buf;
{
  int i;
  if (detect_dcc_flood(dcc[idx].u.file->chat,idx)) return;
  dcc[idx].u.file->chat->timer=time(NULL);
  strcpy(buf,check_tcl_filt(idx,buf));
  if (!buf[0]) return;
  if (buf[0]==',') {
    for (i=0; i<dcc_total; i++) {
      if ((dcc[i].type==DCC_CHAT) &&
	  (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	  (dcc[i].u.chat->channel>=0) &&
	  ((i!=idx) || (dcc[idx].u.chat->status&STAT_ECHO)))
	dprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
      if ((dcc[i].type==DCC_FILES) &&
	  (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	  ((i!=idx) || (dcc[idx].u.file->chat->status&STAT_ECHO)))
	dprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
    }
  }
  else if (got_files_cmd(idx,buf)) {
    dprintf(idx,"*** Ja mata!\n");
    putlog(LOG_FILES,"*","DCC user [%s]%s left file system",dcc[idx].nick,
	   dcc[idx].host);
    set_handle_dccdir(userlist,dcc[idx].nick,dcc[idx].u.file->dir);
    if (dcc[idx].u.file->chat->status&STAT_CHAT) {
      struct chat_info *ci;
      dprintf(idx,"Returning you to command mode...\n");
      ci=dcc[idx].u.file->chat; nfree(dcc[idx].u.file);
      dcc[idx].u.chat=ci;
      dcc[idx].u.chat->status&=(~STAT_CHAT); dcc[idx].type=DCC_CHAT;
      if (dcc[idx].u.chat->channel>=0) {
	chanout2(dcc[idx].u.chat->channel,"%s has returned.\n",dcc[idx].nick);
        context;
	if (dcc[idx].u.chat->channel<100000)
	  tandout("unaway %s %d\n",botnetnick,dcc[idx].sock);
      }
    }
    else {
      dprintf(idx,"Dropping connection now.\n");
      putlog(LOG_FILES,"*","Left files: [%s]%s/%d",dcc[idx].nick,
	     dcc[idx].host,dcc[idx].port);
      killsock(dcc[idx].sock); lostdcc(idx);
    }
  }
}
#endif

void dcc_telnet(idx,buf)
int idx; char *buf;
{
  unsigned long ip; unsigned short port; int i,j; char s[121],s1[81];
  i=dcc_total;
  if (i+1>MAXDCC) {
    j=answer(dcc[idx].sock,s,&ip,&port,0);
    if (j!=-1) {
      tprintf(j,"Sorry, too many connections already.\r\n");
      killsock(j);
    }
    return;
  }
  dcc[i].sock=answer(dcc[idx].sock,s,&ip,&port,0);
  while ((dcc[i].sock==(-1)) && (errno==EAGAIN))
    dcc[i].sock=answer(dcc[idx].sock,s,&ip,&port,0);
  if (dcc[i].sock < 0) {
    neterror(s1);
    putlog(LOG_MISC,"*","Failed TELNET incoming (%s)",s1);
    killsock(dcc[i].sock);
    return;
  }
  sprintf(dcc[i].host,"telnet!telnet@%s",s);
  if (match_ignore(dcc[i].host)) {
/*    tprintf(dcc[i].sock,"\r\nSorry, your site is being ignored.\r\n\n"); */
    killsock(dcc[i].sock); return;
  }
  if (dcc[idx].host[0]=='@') {
    /* restrict by hostname */
    if (!wild_match(&dcc[idx].host[1],s)) {
/*      tprintf(dcc[i].sock,"\r\nSorry, this port is busy.\r\n");  */
      putlog(LOG_BOTS,"*","Refused %s (bad hostname)",s);
      killsock(dcc[i].sock);
      return;
    }
  }
  dcc[i].addr=ip;
  dcc[i].port=port;
  sprintf(dcc[i].host,"telnet:%s",s);
  /* script? */
  if (strcmp(dcc[idx].nick,"(script)")==0) {
    strcpy(dcc[i].nick,"*");
    dcc[i].type=DCC_SOCKET;
    dcc[i].u.other=NULL;
    dcc_total++;
    check_tcl_listen(dcc[idx].host,dcc[i].sock);
    return;
  }
  dcc[i].type=DCC_TELNET_ID;
  set_chat(i);
  /* copy acceptable-nick/host mask */
  strncpy(dcc[i].nick,dcc[idx].host,9); dcc[i].nick[9]=0;
  dcc[i].u.chat->away=NULL;
  dcc[i].u.chat->status=STAT_TELNET|STAT_ECHO;
  if (strcmp(dcc[idx].nick,"(bots)")==0)
    dcc[i].u.chat->status|=STAT_BOTONLY;
  if (strcmp(dcc[idx].nick,"(users)")==0)
    dcc[i].u.chat->status|=STAT_USRONLY;
  dcc[i].u.chat->timer=time(NULL);
  dcc[i].u.chat->msgs_per_sec=0;
  dcc[i].u.chat->con_flags=0;
  dcc[i].u.chat->buffer=NULL;
  dcc[i].u.chat->max_line=0;
  dcc[i].u.chat->line_count=0;
  dcc[i].u.chat->current_lines=0;
#ifdef NO_IRC
  if (chanset==NULL) strcpy(dcc[i].u.chat->con_chan,"*");
  else strcpy(dcc[i].u.chat->con_chan,chanset->name);
#else
  strcpy(dcc[i].u.chat->con_chan,chanset->name);
#endif
  dcc[i].u.chat->channel=0;   /* party line */
  tprintf(dcc[i].sock,"\r\n\r\n");
  telltext(i,"banner",0);
  if (allow_new_telnets)
    tprintf(dcc[i].sock,"(If you are new, enter 'NEW' here.)\r\n");
  dcc_total++;
  putlog(LOG_MISC,"*","Telnet connection: %s/%d",s,port);
}

#ifndef NO_FILE_SYSTEM
void dcc_get(idx,buf,len)
int idx; char *buf; int len;
{
  unsigned char bbuf[121],xnick[NICKLEN],*bf; unsigned long cmp,l;
  context;
  /* prepend any previous partial-acks: */
  my_memcpy(bbuf,buf,len);
  if (dcc[idx].u.xfer->sofar) {
    my_memcpy(&dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar],bbuf,len);
    my_memcpy(bbuf,dcc[idx].u.xfer->buf,dcc[idx].u.xfer->sofar+len);
    len+=dcc[idx].u.xfer->sofar; dcc[idx].u.xfer->sofar=0;
  }
  context;
  /* toss previous pent-up acks */
  while (len>4) { len-=4; my_memcpy(bbuf,&bbuf[4],len); }
  if (len<4) {
    /* not a full answer -- store and wait for the rest later */
    dcc[idx].u.xfer->sofar=len;
    my_memcpy(dcc[idx].u.xfer->buf,bbuf,len);
    return;
  }
  context;
  /* this is more compatable than ntohl for machines where an int */
  /* is more than 4 bytes: */
  cmp=((unsigned int)(bbuf[0])<<24)+((unsigned int)(bbuf[1])<<16)+
       ((unsigned int)(bbuf[2])<<8)+bbuf[3];
  dcc[idx].u.xfer->acked=cmp;
  if ((cmp>dcc[idx].u.xfer->sent) && (cmp<=dcc[idx].u.xfer->length)) {
    /* attempt to resume I guess */
    if (strcmp(dcc[idx].nick,"*users")==0) {
      putlog(LOG_MISC,"*","!!! Trying to skip ahead on userfile transfer");
    }
    else {
      fseek(dcc[idx].u.xfer->f,cmp,SEEK_SET);
      dcc[idx].u.xfer->sent=cmp;
      putlog(LOG_FILES,"*","Resuming file transfer at %dk for %s to %s",
	     (int)(cmp/1024),dcc[idx].u.xfer->filename,dcc[idx].nick);
    }
  }
  if (cmp!=dcc[idx].u.xfer->sent) return;
  if (dcc[idx].u.xfer->sent==dcc[idx].u.xfer->length) {
    /* successful send, we're done */
    killsock(dcc[idx].sock);
    fclose(dcc[idx].u.xfer->f);
    if (strcmp(dcc[idx].nick,"*users")==0) {
      int x,y=0;
      for (x=0; x<dcc_total; x++)
	if ((strcasecmp(dcc[x].nick,dcc[idx].host)==0) &&
	    (dcc[x].type==DCC_BOT))
	  y=x;
      if (y!=0) dcc[y].u.bot->status&=~STAT_SENDING;
      putlog(LOG_FILES,"*","Completed userfile transfer to %s.",
	     dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* any sharebot things that were queued: */
      dump_resync(dcc[y].sock,dcc[y].nick);
      xnick[0]=0;
    }
    else {
      check_tcl_sent(dcc[idx].u.xfer->from,dcc[idx].nick,
		     dcc[idx].u.xfer->dir);
      incr_file_gots(dcc[idx].u.xfer->dir);
      /* download is credited to the user who requested it */
      /* (not the user who actually received it) */
      stats_add_dnload(dcc[idx].u.xfer->from,dcc[idx].u.xfer->length);
      putlog(LOG_FILES,"*","Finished dcc send %s to %s",
	     dcc[idx].u.xfer->filename,dcc[idx].nick);
      wipe_tmp_file(idx); strcpy((char *)xnick,dcc[idx].nick);
    }
    lostdcc(idx);
    /* any to dequeue? */
    if (!at_limit(xnick)) send_next_file(xnick);
    return;
  }
  context;
  /* note: is this fseek necessary any more? */
/*    fseek(dcc[idx].u.xfer->f,dcc[idx].u.xfer->sent,0);   */
  l=dcc_block;
  if ((l==0) || (dcc[idx].u.xfer->sent+l > dcc[idx].u.xfer->length))
    l=dcc[idx].u.xfer->length - dcc[idx].u.xfer->sent;
  bf=(unsigned char *)nmalloc(l+1); fread(bf,l,1,dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock,bf,l);
  nfree(bf);
  dcc[idx].u.xfer->sent+=l;
  dcc[idx].u.xfer->pending=time(NULL);
}

void eof_dcc_get(idx)
int idx;
{
  char xnick[NICKLEN];
  context;
  fclose(dcc[idx].u.xfer->f);
  if (strcmp(dcc[idx].nick,"*users")==0) {
    int x,y=0;
    for (x=0; x<dcc_total; x++)
      if ((strcasecmp(dcc[x].nick,dcc[idx].host)==0) &&
	  (dcc[x].type==DCC_BOT))
	y=x;
    putlog(LOG_MISC,"*","Lost userfile transfer; aborting.");
    /* unlink(dcc[idx].u.xfer->filename); */ /* <- already unlinked */
    flush_tbuf(dcc[y].nick);
    xnick[0]=0;
    /* drop that bot */
    tprintf(dcc[y].sock,"bye\n");
    tandout_but(y,"unlinked %s\n",dcc[y].nick);
    tandout_but(y,"chat %s Disconnected %s (aborted userfile transfer)\n",
		botnetnick,dcc[y].nick);
    chatout("*** Disconnected %s (aborted userfile transfer)\n",dcc[y].nick);
    cancel_user_xfer(y);
    rembot(dcc[y].nick,dcc[y].nick); unvia(y,dcc[y].nick);
    killsock(dcc[y].sock); dcc[y].sock=dcc[y].type; dcc[y].type=DCC_LOST;
  }
  else {
    putlog(LOG_FILES,"*","Lost dcc get %s from %s!%s",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    wipe_tmp_file(idx); strcpy(xnick,dcc[idx].nick);
  }
  killsock(dcc[idx].sock); lostdcc(idx);
  /* send next queued file if there is one */
  if (!at_limit(xnick)) send_next_file(xnick);
}

void dcc_get_pending(idx,buf)
int idx; char *buf;
{
  unsigned long ip; unsigned short port; int i; char *bf,s[UHOSTLEN];
  context;
  i=answer(dcc[idx].sock,s,&ip,&port,1); killsock(dcc[idx].sock); 
  dcc[idx].sock=i; dcc[idx].addr=ip; dcc[idx].port=(int)port;
  if (dcc[idx].sock==-1) {
    neterror(s);
    mprintf(serv,"NOTICE %s :Bad connection (%s)\n",dcc[idx].nick,s);
    putlog(LOG_FILES,"*","DCC bad connection: GET %s (%s!%s)",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    lostdcc(idx); return;
  }
  /* file was already opened */
  if ((dcc_block==0) || (dcc[idx].u.xfer->length < dcc_block))
    dcc[idx].u.xfer->sent=dcc[idx].u.xfer->length;
  else dcc[idx].u.xfer->sent=dcc_block;
  dcc[idx].type=DCC_GET;
  bf=(char *)nmalloc(dcc[idx].u.xfer->sent+1);
  fread(bf,dcc[idx].u.xfer->sent,1,dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock,bf,dcc[idx].u.xfer->sent);
  nfree(bf); dcc[idx].u.xfer->pending=time(NULL);
  /* leave f open until file transfer is complete */
}

void dcc_send(idx,buf,len)
int idx; char *buf; int len;
{
  char s[512]; unsigned long sent;
  context;
  fwrite(buf,len,1,dcc[idx].u.xfer->f);
  dcc[idx].u.xfer->sent+=len;
  /* put in network byte order */
  sent=dcc[idx].u.xfer->sent;
  s[0]=(sent/(1<<24)); s[1]=(sent%(1<<24))/(1<<16);
  s[2]=(sent%(1<<16))/(1<<8); s[3]=(sent%(1<<8));
  tputs(dcc[idx].sock,s,4);
  dcc[idx].u.xfer->pending=time(NULL);
  if ((dcc[idx].u.xfer->sent > dcc[idx].u.xfer->length) &&
      (dcc[idx].u.xfer->length > 0)) {
    mprintf(serv,"NOTICE %s :Bogus file length.\n",dcc[idx].nick);
    putlog(LOG_FILES,"*","File too long: dropping dcc send %s from %s!%s",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    sprintf(s,"%s%s",tempdir,dcc[idx].u.xfer->filename);
    unlink(s); killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}
#endif   /* !NO_FILE_SYSTEM */

void dcc_telnet_id(idx,buf)
int idx; char *buf;
{
  int ok=0,atr;
  buf[10]=0;
  /* toss out bad nicknames */
  if ((dcc[idx].nick[0]!='@') && (!wild_match(dcc[idx].nick,buf))) {
    tprintf(dcc[idx].sock,"Sorry, this port is busy.\r\n");
    putlog(LOG_BOTS,"*","Refused %s (bad nick)",buf);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  atr=get_attr_handle(buf);
  /* make sure users-only/bots-only connects are honored */
  if ((dcc[idx].u.chat->status & STAT_BOTONLY) && !(atr & USER_BOT)) {
    tprintf(dcc[idx].sock,"This telnet port is for bots only.\r\n");
    putlog(LOG_BOTS,"*","Refused %s (non-bot)",buf);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  if ((dcc[idx].u.chat->status & STAT_USRONLY) && (atr & USER_BOT)) {
    tprintf(dcc[idx].sock,"error Only users may connect at this port.\n");
    putlog(LOG_BOTS,"*","Refused %s (non-user)",buf);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  dcc[idx].u.chat->status &= ~(STAT_BOTONLY|STAT_USRONLY);
  if (op_anywhere(buf)) {
    if (!require_p) ok=1;
  }
  if (atr & (USER_MASTER|USER_BOTMAST|USER_BOT|USER_PARTY)) ok=1;
  if ((atr&USER_XFER) && (dccdir[0])) ok=1;
#ifdef NO_IRC
  if ((strcasecmp(buf,"NEW")==0) && ((allow_new_telnets) || (make_userfile))) {
#else
  if ((strcasecmp(buf,"NEW")==0) && (allow_new_telnets)) {
#endif
    dcc[idx].type=DCC_TELNET_NEW;
    dcc[idx].u.chat->timer=time(NULL);
    tprintf(dcc[idx].sock,"\r\n");
    telltext(idx,"newuser",0);
    tprintf(dcc[idx].sock,"\r\nEnter the nickname you would like to use.\r\n");
    return;
  }
  if (!ok) {
    tprintf(dcc[idx].sock,"You don't have access.\r\n");
    putlog(LOG_BOTS,"*","Refused %s (invalid handle: %s)",
	   dcc[idx].host,buf);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  if (atr & USER_BOT) {
    if (in_chain(buf)) {
      tprintf(dcc[idx].sock,"error Already connected.\n");
      putlog(LOG_BOTS,"*","Refused telnet connection from %s (duplicate)",buf);
      killsock(dcc[idx].sock); lostdcc(idx); return;
    }
    if (!online) {
      tprintf(dcc[idx].sock,"error Not accepting links yet.\n");
      putlog(LOG_BOTS,"*","Refused telnet connection from %s (premature)",buf);
      killsock(dcc[idx].sock); lostdcc(idx); return;
    }
  }
  /* no password set? */
  if (pass_match_by_handle("-",buf)) {
    if (atr & USER_BOT) {
      char ps[20];
      makepass(ps); change_pass_by_handle(buf,ps);
      correct_handle(buf);
      strcpy(dcc[idx].nick,buf); nfree(dcc[idx].u.chat);
      set_tand(idx);
      dcc[idx].type=DCC_BOT;
      dcc[idx].u.bot->status=STAT_CALLED;
      tprintf(dcc[idx].sock,"*hello!\n");
      tprintf(dcc[idx].sock,"handshake %s\n",ps);
      greet_new_bot(idx);
      return;
    }
    tprintf(dcc[idx].sock,"Can't telnet until you have a password set.\r\n");
    putlog(LOG_MISC,"*","Refused [%s]%s (no password)",buf,dcc[idx].host);
    killsock(dcc[idx].sock); lostdcc(idx); return;
  }
  ok=0;
  dcc[idx].type=DCC_CHAT_PASS;
  dcc[idx].u.chat->timer=time(NULL);
  if (atr & (USER_MASTER|USER_BOTMAST)) ok=1;
  else if (op_anywhere(dcc[idx].nick)) {
    if (!require_p) ok=1;
    else if (atr & USER_PARTY) ok=1;
  }
  else if (atr & USER_PARTY) {
    ok=1; dcc[idx].u.chat->status|=STAT_PARTY;
  }
  if (atr & USER_BOT) ok=1;
  if (!ok) {
    set_files(idx);
    dcc[idx].type=DCC_FILES_PASS;
  }
  correct_handle(buf);
  strcpy(dcc[idx].nick,buf);
  if (atr & USER_BOT)
    tprintf(dcc[idx].sock,"passreq\n");
  else {
    dprintf(idx,"\nEnter your password.\377\373\001\n");
    /* turn off remote telnet echo: IAC WILL ECHO */
  }
}

void dcc_relay(idx,buf)
int idx; char *buf;
{
  int j;
  for (j=0; (dcc[j].sock!=dcc[idx].u.relay->sock) ||
       (dcc[j].type!=DCC_RELAYING); j++);
  /* if redirecting to a non-telnet user, swallow telnet codes */
  if (!(dcc[j].u.relay->chat->status&STAT_TELNET)) {
    swallow_telnet_codes(buf);
    if (!buf[0]) tprintf(dcc[idx].u.relay->sock," \n");
    else tprintf(dcc[idx].u.relay->sock,"%s\n",buf);
    return;
  }
  /* telnet user */
  if (!buf[0]) tprintf(dcc[idx].u.relay->sock," \r\n");
  else tprintf(dcc[idx].u.relay->sock,"%s\r\n",buf);
}

void dcc_relaying(idx,buf)
int idx; char *buf;
{
  int j; struct chat_info *ci;
  if (strcasecmp(buf,"*BYE*")!=0) {
    tprintf(dcc[idx].u.relay->sock,"%s\n",buf);
    return;
  }
  for (j=0; (dcc[j].sock!=dcc[idx].u.relay->sock) ||
       (dcc[j].type!=DCC_RELAY); j++);
  /* in case echo was off, turn it back on: */
  if (dcc[idx].u.relay->chat->status&STAT_TELNET)
    tprintf(dcc[idx].sock,"\377\374\001\r\n");
  dprintf(idx,"\n(Breaking connection to %s.)\n",dcc[j].nick);
  dprintf(idx,"You are now back on %s.\n\n",botnetnick);
  putlog(LOG_MISC,"*","Relay broken: %s -> %s",dcc[idx].nick,dcc[j].nick);
  if (dcc[idx].u.relay->chat->channel>=0) {
    chanout2(dcc[idx].u.relay->chat->channel,
	     "%s joined the party line.\n",dcc[idx].nick);
    context;
    if (dcc[idx].u.relay->chat->channel<100000)
      tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[idx].nick,
	      dcc[idx].u.relay->chat->channel,geticon(idx),dcc[idx].sock,
	      dcc[idx].host);
  }
  ci=dcc[idx].u.relay->chat; nfree(dcc[idx].u.relay);
  dcc[idx].u.chat=ci;
  dcc[idx].type=DCC_CHAT;
  if (dcc[idx].u.chat->channel>=0)
    check_tcl_chjn(botnetnick,dcc[idx].nick,dcc[idx].u.chat->channel,
		   geticon(idx),dcc[idx].sock,dcc[idx].host);
  notes_read(dcc[idx].nick,"",-1,idx);
  killsock(dcc[j].sock); lostdcc(j);
}

void dcc_telnet_new(idx,buf)
int idx; char *buf;
{
  int x,ok=1;
  buf[9]=0;
  strcpy(dcc[idx].nick,buf);
  dcc[idx].u.chat->timer=time(NULL);
  for (x=0; x<strlen(buf); x++)
    if ((buf[x]<=32) || (buf[x]>=127)) ok=0;
  if (!ok) {
    dprintf(idx,"\nYou can't use weird symbols in your nick.\n");
    dprintf(idx,"Try another one please:\n");
    return;
  }
  if (is_user(buf)) {
    dprintf(idx,"\nSorry, that nickname is taken already.\n");
    dprintf(idx,"Try another one please:\n");
    return;
  }
  if ((strcasecmp(buf,origbotname)==0) || (strcasecmp(buf,botnetnick)==0)) {
    dprintf(idx,"Sorry, can't use my name for a nick.\n");
    return;
  }
#ifdef NO_IRC
  if (make_userfile)
    userlist=adduser(userlist,buf,"none","-",default_flags|USER_PARTY|
		     USER_MASTER|USER_OWNER);
  else userlist=adduser(userlist,buf,"none","-",USER_PARTY|default_flags);
#else
  userlist=adduser(userlist,buf,"none","-",USER_PARTY|default_flags);
#endif
  dcc[idx].u.chat->status=STAT_ECHO;
  dcc[idx].type=DCC_CHAT;   /* just so next line will work */
  check_dcc_attrs(buf,USER_PARTY|default_flags,USER_PARTY|default_flags);
  dcc[idx].type=DCC_TELNET_PW;
#ifdef NO_IRC
  if (make_userfile) {
#ifdef OWNER
    dprintf(idx,"\nYOU ARE THE MASTER/OWNER ON THIS BOT NOW\n");
#else
    dprintf(idx,"\nYOU ARE THE MASTER ON THIS BOT NOW\n");
#endif  /* OWNER */
    telltext(idx,"newbot-limbo",0);
    putlog(LOG_MISC,"*","Bot installation complete, first master is %s",buf);
    make_userfile=0;
    write_userfile();
    add_note(buf,botnetnick,"Welcome to eggdrop! :)",-1,0);
  }
#endif  /* NO_IRC */
  dprintf(idx,"\nOkay, now choose and enter a password:\n");
  dprintf(idx,"(Only the first 9 letters are significant.)\n");
}

void dcc_telnet_pw(idx,buf)
int idx; char *buf;
{
  char newpass[10]; int x,ok;
  buf[10]=0; ok=1;
  if (strlen(buf)<4) {
    dprintf(idx,"\nTry to use at least 4 characters in your password.\n");
    dprintf(idx,"Choose and enter a password:\n");
    return;
  }
  for (x=0; x<strlen(buf); x++)
    if ((buf[x]<=32) || (buf[x]==127)) ok=0;
  if (!ok) {
    dprintf(idx,"\nYou can't use weird symbols in your password.\n");
    dprintf(idx,"Try another one please:\n");
    return;
  }
  putlog(LOG_MISC,"*","New user via telnet: [%s]%s/%d",dcc[idx].nick,
	 dcc[idx].host,dcc[idx].port);
  if (notify_new[0]) {
    char s[121],s1[121],*p1;
    sprintf(s,"Introduced to %s, %s",dcc[idx].nick,dcc[idx].host);
    strcpy(s1,notify_new); while (s1[0]) {
      p1=strchr(s1,','); if (p1!=NULL) { *p1=0; p1++; rmspace(p1); }
      rmspace(s1); add_note(s1,botnetnick,s,-1,0);
      if (p1==NULL) s1[0]=0; else strcpy(s1,p1);
    }
  }
  nsplit(newpass,buf);
  change_pass_by_handle(dcc[idx].nick,newpass);
  dprintf(idx,"\nRemember that!  You'll need it next time you log in.\n");
  dprintf(idx,"You now have an account on %s...\n\n\n",botnetnick);
  dcc[idx].type=DCC_CHAT;
  dcc_chatter(idx);
}

void dcc_script(idx,buf)
int idx; char *buf;
{
  void *old;
  if (!buf[0]) return;
  if (dcc[idx].u.script->type==DCC_CHAT)
    dcc[idx].u.script->u.chat->timer=time(NULL);
  else if (dcc[idx].u.script->type==DCC_FILES)
    dcc[idx].u.script->u.file->chat->timer=time(NULL);
  set_tcl_vars();
  if (call_tcl_func(dcc[idx].u.script->command,dcc[idx].sock,buf)) {
    old=dcc[idx].u.script->u.other;
    dcc[idx].type=dcc[idx].u.script->type;
    nfree(dcc[idx].u.script);
    dcc[idx].u.other=old;
    if (dcc[idx].type==DCC_SOCKET) {
      /* kill the whole thing off */
      killsock(dcc[idx].sock); lostdcc(idx);
      return;
    }
    notes_read(dcc[idx].nick,"",-1,idx);
    if ((dcc[idx].type==DCC_CHAT) && (dcc[idx].u.chat->channel>=0)) {
      chanout2(dcc[idx].u.chat->channel,"%s has joined the party line.\n",
	       dcc[idx].nick); 
      context;
      if (dcc[idx].u.chat->channel<10000)
        tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[idx].nick,
	        dcc[idx].u.chat->channel,geticon(idx),dcc[idx].sock,
	        dcc[idx].host);
      check_tcl_chjn(botnetnick,dcc[idx].nick,dcc[idx].u.chat->channel,
		     geticon(idx),dcc[idx].sock,dcc[idx].host);
    }
  }
}

/**********************************************************************/

/* main loop calls here when activity is found on a dcc socket */
void dcc_activity(int z,char *buf,int len)
{
  int idx;
  context;
  for (idx=0; (dcc[idx].sock!=z) && (idx<dcc_total); idx++);
  if (idx>=dcc_total) return;
  if ((dcc[idx].type!=DCC_SEND) && (dcc[idx].type!=DCC_GET) &&
      (dcc[idx].type!=DCC_GET_PENDING) && (dcc[idx].type!=DCC_TELNET) &&
      (dcc[idx].type!=DCC_RELAY) && (dcc[idx].type!=DCC_RELAYING) &&
      (dcc[idx].type!=DCC_FORK)) {
    /* interpret embedded telnet codes */
    strip_telnet(z,buf,&len);
  }
  context;
  if (dcc[idx].type==DCC_FORK) { dcc_fork(idx,buf); context; }
  else if (dcc[idx].type==DCC_TELNET) { dcc_telnet(idx,buf); context; }
#ifndef NO_FILE_SYSTEM
  else if (dcc[idx].type==DCC_GET_PENDING) { 
    dcc_get_pending(idx,buf); context;
  }
#endif
  else if (dcc[idx].type==DCC_CHAT) {	/* move this up here, blank */
    dcc_chat(idx,buf); context;		/* lines have meaning now   */
  }
  else if (len==0) return;   /* will just confuse anything else */
  else if (dcc[idx].type==DCC_CHAT_PASS) { 
    dcc_chat_pass(idx,buf); context;
  }
#ifndef NO_FILE_SYSTEM
  else if (dcc[idx].type==DCC_FILES_PASS) { 
    dcc_files_pass(idx,buf); context;
  }
  else if (dcc[idx].type==DCC_FILES) { dcc_files(idx,buf); context; }
  else if (dcc[idx].type==DCC_SEND) { dcc_send(idx,buf,len); context; }
  else if (dcc[idx].type==DCC_GET) { dcc_get(idx,buf,len); context; }
#endif
  else if (dcc[idx].type==DCC_BOT_NEW) { dcc_bot_new(idx,buf); context; }
  else if (dcc[idx].type==DCC_BOT) { dcc_bot(idx,buf); context; }
  else if (dcc[idx].type==DCC_TELNET_ID) { dcc_telnet_id(idx,buf); context; }
  else if (dcc[idx].type==DCC_RELAY) { dcc_relay(idx,buf); context; }
  else if (dcc[idx].type==DCC_RELAYING) { dcc_relaying(idx,buf); context; }
  else if (dcc[idx].type==DCC_TELNET_NEW) { dcc_telnet_new(idx,buf); context; }
  else if (dcc[idx].type==DCC_TELNET_PW) { dcc_telnet_pw(idx,buf); context; }
  else if (dcc[idx].type==DCC_SCRIPT) { dcc_script(idx,buf); context; }
  else if (dcc[idx].type==DCC_SOCKET) ;  /* do nothing, toss it */
  else {
    context;
    putlog(LOG_MISC,"!!! untrapped dcc activity: type %d, sock %d",
	   dcc[idx].type,dcc[idx].sock);
  }
}

/* eof from dcc goes here from I/O... */
void eof_dcc(int z)
{
  int idx;
  context;
  for (idx=0; (dcc[idx].sock!=z) || (dcc[idx].type==DCC_LOST); idx++);
  if (idx>=dcc_total) {
    putlog(LOG_MISC,"*","(@) EOF socket %d, not a dcc socket, not anything.",
	   z);
    close(z); killsock(z);
    return;
  }
  if (dcc[idx].type==DCC_SCRIPT) {
    void *old;
    /* tell the script they're gone: */
    call_tcl_func(dcc[idx].u.script->command,dcc[idx].sock,"");
    old=dcc[idx].u.script->u.other;
    dcc[idx].type=dcc[idx].u.script->type;
    nfree(dcc[idx].u.script);
    dcc[idx].u.other=old;
    /* then let it fall thru to the real one */
  }
  if ((dcc[idx].type==DCC_CHAT) || (dcc[idx].type==DCC_CHAT_PASS) ||
      (dcc[idx].type==DCC_FILES) || (dcc[idx].type==DCC_FILES_PASS)) {
    if (dcc[idx].type==DCC_CHAT) dcc[idx].u.chat->con_flags=0;
    putlog(LOG_MISC,"*","Lost dcc connection to %s (%s/%d)",dcc[idx].nick,
	   dcc[idx].host,dcc[idx].port);
    if (dcc[idx].type==DCC_CHAT) {
      if (dcc[idx].u.chat->channel>=0) {
	chanout2_but(idx,dcc[idx].u.chat->channel,"%s lost dcc link.\n",
		     dcc[idx].nick);
	context;
	if (dcc[idx].u.chat->channel<100000)
	  tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
      }
      check_tcl_chpt(botnetnick,dcc[idx].nick,dcc[idx].sock);
      check_tcl_chof(dcc[idx].nick,dcc[idx].sock);
    }
    killsock(z); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_BOT) {
    putlog(LOG_BOTS,"*","Lost bot: %s",dcc[idx].nick);
    chatout("*** Lost bot: %s\n",dcc[idx].nick);
    tandout_but(idx,"chat %s Lost bot: %s\n",botnetnick,dcc[idx].nick);
    tandout_but(idx,"unlinked %s\n",dcc[idx].nick);
    rembot(dcc[idx].nick,dcc[idx].nick); unvia(idx,dcc[idx].nick);
    cancel_user_xfer(idx); killsock(z); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_BOT_NEW) {
    putlog(LOG_BOTS,"*","Lost bot: %s",dcc[idx].nick,dcc[idx].port);
    killsock(z); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_TELNET_ID) {
    putlog(LOG_MISC,"*","Lost telnet connection to %s/%d",dcc[idx].host,
	   dcc[idx].port);
    killsock(z); lostdcc(idx);
  }
#ifndef NO_FILE_SYSTEM
  else if (dcc[idx].type==DCC_SEND) eof_dcc_send(idx);
  else if ((dcc[idx].type==DCC_GET_PENDING) || (dcc[idx].type==DCC_GET))
    eof_dcc_get(idx);
#endif
  else if (dcc[idx].type==DCC_RELAY) {
    int j; struct chat_info *ci;
    for (j=0; dcc[j].sock!=dcc[idx].u.relay->sock; j++);
    /* in case echo was off, turn it back on: */
    if (dcc[j].u.relay->chat->status&STAT_TELNET)
      tprintf(dcc[j].sock,"\377\374\001\r\n");
    putlog(LOG_MISC,"*","Ended relay link: %s -> %s",dcc[j].nick,
	   dcc[idx].nick);
    dprintf(j,"\n\n*** RELAY CONNECTION DROPPED.\n");
    dprintf(j,"You are now back on %s.\n",botnetnick);
    if (dcc[j].u.chat->channel>=0) {
      chanout2(dcc[j].u.relay->chat->channel,"%s rejoined the party line.\n",
	       dcc[j].nick);
      context;
      if (dcc[j].u.relay->chat->channel<100000)
        tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[j].nick,
	        dcc[j].u.relay->chat->channel,geticon(j),dcc[j].sock,
	        dcc[j].host);
    }
    ci=dcc[j].u.relay->chat; nfree(dcc[j].u.relay);
    dcc[j].u.chat=ci; dcc[j].type=DCC_CHAT;
    check_tcl_chjn(botnetnick,dcc[j].nick,dcc[j].u.chat->channel,
		   geticon(j),dcc[j].sock,dcc[j].host);
    notes_read(dcc[j].nick,"",-1,j);
    killsock(dcc[idx].sock); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_RELAYING) {
    int j,x=dcc[idx].u.relay->sock;
    putlog(LOG_MISC,"*","Lost dcc connection to [%s]%s/%d",dcc[idx].nick,
	   dcc[idx].host,dcc[idx].port);
    killsock(dcc[idx].sock); lostdcc(idx);
    for (j=0; (dcc[j].sock!=x) || (dcc[j].type==DCC_FORK) ||
	 (dcc[j].type==DCC_LOST); j++);
    putlog(LOG_MISC,"*","(Dropping relay link to %s)",dcc[j].nick);
    killsock(dcc[j].sock);
    lostdcc(j);   /* drop connection to the bot */
  }
  else if (dcc[idx].type==DCC_TELNET_NEW) {
    putlog(LOG_MISC,"*","Lost new telnet user (%s/%d)",dcc[idx].host,
	   dcc[idx].port);
    killsock(dcc[idx].sock); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_TELNET_PW) {
    putlog(LOG_MISC,"*","Lost new telnet user %s (%s/%d)",dcc[idx].nick,
	   dcc[idx].host,dcc[idx].port);
    deluser(dcc[idx].nick);
    killsock(dcc[idx].sock); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_FORK) eof_dcc_fork(idx);
  else if (dcc[idx].type==DCC_SOCKET) {
    killsock(dcc[idx].sock); lostdcc(idx);
  }
  else if (dcc[idx].type==DCC_TELNET) {
    putlog(LOG_MISC,"*","(!) Listening port %d abruptly died.",dcc[idx].port);
    killsock(dcc[idx].sock); lostdcc(idx);
  }
  else {
    putlog(LOG_MISC,"*","*** ATTENTION: DEAD SOCKET (%d) OF TYPE %d UNTRAPPED",
	   z,dcc[idx].type);
    killsock(z); lostdcc(idx);
  }
}
