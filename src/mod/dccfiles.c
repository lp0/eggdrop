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

#include "module.h"
#ifdef MODULES
#define MODULE_NAME "filesys"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "filesys.h"
#else
#include "../tclegg.h"
#endif

/* root dcc directory */
char dccdir[121]="";
/* directory to put incoming dcc's into */
char dccin[121]="";
/* let all uploads go to the user's current directory? */
int upload_to_cd=0;
/* how many bytes should we send at once? */
int dcc_block=1024;

#ifndef NO_FILE_SYSTEM
#ifndef NO_FILE_SYSTEM
/* check for tcl-bound file command, return 1 if found */
/* fil: proc-name <handle> <dcc-handle> <args...> */
#ifdef MODULES
static
#endif
int check_tcl_fil PROTO3(char *,cmd,int,idx,char *,args)
{
  int atr,chatr,x; char s[5];
  modcontext;
  atr=get_attr_handle(dcc[idx].nick);
  chatr=get_chanattr_handle(dcc[idx].nick,dcc[idx].u.file->chat->con_chan);
  if (chatr & CHANUSER_OP) atr |= USER_PSUEDOOP;
  if (chatr & CHANUSER_MASTER) atr |= USER_PSUMST;
  if (chatr & CHANUSER_OWNER) atr |= USER_PSUOWN;
  sprintf(s,"%d",dcc[idx].sock);
  Tcl_SetVar(interp,"_n",dcc[idx].nick,0);
  Tcl_SetVar(interp,"_i",s,0);
  Tcl_SetVar(interp,"_a",args,0);
  modcontext;
  x=check_tcl_bind(&H_fil,cmd,atr," $_n $_i $_a",
		   MATCH_PARTIAL|BIND_USE_ATTR|BIND_HAS_BUILTINS);
  modcontext;
  if (x==BIND_AMBIGUOUS) {
    modprintf(idx,"Ambigious command.\n");
    return 0;
  }
  if (x==BIND_NOMATCH) {
    modprintf(idx,"What?  You need 'help'\n");
    return 0;
  }
  if (x==BIND_EXEC_BRK) return 1;
  if (x==BIND_EXEC_LOG)
    putlog(LOG_FILES,"*","#%s# files: %s %s",dcc[idx].nick,cmd,args);
  return 0;
}

#ifdef MODULES
static
#endif
void check_tcl_sent PROTO3(char *,hand,char *,nick,char *,path)
{
  int atr;
  modcontext;
  atr=get_allattr_handle(hand);
  Tcl_SetVar(interp,"_n",hand,0);
  Tcl_SetVar(interp,"_a",nick,0);
  Tcl_SetVar(interp,"_aa",path,0);
  modcontext;
  check_tcl_bind(&H_sent,hand,atr," $_n $_a $_aa",
		 MATCH_MASK|BIND_USE_ATTR|BIND_STACKABLE);
  modcontext;
}

#ifdef MODULES
static
#endif
void check_tcl_rcvd PROTO3(char *,hand,char *,nick,char *,path)
{
  int atr;
  modcontext;
  atr=get_allattr_handle(hand);
  Tcl_SetVar(interp,"_n",hand,0);
  Tcl_SetVar(interp,"_a",nick,0);
  Tcl_SetVar(interp,"_aa",path,0);
  modcontext;
  check_tcl_bind(&H_rcvd,hand,atr," $_n $_a $_aa",
		 MATCH_MASK|BIND_USE_ATTR|BIND_STACKABLE);
  modcontext;
}
#endif

#ifdef MODULES
static
#endif
void dcc_files_pass PROTO2(int,idx,char *,buf)
{
   if (pass_match_by_handle(buf,dcc[idx].nick)) {
      if (too_many_filers()) {
	 modprintf(idx,"Too many people are in the file system right now.\n");
	 modprintf(idx,"Please try again later.\n");
	 putlog(LOG_MISC,"*","File area full: DCC chat [%s]%s",dcc[idx].nick,
		dcc[idx].host);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);  
	 return;
      }
      dcc[idx].type=DCC_FILES;
      if (dcc[idx].u.file->chat->status&STAT_TELNET)
	modprintf(idx,"\377\374\001\n");  /* turn echo back on */
      putlog(LOG_FILES,"*","File system: [%s]%s/%d",dcc[idx].nick,
	     dcc[idx].host,dcc[idx].port);
      if (!welcome_to_files(idx)) {
	 putlog(LOG_FILES,"*","File system broken.");
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
      }
      return;
   }
   modprintf(idx,"Negative on that, Houston.\n");
   putlog(LOG_MISC,"*","Bad password: DCC chat [%s]%s",dcc[idx].nick,
	  dcc[idx].host);
   killsock(dcc[idx].sock); lostdcc(idx);
}

#ifdef MODULES
static
#endif
void eof_dcc_send PROTO1(int,idx)
{
   int ok,j;
   char ofn[121],nfn[121],hand[41],s[161];
   
   modcontext;
   if (dcc[idx].u.xfer->length==dcc[idx].u.xfer->sent) {
      /* success */
      ok=0;
      fclose(dcc[idx].u.xfer->f);
      if (strcmp(dcc[idx].nick,"*users")==0) {
	 finish_share(idx);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
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
      modcontext;
      for (j=0;j<dcc_total; j++)
	if ((!ok) && ((dcc[j].type==DCC_CHAT) || (dcc[j].type==DCC_FILES)) &&
	    (strcasecmp(dcc[j].nick,hand)==0)) {
	   ok=1;
	   modprintf(j,"Thanks for the file!\n");
	}
      modcontext;
      if (!ok) modprintf(DP_HELP,"NOTICE %s :Thanks for the file!\n",
		       dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   /* failure :( */
   fclose(dcc[idx].u.xfer->f);
   if (strcmp(dcc[idx].nick,"*users")==0) {
      int x,y=0;
      for (x=0;x<dcc_total; x++)
	if ((strcasecmp(dcc[x].nick,dcc[idx].host)==0) &&
	    (dcc[x].type==DCC_BOT))
	   y=x;
      if (y) {
	 putlog(LOG_MISC,"*","Lost userfile transfer to %s; aborting.",
		dcc[y].nick);
	 unlink(dcc[idx].u.xfer->filename);
	 /* drop that bot */
	 modprintf(y,"bye\n");
	 tandout_but(y,"unlinked %s\n",dcc[y].nick);
	 tandout_but(y,"chat %s Disconnected %s (aborted userfile transfer)\n",
		     botnetnick,dcc[y].nick);
	 chatout("*** Disconnected %s (aborted userfile transfer)\n",
		 dcc[y].nick);
	 cancel_user_xfer(y);
	 killsock(dcc[y].sock);
	 dcc[y].sock=dcc[y].type; 
	 dcc[y].type=DCC_LOST;
      }
      else {
	 putlog(LOG_FILES,"*","Lost dcc send %s from %s!%s (%lu/%lu)",
		dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host,
		dcc[idx].u.xfer->sent,dcc[idx].u.xfer->length);
	 sprintf(s,"%s%s",tempdir,dcc[idx].u.xfer->filename); 
	 unlink(s);
      }
      killsock(dcc[idx].sock); lostdcc(idx);
   }
}

/* hash function for file area commands */
#ifdef MODULES
static
#endif
int got_files_cmd PROTO2(int,idx,char *,msg)
{
  char total[512],code[512];
   modcontext;
  strcpy(msg,check_tcl_filt(idx,msg));
   modcontext;
  if (!msg[0]) return 1;
  if (msg[0]=='.') strcpy(msg,&msg[1]);
  strcpy(total,msg); rmspace(msg); nsplit(code,msg); rmspace(msg);
  return check_tcl_fil(code,idx,msg);
}

#ifdef MODULES
static
#endif
void dcc_files PROTO2(int,idx,char *,buf)
{
   int i;
   
   modcontext;
   if (detect_dcc_flood(dcc[idx].u.file->chat,idx)) return;
   dcc[idx].u.file->chat->timer=time(NULL);
   modcontext;
   strcpy(buf,check_tcl_filt(idx,buf));
   modcontext;
   if (!buf[0]) return;
   if (buf[0]==',') {
      for (i=0; i<dcc_total; i++) {
	 if ((dcc[i].type==DCC_CHAT) &&
	     (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	     (dcc[i].u.chat->channel>=0) &&
	     ((i!=idx) || (dcc[idx].u.chat->status&STAT_ECHO)))
	   modprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
	 if ((dcc[i].type==DCC_FILES) &&
	     (get_attr_handle(dcc[i].nick) & USER_MASTER) &&
	     ((i!=idx) || (dcc[idx].u.file->chat->status&STAT_ECHO)))
	   modprintf(i,"-%s- %s\n",dcc[idx].nick,&buf[1]);
      }
   } else if (got_files_cmd(idx,buf)) {
      modprintf(idx,"*** Ja mata!\n");
      putlog(LOG_FILES,"*","DCC user [%s]%s left file system",dcc[idx].nick,
	     dcc[idx].host);
      set_handle_dccdir(userlist,dcc[idx].nick,dcc[idx].u.file->dir);
      if (dcc[idx].u.file->chat->status&STAT_CHAT) {
	 struct chat_info *ci;
	 modprintf(idx,"Returning you to command mode...\n");
	 ci=dcc[idx].u.file->chat; modfree(dcc[idx].u.file);
	 dcc[idx].u.chat=ci;
	 dcc[idx].u.chat->status&=(~STAT_CHAT); dcc[idx].type=DCC_CHAT;
	 if (dcc[idx].u.chat->channel>=0) {
	    chanout2(dcc[idx].u.chat->channel,"%s has returned.\n",dcc[idx].nick);
        modcontext;
	    if (dcc[idx].u.chat->channel<100000)
	      tandout("unaway %s %d\n",botnetnick,dcc[idx].sock);
	 }
      } else {
	 modprintf(idx,"Dropping connection now.\n");
	 putlog(LOG_FILES,"*","Left files: [%s]%s/%d",dcc[idx].nick,
		dcc[idx].host,dcc[idx].port);
	 killsock(dcc[idx].sock); lostdcc(idx);
      }
   }
}

#ifdef MODULES
static
#endif
void dcc_get PROTO3(int,idx,char *,buf,int,len)
{
  unsigned char bbuf[121],xnick[NICKLEN],*bf; unsigned long cmp,l;
  modcontext;
  /* prepend any previous partial-acks: */
  my_memcpy(bbuf,buf,len);
  if (dcc[idx].u.xfer->sofar) {
    my_memcpy(&dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar],bbuf,len);
    my_memcpy(bbuf,dcc[idx].u.xfer->buf,dcc[idx].u.xfer->sofar+len);
    len+=dcc[idx].u.xfer->sofar; dcc[idx].u.xfer->sofar=0;
  }
  modcontext;
  /* toss previous pent-up acks */
  while (len>4) { len-=4; my_memcpy(bbuf,&bbuf[4],len); }
  if (len<4) {
    /* not a full answer -- store and wait for the rest later */
    dcc[idx].u.xfer->sofar=len;
    my_memcpy(dcc[idx].u.xfer->buf,bbuf,len);
    return;
  }
  modcontext;
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
  modcontext;
  /* note: is this fseek necessary any more? */
/*    fseek(dcc[idx].u.xfer->f,dcc[idx].u.xfer->sent,0);   */
  l=dcc_block;
  if ((l==0) || (dcc[idx].u.xfer->sent+l > dcc[idx].u.xfer->length))
    l=dcc[idx].u.xfer->length - dcc[idx].u.xfer->sent;
  bf=(unsigned char *)modmalloc(l+1); fread(bf,l,1,dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock,bf,l);
  modfree(bf);
  dcc[idx].u.xfer->sent+=l;
  dcc[idx].u.xfer->pending=time(NULL);
}

#ifdef MODULES
static
#endif
void eof_dcc_get PROTO1(int,idx)
{
  char xnick[NICKLEN];
  modcontext;
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
    modprintf(-dcc[y].sock,"bye\n");
    tandout_but(y,"unlinked %s\n",dcc[y].nick);
    tandout_but(y,"chat %s Disconnected %s (aborted userfile transfer)\n",
		botnetnick,dcc[y].nick);
    chatout("*** Disconnected %s (aborted userfile transfer)\n",dcc[y].nick);
    cancel_user_xfer(y);
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

#ifdef MODULES
static
#endif
void dcc_get_pending PROTO2(int,idx,char *,buf)
{
  unsigned long ip; unsigned short port; int i; char *bf,s[UHOSTLEN];
  modcontext;
  i=answer(dcc[idx].sock,s,&ip,&port,1); killsock(dcc[idx].sock); 
  dcc[idx].sock=i; dcc[idx].addr=ip; dcc[idx].port=(int)port;
  if (dcc[idx].sock==-1) {
    neterror(s);
    modprintf(DP_HELP,"NOTICE %s :Bad connection (%s)\n",dcc[idx].nick,s);
    putlog(LOG_FILES,"*","DCC bad connection: GET %s (%s!%s)",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    lostdcc(idx); return;
  }
  /* file was already opened */
  if ((dcc_block==0) || (dcc[idx].u.xfer->length < dcc_block))
    dcc[idx].u.xfer->sent=dcc[idx].u.xfer->length;
  else dcc[idx].u.xfer->sent=dcc_block;
  dcc[idx].type=DCC_GET;
  bf=(char *)modmalloc(dcc[idx].u.xfer->sent+1);
  fread(bf,dcc[idx].u.xfer->sent,1,dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock,bf,dcc[idx].u.xfer->sent);
  modfree(bf); dcc[idx].u.xfer->pending=time(NULL);
  /* leave f open until file transfer is complete */
}

#ifdef MODULES
static
#endif
void dcc_send PROTO3(int,idx,char *,buf,int,len)
{
  char s[512]; unsigned long sent;
  modcontext;
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
    modprintf(DP_HELP,"NOTICE %s :Bogus file length.\n",dcc[idx].nick);
    putlog(LOG_FILES,"*","File too long: dropping dcc send %s from %s!%s",
	   dcc[idx].u.xfer->filename,dcc[idx].nick,dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    sprintf(s,"%s%s",tempdir,dcc[idx].u.xfer->filename);
    unlink(s); killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

#ifdef MODULES
int filesys_activity_hook PROTO3(int,idx,char *,buf,int,len) 
{
   modcontext;
   switch (dcc[idx].type) {
    case DCC_GET_PENDING:
      modcontext;
      dcc_get_pending(idx,buf);
      return 1;
    case DCC_FILES_PASS:
      modcontext;
      dcc_files_pass(idx,buf);
      return 1;
    case DCC_FILES:
      modcontext;
      dcc_files(idx,buf); 
      return 1;
    case DCC_SEND:
      modcontext;
      dcc_send(idx,buf,len);
      return 1;
    case DCC_GET:
      modcontext;
      dcc_get(idx,buf,len);
      return 1;
  }
   return 0;
}

int filesys_eof_hook PROTO1(int,idx)
{ 
   modcontext;
   switch (dcc[idx].type) {
    case DCC_SEND:
      eof_dcc_send(idx);
      return 1;
    case DCC_GET_PENDING:
    case DCC_GET:
      eof_dcc_get(idx);
      return 1;
   }
   return 0;
}
#endif
#endif   /* !NO_FILE_SYSTEM */
