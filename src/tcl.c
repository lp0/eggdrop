/*
   tcl.c -- handles:
     the code for every command eggdrop adds to Tcl
     Tcl initialization
     getting and setting Tcl/eggdrop variables

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
#include "tclegg.h"
#include "cmdt.h"

/* used for read/write to internal strings */
typedef struct {
  char *str;     /* pointer to actual string in eggdrop */
  int max;       /* max length (negative: read-only var when protect is on) */
		 /*   (0: read-only ALWAYS) */
  int flags;     /* 1 = directory */
} strinfo;

/* used for read/write to integer couplets */
typedef struct {
  int *left;     /* left side of couplet */
  int *right;	 /* right side */
} coupletinfo;

/* used for read/write to flags */
typedef struct {
  char *flag;    /* pointer to actual flag in eggdrop */
  char def;      /* normal value for the name of the flag */
} flaginfo;

/* turn on/off readonly protection */
int protect_readonly=0;
/* fields to display in a .whois */
char whois_fields[121]="";

/* timezone bot is in */
char time_zone[41]="EST";

/* eggdrop always uses the same interpreter */
Tcl_Interp *interp;

extern int curserv, serv, backgrd;
extern int shtime, learn_users, share_users, share_greet, use_info,
  passive, strict_host, require_p, keep_all_logs, copy_to_tmp,
  use_stderr, upload_to_cd, never_give_up, allow_new_telnets, keepnick;
extern int botserverport, min_servs, default_flags, conmask, newserverport,
  save_users_at, switch_logfiles_at, server_timeout, connect_timeout,
  firewallport, reserved_port, notify_users_at;
extern int flood_thr, flood_pub_thr, flood_join_thr, ban_time, ignore_time,
  flood_ctcp_thr, flood_time, flood_pub_time, flood_join_time,
  flood_ctcp_time;
extern char botname[], origbotname[], botuser[], botrealname[], botserver[],
  motdfile[], admin[], userfile[], altnick[], firewall[],
  helpdir[], initserver[], notify_new[], notefile[], hostname[], myip[],
  botuserhost[], tempdir[], newserver[], textdir[], ctcp_version[],
  ctcp_finger[], ctcp_userinfo[], owner[], newserverpass[], newbotname[],
  network[], botnetnick[], chanfile[];
extern char flag1, flag2, flag3, flag4, flag5, flag6, flag7, flag8, flag9,
  flag0, chanflag1, chanflag2, chanflag3, chanflag4, chanflag5, chanflag6,
  chanflag7, chanflag8, chanflag9, chanflag0;
extern int online, maxnotes,modesperline,maxqmsg,wait_split,wait_info,
  wait_dcc_xfer,note_life,default_port;
extern struct eggqueue *serverlist;
extern struct dcc_t dcc[];
extern int dcc_total;
extern char egg_version[];
extern tcl_timer_t *timer,*utimer;
extern time_t online_since;
extern log_t logs[];
#ifndef NO_FILE_SYSTEM
extern char filedb_path[], dccdir[], dccin[];
extern int dcc_block, dcc_limit, dcc_maxsize, dcc_users;
#endif
#ifdef HAVE_NAT
extern char natip[];
#endif

/* prototypes for tcl */
Tcl_Interp *Tcl_CreateInterp();
int strtot=0;

int expmem_tcl()
{
  int i,tot=0;
  context;
  for (i=0; i<MAXLOGS; i++) if (logs[i].filename!=NULL) {
    tot+=strlen(logs[i].filename)+1;
    tot+=strlen(logs[i].chname)+1;
  }
  return tot+strtot;
}

/***********************************************************************/

/* logfile [<modes> <channel> <filename>] */
int tcl_logfile STDVAR
{
  int i; char s[151];
  BADARGS(1,4," ?logModes channel logFile?");
  if (argc==1) {
    /* they just want a list of the logfiles and modes */
    for (i=0; i<MAXLOGS; i++) if (logs[i].filename!=NULL) {
      strcpy(s,masktype(logs[i].mask)); strcat(s," ");
      strcat(s,logs[i].chname); strcat(s," ");
      strcat(s,logs[i].filename);
      Tcl_AppendElement(interp,s);
    }
    return TCL_OK;
  }
  BADARGS(4,4," ?logModes channel logFile?");
  for (i=0; i<MAXLOGS; i++) 
    if ((logs[i].filename!=NULL) && (strcmp(logs[i].filename,argv[3])==0)) {
      logs[i].mask=logmodes(argv[1]);
      nfree(logs[i].chname); logs[i].chname=NULL;
      if (!logs[i].mask) {
	/* ending logfile */
	nfree(logs[i].filename); logs[i].filename=NULL; 
	if (logs[i].f!=NULL) { fclose(logs[i].f); logs[i].f=NULL; }
      }
      else {
	logs[i].chname=(char *)nmalloc(strlen(argv[2])+1);
	strcpy(logs[i].chname,argv[2]);
      }
      Tcl_AppendResult(interp,argv[3],NULL);
      return TCL_OK;
    }
  for (i=0; i<MAXLOGS; i++) if (logs[i].filename==NULL) {
    logs[i].mask=logmodes(argv[1]);
    logs[i].filename=(char *)nmalloc(strlen(argv[3])+1);
    strcpy(logs[i].filename,argv[3]);
    logs[i].chname=(char *)nmalloc(strlen(argv[2])+1);
    strcpy(logs[i].chname,argv[2]);
    Tcl_AppendResult(interp,argv[3],NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(interp,"reached max # of logfiles",NULL);
  return TCL_ERROR;
}

int findidx PROTO1(int,z)
{
  int j;
  for (j=0; j<dcc_total; j++)
    if (dcc[j].sock==z)
      if ((dcc[j].type==DCC_CHAT) || (dcc[j].type==DCC_FILES) ||
	  (dcc[j].type==DCC_SCRIPT) || (dcc[j].type==DCC_SOCKET))
	return j;
  return -1;
}

void nick_change PROTO1(char *,new)
{
#ifndef NO_IRC
  if (strcasecmp(origbotname,new)!=0) {
    if (origbotname[0])
      putlog(LOG_MISC,"*","* IRC NICK CHANGE: %s -> %s",origbotname,new);
    strcpy(origbotname,new);
    /* start all over with nick chasing: */
    strcpy(newbotname,botname); /* store old nick in case something goes wrong*/
    strcpy(botname,origbotname);/* blah, this is kinda silly */
    if (serv>=0) tprintf(serv,"NICK %s\n",botname);
  }
#endif
}

void botnet_change PROTO1(char *,new)
{
  if (strcasecmp(botnetnick,new)!=0) {
    /* trying to change bot's nickname */
    if (get_tands() > 0) {
      putlog(LOG_MISC,"*","* Tried to change my botnet nick, but I'm still linked to a botnet.");
      putlog(LOG_MISC,"*","* (Unlink and try again.)");
      return;
    }
    else {
      if (botnetnick[0])
	putlog(LOG_MISC,"*","* IDENTITY CHANGE: %s -> %s",botnetnick,new);
      strcpy(botnetnick,new);
    }
  }
}

/**********************************************************************/

/* called when some script tries to change flag1..flag0 */
/* (possible that the new value will be invalid, so we ignore the change) */
char *tcl_eggflag PROTO5(ClientData,cdata,Tcl_Interp *,irp,char *,name1,
			 char *,name2,int,flags)
{
  char s1[2],*s; flaginfo *fi=(flaginfo *)cdata;
  if (flags&(TCL_TRACE_READS|TCL_TRACE_UNSETS)) {
    sprintf(s1,"%c",*(fi->flag));
    Tcl_SetVar2(interp,name1,name2,s1,TCL_GLOBAL_ONLY);
    return NULL;
  }
  else {  /* writes */
    s=Tcl_GetVar2(interp,name1,name2,TCL_GLOBAL_ONLY);
    if (s!=NULL) {
      s1[0]=*s; s1[1]=0;
      if (s1[0]==*(fi->flag)) return NULL;  /* nothing changed */
      if ((str2flags(s1)) && (s1[0]!=fi->def)) {   /* already in use! */
	s1[0]=*(fi->flag);
	Tcl_SetVar2(interp,name1,name2,s1,TCL_GLOBAL_ONLY);
	return NULL;
      }
      *(fi->flag)=s1[0];
    }
    return NULL;
  }
}

/* read/write normal integer */
char *tcl_eggint PROTO5(ClientData,cdata,Tcl_Interp *,irp,char *,name1,
			char *,name2,int,flags)
{
  char *s,s1[40]; long l;
  if (flags&(TCL_TRACE_READS|TCL_TRACE_UNSETS)) {
    /* special cases */
    if ((int *)cdata==&conmask) strcpy(s1,masktype(conmask));
    else if ((int *)cdata==&default_flags) flags2str(default_flags,s1);
    else if ((time_t *)cdata==&online_since)
      sprintf(s1,"%lu",*(unsigned long *)cdata);
    else sprintf(s1,"%d",*(int *)cdata);
    Tcl_SetVar2(interp,name1,name2,s1,TCL_GLOBAL_ONLY);
    return NULL;
  }
  else {  /* writes */
    s=Tcl_GetVar2(interp,name1,name2,TCL_GLOBAL_ONLY);
    if (s!=NULL) {
      if ((int *)cdata==&conmask) {
	if (s[0]) conmask=logmodes(s);
	else conmask=LOG_MODES|LOG_MISC|LOG_CMDS;
      }
      else if ((int *)cdata==&default_flags)
	default_flags=str2flags(s);
      else if ((time_t *)cdata==&online_since)
	return "read-only variable";
      else {
	if (Tcl_ExprLong(interp,s,&l)==TCL_ERROR) return interp->result;
	if ((int *)cdata == &modesperline) {
	   if (l < 3) 
	     l = 3;
	   if (l > 6)
	     l = 6;
	}
	*(int *)cdata=(int)l;
      }
    }
    return NULL;
  }
}

/* read/write normal string variable */
char *tcl_eggstr PROTO5(ClientData,cdata,Tcl_Interp *,irp,char *,name1,
			char *,name2,int,flags)
{
  char *s; strinfo *st=(strinfo *)cdata;
  if (flags&(TCL_TRACE_READS|TCL_TRACE_UNSETS)) {
    if ((st->str==firewall) && (firewall[0])) {
      char s1[161];
      sprintf(s1,"%s:%d",firewall,firewallport);
      Tcl_SetVar2(interp,name1,name2,s1,TCL_GLOBAL_ONLY);
    }
    else Tcl_SetVar2(interp,name1,name2,st->str,TCL_GLOBAL_ONLY);
    if (st->max<=0) {
      if ((flags&TCL_TRACE_UNSETS) && (protect_readonly || (st->max==0))) {
        /* no matter what we do, it won't return the error */
        Tcl_SetVar2(interp,name1,name2,st->str,TCL_GLOBAL_ONLY);
        Tcl_TraceVar(interp,name1,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                     tcl_eggstr,(ClientData)st);
	return "read-only variable";
      }
    }
    return NULL;
  }
  else {  /* writes */
    if ((st->max < 0) && (protect_readonly || (st->max==0))) {
      Tcl_SetVar2(interp,name1,name2,st->str,TCL_GLOBAL_ONLY);
      return "read-only variable";
    }
    s=Tcl_GetVar2(interp,name1,name2,TCL_GLOBAL_ONLY);
    if (s!=NULL) {
      if (strlen(s) > st->max) s[st->max]=0;
      if (st->str==origbotname) nick_change(s);
      if (st->str==botnetnick) botnet_change(s);
      else if (st->str==firewall) {
	splitc(firewall,s,':');
	if (!firewall[0]) strcpy(firewall,s);
	else firewallport=atoi(s);
      }
      else strcpy(st->str,s);
      if ((st->flags) && (s[0])) {
	if (st->str[strlen(st->str)-1]!='/') strcat(st->str,"/");
      }
      /* special cases */
      if (st->str==textdir) {
	if (!(st->str[0])) strcpy(st->str,helpdir);
      }
    }
    return NULL;
  }
}

/* trace a flag */
void tcl_traceflag PROTO3(char *,name,char *,ptr,char,def)
{
  flaginfo *fi;
  fi=(flaginfo *)nmalloc(sizeof(flaginfo));
  strtot+=sizeof(flaginfo);
  fi->def=def; fi->flag=ptr;
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	       tcl_eggflag,(ClientData)fi);
}

/* trace an int */
#define tcl_traceint(name,ptr) \
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggint,(ClientData)ptr)

/* set up a string variable to be traced (takes a little more effort than */
/* the others, cos the max length has to be stored too) */
void tcl_tracestr2 PROTO4(char *,name,char *,ptr,int,len,int,dir)
{
  strinfo *st;
  st=(strinfo *)nmalloc(sizeof(strinfo));
  strtot+=sizeof(strinfo);
  st->max=len-dir; st->str=ptr; st->flags=dir;
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	       tcl_eggstr,(ClientData)st);
}
#define tcl_tracestr(a,b,c) tcl_tracestr2(a,b,c,0)
#define tcl_tracedir(a,b,c) tcl_tracestr2(a,b,c,1)

/**********************************************************************/

/* oddballs */

/* read/write the server list */
char *tcl_eggserver PROTO5(ClientData,cdata,Tcl_Interp *,irp,char *,name1,
			   char *,name2,int,flags)
{
  Tcl_DString ds; char *slist,**list; struct eggqueue *q; int lc,code,i;
  if (flags&(TCL_TRACE_READS|TCL_TRACE_UNSETS)) {
    /* create server list */
    Tcl_DStringInit(&ds); q=serverlist;
    while (q!=NULL) {
      Tcl_DStringAppendElement(&ds,q->item);
      q=q->next;
    }
    slist=Tcl_DStringValue(&ds);
    Tcl_SetVar2(interp,name1,name2,slist,TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&ds);
    return NULL;
  }
  else {  /* writes */
    wipe_serverlist();   /* ouch. */
    slist=Tcl_GetVar2(interp,name1,name2,TCL_GLOBAL_ONLY);
    if (slist!=NULL) {
      code=Tcl_SplitList(interp,slist,&lc,&list);
      if (code==TCL_ERROR) return interp->result;
      for (i=0; i<lc; i++) add_server(list[i]);
      /* tricky way to make the bot reset its server pointers */
      /* perform part of a '.jump <current-server>': */
      curserv=(-1); n_free(list,"",0);
      if (botserver[0]) next_server(&curserv,botserver,&botserverport,"");
    }
    return NULL;
  }
}

/* read/write integer couplets (int1:int2) */
char *tcl_eggcouplet PROTO5(ClientData,cdata,Tcl_Interp *,irp,char *,name1,
			    char *,name2,int,flags)
{
  char *s,s1[41]; coupletinfo *cp=(coupletinfo *)cdata;
  if (flags&(TCL_TRACE_READS|TCL_TRACE_UNSETS)) {
    sprintf(s1,"%d:%d",*(cp->left),*(cp->right));
    Tcl_SetVar2(interp,name1,name2,s1,TCL_GLOBAL_ONLY);
    return NULL;
  }
  else {  /* writes */
    s=Tcl_GetVar2(interp,name1,name2,TCL_GLOBAL_ONLY);
    if (s!=NULL) {
      if (strlen(s) > 40) s[40]=0;
      splitc(s1,s,':');
      if (s1[0]) { *(cp->left)=atoi(s1); *(cp->right)=atoi(s); }
      else *(cp->left)=atoi(s);
    }
    return NULL;
  }
}

/* trace the servers */
#define tcl_traceserver(name,ptr) \
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggserver,(ClientData)ptr)

/* allocate couplet space for tracing couplets */
void tcl_tracecouplet PROTO3(char *,name,int *,lptr,int *,rptr)
{
  coupletinfo *cp;
  cp=(coupletinfo *)nmalloc(sizeof(coupletinfo));
  strtot+=sizeof(coupletinfo);
  cp->left=lptr; cp->right=rptr;
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	       tcl_eggcouplet,(ClientData)cp);
}

/**********************************************************************/

#ifdef EBUG
int check_cmd PROTO2(int,idx,char *,msg)
{
  char s[512];
  context;
  stridx(s,msg,1);
  if (strcasecmp(s,"chpass")==0) {
    stridx(s,msg,3);
    if (s[0]) return 1;
  }
  return 0;
}
#endif

void protect_tcl()
{
  protect_readonly=1;
}

void unprotect_tcl()
{
  protect_readonly=0;
}

/* set up Tcl variables that will hook into eggdrop internal vars via */
/* trace callbacks */
void init_traces()
{
  /* put traces on the 10 flag variables */
  tcl_traceflag("flag1",&flag1,'1');
  tcl_traceflag("flag2",&flag2,'2');
  tcl_traceflag("flag3",&flag3,'3');
  tcl_traceflag("flag4",&flag4,'4');
  tcl_traceflag("flag5",&flag5,'5');
  tcl_traceflag("flag6",&flag6,'6');
  tcl_traceflag("flag7",&flag7,'7');
  tcl_traceflag("flag8",&flag8,'8');
  tcl_traceflag("flag9",&flag9,'9');
  tcl_traceflag("flag0",&flag0,'0');
  tcl_traceflag("chanflag1",&chanflag1,'1');
  tcl_traceflag("chanflag2",&chanflag2,'2');
  tcl_traceflag("chanflag3",&chanflag3,'3');
  tcl_traceflag("chanflag4",&chanflag4,'4');
  tcl_traceflag("chanflag5",&chanflag5,'5');
  tcl_traceflag("chanflag6",&chanflag6,'6');
  tcl_traceflag("chanflag7",&chanflag7,'7');
  tcl_traceflag("chanflag8",&chanflag8,'8');
  tcl_traceflag("chanflag9",&chanflag9,'9');
  tcl_traceflag("chanflag0",&chanflag0,'0');
  tcl_tracestr("nick",origbotname,9);
  tcl_tracestr("botnet-nick",botnetnick,9);
  tcl_tracestr("altnick",altnick,9);
  tcl_tracestr("realname",botrealname,80);
  tcl_tracestr("username",botuser,10);
  tcl_tracestr("userfile",userfile,-120);
  tcl_tracestr("channel-file",chanfile,-120);
  tcl_tracestr("motd",motdfile,120);
  tcl_tracestr("admin",admin,120);
  tcl_tracestr("init-server",initserver,120);
  tcl_tracestr("notefile",notefile,120);
#ifndef NO_FILE_SYSTEM
# ifndef MODULES
  tcl_tracedir("files-path",dccdir,-120);
  tcl_tracedir("incoming-path",dccin,-120);
  tcl_tracedir("filedb-path",filedb_path,-120);
# endif
#endif
  tcl_tracedir("help-path",helpdir,120);
  tcl_tracedir("temp-path",tempdir,120);
  tcl_tracedir("text-path",textdir,120);
  tcl_tracestr("notify-newusers",notify_new,120);
  tcl_tracestr("ctcp-version",ctcp_version,120);
  tcl_tracestr("ctcp-finger",ctcp_finger,120);
  tcl_tracestr("ctcp-userinfo",ctcp_userinfo,120);
  tcl_tracestr("owner",owner,-120);
  tcl_tracestr("my-hostname",hostname,120);
  tcl_tracestr("my-ip",myip,120);
  tcl_tracestr("network",network,40);
  tcl_tracestr("whois-fields",whois_fields,120);
  tcl_tracestr("timezone",time_zone,40);
#ifdef HAVE_NAT
  tcl_tracestr("nat-ip",natip,120);
#endif
  /* ints */
  tcl_traceint("servlimit",&min_servs);
  tcl_traceint("ban-time",&ban_time);
  tcl_traceint("ignore-time",&ignore_time);
#ifndef NO_FILE_SYSTEM
#ifndef MODULES
  tcl_traceint("max-dloads",&dcc_limit);
  tcl_traceint("dcc-block",&dcc_block);
  tcl_traceint("max-filesize",&dcc_maxsize);
  tcl_traceint("max-file-users",&dcc_users);
  tcl_traceint("upload-to-pwd",&upload_to_cd);
  tcl_traceint("copy-to-tmp",&copy_to_tmp);
  tcl_traceint("xfer-timeout",&wait_dcc_xfer);
#endif
#endif
  tcl_traceint("save-users-at",&save_users_at);
  tcl_traceint("notify-users-at",&notify_users_at);
  tcl_traceint("switch-logfiles-at",&switch_logfiles_at);
  tcl_traceint("server-timeout",&server_timeout);
  tcl_traceint("connect-timeout",&connect_timeout);
  tcl_traceint("reserved-port",&reserved_port);
  /* booleans (really just ints) */
  tcl_traceint("log-time",&shtime);
  tcl_traceint("learn-users",&learn_users);
  tcl_traceint("require-p",&require_p);
  tcl_traceint("use-info",&use_info);
  tcl_traceint("share-users",&share_users);
  tcl_traceint("share-greet",&share_greet);
  tcl_traceint("passive",&passive);
  tcl_traceint("strict-host",&strict_host);
  tcl_traceint("keep-all-logs",&keep_all_logs);
  tcl_traceint("never-give-up",&never_give_up);
  tcl_traceint("open-telnets",&allow_new_telnets);
  tcl_traceint("keep-nick",&keepnick);
  /* always very read-only */
  tcl_tracestr("version",egg_version,0);
  tcl_tracestr("botnick",botname,0);
  tcl_traceint("uptime",&online_since);
  /* weird ones */
  tcl_traceserver("servers",NULL);
  tcl_tracestr("firewall",firewall,120);
  tcl_traceint("console",&conmask);
  tcl_traceint("default-flags",&default_flags);
  tcl_tracecouplet("flood-msg",&flood_thr,&flood_time);
  tcl_tracecouplet("flood-chan",&flood_pub_thr,&flood_pub_time);
  tcl_tracecouplet("flood-join",&flood_join_thr,&flood_join_time);
  tcl_tracecouplet("flood-ctcp",&flood_ctcp_thr,&flood_ctcp_time);
  /* moved from eggdrop.h */
  tcl_traceint("modes-per-line",&modesperline);
  tcl_traceint("max-queue-msg",&maxqmsg);
  tcl_traceint("wait-split",&wait_split);
  tcl_traceint("wait-info",&wait_info);
  tcl_traceint("default-port",&default_port);
  tcl_traceint("note-life",&note_life);
  tcl_traceint("max-notes",&maxnotes);
}

/* not going through Tcl's crazy main() system (what on earth was he
   smoking?!) so we gotta initialize the Tcl interpreter */
void init_tcl()
{
  /* initialize the interpreter */
  interp=Tcl_CreateInterp();
  Tcl_Init(interp);
  init_hash();
  init_builtins();
  init_traces();
/* see note (1) at the bottom of this file */
/*  Tcl_DeleteCommand(interp,"exec"); */
  /* add new commands */
  Tcl_CreateCommand(interp,"bind",tcl_bind,(ClientData)0,NULL);
  Tcl_CreateCommand(interp,"unbind",tcl_bind,(ClientData)1,NULL);
#define Q(A,B) Tcl_CreateCommand(interp,A,B,NULL,NULL)
  Q("logfile",tcl_logfile);
  Q("putserv",tcl_putserv); Q("puthelp",tcl_puthelp); Q("putdcc",tcl_putdcc);
  Q("putlog",tcl_putlog); Q("putcmdlog",tcl_putcmdlog); Q("putidx",tcl_putidx);
  Q("putxferlog",tcl_putxferlog); Q("putloglev",tcl_putloglev);
  Q("countusers",tcl_countusers); Q("validuser",tcl_validuser);
  Q("finduser",tcl_finduser); Q("passwdok",tcl_passwdOk);
  Q("chattr",tcl_chattr); Q("matchattr",tcl_matchattr);
  Q("botisop",tcl_botisop); Q("isop",tcl_isop); Q("isvoice",tcl_isvoice);
  Q("onchan",tcl_onchan); Q("handonchan",tcl_handonchan);
  Q("ischanban",tcl_ischanban); Q("getchanhost",tcl_getchanhost);
  Q("onchansplit",tcl_onchansplit); Q("chanlist",tcl_chanlist);
  Q("adduser",tcl_adduser); Q("addbot",tcl_addbot); Q("deluser",tcl_deluser);
  Q("boot",tcl_boot); Q("rehash",tcl_rehash); Q("restart",tcl_restart);
#ifdef ENABLE_TCL_DCCSIMUL
  Q("dccsimul",tcl_dccsimul);
#endif
  Q("addhost",tcl_addhost); Q("delhost",tcl_delhost); Q("getaddr",tcl_getaddr);
  Q("timer",tcl_timer); Q("killtimer",tcl_killtimer); Q("utimer",tcl_utimer);
  Q("killutimer",tcl_killutimer); Q("unixtime",tcl_unixtime);
  Q("time",tcl_time); Q("date",tcl_date); Q("jump",tcl_jump);
  Q("getinfo",tcl_getinfo); Q("maskhost",tcl_maskhost);
  Q("getdccdir",tcl_getdccdir); Q("getcomment",tcl_getcomment);
  Q("getemail",tcl_getemail); Q("getxtra",tcl_getxtra);
  Q("setinfo",tcl_setinfo); Q("setdccdir",tcl_setdccdir);
  Q("setcomment",tcl_setcomment); Q("setemail",tcl_setemail);
  Q("setxtra",tcl_setxtra);   /* setaddr? */
  Q("isban",tcl_isban); Q("ispermban",tcl_ispermban);
  Q("matchban",tcl_matchban); Q("ctime",tcl_ctime); Q("myip",tcl_myip);
  Q("getlaston",tcl_getlaston); Q("setlaston",tcl_setlaston);
  Q("timers",tcl_timers); Q("utimers",tcl_utimers); Q("rand",tcl_rand);
  Q("hand2idx",tcl_hand2idx); Q("idx2hand",tcl_idx2hand);
  Q("getchan",tcl_getchan); Q("setchan",tcl_setchan);
  Q("dccbroadcast",tcl_dccbroadcast); Q("dccputchan",tcl_dccputchan);
  Q("console",tcl_console); Q("echo",tcl_echo); Q("control",tcl_control);
  Q("putbot",tcl_putbot); Q("putallbots",tcl_putallbots);
  Q("getchanidle",tcl_getchanidle); Q("killdcc",tcl_killdcc);
  Q("userlist",tcl_userlist); Q("sendnote",tcl_sendnote);
  Q("save",tcl_save); Q("reload",tcl_reload); Q("bots",tcl_bots);
  Q("chanbans",tcl_chanbans); Q("gethosts",tcl_gethosts);
  Q("nick2hand",tcl_nick2hand); Q("hand2nick",tcl_hand2nick);
  Q("getdccidle",tcl_getdccidle); Q("dcclist",tcl_dcclist);
  Q("dccused",tcl_dccused); Q("chpass",tcl_chpass); Q("chnick",tcl_chnick);
  Q("link",tcl_link); Q("unlink",tcl_unlink); Q("banlist",tcl_banlist);
  Q("channel",tcl_channel); Q("channels",tcl_channels);
  Q("resetchan",tcl_resetchan); Q("validchan",tcl_validchan);
  Q("getting-users",tcl_getting_users); Q("strip",tcl_strip);
  Q("page",tcl_page); Q("savechannels",tcl_savechannels);
  Q("loadchannels",tcl_loadchannels); Q("isdynamic",tcl_isdynamic);
#ifndef MODULES
#ifndef NO_FILE_SYSTEM
  Q("dccsend",tcl_dccsend); Q("getfileq",tcl_getfileq);
  Q("getdesc",tcl_getdesc); Q("getowner",tcl_getowner);
  Q("setdesc",tcl_setdesc); Q("setowner",tcl_setowner);
  Q("getgots",tcl_getgots); Q("getpwd",tcl_getpwd); Q("setpwd",tcl_setpwd);
  Q("getlink",tcl_getlink); Q("setlink",tcl_setlink);
  Q("getfiles",tcl_getfiles); Q("getdirs",tcl_getdirs);
  Q("hide",tcl_hide); Q("unhide",tcl_unhide); Q("share",tcl_share);
  Q("unshare",tcl_unshare); Q("filesend",tcl_filesend);
  Q("getuploads",tcl_getuploads); Q("setuploads",tcl_setuploads);
  Q("getdnloads",tcl_getdnloads); Q("setdnloads",tcl_setdnloads);
  Q("mkdir",tcl_mkdir); Q("rmdir",tcl_rmdir); Q("cp",tcl_cp); Q("mv",tcl_mv);
  Q("getflags",tcl_getflags); Q("setflags",tcl_setflags);
#endif
  Q("assoc",tcl_assoc); Q("killassoc",tcl_killassoc);
#endif
  Q("getchanmode",tcl_getchanmode); Q("pushmode",tcl_pushmode);
  Q("flushmode",tcl_flushmode); Q("isignore",tcl_isignore);
#ifndef MODULES
  Q("encrypt",tcl_encrypt); Q("decrypt",tcl_decrypt);
#endif
  Q("connect",tcl_connect);
  Q("getdccaway",tcl_getdccaway); Q("setdccaway",tcl_setdccaway);
  Q("newchanban",tcl_newchanban); Q("newban",tcl_newban);
  Q("killchanban",tcl_killchanban); Q("killban",tcl_killban);
  Q("newignore",tcl_newignore); Q("killignore",tcl_killignore);
  Q("ignorelist",tcl_ignorelist); Q("whom",tcl_whom);
  Q("dumpfile",tcl_dumpfile); Q("dccdumpfile",tcl_dccdumpfile);
  Q("valididx",tcl_valididx); Q("backup",tcl_backup); Q("listen",tcl_listen);
  Q("resetbans",tcl_resetbans); Q("topic",tcl_topic); Q("die",tcl_die);
  Q("matchchanattr",tcl_matchchanattr); Q("getchanjoin",tcl_getchanjoin);
  Q("getchaninfo",tcl_getchaninfo); Q("setchaninfo",tcl_setchaninfo);
  Q("addchanrec",tcl_addchanrec); Q("delchanrec",tcl_delchanrec);
  Q("getchanlaston",tcl_getchanlaston);
  Q("strftime",tcl_strftime); Q("notes",tcl_notes);
#ifdef MODULES
  Q("loadmodule",tcl_loadmodule); Q("unloadmodule",tcl_unloadmodule);
#endif
}

/* set Tcl variables to match eggdrop internal variables */
void set_tcl_vars()
{
  char s[121];
  /* variables that we won't re-read... only for convenience of scripts */
  sprintf(s,"%s:%d",botserver,botserverport);
  Tcl_SetVar(interp,"server",s,TCL_GLOBAL_ONLY);
  sprintf(s,"%s!%s",botname,botuserhost);
  Tcl_SetVar(interp,"botname",s,TCL_GLOBAL_ONLY);
  /* cos we have to: */
  Tcl_SetVar(interp,"tcl_interactive","0",TCL_GLOBAL_ONLY);
}

/**********************************************************************/

/* show user-defined whois fields */
void tcl_tell_whois PROTO2(int,idx,char *,xtra)
{
  int code,lc,xc,qc,i,j; char **list,**xlist,**qlist;
  context;
  code=Tcl_SplitList(interp,whois_fields,&lc,&list);
  if (code==TCL_ERROR) return;
  context;
  code=Tcl_SplitList(interp,xtra,&xc,&xlist);
  if (code==TCL_ERROR) { n_free(list,"",0); return; }
  /* scan thru xtra field, searching for matches */
  context;
  for (i=0; i<xc; i++) {
    code=Tcl_SplitList(interp,xlist[i],&qc,&qlist);
    context;
    if ((code==TCL_OK) && (qc==2)) {
      /* ok, it's a valid xtra field entry */
      context;
      for (j=0; j<lc; j++) if (strcasecmp(list[j],qlist[0])==0) {
	dprintf(idx,"  %s: %s\n",qlist[0],qlist[1]);
      }
      n_free(qlist,"",0);
    }
  }
  n_free(list,"",0); n_free(xlist,"",0);
  context;
}

/* evaluate a Tcl command, send output to a dcc user */
void cmd_tcl PROTO2(int,idx,char *,msg)
{
  int code;
#ifdef EBUG
  int i=0; char s[512];
  context;
  if (msg[0]) i=check_cmd(idx,msg);
  if (i) {
    stridx(s,msg,2);
    if (s[0]) debug1("tcl: evaluate (.tcl): chpass %s [something]",s);
    else debug1("tcl: evaluate (.tcl): %s",msg);
  }
  else debug1("tcl: evaluate (.tcl): %s",msg);
#endif
  context;
  set_tcl_vars();
  context;
  code=Tcl_GlobalEval(interp,msg);
  context;
  if (code==TCL_OK) dumplots(idx,"Tcl: ",interp->result);
  else dumplots(idx,"TCL error: ",interp->result);
  context;
  /* refresh internal vars */
}

/* perform a 'set' command */
void cmd_set PROTO2(int,idx,char *,msg)
{
  int code; char s[512];
  putlog(LOG_CMDS,"*","#%s# set %s",dcc[idx].nick,msg);
  set_tcl_vars();
  strcpy(s,"set "); strcat(s,msg);
  if (!msg[0]) {
    strcpy(s,"info globals");
    Tcl_Eval(interp,s);
    dumplots(idx,"global vars: ",interp->result);
    return;
  }
  code=Tcl_Eval(interp,s);
  if (code==TCL_OK) {
    if (strchr(msg,' ')==NULL) dumplots(idx,"currently: ",interp->result);
    else dprintf(idx,"Ok, set.\n");
  }
  else dprintf(idx,"Error: %s\n",interp->result);
}

void do_tcl PROTO2(char *,whatzit,char *,script)
{
  int code;
#ifdef EBUG_TCL
  FILE *f=fopen("DEBUG.TCL","a");
  if (f!=NULL) fprintf(f,"eval: %s\n",script);
#endif
  set_tcl_vars();
  context;
  code=Tcl_Eval(interp,script);
#ifdef EBUG_TCL
  if (f!=NULL) {
    fprintf(f,"done eval, result=%d\n",code);
    fclose(f);
  }
#endif
  if (code!=TCL_OK) {
    putlog(LOG_MISC,"*","Tcl error in script for '%s':",whatzit);
    putlog(LOG_MISC,"*","%s",interp->result);
  }
}

/* read and interpret the configfile given */
/* return 1 if everything was okay */
int readtclprog PROTO1(char *,fname)
{
  int code; FILE *f;
  set_tcl_vars();
  f=fopen(fname,"r"); if (f==NULL) return 0;
  fclose(f);
#ifdef EBUG_TCL
  f=fopen("DEBUG.TCL","a");
  if (f!=NULL) {
    fprintf(f,"Sourcing file %s ...\n",fname);
    fclose(f);
  }
#endif
  code=Tcl_EvalFile(interp,fname);
  if (code!=TCL_OK) {
    if (use_stderr) {
      tprintf(STDERR,"Tcl error in file '%s':\n",fname);
      tprintf(STDERR,"%s\n",Tcl_GetVar(interp,"errorInfo",TCL_GLOBAL_ONLY));
    }
    else {
      putlog(LOG_MISC,"*","Tcl error in file '%s':",fname);
      putlog(LOG_MISC,"*","%s\n",Tcl_GetVar(interp,"errorInfo",TCL_GLOBAL_ONLY));
    }
    /* try to go on anyway (shrug) */
  }
  /* refresh internal variables */
  return 1;
}

/*
   note (1):
     the tcl 'exec' command is no longer removed, since it is assumed
     that the tcl command will be left at its default flag requirement,
     ie: only owners can do tcl commands directly.  also, removing the
     'exec' command doesn't block up all holes -- tcl allows you to open
     a "pipe" which really just executes a shell command and redirects
     output.  so you were never truly safe anyway.  gee.
*/

#ifdef MODULES
#include "modules.h"

void add_tcl_strings PROTO1(tcl_strings *,list) {
   int i;
   for (i=0;list[i].name;i++) {
      if (list[i].length > 0) {
	 char * p = Tcl_GetVar(interp,list[i].name,TCL_GLOBAL_ONLY);
	 if (p!=NULL) {
	   strncpy(list[i].buf,p,list[i].length);
	   list[i].buf[list[i].length]=0;
	 }
      }
      tcl_tracestr2(list[i].name,list[i].buf,
		    (list[i].flags&STR_PROTECT)?-list[i].length:list[i].length,
		    (list[i].flags&STR_DIR));
      
   }
}
void rem_tcl_strings PROTO1(tcl_strings *,list) {
   int i;
   strinfo *st;
   
   for (i=0;list[i].name;i++) {
      st = (strinfo *)Tcl_VarTraceInfo(interp,list[i].name,
	    TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    tcl_eggstr,NULL);
      if (st != NULL) {
	 strtot-=sizeof(strinfo);
	 nfree(st);
      }
      Tcl_UntraceVar(interp,list[i].name,
		     TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     tcl_eggstr,st);
   }
}

void add_tcl_ints PROTO1(tcl_ints *,list) {
   int i;
   for (i=0;list[i].name;i++) {
      char * p = Tcl_GetVar(interp,list[i].name,TCL_GLOBAL_ONLY);
      if (p!=NULL)
	*(list[i].val) = atoi(p);
      Tcl_TraceVar(interp,list[i].name,
		   TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		   tcl_eggint,(ClientData)list[i].val);
   }
}

void rem_tcl_ints PROTO1(tcl_ints *,list) {
   int i;
   for (i=0;list[i].name;i++) {
      char * p = Tcl_GetVar(interp,list[i].name,TCL_GLOBAL_ONLY);
      if (p!=NULL)
	 *(list[i].val) = atoi(p);
      Tcl_UntraceVar(interp,list[i].name,
		     TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     tcl_eggint,(ClientData)list[i].val);
   }
}
#endif
