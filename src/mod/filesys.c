/*
 * assoc.c - the assoc module, moved here mainly from botnet.c for module
 * work
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#define MOD_FILESYS
#define MODULE_NAME "filesys"

#include "module.h"
#include "filesys.h"
#include "../tandem.h"
#include "../files.h"
#include "../users.h"
#include "../cmdt.h"
#include <sys/stat.h>
#ifdef HAVE_NAT
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifndef NO_FILE_SYSTEM
#ifdef MODULES
extern char dccdir[];
extern char dccin[];
extern char filedb_path[];
extern int dcc_users;
extern int dcc_limit;
extern int wait_dcc_xfer;
extern int upload_to_cd;
extern int dcc_block;
#else

#endif
#ifdef HAVE_NAT
extern char natip[];
#endif
/* maximum allowable file size for dcc send (1M) */
int dcc_maxsize=1024;
/* copy files to /tmp before transmitting? */
int copy_to_tmp=1;
/* timeout time on DCC xfers */
int wait_dcc_xfer = 300;

int filesys_eof_hook PROTO((int idx));
int filesys_activity_hook PROTO((int idx,char * buf,int len));

void tell_file_stats PROTO2(int,idx,char *,hand)
{
  struct userrec *u; float fr=(-1.0),kr=(-1.0);
  u=get_user_by_handle(userlist,hand);
  if (u==NULL) return;
  modprintf(idx,"  uploads: %4u / %6luk\n",u->uploads,u->upload_k);
  modprintf(idx,"downloads: %4u / %6luk\n",u->dnloads,u->dnload_k);
  if (u->uploads) fr=((float)u->dnloads / (float)u->uploads);
  if (u->upload_k) kr=((float)u->dnload_k / (float)u->upload_k);
  if (fr<0.0) modprintf(idx,"(infinite file leech)\n");
  else modprintf(idx,"leech ratio (files): %6.2f\n",fr);
  if (kr<0.0) modprintf(idx,"(infinite size leech)\n");
  else modprintf(idx,"leech ratio (size) : %6.2f\n",kr);
}

#ifdef MODULES
static
#endif
int cmd_files PROTO2(int,idx,char *,par)
{
  int atr=get_attr_handle(dcc[idx].nick);
  modcontext;
  
  if (dccdir[0]==0) modprintf(idx,"There is no file transfer area.\n");
  else if (too_many_filers()) {
   modcontext;
    modprintf(idx,"The maximum of %d people are in the file area right now.\n",
	    dcc_users);
    modprintf(idx,"Please try again later.\n");
  }
  else {
   modcontext;
    if (!(atr & (USER_MASTER|USER_XFER)))
      modprintf(idx,"You don't have access to the file area.\n");
    else {
   modcontext;
      putlog(LOG_CMDS,"*","#%s# files",dcc[idx].nick);
      modprintf(idx,"Entering file system...\n");
   modcontext;
      if (dcc[idx].u.chat->channel>=0) {
	chanout2(dcc[idx].u.chat->channel,"%s is away: file system\n",
		 dcc[idx].nick);
	modcontext;
	if (dcc[idx].u.chat->channel<100000)
          tandout("away %s %d file system\n",botnetnick,dcc[idx].sock);
      }
   modcontext;
      set_files(idx); dcc[idx].type=DCC_FILES;
      dcc[idx].u.file->chat->status|=STAT_CHAT;
   modcontext;
      if (!welcome_to_files(idx)) {
	struct chat_info *ci=dcc[idx].u.file->chat;
   modcontext;
	modfree(dcc[idx].u.file); dcc[idx].u.chat=ci;
	dcc[idx].type=DCC_CHAT;
	putlog(LOG_FILES,"*","File system broken.");
	if (dcc[idx].u.chat->channel>=0) {
   modcontext;
	  chanout2(dcc[idx].u.chat->channel,"%s has returned.\n",
		   dcc[idx].nick);
          modcontext;
	  if (dcc[idx].u.chat->channel<100000)
	    tandout("unaway %s %d\n",botnetnick,dcc[idx].sock);
	}
      }
    }
  }
   modcontext;
  return 0;
}

#ifdef MODULES
static
#endif
int cmd_filestats PROTO2(int,idx,char *,par)
{
  char nick[512];
  modcontext;
  if (!par[0]) {
   modprintf(idx,"Usage: filestats <user>\n");
   return 0;
  }
  nsplit(nick,par);
  putlog(LOG_CMDS,"*","#%s# filestats %s",dcc[idx].nick,par);
  if (nick[0]==0) tell_file_stats(idx,dcc[idx].nick);
  else if (!is_user(nick)) modprintf(idx,"No such user.\n");
  else if ((!strcmp(par,"clear")) &&
	   !(get_attr_handle(dcc[idx].nick)&USER_MASTER)) {
    set_handle_uploads(userlist,nick,0,0);
    set_handle_dnloads(userlist,nick,0,0);
  }
  else tell_file_stats(idx,nick);
  return 0;
}

void wipe_tmp_filename PROTO2(char *,fn,int,idx)
{
  int i,ok=1;
  if (!copy_to_tmp) return;
  for (i=0; i<dcc_total; i++) if (i!=idx)
    if ((dcc[i].type==DCC_GET) || (dcc[i].type==DCC_GET_PENDING))
      if (strcmp(dcc[i].u.xfer->filename,fn)==0) ok=0;
  if (ok) unlink(fn);
}

/* given idx of a completed file operation, check to make sure no other
   file transfers are happening currently on that file -- if there aren't
   any, erase the file (it's just a copy anyway) */
void wipe_tmp_file PROTO1(int,idx)
{
  wipe_tmp_filename(dcc[idx].u.xfer->filename,idx);
}

#define DCCSEND_OK     0
#define DCCSEND_FULL   1     /* dcc table is full */
#define DCCSEND_NOSOCK 2     /* can't open a listening socket */
#define DCCSEND_BADFN  3     /* no such file */

int _dcc_send PROTO4(int,idx,char *,filename,char *,nick,char *,dir)
{
  int x; char *nfn;
  modcontext;
  x=raw_dcc_send(filename,nick,dcc[idx].nick,dir);
  if (x==DCCSEND_FULL) {
    modprintf(idx,"Sorry, too many DCC connections.  (try again later)\n");
    putlog(LOG_FILES,"*","DCC connections full: GET %s [%s]",filename,
	   dcc[idx].nick);
    return 0;
  }
  if (x==DCCSEND_NOSOCK) {
    if (reserved_port) {
      modprintf(idx,"My DCC SEND port is in use.  Try later.\n");
      putlog(LOG_FILES,"*","DCC port in use (can't open): GET %s [%s]",
	     filename,dcc[idx].nick);
    }
    else {
      modprintf(idx,"Unable to listen at a socket.\n");
      putlog(LOG_FILES,"*","DCC socket error: GET %s [%s]",filename,
	     dcc[idx].nick);
    }
    return 0;
  }
  if (x==DCCSEND_BADFN) {
    modprintf(idx,"File not found (???)\n");
    putlog(LOG_FILES,"*","DCC file not found: GET %s [%s]",filename,
	   dcc[idx].nick);
    return 0;
  }
  nfn=strrchr(filename,'/');
  if (nfn==NULL) nfn=filename; else nfn++;
  if (strcasecmp(nick,dcc[idx].nick)!=0)
    modprintf(DP_HELP,"NOTICE %s :Here is a file from %s ...\n",nick,dcc[idx].nick);
  modprintf(idx,"Type '/DCC GET %s %s' to receive.\n",botname,nfn);
  modprintf(idx,"Sending: %s to %s\n",nfn,nick);
  return 1;
}

int do_dcc_send PROTO3(int,idx,char *,dir,char *,filename)
{
  char s[161],s1[161],fn[512],nick[512]; FILE *f; int x;

  modcontext;
  /* nickname? */
  strcpy(nick,filename);
  nsplit(fn,nick); nick[9]=0;
  if (dccdir[0]==0) {
    modprintf(idx,"DCC file transfers not supported.\n");
    putlog(LOG_FILES,"*","Refused dcc get %s from [%s]",fn,dcc[idx].nick);
    return 0;
  }
  if (strchr(fn,'/')!=NULL) {
    modprintf(idx,"Filename cannot have '/' in it...\n");
    putlog(LOG_FILES,"*","Refused dcc get %s from [%s]",fn,dcc[idx].nick);
    return 0;
  }
  if (dir[0]) sprintf(s,"%s%s/%s",dccdir,dir,fn);
  else sprintf(s,"%s%s",dccdir,fn);
  f=fopen(s,"r"); if (f==NULL) {
    modprintf(idx,"No such file.\n");
    putlog(LOG_FILES,"*","Refused dcc get %s from [%s]",fn,dcc[idx].nick);
    return 0;
  }
  fclose(f);
  if (!nick[0]) strcpy(nick,dcc[idx].nick);
  /* already have too many transfers active for this user?  queue it */
  if (at_limit(nick)) {
    queue_file(dir,fn,dcc[idx].nick,nick);
    modprintf(idx,"Queued: %s to %s\n",fn,nick);
    return 1;
  }
  if (copy_to_tmp) {
    /* copy this file to /tmp */
    sprintf(s,"%s%s%s%s",dccdir,dir,dir[0]?"/":"",fn);
    sprintf(s1,"%s%s",tempdir,fn);
    if (copyfile(s,s1)!=0) {
      modprintf(idx,"Can't make temporary copy of file!\n");
      putlog(LOG_FILES|LOG_MISC,"*","Refused dcc get %s: copy to %s FAILED!",
	     fn,tempdir);
      return 0;
    }
  }
  else sprintf(s1,"%s%s%s%s",dccdir,dir,dir[0]?"/":"",fn);
  sprintf(s,"%s%s%s",dir,dir[0]?"/":"",fn);
  x=_dcc_send(idx,s1,nick,s);
  if (x!=DCCSEND_OK) wipe_tmp_filename(s1,-1);
  return x;
}  

int raw_dcc_send PROTO4(char *,filename,char *,nick,char *,from,char *,dir)
{
  int zz,port,i; char *nfn; IP host; struct stat ss;
  modcontext;
  if ((i=new_dcc(DCC_GET_PENDING))==-1)
     return DCCSEND_FULL;
  port=reserved_port;
  zz=open_listen(&port);
  if (zz==(-1)) return DCCSEND_NOSOCK;
  nfn=strrchr(filename,'/');
  if (nfn==NULL) nfn=filename; else nfn++;
  host=getmyip();
  stat(filename,&ss);
  dcc[i].sock=zz;
  dcc[i].addr=(IP)(-559026163);
  dcc[i].port=port;
  strcpy(dcc[i].nick,nick);
  strcpy(dcc[i].host,"irc");
  strcpy(dcc[i].u.xfer->filename,filename);
  strcpy(dcc[i].u.xfer->from,from);
  strcpy(dcc[i].u.xfer->dir,dir);
  dcc[i].u.xfer->length=ss.st_size;
  dcc[i].u.xfer->sent=0;
  dcc[i].u.xfer->sofar=0;
  dcc[i].u.xfer->acked=0;
  dcc[i].u.xfer->pending=time(NULL);
  dcc[i].u.xfer->f=fopen(filename,"r");
  if (dcc[i].u.xfer->f==NULL) {
    lostdcc(i);
    return DCCSEND_BADFN;
  }
  if (nick[0]!='*') {
#ifndef NO_IRC
#ifdef HAVE_NAT
     modprintf(DP_HELP,"PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n",nick,nfn,
	    iptolong((IP)inet_addr(natip)),port,ss.st_size);
#else
     modprintf(DP_HELP,"PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n",nick,nfn,
	    iptolong(host),port,ss.st_size);
#endif
#endif
    putlog(LOG_FILES,"*","Begin DCC send %s to %s",nfn,nick);
  }
  return DCCSEND_OK;
}
#endif

#ifdef MODULES
static int filesys_timeout PROTO1(int,i) {
   time_t now = time(NULL);
   char xx[1024];
   
   modcontext;
   switch (dcc[i].type) {
    case DCC_GET_PENDING:
      if (now-dcc[i].u.xfer->pending > wait_dcc_xfer) {
	 if (strcmp(dcc[i].nick,"*users")==0) {
	    int x,y=0; 
	    for (x=0;x<dcc_total; x++)
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
	    char * p;
	    strcpy(xx,dcc[i].u.xfer->filename); p=strrchr(xx,'/');
	    modprintf(DP_HELP,"NOTICE %s :Timeout during transfer, aborting %s.\n",
		    dcc[i].nick,p?p+1:xx);
	    putlog(LOG_FILES,"*","DCC timeout: GET %s (%s) at %lu/%lu",p?p+1:xx,
		   dcc[i].nick,dcc[i].u.xfer->sent,dcc[i].u.xfer->length);
	    wipe_tmp_file(i); strcpy(xx,dcc[i].nick);
	 }
	 killsock(dcc[i].sock); lostdcc(i); i--;
	 if (!at_limit(xx)) send_next_file(xx);
      }
      return 1;
    case DCC_SEND:
      if (now-dcc[i].u.xfer->pending > wait_dcc_xfer) {
	 if (strcmp(dcc[i].nick,"*users")==0) {
	    int x,y=0;
	    for (x=0;x<dcc_total; x++)
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
	    modprintf(DP_HELP,"NOTICE %s :Timeout during transfer, aborting %s.\n",
		    dcc[i].nick,dcc[i].u.xfer->filename);
	    putlog(LOG_FILES,"*","DCC timeout: SEND %s (%s) at %lu/%lu",
		   dcc[i].u.xfer->filename,dcc[i].nick,dcc[i].u.xfer->sent,
		   dcc[i].u.xfer->length);
	    sprintf(xx,"%s%s",tempdir,dcc[i].u.xfer->filename);
	    unlink(xx);
	 }
	 killsock(dcc[i].sock); lostdcc(i); i--;
      }
      return 1;
    case DCC_FILES_PASS:
      if (now-dcc[i].u.file->chat->timer > 180) {
	 modprintf(i,"Timeout.\n");
	 putlog(LOG_MISC,"*","Password timeout on dcc chat: [%s]%s",dcc[i].nick,
		dcc[i].host);
	 killsock(dcc[i].sock); lostdcc(i); i--;
      }
      return 1;
   }
   return 0;
}

#ifndef NO_IRC
/* received a ctcp-dcc */
static int filesys_gotdcc PROTO4(char *,nick,char *,from,char *,code, char *,msg)
{
   char param[512],ip[512],s1[512],prt[81],nk[10];
   FILE *f;
   int atr, ok=0,i,j;
   
   modcontext;
   if ((strcasecmp(code,"send")!=0)) return 0;
   /* dcc chat or send! */
   nsplit(param,msg); 
   nsplit(ip,msg);
   nsplit(prt,msg);
   sprintf(s1,"%s!%s",nick,from);
   atr=get_attr_host(s1);
   get_handle_by_host(nk,s1);
   if (atr & (USER_MASTER|USER_XFER)) ok=1;
   if (!ok) {
#ifndef QUIET_REJECTION
      modprintf(DP_HELP,"NOTICE %s :I don't accept files from strangers. :)\n",
	      nick);
#endif
      putlog(LOG_FILES,"*","Refused DCC SEND %s (no access): %s!%s",param,
	     nick,from);
      return 1;
   }
   if ((dccin[0]==0) && (!upload_to_cd)) {
      modprintf(DP_HELP,"NOTICE %s :DCC file transfers not supported.\n",nick);
      putlog(LOG_FILES,"*","Refused dcc send %s from %s!%s",param,nick,from);
      return 1;
   }
   if ((strchr(param,'/')!=NULL)) {
      modprintf(DP_HELP,"NOTICE %s :Filename cannot have '/' in it...\n",nick);
      putlog(LOG_FILES,"*","Refused dcc send %s from %s!%s",param,nick,from);
      return 1;
   }
   i = new_fork(DCC_SEND);
   if (i < 0) {
      modprintf(DP_HELP,"NOTICE %s :Sorry, too many DCC connections.\n",nick);
      putlog(LOG_MISC,"*","DCC connections full: %s %s (%s!%s)",code,param,
	     nick,from);
      return 1;
   }
   dcc[i].addr=my_atoul(ip);
   dcc[i].port=atoi(prt);
   dcc[i].sock=(-1);
   strcpy(dcc[i].nick,nick);
   strcpy(dcc[i].host,from);
   if (param[0]=='.') param[0]='_';
   strncpy(dcc[i].u.fork->u.xfer->filename,param,120);
   dcc[i].u.fork->u.xfer->filename[120]=0;
   if (upload_to_cd) {
      get_handle_dccdir(nk,s1);
      sprintf(dcc[i].u.fork->u.xfer->dir,"%s%s/",dccdir,s1);
   } else 
     strcpy(dcc[i].u.fork->u.xfer->dir,dccin);
   dcc[i].u.fork->u.xfer->length=atol(msg);
   dcc[i].u.fork->u.xfer->sent=0;
   dcc[i].u.fork->u.xfer->sofar=0;
   if (atol(msg)==0) {
      modprintf(DP_HELP,"NOTICE %s :Sorry, file size info must be included.\n",
	      nick);
      putlog(LOG_FILES,"*","Refused dcc send %s (%s): no file size",param,
	     nick);
      lostdcc(i);
      return 1;
   }
   if (atol(msg) > (dcc_maxsize*1024)) {
      modprintf(DP_HELP,"NOTICE %s :Sorry, file too large.\n",nick);
      putlog(LOG_FILES,"*","Refused dcc send %s (%s): file too large",param,
	     nick);
      lostdcc(i);
      return 1;
   }
   sprintf(s1,"%s%s",dcc[i].u.fork->u.xfer->dir,param);
   f=fopen(s1,"r"); 
   if (f!=NULL) {
      fclose(f);
      modprintf(DP_HELP,"NOTICE %s :That file already exists.\n",nick);
      lostdcc(i);
      return 1;
   }
   /* check for dcc-sends in process with the same filename */
   for (j=0; j<dcc_total; j++) if (j != i) {
      if (dcc[j].type==DCC_SEND) {
	 if (strcmp(param,dcc[j].u.xfer->filename)==0) {
	    modprintf(DP_HELP,"NOTICE %s :That file is already being sent.\n",nick);
	    lostdcc(i);
	    return 1;
	 }
      } else if ((dcc[j].type==DCC_FORK) && (dcc[j].u.fork->type==DCC_SEND)) {
	 if (strcmp(param,dcc[j].u.fork->u.xfer->filename)==0) {
	    modprintf(DP_HELP,"NOTICE %s :That file is already being sent.\n",nick);
	    lostdcc(i);
	    return 1;
	 }
      }
   }
   /* put uploads in /tmp first */
   sprintf(s1,"%s%s",tempdir,param);
   dcc[i].u.fork->u.xfer->f=fopen(s1,"w");
   if (dcc[i].u.fork->u.xfer->f==NULL) {
      modprintf(DP_HELP,"NOTICE %s :Can't create that file (temp dir error)\n",
	      nick);
      lostdcc(i);
   } else {
      dcc[i].u.fork->start=time(NULL);
      dcc[i].sock=getsock(SOCK_BINARY);   /* doh. */
      if (open_telnet_dcc(dcc[i].sock,ip,prt) < 0) {
	 /* can't connect (?) */
	 filesys_eof_hook(i); 
      }
   }
   return 1;
}
#endif
   
static int filesys_cont_got_dcc PROTO1(int,idx)
{
   char s1[121];
   
   if (dcc[idx].type != DCC_SEND)
     return 0;
   sprintf(s1,"%s!%s",dcc[idx].nick,dcc[idx].host);
   if (strcmp(dcc[idx].nick,"*users")!=0) {
      putlog(LOG_MISC,"*","DCC connection: SEND %s (%s)",
	     dcc[idx].type==DCC_SEND?dcc[idx].u.xfer->filename:"",
	     s1);
   }
   return 1;
}

static cmd_t mydcc[] = {
     { "files", '-', cmd_files },
     { "filestats", 'o', cmd_filestats },
     { 0, 0, 0 }
};

static tcl_strings mystrings[] = {
     { "files-path",    dccdir,      120, STR_DIR|STR_PROTECT },
     { "incoming-path", dccin,       120, STR_DIR|STR_PROTECT },
     { "filedb-path",   filedb_path, 120, STR_DIR|STR_PROTECT },
     { 0, 0, 0, 0 }
};

static tcl_ints myints[] = {
     { "max-dloads",     &dcc_limit },
     { "dcc-block",      &dcc_block },
     { "max-filesize",   &dcc_maxsize },
     { "max-file-users", &dcc_users },
     { "upload-to-pwd",  &upload_to_cd },
     { "copy-to-tmp",    &copy_to_tmp },
     { "xfer-timeout",   &wait_dcc_xfer },
     { 0, 0 }
};

extern cmd_t myfiles[];
extern tcl_cmds mytcls[];

static char * filesys_close () {
   int i;
   
   modcontext;
   putlog(LOG_MISC,"*","Unloading filesystem, killing all filesystem connections..");
   for (i=0;i<dcc_total;i++)
     if (dcc[i].type == DCC_FILES) {
	do_boot(i,(char *)botnetnick,"file system closing down");
     } else {
	filesys_eof_hook(i); 
	/* simulates an EOF on every filesys type connection*/
     }
   del_hook(HOOK_ACTIVITY,filesys_activity_hook);
   del_hook(HOOK_EOF,filesys_eof_hook);
   del_hook(HOOK_TIMEOUT,filesys_timeout);
   del_hook(HOOK_GOT_DCC,filesys_gotdcc);
   del_hook(HOOK_CONNECT,filesys_cont_got_dcc);
   del_hook(HOOK_REMOTE_FILEREQ,remote_filereq);
   del_hook(HOOK_RAW_DCC,raw_dcc_send);
   rem_tcl_commands(mytcls);
   rem_tcl_strings(mystrings);
   rem_tcl_ints(myints);
   rem_builtins(BUILTIN_DCC,mydcc);
   rem_builtins(BUILTIN_FILES,myfiles);
   module_undepend(MODULE_NAME,"eggdrop");
   return NULL;
}

static int filesys_expmem() {
   return expmem_fileq();
}

static void filesys_report PROTO1(int,idx) {
   if (dccdir[0]) {
      modprintf(idx,"   DCC file path: %s",dccdir);
      if (upload_to_cd) 
	modprintf(idx,"\n        incoming: (go to the current dir)\n");
      else if (dccin[0]) 
	modprintf(idx,"\n        incoming: %s\n",dccin);
      else 
	modprintf(idx,"    (no uploads)\n");
      modprintf(idx,"   DCC block is %d%s, max concurrent d/ls is %d\n",dcc_block,
	      (dcc_block==0)?" (turbo dcc)":"",dcc_limit);
      if (dcc_users) 
	modprintf(idx,"       max users is %d\n",dcc_users);
      if ((upload_to_cd) || (dccin[0]))
	modprintf(idx,"   DCC max file size: %dk\n",dcc_maxsize);
      modprintf(idx,"   Using %d bytes of memory\n",filesys_expmem());
   } else 
     modprintf(idx,"  (Filesystem module loaded, but no active dcc path.)\n");
}

module_function * global;
char * filesys_start PROTO((module_function *));
   
module_function filesys_table[] = {
   (module_function)filesys_start,
   (module_function)filesys_close,
   (module_function)filesys_expmem,
   (module_function)filesys_report,
};

char * filesys_start PROTO1(module_function *,egg_func_table) {
   global = egg_func_table;
   module_register(MODULE_NAME,filesys_table,1,0);
   module_depend(MODULE_NAME,"eggdrop",101,0);
   add_hook(HOOK_ACTIVITY,filesys_activity_hook);
   add_hook(HOOK_EOF,filesys_eof_hook);
   add_hook(HOOK_TIMEOUT,filesys_timeout);
   add_hook(HOOK_GOT_DCC,filesys_gotdcc);
   add_hook(HOOK_CONNECT,filesys_cont_got_dcc);
   add_hook(HOOK_REMOTE_FILEREQ,remote_filereq);
   add_hook(HOOK_RAW_DCC,raw_dcc_send);
   add_tcl_commands(mytcls);
   add_tcl_strings(mystrings);
   add_tcl_ints(myints);
   add_builtins(BUILTIN_DCC,mydcc);
   add_builtins(BUILTIN_FILES,myfiles);
   return NULL;
}
#endif
