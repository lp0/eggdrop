/*
   fileq.c -- handles:
     adding and removing files to/from the file queue
     listing files on the queue
     sending the next queued file for a user
     cancelling a queued file

   dprintf'ized, 1nov95
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
#include <netinet/in.h>
#include "eggdrop.h"
#include "proto.h"
#include "../lush.h"

#ifndef NO_FILE_SYSTEM

extern char dccdir[];
extern char tempdir[];
extern int serv;
extern int dcc_total;
extern struct dcc_t dcc[];
extern int copy_to_tmp;

typedef struct zarrf {
  char *dir;             /* starts with '*' -> absolute dir */
  char *file;            /*    (otherwise -> dccdir) */
  char nick[NICKLEN];    /* who queued this file */
  char to[NICKLEN];      /* who will it be sent to */
  struct zarrf *next;
} fileq_t;

fileq_t *fileq=NULL;

int expmem_fileq()
{
  fileq_t *q=fileq; int tot=0;
  context;
  while (q!=NULL) {
    tot+=strlen(q->dir)+strlen(q->file)+2+sizeof(fileq_t);
    q=q->next;
  }
  return tot;
}

void queue_file(char *dir,char *file,char *from,char *to)
{
  fileq_t *q=fileq;
  fileq=(fileq_t *)nmalloc(sizeof(fileq_t));
  fileq->next=q;
  fileq->dir=(char *)nmalloc(strlen(dir)+1);
  fileq->file=(char *)nmalloc(strlen(file)+1);
  strcpy(fileq->dir,dir); strcpy(fileq->file,file);
  strcpy(fileq->nick,from); strcpy(fileq->to,to);
}

void deq_this(this)
fileq_t *this;
{
  fileq_t *q=fileq,*last=NULL;
  while ((q!=this) && (q!=NULL)) {
    last=q; q=q->next;
  }
  if (q==NULL) return;   /* bogus ptr */
  if (last!=NULL) last->next=q->next;
  else fileq=q->next;
  nfree(q->dir); nfree(q->file); nfree(q);
}

/* remove all files queued to a certain user */
void flush_fileq(to)
char *to;
{
  fileq_t *q=fileq; int fnd=1;
  while (fnd) {
    q=fileq; fnd=0;
    while (q!=NULL) {
      if (strcasecmp(q->to,to)==0) { deq_this(q); q=NULL; fnd=1; }
      if (q!=NULL) q=q->next;
    }
  }
}

void send_next_file(char *to)
{
  fileq_t *q=fileq,*this=NULL; char s[256],s1[256]; int x;
  while (q!=NULL) {
    if (strcasecmp(q->to,to)==0) this=q;
    q=q->next;
  }
  if (this==NULL) return;   /* none */
  /* copy this file to /tmp */
  if (this->dir[0]=='*')      /* absolute path */
    sprintf(s,"%s/%s",&this->dir[1],this->file);
  else
    sprintf(s,"%s%s%s%s",dccdir,this->dir,this->dir[0]?"/":"",this->file);
  if (copy_to_tmp) {
    sprintf(s1,"%s%s",tempdir,this->file);
    if (copyfile(s,s1)!=0) {
      putlog(LOG_FILES|LOG_MISC,"*","Refused dcc get %s: copy to %s FAILED!",
	     this->file,tempdir);
      mprintf(serv,"NOTICE %s :File system is broken; aborting queued files.\n",
	      this->to);
      strcpy(s,this->to); flush_fileq(s);
      return;
    }
  }
  else strcpy(s1,s);
  if (this->dir[0]=='*') sprintf(s,"%s/%s",&this->dir[1],this->file);
  else sprintf(s,"%s%s%s",this->dir,this->dir[0]?"/":"",this->file);
  x=raw_dcc_send(s1,this->to,this->nick,s);
  if (x==1) {
    wipe_tmp_filename(s1,-1);
    putlog(LOG_FILES,"*","DCC connections full: GET %s [%s]",s1,this->nick);
    mprintf(serv,"NOTICE %s :DCC connections full; aborting queued files.\n",
	    this->to);
    strcpy(s,this->to); flush_fileq(s);
    return;
  }
  if (x==2) {
    wipe_tmp_filename(s1,-1);
    putlog(LOG_FILES,"*","DCC socket error: GET %s [%s]",s1,this->nick);
    mprintf(serv,"NOTICE %s :DCC socket error; aborting queued files.\n",
	    this->to);
    strcpy(s,this->to); flush_fileq(s);
    return;
  }
  if (strcasecmp(this->to,this->nick)!=0)
    mprintf(serv,"NOTICE %s :Here is a file from %s ...\n",this->to,
	    this->nick);
  deq_this(this);
}

/* let tcl see the queued files for (x) */
void tcl_get_queued(Tcl_Interp *irp,char *who)
{
  char s[512]; fileq_t *q=fileq;
  while (q!=NULL) {
    if (strcasecmp(q->nick,who)==0) {
      if (q->dir[0]=='*')
	sprintf(s,"%s %s/%s",q->to,&q->dir[1],q->file);
      else
	sprintf(s,"%s /%s%s%s",q->to,q->dir,q->dir[0]?"/":"",q->file);
      Tcl_AppendElement(irp,s);
    }
    q=q->next;
  }
}

void show_queued_files(int idx)
{
  int i,cnt=0; fileq_t *q=fileq;
  while (q!=NULL) {
    if (strcasecmp(q->nick,dcc[idx].nick)==0) {
      if (!cnt) {
	dprintf(idx,"  Send to    Filename\n");
	dprintf(idx,"  ---------  --------------------\n");
      }
      cnt++;
      if (q->dir[0]=='*')
	dprintf(idx,"  %-9s  %s/%s\n",q->to,&q->dir[1],q->file);
      else
	dprintf(idx,"  %-9s  /%s%s%s\n",q->to,q->dir,q->dir[0]?"/":"",
		q->file);
    }
    q=q->next;
  }
  for (i=0; i<dcc_total; i++) {
    if (((dcc[i].type==DCC_GET_PENDING) || (dcc[i].type==DCC_GET)) &&
	((strcasecmp(dcc[i].nick,dcc[idx].nick)==0) ||
	 (strcasecmp(dcc[i].u.xfer->from,dcc[idx].nick)==0))) {
      char *nfn;
      if (!cnt) {
	dprintf(idx,"  Send to    Filename\n");
	dprintf(idx,"  ---------  --------------------\n");
      }
      nfn=strrchr(dcc[i].u.xfer->filename,'/');
      if (nfn==NULL) nfn=dcc[i].u.xfer->filename; else nfn++;
      cnt++; 
      if (dcc[i].type==DCC_GET_PENDING)
	dprintf(idx,"  %-9s  %s  [WAITING]\n",dcc[i].nick,nfn);
      else
	dprintf(idx,"  %-9s  %s  (%.1f%% done)\n",dcc[i].nick,nfn,
		(100.0*((float)dcc[i].u.xfer->sent / 
			(float)dcc[i].u.xfer->length)));
    }
  }
  if (!cnt) dprintf(idx,"No files queued up.\n");
  else dprintf(idx,"Total: %d\n",cnt);
}

void fileq_cancel(int idx,char *par)
{
  int fnd=1,matches=0,atot=0,i; fileq_t *q; char s[256];
  while (fnd) {
    q=fileq; fnd=0; while (q!=NULL) {
      if (strcasecmp(dcc[idx].nick,q->nick)==0) {
	if (q->dir[0]=='*')
	  sprintf(s,"%s/%s",&q->dir[1],q->file);
	else
	  sprintf(s,"/%s%s%s",q->dir,q->dir[0]?"/":"",q->file);
	if (wild_match_file(par,s)) {
	  dprintf(idx,"Cancelled: %s to %s\n",s,q->to);
	  fnd=1; deq_this(q); q=NULL; matches++;
	}
	if ((!fnd) && (wild_match_file(par,q->file))) {
	  dprintf(idx,"Cancelled: %s to %s\n",s,q->to);
	  fnd=1; deq_this(q); q=NULL; matches++;
	}
      }
      if (q!=NULL) q=q->next;
    }
  }
  for (i=0; i<dcc_total; i++) {
    if (((dcc[i].type==DCC_GET_PENDING) || (dcc[i].type==DCC_GET)) &&
	((strcasecmp(dcc[i].nick,dcc[idx].nick)==0) ||
	 (strcasecmp(dcc[i].u.xfer->from,dcc[idx].nick)==0))) {
      char *nfn=strrchr(dcc[i].u.xfer->filename,'/');
      if (nfn==NULL) nfn=dcc[i].u.xfer->filename; else nfn++;
      if (wild_match_file(par,nfn)) {
	dprintf(idx,"Cancelled: %s  (aborted dcc send)\n",nfn);
	if (strcasecmp(dcc[i].nick,dcc[idx].nick)!=0)
	  mprintf(serv,"NOTICE %s :Transfer of %s aborted by %s\n",dcc[i].nick,
		  nfn,dcc[idx].nick);
	if (dcc[i].type==DCC_GET)
	  putlog(LOG_FILES,"*","DCC cancel: GET %s (%s) at %lu/%lu",nfn,
		 dcc[i].nick,dcc[i].u.xfer->sent,dcc[i].u.xfer->length);
	wipe_tmp_file(i); atot++; matches++;
	killsock(dcc[i].sock); lostdcc(i); i--;
      }
    }
  }
  if (!matches) dprintf(idx,"No matches.\n");
  else dprintf(idx,"Cancelled %d file%s.\n",matches,matches>1?"s":"");
  for (i=0; i<atot; i++)
    if (!at_limit(dcc[idx].nick)) send_next_file(dcc[idx].nick);
}

#endif  /* !NO_FILE_SYSTEM */
