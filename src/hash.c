/*
   hash.c -- handles:
     (non-Tcl) procedure lookups for msg/dcc/file commands
     (Tcl) binding internal procedures to msg/dcc/file commands

   dprintf'ized, 15nov95
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
#include "cmdt.h"
#include "hash.h"
#include "proto.h"
#include "tclegg.h"

extern struct dcc_t dcc[];
extern int dcc_total;
extern int serv;
extern Tcl_HashTable H_msg, H_dcc, H_fil;
extern Tcl_Interp *interp;
extern int hashtot;


/* new hashing function */
void gotcmd(char *nick,char *from,char *msg,int ignoring)
{
  char code[512],hand[41],s[121],total[512];
  sprintf(s,"%s!%s",nick,from); strcpy(total,msg); rmspace(msg);
  nsplit(code,msg); get_handle_by_host(hand,s); rmspace(msg);
#ifndef TRIGGER_BINDS_ON_IGNORE
  if (!ignoring)
#endif
  check_tcl_msgm(code,nick,from,hand,msg);
  if (!ignoring) if (check_tcl_msg(code,nick,from,hand,msg)) return;
  if (ignoring) return;
  putlog(LOG_MSGS,"*","[%s!%s] %s",nick,from,total);
}

/* for dcc commands -- hash the function */
int got_dcc_cmd(int idx,char *msg)
{
  char total[512],code[512];
  strcpy(total,msg); rmspace(msg); nsplit(code,msg); rmspace(msg);
  return check_tcl_dcc(code,idx,msg);
}

#ifndef NO_FILE_SYSTEM
/* hash function for file area commands */
int got_files_cmd(int idx,char *msg)
{
  char total[512],code[512];
  strcpy(msg,check_tcl_filt(idx,msg));
  if (!msg[0]) return 1;
  if (msg[0]=='.') strcpy(msg,&msg[1]);
  strcpy(total,msg); rmspace(msg); nsplit(code,msg); rmspace(msg);
  return check_tcl_fil(code,idx,msg);
}
#endif

/* hash function for tandem bot commands */
void dcc_bot(int idx,char *msg)
{
  char total[512],code[512]; int i,f;
  strcpy(total,msg); nsplit(code,msg);
  f=0; i=0; while ((C_bot[i].name!=NULL) && (!f)) {
    if (strcasecmp(code,C_bot[i].name)==0) {
      /* found a match */
      (C_bot[i].func)(idx,msg); f=1;
    }
    i++;
  }
}

/* bring the default msg/dcc/fil commands into the Tcl interpreter */
void init_builtins()
{
  int i,j,flags,new; Tcl_HashTable *ht=NULL; Tcl_HashEntry *he;
  tcl_cmd_t *tt; cmd_t *cc=NULL; char s[2];
#ifdef NO_FILE_SYSTEM
# define _max 2   /* only dcc, msg */
#else
# define _max 3   /* dcc, msg, fil */
#endif
  for (j=0; j<_max; j++) {
#ifdef NO_IRC
    if (j==1) j++;
#endif
    switch(j) {
    case 0:
      ht=&H_dcc; cc=C_dcc; break;
#ifndef NO_IRC
    case 1:
      ht=&H_msg; cc=C_msg; break;
#endif
#ifndef NO_FILE_SYSTEM
    case 2:
      ht=&H_fil; cc=C_file; break;
#endif
    }
    i=0; s[1]=0;
    while (cc[i].name!=NULL) {
      s[0]=cc[i].flag; flags=str2flags(s);
      tt=(tcl_cmd_t *)tclcmd_alloc(strlen(cc[i].name)+6);
      tt->flags_needed=flags; tt->next=NULL;
      strcpy(tt->func_name,(j==0?"*dcc:" : (j==1?"*msg:" : "*fil:")));
      strcat(tt->func_name,cc[i].name);
      he=Tcl_CreateHashEntry(ht,cc[i].name,&new);
      if (!new) {
	/* append old entry */
	tcl_cmd_t *ttx=(tcl_cmd_t *)Tcl_GetHashValue(he);
	Tcl_DeleteHashEntry(he);
	tt->next=ttx;
      }
      Tcl_SetHashValue(he,tt);
      /* create command entry in Tcl interpreter */
      Tcl_CreateCommand(interp,tt->func_name,tcl_builtin,
			(ClientData)cc[i].func,NULL);
      i++;
    }
  }
}
