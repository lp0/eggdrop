/*
   mem.c -- handles:
     memory allocation and deallocation
     keeping track of what memory is being used by whom

   dprintf'ized, 15nov95
*/
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#define LOG_MISC 32
#define MEMTBLSIZE 25000       /* yikes! */

#ifdef EBUG_MEM
#define DEBUG
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int serv;

#ifdef DEBUG
unsigned long memused=0;
static int lastused=0;

struct {
  void *ptr;
  short size;
  char file[15];
  short line;
} memtbl[MEMTBLSIZE];
#endif

#ifdef STDC_HEADERS
#define PROTO(x) x
#define PROTO1(a,b) (a b)
#define PROTO2(a1,b1,a2,b2) (a1 b1, a2 b2) 
#define PROTO3(a1,b1,a2,b2,a3,b3) (a1 b1, a2 b2, a3 b3)
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) \
              (a1 b1, a2 b2, a3 b3, a4 b4)
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5)
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5, a6 b6)
#else
#define PROTO(x) ()
#define PROTO1(a,b) (b) a b;
#define PROTO2(a1,b1,a2,b2) (b1, b2) a1 b1; a2 b2; 
#define PROTO3(a1,b1,a2,b2,a3,b3) (b1, b2, b3) a1 b1; a2 b2; a3 b3;
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) (b1, b2, b3, b4) \
              a1 b1; a2 b2; a3 b3; a4 b4;
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (b1, b2, b3, b4, b5) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5;
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (b1, b2, b3, b4, b5, b6) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5; a6 b6;
#endif

#ifdef HAVE_DPRINTF 
#define dprintf dprintf_eggdrop
#endif

/* prototypes */
void mprintf();
void tprintf();
void dprintf();
void putlog();
int expected_memory();
int expmem_chan();
int expmem_chanprog();
int expmem_misc();
int expmem_fileq();
int expmem_users();
int expmem_dccutil();
int expmem_botnet();
int expmem_tcl();
int expmem_tclhash();
int expmem_net();
int expmem_blowfish();
void tell_netdebug();
void debug_blowfish();

/* initialize the memory structure */
void init_mem()
{
#ifdef DEBUG
  int i;
  for (i=0; i<MEMTBLSIZE; i++) memtbl[i].ptr=NULL;
#endif
}

/* tell someone the gory memory details */
void tell_mem_status PROTO1(char *,nick)
{
#ifdef DEBUG
  float per;
  per=((lastused*1.0)/(MEMTBLSIZE*1.0))*100.0;
  mprintf(serv,"NOTICE %s :Memory table usage: %d/%d (%.1f%% full)\n",nick,
	  lastused,MEMTBLSIZE,per);
#endif
  mprintf(serv,"NOTICE %s :Think I'm using about %dk.\n",nick,
	  (int)(expected_memory()/1024));
}

void tell_mem_status_dcc PROTO1(int,idx)
{
#ifdef DEBUG
  int exp; float per;
  exp=expected_memory();  /* in main.c ? */
  per=((lastused*1.0)/(MEMTBLSIZE*1.0))*100.0;
  dprintf(idx,"Memory table: %d/%d (%.1f%% full)\n",lastused,MEMTBLSIZE,per);
  per=((exp*1.0)/(memused*1.0))*100.0;
  if (per!=100.0)
    dprintf(idx,"Memory fault: only accounting for %d/%ld (%.1f%%)\n",
	    exp,memused,per);
  dprintf(idx,"Memory table itself occupies an additional %dk static\n",
	  (int)(sizeof(memtbl)/1024));
#endif
}

void debug_mem_to_dcc PROTO1(int,idx)
{
#ifdef DEBUG
  unsigned long exp[11],use[11],l; int i,j; char fn[20],sofar[81];
  exp[0]=expmem_chan();
  exp[1]=expmem_chanprog();
  exp[2]=expmem_misc();
  exp[3]=expmem_fileq();
  exp[4]=expmem_users();
  exp[5]=expmem_net();
  exp[6]=expmem_dccutil();
  exp[7]=expmem_botnet();
  exp[8]=expmem_tcl();
  exp[9]=expmem_tclhash();
  exp[10]=expmem_blowfish();
  for (i=0; i<11; i++) use[i]=0;
  for (i=0; i<lastused; i++) {
    strcpy(fn,memtbl[i].file); l=memtbl[i].size;
    if (strcasecmp(fn,"chanset.c")==0) use[0]+=l;
    else if (strcasecmp(fn,"chanprog.c")==0) use[1]+=l;
    else if (strcasecmp(fn,"misc.c")==0) use[2]+=l;
    else if (strcasecmp(fn,"fileq.c")==0) use[3]+=l;
    else if (strcasecmp(fn,"userrec.c")==0) use[4]+=l;
    else if (strcasecmp(fn,"net.c")==0) use[5]+=l;
    else if (strcasecmp(fn,"dccutil.c")==0) use[6]+=l;
    else if (strcasecmp(fn,"botnet.c")==0) use[7]+=l;
    else if (strcasecmp(fn,"tcl.c")==0) use[8]+=l;
    else if (strcasecmp(fn,"tclhash.c")==0) use[9]+=l;
    else if (strcasecmp(fn,"blowfish.c")==0) use[10]+=l;
    else {
      if (idx<0) tprintf(-idx,"Not logging file %s!\n",fn);
      else dprintf(idx,"Not logging file %s!\n",fn);
    }
  }
  for (i=0; i<11; i++) {
    switch(i) {
    case 0: strcpy(fn,"chanset.c"); break;
    case 1: strcpy(fn,"chanprog.c"); break;
    case 2: strcpy(fn,"misc.c"); break;
    case 3: strcpy(fn,"fileq.c"); break;
    case 4: strcpy(fn,"userrec.c"); break;
    case 5: strcpy(fn,"net.c"); break;
    case 6: strcpy(fn,"dccutil.c"); break;
    case 7: strcpy(fn,"botnet.c"); break;
    case 8: strcpy(fn,"tcl.c"); break;
    case 9: strcpy(fn,"tclhash.c"); break;
    case 10: strcpy(fn,"blowfish.c"); break;
    }
    if (use[i]==exp[i]) {
      if (idx<0)
	tprintf(-idx,"File '%-10s' accounted for %lu/%lu (ok)\n",fn,exp[i],
		use[i]);
      else
	dprintf(idx,"File '%-10s' accounted for %lu/%lu (ok)\n",fn,exp[i],
		use[i]);
    }
    else {
      if (idx<0)
	tprintf(-idx,"File '%-10s' accounted for %lu/%lu (debug follows:)\n",
		fn,exp[i],use[i]);
      else
	dprintf(idx,"File '%-10s' accounted for %lu/%lu (debug follows:)\n",fn,
		exp[i],use[i]);
      strcpy(sofar,"   ");
      for (j=0; j<lastused; j++) 
	if (strcasecmp(memtbl[j].file,fn)==0) {
 	  sprintf(&sofar[strlen(sofar)],"%-3d:(%03X) ",memtbl[j].line,
		  memtbl[j].size);
	  if (strlen(sofar)>60) {
	    sofar[strlen(sofar)-1]=0;
	    if (idx<0) tprintf(-idx,"%s\n",sofar);
	    else dprintf(idx,"%s\n",sofar);
	    strcpy(sofar,"   ");
	  }
	}
      if (sofar[0]) {
	sofar[strlen(sofar)-1]=0;
	if (idx<0) tprintf(-idx,"%s\n",sofar);
	else dprintf(idx,"%s\n",sofar);
      }
    }
  }
  if (idx<0) tprintf(-idx,"--- End of debug memory list.\n");
  else dprintf(idx,"--- End of debug memory list.\n");
#else
  if (idx<0) tprintf(-idx,"Compiled without debug info.\n");
  else dprintf(idx,"Compiled without extensive memory debugging (sorry).\n");
#endif
  tell_netdebug(idx);
  debug_blowfish(idx);
}

void *n_malloc PROTO3(int,size,char *,file,int,line)
{
  void *x; int i=0;
  x=(void *)malloc(size);
  if (x==NULL) {
    i=i;
    putlog(LOG_MISC,"*","*** FAILED MALLOC %s (%d)",file,line);
    return NULL;
  }
#ifdef DEBUG
  if (lastused==MEMTBLSIZE) {
    putlog(LOG_MISC,"*","*** MEMORY TABLE FULL: %s (%d)",file,line);
    return x;
  }
  i=lastused;
  memtbl[i].ptr=x; memtbl[i].line=line; memtbl[i].size=size;
  strcpy(memtbl[i].file,file);
  memused+=size; lastused++;
#endif
  return x;
}

void *n_realloc PROTO4(void *,ptr,int,size,char *,file,int,line)
{
  void *x; int i=0;
  x=(void *)realloc(ptr,size);
  if (x==NULL) {
    i=i;
    putlog(LOG_MISC,"*","*** FAILED REALLOC %s (%d)",file,line);
    return NULL;
  }
#ifdef DEBUG
  for (i=0; (i<lastused) && (memtbl[i].ptr!=ptr); i++);
  if (i==lastused) {
    putlog(LOG_MISC,"*","*** ATTEMPTING TO REALLOC NON-MALLOC'D PTR: %s (%d)",
	   file,line);
    return NULL;
  }
  memused-=memtbl[i].size;
  memtbl[i].ptr=x; memtbl[i].line=line; memtbl[i].size=size;
  strcpy(memtbl[i].file,file);
  memused+=size;
#endif
  return x;
}

void n_free PROTO3(void *,ptr,char *,file,int,line)
{
  int i=0;
  if (ptr==NULL) {
    putlog(LOG_MISC,"*","*** ATTEMPTING TO FREE NULL PTR: %s (%d)",file,line);
    i=i;
    return;
  }
#ifdef DEBUG
  /* give tcl builtins an escape mechanism */
  if (line) {
    for (i=0; (i<lastused) && (memtbl[i].ptr!=ptr); i++);
    if (i==lastused) {
      putlog(LOG_MISC,"*","*** ATTEMPTING TO FREE NON-MALLOC'D PTR: %s (%d)",
	     file,line);
      return;
    }
    memused-=memtbl[i].size;
    lastused--;
    memtbl[i].ptr=memtbl[lastused].ptr; memtbl[i].size=memtbl[lastused].size;
    memtbl[i].line=memtbl[lastused].line;
    strcpy(memtbl[i].file,memtbl[lastused].file);
  }
#endif
  free(ptr);
}
