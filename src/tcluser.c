/*
   tcluser.c -- handles:
     Tcl stubs for the user-record-oriented commands

   dprintf'ized, 1aug96
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
#include "cmdt.h"
#include "tclegg.h"
#include "chan.h"

/* eggdrop always uses the same interpreter */
extern Tcl_Interp *interp;
extern struct userrec *userlist;
extern int default_flags;
extern struct dcc_t dcc[];
extern int dcc_total;
extern char origbotname[];
extern char botnetnick[];
extern int ignore_time;
extern char notefile[];

/***********************************************************************/

int tcl_countusers STDVAR
{
  char s[40];
  BADARGS(1,1,"");
  sprintf(s,"%d",count_users(userlist));
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_validuser STDVAR
{
  BADARGS(2,2," handle");
  if (is_user(argv[1])) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_finduser STDVAR
{
  char s[20];
  BADARGS(2,2," nick!user@host");
  get_handle_by_host(s,argv[1]);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_passwdOk STDVAR
{
  BADARGS(3,3," handle passwd");
  if (pass_match_by_handle(argv[2],argv[1])) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_chattr STDVAR
{
  int atr,oatr,f,i,pos=1,recheck=0; char s[20];
  BADARGS(2,4," handle ?changes ?channel??");
  if ((argv[1][0]=='*') || (!is_user(argv[1]))) {
    Tcl_AppendResult(irp,"*",NULL);
    return TCL_OK;
  }
  if ((argc==4) && (!defined_channel(argv[3]))) {
    Tcl_AppendResult(irp,"no such channel",NULL);
    return TCL_ERROR;
  }
  if (argc==4) oatr=atr=get_chanattr_handle(argv[1],argv[3]);
  else oatr=atr=get_attr_handle(argv[1]);
  if (argc>=3) {
    /* make changes */
    for (i=0; i<strlen(argv[2]); i++) {
      if (argv[2][i]=='+') pos=1;
      else if (argv[2][i]=='-') pos=0;
      else {
	s[1]=0; s[0]=argv[2][i];
	if (argc==4) {
	  /* channel-specific */
	  f=str2chflags(s);
	  recheck=1;
	}
	else f=str2flags(s)&(~USER_BOT);
	atr=pos?(atr|f):(atr&~f);
      }
    }
    if (argc==4) set_chanattr_handle(argv[1],argv[3],atr);
    else {
      atr=sanity_check(atr);
      set_attr_handle(argv[1],atr);
      atr=check_dcc_attrs(argv[1],atr,oatr);
    }
  }
  /* retrieve current flags and return them */
  if (argc==4) chflags2str(atr,s);
  else flags2str(atr,s);
  Tcl_AppendResult(irp,s,NULL);
  if (recheck) recheck_channels();
  return TCL_OK;
}

int tcl_matchattr STDVAR
{
  int i,f,ok=1; char s[2];
  BADARGS(3,3," handle flags");
  for (i=0; i<strlen(argv[2]); i++) {
    s[1]=0; s[0]=argv[2][i]; f=str2flags(s);
    if ((!f) || ((get_attr_handle(argv[1])&f)!=f)) ok=0;
  }
  if (ok) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_matchchanattr STDVAR
{
  int i,f,ok=1; char s[2];
  BADARGS(4,4," handle flags channel");
  if (!defined_channel(argv[3])) {
    Tcl_AppendResult(irp,"no such channel defined",NULL);
    return TCL_ERROR;
  }
  for (i=0; i<strlen(argv[2]); i++) {
    s[1]=0; s[0]=argv[2][i]; f=str2chflags(s);
    if ((!f) || ((get_chanattr_handle(argv[1],argv[3])&f)!=f)) ok=0;
  }
  if (ok) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_adduser STDVAR
{
  BADARGS(3,3," handle hostmask");
  if (strlen(argv[1]) > 9)
    argv[1][9] = 0;
  if (is_user(argv[1])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  if (argv[1][0]=='*') {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  userlist=adduser(userlist,argv[1],argv[2],"-",default_flags);
  Tcl_AppendResult(irp,"1",NULL);
  return TCL_OK;
}

int tcl_addbot STDVAR
{
  BADARGS(3,3," handle address");
  if (is_user(argv[1])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  if (argv[1][0]=='*') {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  userlist=adduser(userlist,argv[1],"none","-",USER_BOT);
  set_handle_info(userlist,argv[1],argv[2]);
  Tcl_AppendResult(irp,"1",NULL);
  return TCL_OK;
}

int tcl_deluser STDVAR
{
  char s[10];
  BADARGS(2,2," handle");
  if (argv[1][0]=='*') {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  sprintf(s,"%d",deluser(argv[1]));
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_addhost STDVAR
{
  BADARGS(3,3," handle hostmask");
  if ((!is_user(argv[1])) || (argv[1][0]=='*')) {
    Tcl_AppendResult(irp,"non-existent user",NULL);
    return TCL_ERROR;
  }
  addhost_by_handle(argv[1],argv[2]);
  return TCL_OK;
}

int tcl_delhost STDVAR
{
  BADARGS(3,3," handle hostmask");
  if ((!is_user(argv[1])) || (argv[1][0]=='*')) {
    Tcl_AppendResult(irp,"non-existent user",NULL);
    return TCL_ERROR;
  }
  if (delhost_by_handle(argv[1],argv[2])) {
    Tcl_AppendResult(irp,"1",NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_getinfo STDVAR
{
  char s[161];
  BADARGS(2,2," handle");
  if (get_attr_handle(argv[1]) & USER_BOT) return TCL_OK;  /* bot */
  get_handle_info(argv[1],s);
/*  if (s[0]=='@') strcpy(s,&s[1]);  */
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getchaninfo STDVAR
{
  char s[161];
  BADARGS(3,3," handle channel");
  if (get_attr_handle(argv[1]) & USER_BOT) return TCL_OK;
  get_handle_chaninfo(argv[1],argv[2],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getaddr STDVAR
{
  char s[161];
  BADARGS(2,2," handle");
  if (!(get_attr_handle(argv[1]) & USER_BOT)) return TCL_OK;  /* non-bot */
  get_handle_info(argv[1],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getdccdir STDVAR
{
  char s[161];
  BADARGS(2,2," handle");
  get_handle_dccdir(argv[1],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getcomment STDVAR
{
  char s[161];
  BADARGS(2,2," handle");
  get_handle_comment(argv[1],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getemail STDVAR
{
  char s[161];
  BADARGS(2,2," handle");
  get_handle_email(argv[1],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getxtra STDVAR
{
  BADARGS(2,2," handle");
  Tcl_AppendResult(irp,get_handle_xtra(argv[1]),NULL);
  return TCL_OK;
}

int tcl_setinfo STDVAR
{
  BADARGS(3,3," handle info");
  set_handle_info(userlist,argv[1],argv[2]);
  return TCL_OK;
}

int tcl_setchaninfo STDVAR
{
  BADARGS(4,4," handle channel info");
  set_handle_chaninfo(userlist,argv[1],argv[2],argv[3]);
  return TCL_OK;
}

int tcl_setdccdir STDVAR
{
  BADARGS(3,3," handle dccdir");
  set_handle_dccdir(userlist,argv[1],argv[2]);
  return TCL_OK;
}

int tcl_setcomment STDVAR
{
  BADARGS(3,3," handle comment");
  set_handle_comment(userlist,argv[1],argv[2]);
  return TCL_OK;
}

int tcl_setemail STDVAR
{
  BADARGS(3,3," handle email");
  set_handle_email(userlist,argv[1],argv[2]);
  return TCL_OK;
}

int tcl_setxtra STDVAR
{
  BADARGS(3,3," handle xtra");
  set_handle_xtra(userlist,argv[1],argv[2]);
  return TCL_OK;
}

int tcl_getlaston STDVAR
{
  char s[21]; time_t t;
  BADARGS(2,3," handle ?channel?");
  if (argc==2)
    get_handle_laston("*",argv[1],&t);
  else 
  get_handle_laston(argv[2],argv[1],&t);
  sprintf(s,"%lu",t);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_getchanlaston STDVAR
{
  char ch[161];
  BADARGS(2,2," handle");
  get_handle_chanlaston(argv[1],ch);
  Tcl_AppendResult(irp,ch,NULL);
  return TCL_OK;
}

int tcl_setlaston STDVAR
{
  time_t t=time(NULL);
  BADARGS(2,4," handle ?channel? ?timestamp?");
  if (argc==4) t=(time_t)atol(argv[3]);
  if (argc==3 && argv[2][0]!='#')
     t=(time_t)atol(argv[2]);
  if (argc==2 || (argc==3 && argv[2][0]!='#'))
    set_handle_laston("*",argv[1],t);
  else 
  set_handle_laston(argv[2],argv[1],t);
  return TCL_OK;
}

int tcl_userlist STDVAR
{
  struct userrec *u=userlist; int f=0;
  BADARGS(1,2," ?flags?");
  if (argc==2) {
    f=str2flags(argv[1]);
    if ((f==0) && (argv[1][0]) && (argv[1][0]!='-')) return TCL_OK;
  }
  while (u!=NULL) {
    if ((u->flags & f)==f) Tcl_AppendElement(interp,u->handle);
    u=u->next;
  }
  return TCL_OK;
}

int tcl_save STDVAR
{
  write_userfile();
  return TCL_OK;
}

int tcl_reload STDVAR
{
  reload();
  return TCL_OK;
}

int tcl_gethosts STDVAR
{
  struct userrec *u; struct eggqueue *q;
  BADARGS(2,2," handle");
  u=get_user_by_handle(userlist,argv[1]);
  if (u==NULL) return TCL_OK;
  q=u->host; while (q!=NULL) {
    Tcl_AppendElement(irp,q->item);
    q=q->next;
  }
  return TCL_OK;
}

int tcl_chpass STDVAR
{
  char par[10],pass[10];
  BADARGS(2,3," handle ?password?");
  if (argc==3 && argv[2][0]) {
    strncpy(par,argv[2],15); par[15]=0;
    nsplit(pass,par);
    change_pass_by_handle(argv[1],pass);
  }
  else change_pass_by_handle(argv[1],"-");
  return TCL_OK;
}

int tcl_chnick STDVAR
{
  char hand[10]; int x=1,i;
  BADARGS(3,3," oldnick newnick");
  strncpy(hand,argv[2],9); hand[9]=0;
  for (i=0; i<strlen(hand); i++)
    if ((hand[i]<=32) || (hand[i]>=127) || (hand[i]=='@')) hand[i]='?';
  if (strchr("-,+*=:!.@#;$",hand[0])!=NULL) x=0;
  else if (strlen(hand)<1) x=0;
  else if (is_user(hand)) x=0;
  else if ((strcasecmp(origbotname,hand)==0) || (strcasecmp(botnetnick,hand)==0))
    x=0;
  else if (hand[0]=='*') x=0;
  if (x) {
    x=change_handle(argv[1],hand);
    if (x) {
      notes_change(-1,argv[1],hand);
      for (i=0; i<dcc_total; i++) {
	if ((strcasecmp(dcc[i].nick,argv[1])==0) && (dcc[i].type!=DCC_BOT)) {
	  char s[10];
	  strcpy(s,dcc[i].nick); strcpy(dcc[i].nick,hand);
	  if ((dcc[i].type==DCC_CHAT) && (dcc[i].u.chat->channel>=0)) {
	    chanout2(dcc[i].u.chat->channel,"Nick change: %s -> %s\n",s,hand);
	    if (dcc[i].u.chat->channel<100000) {
	      tandout("part %s %s %d\n",botnetnick,s,dcc[i].sock);
	      tandout("join %s %s %d %c%d %s\n",botnetnick,s,
		      dcc[i].u.chat->channel,geticon(i),dcc[i].sock,
		      dcc[i].host);
	    }
	  }
	}
      }
    }
  }
  sprintf(hand,"%d",x); Tcl_AppendResult(irp,hand,NULL);
  return TCL_OK;
}

int tcl_getting_users STDVAR
{
  int i;
  BADARGS(1,1,"");
  for (i=0; i<dcc_total; i++) {
    if ((dcc[i].type==DCC_BOT) && (dcc[i].u.bot->status&STAT_GETTING)) {
      Tcl_AppendResult(irp,"1",NULL);
      return TCL_OK;
    }
  }
  Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_isignore STDVAR
{
  int x;
  BADARGS(2,2," nick!user@host");
  x=match_ignore(argv[1]);
  if (x) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_newignore STDVAR
{
  time_t now=time(NULL),expire_time; char ign[UHOSTLEN],cmt[66],from[10];
  BADARGS(4,5," hostmask creator comment ?lifetime?");
  strncpy(ign,argv[1],UHOSTLEN-1); ign[UHOSTLEN-1]=0;
  strncpy(from,argv[2],9); from[9]=0;
  strncpy(cmt,argv[3],65); cmt[65]=0;
  if (argc==4) expire_time=now+(60*ignore_time);
  else {
    if (atol(argv[4])==0) expire_time=0L;
    else expire_time=now+(60*atol(argv[4]));
  }
  addignore(ign,from,cmt,expire_time);
  return TCL_OK;
}

int tcl_killignore STDVAR
{
  int x;
  BADARGS(2,2," hostmask");
  x=delignore(argv[1]);
  if (x) Tcl_AppendResult(irp,"1",NULL);
  else Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

/* { hostmask note expire-time create-time creator } */
int tcl_ignorelist STDVAR
{
  struct userrec *u; struct eggqueue *q; time_t t;
  char s[256],host[UHOSTLEN],ts[21],ts1[21],from[81],*list[5],*p;
  BADARGS(1,1,"");
  u=get_user_by_handle(userlist,IGNORE_NAME);
  if (u==NULL) return TCL_OK;
  q=u->host; while ((q!=NULL) && (strcmp(q->item,"none")!=0)) {
    strcpy(s,q->item); splitc(host,s,':'); splitc(ts,s,':');
    if (ts[0]=='+') {
      /* new-style expiration */
      strcpy(ts,&ts[1]);
    }
    else {
      /* old-style (convert) */
      t=(time_t)atol(ts);
      if (t!=0L) t+=(60*ignore_time);
      sprintf(ts,"%lu",t);
    }
    splitc(from,s,':');
    if (!from[0]) {
      /* very old */
      strcpy(from,s); s[0]=0;
      strcpy(ts1,"0");
    }
    else splitc(ts1,s,':');
    if (s[0]) {
      /* decode gibberish stuff */
      p=strchr(s,'~'); while (p!=NULL) { *p=' '; p=strchr(s,'~'); }
      p=strchr(s,'`'); while (p!=NULL) { *p=','; p=strchr(s,'`'); }
    }
    list[0]=host; list[1]=s; list[2]=ts; list[3]=ts1; list[4]=from;
    p=Tcl_Merge(5,list);
    Tcl_AppendElement(irp,p); n_free(p,"",0);
    q=q->next;
  }
  return TCL_OK;
}

int tcl_addchanrec STDVAR
{
  struct userrec *u;
  context;
  BADARGS(3,3," handle channel");
  if(!is_user(argv[1])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  if (!findchan(argv[2])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  u=get_user_by_handle(userlist,argv[1]);
  if (get_chanrec(u,argv[2])!=NULL) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  } 
  add_chanrec(u,argv[2],0,0);
  Tcl_AppendResult(irp,"1",NULL);
  return TCL_OK;
}
   
int tcl_delchanrec STDVAR
{
  struct userrec *u;
  context;
  BADARGS(3,3," handle channel");
  if(!is_user(argv[1])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  if (!findchan(argv[2])) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  u=get_user_by_handle(userlist,argv[1]);
  if (get_chanrec(u,argv[2])==NULL) {
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  del_chanrec(u,argv[2]);
  Tcl_AppendResult(irp,"1",NULL);
  return TCL_OK;
}

int tcl_notes STDVAR
{
  FILE * f; char s[601],to[34],from[34],dt[34];
  int count; char * list[3], *p;
  context;
  BADARGS(2,3," handle ?note#?");
  if(!is_user(argv[1])) {
    Tcl_AppendResult(irp,"-1",NULL);
    return TCL_OK;
  }
  if (argc == 2) {
    sprintf(s,"%d",num_notes(argv[1]));
    Tcl_AppendResult(irp,s,NULL);
    return TCL_OK;
  }
  count = atoi(argv[2]);
  if (!notefile[0]) {
    Tcl_AppendResult(irp,"-2",NULL);
    return TCL_OK;
  }
  f=fopen(notefile,"r"); if (f==NULL) {
    Tcl_AppendResult(irp,"-2",NULL);
    return TCL_OK;
  }
  while (!feof(f)) {
    fgets(s,600,f);
    if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
    if (!feof(f)) {
      rmspace(s);
      if ((s[0]) && (s[0]!='#') & (s[0]!=';')) {   /* not comment */
	split(to,s);
	if (strcasecmp(to,argv[1])==0) {
	  split(from,s); split(dt,s); 
	  count--;
	  list[0]=from; list[1]=dt; list[2]=s;
	  p=Tcl_Merge(3,list);
	  Tcl_AppendElement(irp,p); 
	  n_free(p,"",0);
	  fclose(f);
	  return TCL_OK;
	}
      }
    }
  } 
  Tcl_AppendResult(irp,"0",NULL);
  fclose(f);
  return TCL_OK;
}

