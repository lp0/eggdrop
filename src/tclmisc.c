/*
   tclmisc.c -- handles:
     Tcl stubs for file system commands
     Tcl stubs for everything else

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
#include <sys/stat.h>
#include "eggdrop.h"
#include "proto.h"
#include "cmdt.h"
#include "tclegg.h"
#include "files.h"

/* eggdrop always uses the same interpreter */
extern Tcl_Interp *interp;
extern int serv;
extern tcl_timer_t *timer,*utimer;
extern struct dcc_t dcc[];
extern int dcc_total;
extern char dccdir[];

/***********************************************************************/

int tcl_putserv STDVAR
{
  char s[512],*p;
  BADARGS(2,2," text");
  strncpy(s,argv[1],511); s[511]=0;
  p=strchr(s,'\n'); if (p!=NULL) *p=0;
  p=strchr(s,'\r'); if (p!=NULL) *p=0;
  mprintf(serv,"%s\n",s); return TCL_OK;
}

int tcl_puthelp STDVAR
{
  char s[512],*p;
  BADARGS(2,2," text");
  strncpy(s,argv[1],511); s[511]=0;
  p=strchr(s,'\n'); if (p!=NULL) *p=0;
  p=strchr(s,'\r'); if (p!=NULL) *p=0;
  hprintf(serv,"%s\n",s); return TCL_OK;
}

int tcl_putlog STDVAR
{
  char logtext[501];
  BADARGS(2,2," text");
  strncpy(logtext,argv[1],500); logtext[500]=0;
  putlog(LOG_MISC,"*","%s",logtext);
  return TCL_OK;
}

int tcl_putcmdlog STDVAR
{
  char logtext[501];
  BADARGS(2,2," text");
  strncpy(logtext,argv[1],500); logtext[500]=0;
  putlog(LOG_CMDS,"*","%s",logtext);
  return TCL_OK;
}

int tcl_putxferlog STDVAR
{
  char logtext[501];
  BADARGS(2,2," text");
  strncpy(logtext,argv[1],500); logtext[500]=0;
  putlog(LOG_FILES,"*","%s",logtext);
  return TCL_OK;
}

int tcl_putloglev STDVAR
{
  int lev=0; char logtext[501];
  BADARGS(4,4," level channel text");
  lev=logmodes(argv[1]);
  if (lev==0) {
    Tcl_AppendResult(irp,"no valid log-level given",NULL);
    return TCL_ERROR;
  }
  strncpy(logtext,argv[3],500); logtext[500]=0;
  putlog(lev,argv[2],"%s",logtext);
  return TCL_OK;
}

int tcl_bind STDVAR
{
  int fl,tp;
  if ((long int)cd==1) {
     BADARGS(5,5," type flags cmd/mask procname")
  } else {
     BADARGS(4,5," type flags cmd/mask ?procname?")
  }
  fl=str2flags(argv[2]);
  tp=get_bind_type(argv[1]);
  if (tp<0) {
    Tcl_AppendResult(irp,"bad type, should be one of: dcc, msg, fil, pub, ",
		     "msgm, pubm, join, part, sign, kick, topc, mode, ctcp, ",
		     "nick, bot, chon, chof, sent, rcvd, chat, link, disc, ",
		     "splt, rejn, filt, raw, wall, chjn, chpt, bcst, time",
                     NULL);
    return TCL_ERROR;
  }
  if ((long int)cd==1) {
    if (!cmd_unbind(tp,fl,argv[3],argv[4])) {
      /* don't error if trying to re-unbind a builtin */
      if ((strcmp(argv[3],&argv[4][5])!=0) || (argv[4][0]!='*') ||
	  (strncmp(argv[1],&argv[4][1],3)!=0) || (argv[4][4]!=':')) {
        Tcl_AppendResult(irp,"no such binding",NULL);
        return TCL_ERROR;
      }
    }
  }
  else {
    if (argc==4) return tcl_getbinds(tp,argv[3]);
    cmd_bind(tp,fl,argv[3],argv[4]);
  }
  Tcl_AppendResult(irp,argv[3],NULL);
  return TCL_OK;
}

int tcl_timer STDVAR
{
  unsigned long x; char s[41];
  BADARGS(3,3," minutes command");
  if (atoi(argv[1]) < 0) {
    Tcl_AppendResult(irp,"time value must be positive",NULL);
    return TCL_ERROR;
  }
  if (argv[2][0]!='#') {
    x=add_timer(&timer,atoi(argv[1]),argv[2],0L);
    sprintf(s,"timer%lu",x); Tcl_AppendResult(irp,s,NULL);
  }
  return TCL_OK;
}

int tcl_utimer STDVAR
{
  unsigned long x; char s[41];
  BADARGS(3,3," seconds command");
  if (atoi(argv[1]) < 0) {
    Tcl_AppendResult(irp,"time value must be positive",NULL);
    return TCL_ERROR;
  }
  if (argv[2][0]!='#') {
    x=add_timer(&utimer,atoi(argv[1]),argv[2],0L);
    sprintf(s,"timer%lu",x); Tcl_AppendResult(irp,s,NULL);
  }
  return TCL_OK;
}

int tcl_killtimer STDVAR
{
  BADARGS(2,2," timerID");
  if (strncmp(argv[1],"timer",5)!=0) {
    Tcl_AppendResult(irp,"argument is not a timerID",NULL);
    return TCL_ERROR;
  }
  if (remove_timer(&timer,atol(&argv[1][5]))) return TCL_OK;
  Tcl_AppendResult(irp,"invalid timerID",NULL);
  return TCL_ERROR;
}

int tcl_killutimer STDVAR
{
  BADARGS(2,2," timerID");
  if (strncmp(argv[1],"timer",5)!=0) {
    Tcl_AppendResult(irp,"argument is not a timerID",NULL);
    return TCL_ERROR;
  }
  if (remove_timer(&utimer,atol(&argv[1][5]))) return TCL_OK;
  Tcl_AppendResult(irp,"invalid timerID",NULL);
  return TCL_ERROR;
}

int tcl_unixtime STDVAR
{
  char s[20]; time_t t;
  BADARGS(1,1,"");
  t=time(NULL); sprintf(s,"%lu",(unsigned long)t);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_time STDVAR
{
  char s[81]; time_t t;
  BADARGS(1,1,"");
  t=time(NULL); strcpy(s,ctime(&t));
  strcpy(s,&s[11]); s[5]=0;
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_date STDVAR
{
  char s[81]; time_t t;
  BADARGS(1,1,"");
  t=time(NULL); strcpy(s,ctime(&t));
  s[10]=s[24]=0; strcpy(s,&s[8]); strcpy(&s[8],&s[20]);
  strcpy(&s[2],&s[3]);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_timers STDVAR
{
  BADARGS(1,1,"");
  list_timers(irp,timer);
  return TCL_OK;
}

int tcl_utimers STDVAR
{
  BADARGS(1,1,"");
  list_timers(irp,utimer);
  return TCL_OK;
}

int tcl_ctime STDVAR
{
  time_t tt; char s[81];
  BADARGS(2,2," unixtime");
  tt=(time_t)atol(argv[1]);
  strcpy(s,ctime(&tt)); s[strlen(s)-1]=0;
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_myip STDVAR
{
  char s[21];
  BADARGS(1,1,"");
  sprintf(s,"%lu",iptolong(getmyip()));
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_rand STDVAR
{
  unsigned long x; char s[41];
  BADARGS(2,2," limit");
  if (atol(argv[1])<=0) {
    Tcl_AppendResult(irp,"random limit must be greater than zero",NULL);
    return TCL_ERROR;
  }
  x=random()%(atol(argv[1]));
  sprintf(s,"%lu",x);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_sendnote STDVAR
{
  char s[5],from[21],to[21],msg[451];
  BADARGS(4,4," from to message");
  strncpy(from,argv[1],20); from[20]=0;
  strncpy(to,argv[2],20); to[20]=0;
  strncpy(msg,argv[3],450); msg[450]=0;
  sprintf(s,"%d",add_note(to,from,msg,-1,0));
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_encrypt STDVAR
{
  char *p;
  BADARGS(3,3," key string");
  p=encrypt_string(argv[1],argv[2]);
  Tcl_AppendResult(irp,p,NULL);
  nfree(p); return TCL_OK;
}

int tcl_decrypt STDVAR
{
  char *p;
  BADARGS(3,3," key string");
  p=decrypt_string(argv[1],argv[2]);
  Tcl_AppendResult(irp,p,NULL);
  nfree(p); return TCL_OK;
}

int tcl_dumpfile STDVAR
{
  char nick[NICKLEN],fn[81];
  BADARGS(3,3," nickname filename");
  strncpy(nick,argv[1],NICKLEN-1); nick[NICKLEN-1]=0;
  strncpy(fn,argv[2],80); fn[80]=0;
  showtext(argv[1],argv[2],0);
  return TCL_OK;
}

int tcl_dccdumpfile STDVAR
{
  char fn[81]; int idx,i,atr;
  BADARGS(3,3," idx filename");
  strncpy(fn,argv[2],80); fn[80]=0;
  i=atoi(argv[1]); idx=findidx(i); if (idx<0) {
    Tcl_AppendResult(irp,"illegal idx",NULL);
    return TCL_ERROR;
  }
  atr=get_attr_handle(dcc[idx].nick);
  telltext(idx,fn,atr);
  return TCL_OK;
}

int tcl_backup STDVAR
{
  BADARGS(1,1,"");
  backup_userfile();
  return TCL_OK;
}

int tcl_die STDVAR
{
  BADARGS(1,2," ?reason?");
  if (argc==2) fatal(argv[1],0);
  else fatal("EXIT",0);
  /* should never return, but, to keep gcc happy: */
  return TCL_OK;
}

int tcl_strftime STDVAR
{
  char buf[512];
  struct tm *tm1;
  time_t t;
  BADARGS(2,3, " format ?time?");
  if (argc==3) t=atol(argv[2]);
  else t=time(NULL);
  tm1=localtime(&t);
  if (strftime(buf, sizeof(buf)-1, argv[1], tm1)) {
    Tcl_AppendResult(irp, buf, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp," error with strftime" , NULL);
  return TCL_ERROR;
}

/***** FILE SYSTEM *****/

#ifndef NO_FILE_SYSTEM

int tcl_getfileq STDVAR
{
  BADARGS(2,2," handle");
  tcl_get_queued(irp,argv[1]);
  return TCL_OK;
}

int tcl_getdesc STDVAR
{
  char s[301];
  BADARGS(3,3," dir file");
  filedb_getdesc(argv[1],argv[2],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_setdesc STDVAR
{
  BADARGS(4,4," dir file desc");
  filedb_setdesc(argv[1],argv[2],argv[3]);
  return TCL_OK;
}

int tcl_getowner STDVAR
{
  char s[121];
  BADARGS(3,3," dir file");
  filedb_getowner(argv[1],argv[2],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_setowner STDVAR
{
  BADARGS(4,4," dir file owner");
  filedb_setowner(argv[1],argv[2],argv[3]);
  return TCL_OK;
}

int tcl_getgots STDVAR
{
  int i; char s[10];
  BADARGS(3,3," dir file");
  i=filedb_getgots(argv[1],argv[2]);
  sprintf(s,"%d",i);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_setlink STDVAR
{
  BADARGS(4,4," dir file link");
  filedb_setlink(argv[1],argv[2],argv[3]);
  return TCL_OK;
}

int tcl_getlink STDVAR
{
  char s[121];
  BADARGS(3,3," dir file");
  filedb_getlink(argv[1],argv[2],s);
  Tcl_AppendResult(irp,s,NULL);
  return TCL_OK;
}

int tcl_setpwd STDVAR
{
  int i,idx;
  BADARGS(3,3," idx dir");
  i=atoi(argv[1]); idx=findidx(i);
  if (idx<0) {
    Tcl_AppendResult(irp,"invalid idx",NULL);
    return TCL_ERROR;
  }
  if (dcc[idx].type!=DCC_FILES) {
    Tcl_AppendResult(irp,"invalid idx",NULL);
    return TCL_ERROR;
  }
  files_setpwd(idx,argv[2]);
  return TCL_OK;
}

int tcl_getpwd STDVAR
{
  int i,idx;
  BADARGS(2,2," idx");
  i=atoi(argv[1]); idx=findidx(i);
  if (idx<0) {
    Tcl_AppendResult(irp,"invalid idx",NULL);
    return TCL_ERROR;
  }
  if (dcc[idx].type!=DCC_FILES) {
    Tcl_AppendResult(irp,"invalid idx",NULL);
    return TCL_ERROR;
  }
  Tcl_AppendResult(irp,dcc[idx].u.file->dir,NULL);
  return TCL_OK;
}

int tcl_getfiles STDVAR
{
  BADARGS(2,2," dir");
  filedb_getfiles(irp,argv[1]);
  return TCL_OK;
}

int tcl_getdirs STDVAR
{
  BADARGS(2,2," dir");
  filedb_getdirs(irp,argv[1]);
  return TCL_OK;
}

int tcl_hide STDVAR
{
  BADARGS(3,3," dir file");
  filedb_change(argv[1],argv[2],FILEDB_HIDE);
  return TCL_OK;
}

int tcl_unhide STDVAR
{
  BADARGS(3,3," dir file");
  filedb_change(argv[1],argv[2],FILEDB_UNHIDE);
  return TCL_OK;
}

int tcl_share STDVAR
{
  BADARGS(3,3," dir file");
  filedb_change(argv[1],argv[2],FILEDB_SHARE);
  return TCL_OK;
}

int tcl_unshare STDVAR
{
  BADARGS(3,3," dir file");
  filedb_change(argv[1],argv[2],FILEDB_UNSHARE);
  return TCL_OK;
}

int tcl_setflags STDVAR
{
  FILE * f; filedb * fdb; int where; char s[512], *p,*d;
  BADARGS(2,3," dir ?flags?");
  strcpy(s,argv[1]);
  if (s[strlen(s)-1]=='/') s[strlen(s)-1]=0;
  p = strrchr(s,'/');
  if (p == NULL) {
     p = s;
     d = "";
  } else {
    *p = 0;
    p++;
    d = s;
  }
  f = filedb_open(d);
  fdb = findfile(f,p,&where);
  if (fdb == NULL) 
    Tcl_AppendResult(irp,"-1",NULL); /* no such dir */
  else if (!(fdb->stat&FILE_DIR)) 
    Tcl_AppendResult(irp,"-2",NULL); /* not a dir */
  else if (argc==3)
    fdb->flags_req=str2flags(argv[2]);
  else 
    fdb->flags_req=0;
  fseek(f,where,SEEK_SET);
  fwrite(fdb,sizeof(filedb),1,f);
  filedb_close(f);
  Tcl_AppendResult(irp,"0",NULL);
  return TCL_OK;
}

int tcl_getflags STDVAR
{
  FILE * f; filedb * fdb; int where; char s[512], *p,*d;
  BADARGS(2,2," dir");
  strcpy(s,argv[1]);
  if (s[strlen(s)-1]=='/') s[strlen(s)-1]=0;
  p = strrchr(s,'/');
  if (p == NULL) {
     p = s;
     d = "";
  } else {
    *p = 0;
    p++;
    d = s;
  }
  f = filedb_open(d);
  fdb = findfile(f,p,&where);
  if (fdb == NULL) 
    Tcl_AppendResult(irp,"",NULL); /* no such dir */
  else if (!(fdb->stat&FILE_DIR)) 
    Tcl_AppendResult(irp,"",NULL); /* not a dir */
  else {
    flags2str(fdb->flags_req,s);
    if (s[0] == '-')
      s[0] = 0;
    Tcl_AppendResult(irp,s,NULL);
  }
  filedb_close(f);
  return TCL_OK;
}

int tcl_mkdir STDVAR
{
  char s[512],t[512],*d,*p; FILE *f; filedb *fdb; long where;
  BADARGS(2,3," dir ?required-flags?");
  strcpy(s,argv[1]);
  if (s[strlen(s)-1]=='/') s[strlen(s)-1]=0;
  p = strrchr(s,'/');
  if (p == NULL) {
     p = s;
     d = "";
  } else {
    *p = 0;
    p++;
    d = s;
  }
  f=filedb_open(d);
  fdb=findfile(f,p,&where);
  if (fdb==NULL) {
    filedb x;
    sprintf(t,"%s%s/%s",dccdir,d,p);
    if (mkdir(t,0755)==0) {
      x.version=FILEVERSION; x.stat=FILE_DIR; x.desc[0]=0;
      x.uploader[0]=0; strcpy(x.filename,argv[1]); x.flags_req=0;
      x.uploaded=time(NULL); x.size=0; x.gots=0; x.sharelink[0]=0;
      Tcl_AppendResult(irp,"0",NULL);
      if (argc == 3) {
	x.flags_req=str2flags(argv[2]);
      }
      where=findempty(f); fseek(f,where,SEEK_SET);
      fwrite(&x,sizeof(filedb),1,f);
      filedb_close(f);
      return TCL_OK;
    }
    Tcl_AppendResult(irp,"1",NULL);
    filedb_close(f);
    return TCL_OK;
  }
  /* already exists! */
  if (!(fdb->stat&FILE_DIR)) {
    Tcl_AppendResult(irp,"2",NULL);
    filedb_close(f); return TCL_OK;
  }
  if (argc==3) {
    fdb->flags_req=str2flags(argv[2]);
  }
  else {
    fdb->flags_req=0;
  }
  Tcl_AppendResult(irp,"0",NULL);
  fseek(f,where,SEEK_SET);
  fwrite(fdb,sizeof(filedb),1,f);
  filedb_close(f);
  return TCL_OK;
}

int tcl_rmdir STDVAR
{
  FILE *f; filedb *fdb; long where; char s[256],t[512],*d,*p;
  BADARGS(2,2," dir");
  if (strlen(argv[1]) > 80) argv[1][80] = 0;
  strcpy(s,argv[1]);
  if (s[strlen(s)-1]=='/') s[strlen(s)-1]=0;
  p = strrchr(s,'/');
  if (p == NULL) {
     p = s;
     d = "";
  } else {
    *p = 0;
    p++;
    d = s;
  }
  f=filedb_open(d);
  fdb=findfile(f,p,&where);
  if (fdb==NULL) {
    Tcl_AppendResult(irp,"1",NULL);
    filedb_close(f); return TCL_OK;
  }
  if (!(fdb->stat & FILE_DIR)) {
    Tcl_AppendResult(irp,"1",NULL);
    filedb_close(f); return TCL_OK;
  }
  /* erase '.filedb' and '.files' if they exist */
  sprintf(t,"%s%s/%s/.filedb",dccdir,d,p); unlink(t);
  sprintf(t,"%s%s/%s/.files",dccdir,d,p); unlink(t);
  sprintf(t,"%s%s/%s",dccdir,d,p);
  if (rmdir(t)==0) {
    fdb->stat|=FILE_UNUSED;
    fseek(f,where,SEEK_SET);
    fwrite(fdb,sizeof(filedb),1,f);
    filedb_close(f);
    Tcl_AppendResult(irp,"0",NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp,"1",NULL);
  filedb_close(f);
  return TCL_OK;
}

int tcl_mv_cp PROTO4(Tcl_Interp *,irp,int,argc,char **,argv,int,copy)
{
  char *p,fn[161],oldpath[161],s[161],s1[161],newfn[161],newpath[161];
  int ok,only_first,skip_this; FILE *f,*g; filedb *fdb,x,*z;
  long where,gwhere,wherez;

  BADARGS(3,3," oldfilepath newfilepath");
  strcpy(fn,argv[1]);
  p=strrchr(fn,'/'); if (p!=NULL) {
    *p=0; strncpy(s,fn,160); s[160]=0; strcpy(fn,p+1);
    if (!resolve_dir("/",s,oldpath,USER_OWNER|USER_BOT)) { /* tcl can do
							    * anything */
      Tcl_AppendResult(irp,"-1",NULL); /* invalid source */
      return TCL_OK; 
    }
  }
  else strcpy(oldpath,"/");
  strncpy(s,argv[2],160); s[160]=0;
  if (!resolve_dir("/",s,newpath,USER_OWNER|USER_BOT)) {
    /* destination is not just a directory */
    p=strrchr(s,'/');
    if (p==NULL) { strcpy(newfn,s); s[0]=0; }
    else { *p=0; strcpy(newfn,p+1); }
    if (!resolve_dir("/",s,newpath,USER_OWNER|USER_BOT)) {
      Tcl_AppendResult(irp,"-2",NULL); /* invalid desto */
      return TCL_OK;
    }
  }
  else newfn[0]=0;
  /* stupidness checks */
  if ((strcmp(oldpath,newpath)==0) &&
      ((!newfn[0]) || (strcmp(newfn,fn)==0))) {
    Tcl_AppendResult(irp,"-3",NULL); /* stoopid copy to self */
    return TCL_OK;
  }
  /* be aware of 'cp * this.file' possibility: ONLY COPY FIRST ONE */
  if (((strchr(fn,'?')!=NULL) || (strchr(fn,'*')!=NULL)) && (newfn[0]))
    only_first=1;
  else only_first=0;
  f=filedb_open(oldpath);
  if (strcmp(oldpath,newpath)==0) g=NULL;
  else g=filedb_open(newpath);
  where=0L; ok=0;
  fdb=findmatch(f,fn,&where);
  if (fdb==NULL) {
    Tcl_AppendResult(irp,"-4",NULL); /* nomatch */
    filedb_close(f);
    if (g!=NULL) filedb_close(g);
    return TCL_OK;
  }
  while (fdb!=NULL) {
    skip_this=0;
    if (!(fdb->stat & (FILE_HIDDEN|FILE_DIR))) {
      sprintf(s,"%s%s%s%s",dccdir,oldpath,oldpath[0]?"/":"",fdb->filename);
      sprintf(s1,"%s%s%s%s",dccdir,newpath,newpath[0]?"/":"",newfn[0]?
	      newfn:fdb->filename);
      if (strcmp(s,s1)==0) {
	Tcl_AppendResult(irp,"-3",NULL); /* stoopid copy to self */
	skip_this=1;
      }
      /* check for existence of file with same name in new dir */
      if (g==NULL) z=findfile2(f,newfn[0]?newfn:fdb->filename,&wherez);
      else z=findfile2(g,newfn[0]?newfn:fdb->filename,&wherez);
      if (z!=NULL) {
	/* it's ok if the entry in the new dir is a normal file (we'll */
	/* just scrap the old entry and overwrite the file) -- but if */
	/* it's a directory, this file has to be skipped */
	if (z->stat & FILE_DIR) {
	  /* skip */
	  skip_this=1;
	}
	else {
	  z->stat|=FILE_UNUSED;
	  if (g==NULL) {
	    fseek(f,wherez,SEEK_SET);
	    fwrite(z,sizeof(filedb),1,f);
	  } 
	  else {
	    fseek(g,wherez,SEEK_SET);
	    fwrite(z,sizeof(filedb),1,g);
	  }
	}
      }
      if (!skip_this) {
	if ((fdb->sharelink[0]) || (copyfile(s,s1)==0)) {
	  /* raw file moved okay: create new entry for it */
	  ok++;
	  if (g==NULL) gwhere=findempty(f);
	  else gwhere=findempty(g);
	  x.version=FILEVERSION; x.stat=fdb->stat; x.flags_req=0;
	  strcpy(x.filename,fdb->filename); strcpy(x.desc,fdb->desc);
	  if (newfn[0]) strcpy(x.filename,newfn);
	  strcpy(x.uploader,fdb->uploader); x.uploaded=fdb->uploaded;
	  x.size=fdb->size; x.gots=fdb->gots;
	  strcpy(x.sharelink,fdb->sharelink);
	  if (g==NULL) {
	    fseek(f,gwhere,SEEK_SET);
	    fwrite(&x,sizeof(filedb),1,f);
	  }
	  else {
	    fseek(g,gwhere,SEEK_SET);
	    fwrite(&x,sizeof(filedb),1,g);
	  }
	  if (!copy) {
	    unlink(s);
	    fdb->stat|=FILE_UNUSED;
	    fseek(f,where,SEEK_SET);
	    fwrite(fdb,sizeof(filedb),1,f);
	  }
	}
      }
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,fn,&where);
    if ((ok) && (only_first)) fdb=NULL;
  }
  if (!ok)
    Tcl_AppendResult(irp,"-4",NULL); /* nomatch */
  else {
    char x[30];
    sprintf(x,"%d",ok);
    Tcl_AppendResult(irp,x,NULL);
  }
  filedb_close(f);
  if (g!=NULL) filedb_close(g);
  return TCL_OK;
}

int tcl_mv STDVAR
{
  return tcl_mv_cp(irp,argc,argv,0);
}

int tcl_cp STDVAR
{
  return tcl_mv_cp(irp,argc,argv,1);
}
#endif
