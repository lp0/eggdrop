/*
   files.c -- handles:
     all the file system commands

   dprintf'ized, 4nov95
   rewritten, 26feb96
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "eggdrop.h"
#include "files.h"
#include "proto.h"

/* 'configure' is supposed to make things easier for me now */
/* PLEASE don't fail me, 'configure'! :)  */

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/* goddamn stupid sunos 4 */
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif

extern struct dcc_t dcc[];
extern int dcc_total;
extern char dccdir[];
extern char dccin[];
extern char botname[];
extern struct userrec *userlist;
extern char botnetnick[];

#ifndef NO_FILE_SYSTEM
/* maximum number of users can be in the file area at once */
int dcc_users=0;
/* maximum number of simultaneous file downloads allowed */
int dcc_limit=3;
#endif

/* low-level stuff for other modules */
int is_file(char *s)
{
  struct stat ss;
  int i=stat(s,&ss);
  if (i<0) return 0;
  if ((ss.st_mode&S_IFREG) || (ss.st_mode&S_IFLNK)) return 1;
  return 0;
}

#ifndef NO_FILE_SYSTEM

/* return true if this user has >= the maximum number of file xfers going */
int at_limit(char *nick)
{
  int i,x=0;
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_GET) || (dcc[i].type==DCC_GET_PENDING))
      if (strcasecmp(dcc[i].nick,nick)==0) x++;
  return (x>=dcc_limit);
}

/* are there too many people in the file system? */
int too_many_filers()
{
  int i,n=0;
  if (dcc_users==0) return 0;
  for (i=0; i<dcc_total; i++) if (dcc[i].type==DCC_FILES) n++;
  return (n>=dcc_users);
}

/* someone uploaded a file -- add it */
void add_file(char *dir,char *file,char *nick)
{
  FILE *f;
  /* gave me a full pathname */
  /* only continue if the destination is within the visible file system */
  if (strncmp(dccdir,dir,strlen(dccdir))!=0) return;
  f=filedb_open(&dir[strlen(dccdir)]);
  if (f==NULL) return;
  filedb_add(f,file,nick);
  filedb_close(f);
}

int welcome_to_files(int idx)
{
  int atr=get_attr_handle(dcc[idx].nick); FILE *f;
  dprintf(idx,"\n");
  if (atr&USER_JANITOR) atr|=USER_MASTER;
  /* show motd if the user went straight here without going thru the party
     line */
  if (!(dcc[idx].u.file->chat->status&STAT_CHAT))
    show_motd(idx);
  telltext(idx,"files",atr);
  get_handle_dccdir(dcc[idx].nick,dcc[idx].u.file->dir);
  /* does this dir even exist any more? */
  f=filedb_open(dcc[idx].u.file->dir);
  if (f==NULL) {
    dcc[idx].u.file->dir[0]=0;
    f=filedb_open(dcc[idx].u.file->dir);
    if (f==NULL) {
      dprintf(idx,FILES_BROKEN);
      dprintf(idx,FILES_INVPATH);
      dprintf(idx,"\n\n");
      dccdir[0]=0;
      chanout2(dcc[idx].u.file->chat->channel,"%s rejoined the party line.\n",
	       dcc[idx].nick);
      return 0;    /* failed */
    }
  }
  filedb_close(f);
  dprintf(idx,"%s: /%s\n\n",FILES_CURDIR,dcc[idx].u.file->dir);
  return 1;
}

/* given current directory, and the desired changes, fill 'real' with */
/* the new current directory.  check directory parmissions along the */
/* way.  return 1 if the change can happen, 0 if not. */
int resolve_dir(current,change,real,atr)
char *current,*change,*real; int atr;
{
  char elem[512],s[1024],new[1024],*p; FILE *f; filedb *fdb;
  strcpy(real,current); strcpy(new,change);
  if (!new[0]) return 1;      /* no change? */
  if (new[0]=='/') {
    /* EVERYONE has access here */
    real[0]=0; strcpy(new,&new[1]);
  }
  /* cycle thru the elements */
  strcat(new,"/"); p=strchr(new,'/');
  while (p!=NULL) {
    *p=0; p++; strcpy(elem,new); strcpy(new,p);
    if ((strcmp(elem,".")==0) || (!elem[0])) {  /* do nothing */  }
    else if (strcmp(elem,"..")==0) {   /* go back */
      /* always allowed */
      p=strrchr(real,'/');
      if (p==NULL) {
	/* can't go back from here? */
        if (!real[0]) {
          strcpy(real,current);
          return 0;
	}
        real[0]=0;
      }
      else *p=0;
    }
    else {
      /* allowed access here? */
      f=filedb_open(real);
      if (f==NULL) {
	/* non-existent starting point! */
	strcpy(real,current);
	return 0;
      }
      fdb=findfile(f,elem,NULL);
      filedb_close(f);
      if (fdb==NULL) {
	/* non-existent */
	strcpy(real,current);
	return 0;
      }
      if (!(fdb->stat & FILE_DIR)) {
	/* not a dir */
	strcpy(real,current);
	return 0;
      }
      if (!flags_ok(fdb->flags_req,atr)) {
        strcpy(real,current);
        return 0;
      }
      strcpy(s,real);
      if (s[0]) if (s[strlen(s)-1]!='/') strcat(s,"/");
      sprintf(real,"%s%s",s,elem);
      sprintf(s,"%s%s",dccdir,real);
    }
    p=strchr(new,'/');
  }
  /* sanity check: does this dir exist? */
  sprintf(s,"%s%s",dccdir,real);
  f=fopen(s,"r");
  if (f==NULL) return 0;
  fclose(f);
  return 1;
}

void incr_file_gots(char *ppath)
{
  char *p,path[256],destdir[121],fn[81]; filedb *fdb; FILE *f; long where;
  /* absolute dir?  probably a tcl script sending it, and it might not */
  /* be in the file system at all, so just leave it alone */
  if ((ppath[0]=='*') || (ppath[0]=='/')) return;
  strcpy(path,ppath);
  p=strrchr(path,'/');
  if (p!=NULL) {
    *p=0; strncpy(destdir,path,120); destdir[120]=0;
    strncpy(fn,p+1,80); fn[80]=0; *p='/';
  }
  else {
    destdir[0]=0;
    strncpy(fn,path,80); fn[80]=0;
  }
  f=filedb_open(destdir);
  if (f==NULL) return;   /* not my concern, then */
  fdb=findfile(f,fn,&where);
  if (fdb==NULL) {
    /* file is gone now */
    filedb_close(f);
    return;
  }
  fdb->gots++;
  fseek(f,where,SEEK_SET);
  fwrite(fdb,sizeof(filedb),1,f);
  filedb_close(f);
}

/*** COMMANDS ***/

void cmd_pwd(idx,par)
int idx; char *par;
{
  putlog(LOG_FILES,"*","files: #%s# pwd",dcc[idx].nick);
  dprintf(idx,"%s: /%s\n",FILES_CURDIR,dcc[idx].u.file->dir);
}

void cmd_pending(idx,par)
int idx; char *par;
{
  show_queued_files(idx);
  putlog(LOG_FILES,"*","files: #%s# pending",dcc[idx].nick);
}

void cmd_cancel(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"%s: cancel <file-mask>\n",USAGE);
    return;
  }
  fileq_cancel(idx,par);
  putlog(LOG_FILES,"*","files: #%s# cancel %s",dcc[idx].nick,par);
}

void cmd_chdir(idx,msg)
int idx; char *msg;
{
  char s[121]; int atr;
  if (!msg[0]) {
    dprintf(idx,"%s: cd <new-dir>\n",USAGE);
    return;
  }
  atr=get_attr_handle(dcc[idx].nick);
  if (!resolve_dir(dcc[idx].u.file->dir,msg,s,atr)) {
    dprintf(idx,FILES_NOSUCHDIR);
    return;
  }
  strcpy(dcc[idx].u.file->dir,s);
  set_handle_dccdir(userlist,dcc[idx].nick,dcc[idx].u.file->dir);
  putlog(LOG_FILES,"*","files: #%s# cd /%s",dcc[idx].nick,
         dcc[idx].u.file->dir);
  dprintf(idx,"%s: /%s\n",FILES_NEWCURDIR,dcc[idx].u.file->dir);
}

void files_ls(idx,par,showall)
int idx; char *par; int showall;
{
  char *p,s[DIRLEN],destdir[DIRLEN],mask[81]; int atr; FILE *f;
  atr=get_attr_handle(dcc[idx].nick);
  if (par[0]) {
    putlog(LOG_FILES,"*","files: #%s# ls %s",dcc[idx].nick,par);
    p=strrchr(par,'/'); if (p!=NULL) {
      *p=0; strncpy(s,par,DIRLEN); s[DIRLEN-1]=0;
      strncpy(mask,p+1,80); mask[80]=0;
      if (!resolve_dir(dcc[idx].u.file->dir,s,destdir,atr)) {
        dprintf(idx,FILES_ILLDIR);
        return;
      }
    }
    else { 
      strcpy(destdir,dcc[idx].u.file->dir);
      strncpy(mask,par,80); mask[80]=0;
    }
    /* might be 'ls dir'? */
    if (resolve_dir(destdir,mask,s,atr)) {
      /* aha! it was! */
      strcpy(destdir,s); strcpy(mask,"*");
    }
    f=filedb_open(destdir);
    filedb_ls(f,idx,atr,mask,showall);
    filedb_close(f);
  }
  else {
    putlog(LOG_FILES,"*","files: #%s# ls",dcc[idx].nick);
    f=filedb_open(dcc[idx].u.file->dir);
    filedb_ls(f,idx,atr,"*",showall);
    filedb_close(f);
  }
}

void cmd_ls(idx,par)
int idx; char *par;
{
  files_ls(idx,par,0);
}
void cmd_lsa(idx,par)
int idx; char *par;
{
  files_ls(idx,par,1);
}
  
void cmd_get(idx,par)
int idx; char *par;
{
  int atr,ok=0,i; char *p,what[512],destdir[121],s[256];
  filedb *fdb; FILE *f; long where;
  if (!par[0]) {
    dprintf(idx,"%s: get <file(s)> [nickname]\n",USAGE);
    return;
  }
  atr=get_attr_handle(dcc[idx].nick);
  nsplit(what,par);   /* anything left in par is a new nickname */
  if (strlen(par)>NICKLEN) {
    dprintf(idx,FILES_BADNICK);
    return;
  }
  p=strrchr(what,'/'); if (p!=NULL) {
    *p=0; strncpy(s,what,120); s[120]=0;  strcpy(what,p+1);
    if (!resolve_dir(dcc[idx].u.file->dir,s,destdir,atr)) {
      dprintf(idx,FILES_ILLDIR);
      return;
    }
  }
  else strcpy(destdir,dcc[idx].u.file->dir);
  f=filedb_open(destdir);
  where=0L;
  fdb=findmatch(f,what,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (!(fdb->stat&(FILE_HIDDEN|FILE_DIR))) {
      ok=1;
      if (fdb->sharelink[0]) {
	char bot[121],whoto[NICKLEN];
	/* this is a link to a file on another bot... */
	splitc(bot,fdb->sharelink,':');
	if (!in_chain(bot)) {
	  dprintf(idx,FILES_NOTAVAIL,fdb->filename);
	}
	else {
	  i=nextbot(bot);
	  strcpy(whoto,par); if (!whoto[0]) strcpy(whoto,dcc[idx].nick);
	  tprintf(dcc[i].sock,"filereq %d:%s@%s %s:%s\n",dcc[idx].sock,
		  whoto,botnetnick,bot,fdb->sharelink);
	  dprintf(idx,FILES_REQUESTED,fdb->sharelink,bot);
	  /* increase got count now (or never) */
	  fdb->gots++;
	  sprintf(s,"%s:%s",bot,fdb->sharelink); strcpy(fdb->sharelink,s);
	  fseek(f,where,SEEK_SET);
	  fwrite(fdb,sizeof(filedb),1,f);
	}
      }
      else {
	char xx[161];
	if (par[0]) sprintf(xx,"%s %s",fdb->filename,par);
	else strcpy(xx,fdb->filename);
	do_dcc_send(idx,destdir,xx);
	/* don't increase got count till later */
      }
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,what,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else putlog(LOG_FILES,"*","files: #%s# get %s %s",dcc[idx].nick,what,par);
}

void cmd_file_help(idx,par)
int idx; char *par;
{
  char s[1024]; int atr;
  atr=get_attr_handle(dcc[idx].nick);
  if (atr&USER_JANITOR) atr|=USER_MASTER;
  if (par[0]) {
    putlog(LOG_FILES,"*","files: #%s# help %s",dcc[idx].nick,par);
    sprintf(s,"filesys/%s",par); s[256]=0;
    tellhelp(idx,s,atr);
  }
  else {
    putlog(LOG_FILES,"*","files: #%s# help",dcc[idx].nick);
    tellhelp(idx,"filesys/help",atr);
  }
}

void cmd_hide(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; int ok=0;
  if (!par[0]) {
    dprintf(idx,"%s: hide <file(s)>\n",USAGE);
    return;
  }
  where=0L;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findmatch(f,par,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (!(fdb->stat & FILE_HIDDEN)) {
      fdb->stat |= FILE_HIDDEN; ok++;
      dprintf(idx,"%s: %s\n",FILES_HID,fdb->filename);
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,par,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# hide %s",dcc[idx].nick,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",FILES_HID,ok,ok==1?"":"s");
  }
}

void cmd_unhide(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; int ok=0;
  if (!par[0]) {
    dprintf(idx,"%s: unhide <file(s)>\n",USAGE);
    return;
  }
  where=0L;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findmatch(f,par,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (fdb->stat & FILE_HIDDEN) {
      fdb->stat &= ~FILE_HIDDEN; ok++;
      dprintf(idx,"%s: %s\n",FILES_UNHID,fdb->filename);
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,par,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# unhide %s",dcc[idx].nick,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",FILES_UNHID,ok,ok==1?"":"s");
  }
}

void cmd_share(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; int ok=0;
  if (!par[0]) {
    dprintf(idx,"%s: share <file(s)>\n",USAGE);
    return;
  }
  where=0L;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findmatch(f,par,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (!(fdb->stat & (FILE_HIDDEN|FILE_DIR|FILE_SHARE))) {
      fdb->stat |= FILE_SHARE; ok++;
      dprintf(idx,"%s: %s\n",FILES_SHARED,fdb->filename);
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,par,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# share %s",dcc[idx].nick,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",FILES_SHARED,ok,ok==1?"":"s");
  }
}

void cmd_unshare(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; int ok=0;
  if (!par[0]) {
    dprintf(idx,"%s: unshare <file(s)>\n",USAGE);
    return;
  }
  where=0L;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findmatch(f,par,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if ((fdb->stat & FILE_SHARE) && !(fdb->stat & (FILE_DIR|FILE_HIDDEN))) {
      fdb->stat &= ~FILE_SHARE; ok++;
      dprintf(idx,"%s: %s\n",FILES_UNSHARED,fdb->filename);
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,par,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# unshare %s",dcc[idx].nick,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",FILES_UNSHARED,ok,ok==1?"":"s");
  }
}

/* link a file from another bot */
void cmd_ln(idx,par)
int idx; char *par;
{
  char share[512],newpath[121],newfn[81],*p; FILE *f; filedb fdb,*x;
  long where; int atr;
  atr=get_attr_handle(dcc[idx].nick);
  nsplit(share,par); share[60]=0;
  /* correct format? */
  if ((strchr(share,':')==NULL) || (!par[0])) {
    dprintf(idx,"%s: ln <bot:path> <localfile>\n",USAGE);
    return;
  }
  p=strrchr(par,'/'); if (p!=NULL) {
    *p=0; strncpy(newfn,p+1,80); newfn[80]=0;
    if (!resolve_dir(dcc[idx].u.file->dir,par,newpath,atr)) {
      dprintf(idx,FILES_NOSUCHDIR);
      return;
    }
  }
  else {
    strcpy(newpath,dcc[idx].u.file->dir);
    strncpy(newfn,par,80); newfn[80]=0;
  }
  f=filedb_open(newpath);
  x=findfile(f,newfn,&where);
  if (x!=NULL) {
    if (!x->sharelink[0]) {
      dprintf(idx,FILES_NORMAL,newfn);
      filedb_close(f); return;
    }
    strcpy(x->sharelink,share);
    fseek(f,where,SEEK_SET);
    fwrite(x,sizeof(filedb),1,f);
    filedb_close(f);
    dprintf(idx,FILES_CHGLINK,share);
    putlog(LOG_FILES,"*","files: #%s# ln %s %s",dcc[idx].nick,par,share);
    return;
  }
  /* new entry */
  where=findempty(f);
  fdb.version=FILEVERSION; fdb.desc[0]=0; fdb.flags_req=0; fdb.size=0;
  fdb.gots=0; strncpy(fdb.filename,newfn,30); fdb.filename[30]=0;
  strcpy(fdb.uploader,dcc[idx].nick); fdb.uploaded=time(NULL);
  strcpy(fdb.sharelink,share); fdb.stat=0;
  fseek(f,where,SEEK_SET);
  fwrite(&fdb,sizeof(filedb),1,f);
  filedb_close(f);
  dprintf(idx,"%s %s -> %s\n",FILES_ADDLINK,fdb.filename,share);
  putlog(LOG_FILES,"*","files: #%s# ln /%s%s%s %s",dcc[idx].nick,newpath,
	 newpath[0]?"/":"",newfn,share);
}

void cmd_desc(idx,par)
int idx; char *par;
{
  char fn[512],desc[301],*p,*q; int atr,ok=0,lin; FILE *f; filedb *fdb;
  long where;
  nsplit(fn,par); if (!fn[0]) {
    dprintf(idx,"%s: desc <filename> <new description>\n",USAGE);
    return;
  }
  /* fix up desc */
  strncpy(desc,par,299); desc[299]=0; strcat(desc,"|");
  /* replace | with linefeeds, limit 5 lines */
  lin=0; q=desc;
  while ((*q<=32) && (*q)) strcpy(q,&q[1]);  /* zapf leading spaces */
  p=strchr(q,'|'); while (p!=NULL) {
    /* check length */
    *p=0; if (strlen(q)>60) {
      /* cut off at last space or truncate */
      *p='|'; p=q+60;
      while ((*p!=' ') && (p!=q)) p--;
      if (p==q) *(q+60)='|';   /* no space, so truncate it */
      else *p='|';
      p=strchr(q,'|');  /* go back, find my place, and continue */
    }
    *p='\n'; q=p+1; lin++;
    while ((*q<=32) && (*q)) strcpy(q,&q[1]);
    if (lin==5) { *p=0; p=NULL; }
    else p=strchr(q,'|');
  }
  /* (whew!) */
  if (desc[strlen(desc)-1]=='\n') desc[strlen(desc)-1]=0;
  atr=get_attr_handle(dcc[idx].nick);
  f=filedb_open(dcc[idx].u.file->dir);
  where=0L;
  fdb=findmatch(f,fn,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (!(fdb->stat&FILE_HIDDEN)) {
      ok=1;
      if ((!(atr&(USER_MASTER|USER_JANITOR))) &&
	  (strcasecmp(fdb->uploader,dcc[idx].nick)!=0))
	dprintf(idx,FILES_NOTOWNER,fdb->filename);
      else {
	strcpy(fdb->desc,desc);
	fseek(f,where,SEEK_SET);
	fwrite(fdb,sizeof(filedb),1,f);
	if (par[0]) dprintf(idx,"%s: %s\n",FILES_CHANGED,fdb->filename);
	else dprintf(idx,"%s: %s\n",FILES_BLANKED,fdb->filename);
      }
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,fn,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else putlog(LOG_FILES,"*","files: #%s# desc %s",dcc[idx].nick,fn);
}

void cmd_rm(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; int ok=0; char s[256];
  if (!par[0]) {  
    dprintf(idx,"%s: rm <file(s)>\n",USAGE);
    return;
  }
  f=filedb_open(dcc[idx].u.file->dir);
  where=0L;
  fdb=findmatch(f,par,&where);
  if (fdb==NULL) {
    filedb_close(f);
    dprintf(idx,FILES_NOMATCH);
    return;
  }
  while (fdb!=NULL) {
    if (!(fdb->stat & (FILE_HIDDEN|FILE_DIR))) {
      sprintf(s,"%s%s/%s",dccdir,dcc[idx].u.file->dir,fdb->filename);
      fdb->stat|=FILE_UNUSED; ok++;
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
      /* shared file links won't be able to be unlinked */
      if (!(fdb->sharelink[0])) unlink(s);
      dprintf(idx,"%s: %s\n",FILES_ERASED,fdb->filename);
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,par,&where);
  }
  filedb_close(f);
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# rm %s",dcc[idx].nick,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",FILES_ERASED,ok,ok==1?"":"s");
  }
}

void cmd_mkdir(idx,par)
int idx; char *par;
{
  char name[512],s[512]; FILE *f; filedb *fdb; long where;
  if (!par[0]) {
    dprintf(idx,"%s: mkdir <dir> [required-flags]\n",USAGE);
    return;
  }
  nsplit(name,par);
  if (name[strlen(name)-1]=='/') name[strlen(name)-1]=0;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findfile(f,name,&where);
  if (fdb==NULL) {
    filedb x;
    sprintf(s,"%s%s/%s",dccdir,dcc[idx].u.file->dir,name);
    if (mkdir(s,0755)==0) {
      x.version=FILEVERSION; x.stat=FILE_DIR; x.desc[0]=0;
      x.uploader[0]=0; strcpy(x.filename,name); x.flags_req=0;
      x.uploaded=time(NULL); x.size=0; x.gots=0; x.sharelink[0]=0;
      dprintf(idx,"%s /%s%s%s\n",FILES_CREADIR,dcc[idx].u.file->dir,
	      dcc[idx].u.file->dir[0]?"/":"",name);
      if (par[0]) {
	x.flags_req=str2flags(par);
	flags2str(x.flags_req,s);
	dprintf(idx,FILES_REQACCESS,s);
      }
      where=findempty(f); fseek(f,where,SEEK_SET);
      fwrite(&x,sizeof(filedb),1,f);
      filedb_close(f);
      putlog(LOG_FILES,"*","files: #%s# mkdir %s %s",dcc[idx].nick,name,par);
      return;
    }
    dprintf(idx,FAILED); filedb_close(f);
    return;
  }
  /* already exists! */
  if (!(fdb->stat&FILE_DIR)) {
    dprintf(idx,FILES_NOSUCHDIR);
    filedb_close(f); return;
  }
  if (par[0]) {
    fdb->flags_req=str2flags(par);
    flags2str(fdb->flags_req,s);
    dprintf(idx,FILES_CHGACCESS,name,s);
  }
  else {
    fdb->flags_req=0;
    dprintf(idx,FILES_CHGNACCESS,name);
  }
  fseek(f,where,SEEK_SET);
  fwrite(fdb,sizeof(filedb),1,f);
  filedb_close(f);
}

void cmd_rmdir(idx,par)
int idx; char *par;
{
  FILE *f; filedb *fdb; long where; char s[256],name[80];
  strncpy(name,par,80); name[80]=0;
  if (name[strlen(name)-1]=='/') name[strlen(name)-1]=0;
  f=filedb_open(dcc[idx].u.file->dir);
  fdb=findfile(f,name,&where);
  if (fdb==NULL) {
    dprintf(idx,FILES_NOSUCHDIR);
    filedb_close(f); return;
  }
  if (!(fdb->stat & FILE_DIR)) {
    dprintf(idx,FILES_NOSUCHDIR);
    filedb_close(f); return;
  }
  /* erase '.filedb' and '.files' if they exist */
  sprintf(s,"%s%s/%s/.filedb",dccdir,dcc[idx].u.file->dir,name); unlink(s);
  sprintf(s,"%s%s/%s/.files",dccdir,dcc[idx].u.file->dir,name); unlink(s);
  sprintf(s,"%s%s/%s",dccdir,dcc[idx].u.file->dir,name);
  if (rmdir(s)==0) {
    dprintf(idx,"%s /%s%s%s\n",FILES_REMDIR,dcc[idx].u.file->dir,
	    dcc[idx].u.file->dir[0]?"/":"",name);
    fdb->stat|=FILE_UNUSED;
    fseek(f,where,SEEK_SET);
    fwrite(fdb,sizeof(filedb),1,f);
    filedb_close(f);
    putlog(LOG_FILES,"*","files: #%s# rmdir %s",dcc[idx].nick,name);
    return;
  }
  dprintf(idx,FAILED);
  filedb_close(f);
}

void cmd_mv_cp(idx,par,copy)
int idx; char *par; int copy;
{
  char *p,fn[512],oldpath[161],s[161],s1[161],newfn[161],newpath[161];
  int atr,ok,only_first,skip_this; FILE *f,*g; filedb *fdb,x,*z;
  long where,gwhere,wherez;
  atr=get_attr_handle(dcc[idx].nick);
  nsplit(fn,par); if (!par[0]) {
    dprintf(idx,"%s: %s <oldfilepath> <newfilepath>\n",USAGE,copy?"cp":"mv");
    return;
  }
  p=strrchr(fn,'/'); if (p!=NULL) {
    *p=0; strncpy(s,fn,160); s[160]=0; strcpy(fn,p+1);
    if (!resolve_dir(dcc[idx].u.file->dir,s,oldpath,atr)) {
      dprintf(idx,FILES_ILLSOURCE);
      return;
    }
  }
  else strcpy(oldpath,dcc[idx].u.file->dir);
  strncpy(s,par,160); s[160]=0;
  if (!resolve_dir(dcc[idx].u.file->dir,s,newpath,atr)) {
    /* destination is not just a directory */
    p=strrchr(s,'/');
    if (p==NULL) { strcpy(newfn,s); s[0]=0; }
    else { *p=0; strcpy(newfn,p+1); }
    if (!resolve_dir(dcc[idx].u.file->dir,s,newpath,atr)) {
      dprintf(idx,FILES_ILLDEST);
      return;
    }
  }
  else newfn[0]=0;
  /* stupidness checks */
  if ((strcmp(oldpath,newpath)==0) &&
      ((!newfn[0]) || (strcmp(newfn,fn)==0))) {
    dprintf(idx,FILES_STUPID,copy?FILES_COPY:FILES_MOVE);
    return;
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
    dprintf(idx,FILES_NOMATCH);
    filedb_close(f);
    if (g!=NULL) filedb_close(g);
    return;
  }
  while (fdb!=NULL) {
    skip_this=0;
    if (!(fdb->stat & (FILE_HIDDEN|FILE_DIR))) {
      sprintf(s,"%s%s%s%s",dccdir,oldpath,oldpath[0]?"/":"",fdb->filename);
      sprintf(s1,"%s%s%s%s",dccdir,newpath,newpath[0]?"/":"",newfn[0]?
	      newfn:fdb->filename);
      if (strcmp(s,s1)==0) {
	dprintf(idx,"%s /%s%s%s %s\n",FILES_SKIPSTUPID,copy?FILES_COPY:
		FILES_MOVE,newpath,newpath[0]?"/":"",newfn[0]?newfn:
		fdb->filename);
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
	  dprintf(idx,"%s /%s%s%s %s\n",FILES_DEST,
		  newpath,newpath[0]?"/":"",newfn[0]?newfn:fdb->filename,
		  FILES_EXISTDIR);
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
	  dprintf(idx,"%s /%s%s%s to /%s%s%s\n",copy?FILES_COPIED:FILES_MOVED,
		  oldpath,oldpath[0]?"/":"",fdb->filename,newpath,newpath[0]?
		  "/":"",newfn[0]?newfn:fdb->filename);
	}
	else dprintf(idx,"%s /%s%s%s\n",FILES_CANTWRITE,newpath,newpath[0]?
		     "/":"",newfn[0]?newfn:fdb->filename);
      }
    }
    where+=sizeof(filedb);
    fdb=findmatch(f,fn,&where);
    if ((ok) && (only_first)) fdb=NULL;
  }
  if (!ok) dprintf(idx,FILES_NOMATCH);
  else {
    putlog(LOG_FILES,"*","files: #%s# %s %s%s%s %s",dcc[idx].nick,
	   copy?"cp":"mv",oldpath,oldpath[0]?"/":"",fn,par);
    if (ok>1) dprintf(idx,"%s %d file%s.\n",copy?FILES_COPIED:FILES_MOVED,ok,
		      ok==1?"":"s");
  }
  filedb_close(f);
  if (g!=NULL) filedb_close(g);
}

void cmd_mv(idx,par)
int idx; char *par;
{
  cmd_mv_cp(idx,par,0);
}

void cmd_cp(idx,par)
int idx; char *par;
{
  cmd_mv_cp(idx,par,1);
}

void cmd_stats(idx,par)
int idx; char *par;
{
  tell_file_stats(idx,dcc[idx].nick);
  putlog(LOG_FILES,"*","files: #%s# stats",dcc[idx].nick);
}

/***** Tcl stub functions *****/

int files_get(int idx,char *fn,char *nick)
{
  int atr,i; char *p,what[512],destdir[121],s[256];
  filedb *fdb; FILE *f; long where;
  atr=get_attr_handle(dcc[idx].nick);
  p=strrchr(fn,'/'); if (p!=NULL) {
    *p=0; strncpy(s,fn,120); s[120]=0;
    strncpy(what,p+1,80); what[80]=0;
    if (!resolve_dir(dcc[idx].u.file->dir,s,destdir,atr))
      return 0;
  }
  else {
    strcpy(destdir,dcc[idx].u.file->dir);
    strncpy(what,fn,80); what[80]=0;
  }
  f=filedb_open(destdir);
  fdb=findfile(f,what,&where);
  if (fdb==NULL) { filedb_close(f); return 0; }
  if (fdb->stat&(FILE_HIDDEN|FILE_DIR)) { filedb_close(f); return 0; }
  if (fdb->sharelink[0]) {
    char bot[121],whoto[NICKLEN];
    /* this is a link to a file on another bot... */
    splitc(bot,fdb->sharelink,':');
    if (!in_chain(bot)) { filedb_close(f); return 0; }
    else {
      i=nextbot(bot);
      strcpy(whoto,nick); if (!whoto[0]) strcpy(whoto,dcc[idx].nick);
      tprintf(dcc[i].sock,"filereq %d:%s@%s %s:%s\n",dcc[idx].sock,
	      whoto,botnetnick,bot,fdb->sharelink);
      dprintf(idx,FILES_REQUESTED,fdb->sharelink,bot);
      /* increase got count now (or never) */
      fdb->gots++;
      sprintf(s,"%s:%s",bot,fdb->sharelink); strcpy(fdb->sharelink,s);
      fseek(f,where,SEEK_SET);
      fwrite(fdb,sizeof(filedb),1,f);
      filedb_close(f);
      return 1;
    }
  }
  filedb_close(f);
  if (nick[0]) sprintf(what,"%s %s",fdb->filename,nick);
  else strcpy(what,fdb->filename);
  do_dcc_send(idx,destdir,what);
  /* don't increase got count till later */
  return 1;
}

void files_setpwd(int idx,char *where)
{
  int atr; char s[121];
  atr=get_attr_handle(dcc[idx].nick);
  if (!resolve_dir(dcc[idx].u.file->dir,where,s,atr)) return;
  strcpy(dcc[idx].u.file->dir,s);
  set_handle_dccdir(userlist,dcc[idx].nick,dcc[idx].u.file->dir);
}

#endif  /* !NO_FILE_SYSTEM */
