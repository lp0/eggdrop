/* 
   misc.c -- handles:
     stristr() split() maskhost() copyfile() movefile() fixfrom()
     dumplots() daysago() days() daysdur()
     logging things
     queueing output for the bot (msg and help)
     resync buffers for sharebots
     help system
     motd display and %var substitution

   dprintf'ized, 12dec95
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
#include <fcntl.h>
#include <varargs.h>
#include "eggdrop.h"
#include "proto.h"
#ifndef MODULES
#include "mod/blowfish.c"
#endif

extern int serv;
extern char notefile[];
extern int dcc_total;
extern struct dcc_t dcc[];
extern char helpdir[];
extern char version[];
extern char botname[];
extern char admin[];
extern int require_p;
extern int backgrd;
extern int con_chan;
extern int term_z;
extern int use_stderr;
extern char motdfile[];
extern char ver[];
extern char textdir[];
extern int strict_host;
extern int keep_all_logs;
extern char botnetnick[];

/* whether or not to display the time with console output */
int shtime=1;
/* logfiles */
log_t logs[MAXLOGS];
/* console mask */
int conmask=LOG_MODES|LOG_CMDS|LOG_MISC;
/* total messages queued on main queue */
int mtot=0;
/* total messages queued on help queue */
int htot=0;
/* maximum messages to store in each queue */
int maxqmsg=300;

struct msgq {
  int sock;
  char *msg;
  struct msgq *next;
} *mq=NULL,*hq=NULL;

/* store info for sharebots */
struct tandbuf {
  char bot[10];
  time_t timer;
  struct msgq *q;
} tbuf[5];

/* expected memory usage */
int expmem_misc()
{
  int tot=0; struct msgq *m=mq,*h=hq;
  context;
  while (m!=NULL) {
    tot+=strlen(m->msg)+1;
    tot+=sizeof(struct msgq);
    m=m->next;
  }
  while (h!=NULL) {
    tot+=strlen(h->msg)+1;
    tot+=sizeof(struct msgq);
    h=h->next;
  }
  return tot;
}

void init_misc()
{
  int i;
  for (i=0; i<5; i++) {
    tbuf[i].q=NULL;
    tbuf[i].bot[0]=0;
  }
  for (i=0; i<MAXLOGS; i++) {
    logs[i].filename=logs[i].chname=NULL;
    logs[i].mask=0;
    logs[i].f=NULL;
  }
}

/***** MISC FUNCTIONS *****/

/* low-level stuff for other modules */
int is_file PROTO1(char *,s)
{
  struct stat ss;
  int i=stat(s,&ss);
  if (i<0) return 0;
  if ((ss.st_mode&S_IFREG) || (ss.st_mode&S_IFLNK)) return 1;
  return 0;
}

#define upcase(c) (((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c))

/* determine if littles is contained in bigs (ignoring case) */
/* if so: return pointer to the littles in bigs */
/* if not: return NULL */
char *stristr PROTO2(char *,bigs,char *,littles)
{
  char *st=bigs,*p,*q;
  while (1) {
    if (!*st) return NULL;
    p=littles; q=st;
    while ((*p) && (*q) && (upcase(*p)==upcase(*q))) { p++; q++; }
    if ((!*q) && (*p)) return NULL;   /* premature end of bigs */
    if (!*p) return st;   /* found it! */
    st++;     /* try again */
  }
}

#if !HAVE_STRCASECMP
/* unixware has no strcasecmp() without linking in a hefty library */
int strcasecmp PROTO2(char *,s1,char *,s2)
{
  while ((*s1) && (*s2) && (upcase(*s1)==upcase(*s2))) { s1++; s2++; }
  return upcase(*s1)-upcase(*s2);
}
#endif

/* split first word off of rest and put it in first */
void splitc PROTO3(char *,first,char *,rest,char,divider)
{
  char *p;
  p=strchr(rest,divider);
  if (p==NULL) { if ((first!=rest)&&(first!=NULL)) first[0]=0; return; }
  *p=0; if (first!=NULL) strcpy(first,rest);
  if (first!=rest) strcpy(rest,p+1);
}

void split PROTO2(char *,first,char *,rest) { splitc(first,rest,' '); }
void splitnick PROTO2(char *,first,char *,rest) { splitc(first,rest,'!'); }

#ifdef EBUG
/* return the index'd word without changing 'rest' */
void stridx PROTO3(char *,first,char *,rest,int,index)
{
  char s[510]; int i;
  context;
  strcpy(s,rest);
  for (i=0; i<index; i++) {
    splitc(first,s,' ');
    rmspace(s);
  }
}
#endif

void nsplit PROTO2(char *,first,char *,rest) 
{
  split(first,rest);
  if (first!=NULL) if (!first[0]) { strcpy(first,rest); rest[0]=0; }
}

/* convert "abc!user@a.b.host" into "*!user@*.b.host"
   or "abc!user@1.2.3.4" into "*!user@1.2.3.*"  */
void maskhost PROTO2(char *,s,char *,nw)
{
  char *p,*q,xx[150];
  strcpy(xx,s);
  p=strchr(s,'!'); if (p!=NULL) {
    /* copy username over, quoting '?' and '*' */
    char *dest=xx,*src=p+1;
    while (*src) {
      if ((*src=='*')||(*src=='?')) *dest++='\\';
      *dest++=*src++;
    }
    *dest=0;
    if (strlen(dest)>10) {
      /* truncate */
      p=strchr(s,'@'); if (p!=NULL) {
	if (*(dest+8)=='\\') {
	  *(dest+8)='*'; strcpy(dest+9,p);
	}
	else {
	  *(dest+9)='*'; strcpy(dest+10,p);
	}
      }
    }
  }
  p=strchr(xx,'@'); if (p!=NULL) {
    q=strchr(p,'.');
    if (q==NULL) {
      /* form xx@yy -> very bizarre */
      sprintf(nw,"*!%s",xx); return;
    }
    if (strchr(q+1,'.')==NULL) {
      /* form xx@yy.com -> don't truncate */
      sprintf(nw,"*!%s",xx); return;
    }
    if ((xx[strlen(xx)-1]>='0') && (xx[strlen(xx)-1]<='9')) {
      /* ip number -> xx@#.#.#.* */
      q=strrchr(p,'.'); if (q!=NULL) strcpy(q,".*");
      sprintf(nw,"*!%s",xx); return;
    }
    /* form xx@yy.zz.etc.edu or whatever -> xx@*.zz.etc.edu */
    if (q!=NULL) { *(p+1)='*'; strcpy(p+2,q); }
    sprintf(nw,"*!%s",xx);
  }
  else strcpy(nw,"*");
}

/* copy a file from one place to another (possibly erasing old copy) */
/* returns 0 if OK, 1 if can't open original file, 2 if can't open new */
/* file, 3 if original file isn't normal, 4 if ran out of disk space */
int copyfile PROTO2(char *,oldpath,char *,newpath)
{
  int fi,fo,x; char buf[512]; struct stat st;
  fi=open(oldpath,O_RDONLY,0);
  if (fi<0) return 1;
  fstat(fi,&st);
  if (!(st.st_mode & S_IFREG)) return 3;
  fo=creat(newpath,(int)(st.st_mode & 0777));
  if (fo<0) { close(fi); return 2; }
  for (x=1; x>0; ) {
    x=read(fi,buf,512);
    if (x>0) {
      if (write(fo,buf,x) < x) {     /* couldn't write */
	close(fo); close(fi); unlink(newpath); return 4;
      }
    }
  }
  close(fo); close(fi); return 0;
}

int movefile PROTO2(char *,oldpath,char *,newpath)
{
  int x=copyfile(oldpath,newpath);
  if (x==0) unlink(oldpath);
  return x;
}

/* make nick!~user@host into nick!user@host if necessary */
/* also the new form: nick!+user@host or nick!-user@host */
void fixfrom PROTO1(char *,s)
{
  char nick[NICKLEN],from[UHOSTLEN];
  if (strict_host) return;
  if (s==NULL) return;
  if (strchr(s,'@')==NULL) return;
  strcpy(from,s); splitnick(nick,from);
  /* these are ludicrous. */
  if (strchr("~+-^=",from[0])!=NULL) strcpy(from,&from[1]);
  sprintf(s,"%s!%s",nick,from);
}

/* dump a potentially super-long string of text */
/* assume prefix 20 chars or less */
void dumplots PROTO3(int,idx,char *,prefix,char *,data)
{
  char *p=data,*q,*n,c;
  if (!(*data)) {
    dprintf(idx,"%s\n",prefix); return;
  }
  while (strlen(p)>480) {
    q=p+480;
    /* search for embedded linefeed first */
    n=strchr(p,'\n'); if ((n!=NULL) && (n<q)) {
      /* great! dump that first line then start over */
      *n=0; dprintf(idx,"%s%s\n",prefix,p);
      *n='\n'; p=n+1;
    }
    else {
      /* search backwards for the last space */
      while ((*q!=' ') && (q!=p)) q--;
      if (q==p) q=p+480;
      /* ^ 1 char will get squashed cos there was no space -- too bad */
      c=*q; *q=0; dprintf(idx,"%s%s\n",prefix,p);
      *q=c; p=q+1;
    }
  }
  /* last trailing bit: split by linefeeds if possible */
  n=strchr(p,'\n'); while (n!=NULL) {
    *n=0; dprintf(idx,"%s%s\n",prefix,p);
    *n='\n'; p=n+1; n=strchr(p,'\n');
  }
  if (*p) dprintf(idx,"%s%s\n",prefix,p);   /* last trailing bit */
}

/* convert an interval (in seconds) to one of: */
/* "19 days ago", "1 day ago", "18:12" */
void daysago PROTO3(time_t,now,time_t,then,char *,out)
{
  char s[81];
  if (now-then > 86400) {
    int days=(now-then)/86400;
    sprintf(out,"%d day%s ago",days,(days==1)?"":"s");
    return;
  }
  strcpy(s,ctime(&then));
  s[16]=0; strcpy(out,&s[11]);
}

/* convert an interval (in seconds) to one of: */
/* "in 19 days", "in 1 day", "at 18:12" */
void days PROTO3(time_t,now,time_t,then,char *,out)
{
  char s[81];
  if (now-then > 86400) {
    int days=(now-then)/86400;
    sprintf(out,"in %d day%s",days,(days==1)?"":"s");
    return;
  }
  strcpy(out,"at ");
  strcpy(s,ctime(&now));
  s[16]=0; strcpy(&out[3],&s[11]);
}

/* convert an interval (in seconds) to one of: */
/* "for 19 days", "for 1 day", "for 09:10" */
void daysdur PROTO3(time_t,now,time_t,then,char *,out)
{
  char s[81]; int hrs,mins;
  if (now-then > 86400) {
    int days=(now-then)/86400;
    sprintf(out,"for %d day%s",days,(days==1)?"":"s");
    return;
  }
  strcpy(out,"for "); now-=then;
  hrs=(int)(now/3600); mins=(int)((now-(hrs*3600))/60);
  sprintf(s,"%02d:%02d",hrs,mins);
  strcat(out,s);
}

/***** LOGGING *****/

/* log something */
/* putlog(level,channel_name,format,...);  */
void putlog(va_alist)
va_dcl
{
  va_list va; int i,type; char *format,*chname,s[768],s1[256],*out;
  time_t tt; char ct[81];
  va_start(va); type=va_arg(va,int); chname=va_arg(va,char *);
  format=va_arg(va,char *);
  /* format log entry at offset 8, then i can prepend the timestamp */
  out=&s[8]; vsprintf(out,format,va);
  tt=time(NULL);
  if (keep_all_logs) {
    strcpy(ct,ctime(&tt)); ct[10]=0; strcpy(ct,&ct[8]); ct[7]=0;
    strcpy(&ct[2],&ct[4]); ct[24]=0; strcpy(&ct[5],&ct[22]);
    if (ct[0]==' ') ct[0]='0';
  }
  if ((out[0]) && (shtime)) {
    strcpy(s1,ctime(&tt)); strcpy(s1,&s1[11]); s1[5]=0;
    out=s; s[0]='['; strncpy(&s[1],s1,5); s[6]=']'; s[7]=' ';
  }
  strcat(out,"\n");
  if (!use_stderr) {
    for (i=0; i<MAXLOGS; i++) {
      if ((logs[i].filename!=NULL) && (logs[i].mask & type) &&
	  ((chname[0]=='*') || (logs[i].chname[0]=='*') ||
	   (strcasecmp(chname,logs[i].chname)==0))) {
	if (logs[i].f==NULL) {
	  /* open this logfile */
	  if (keep_all_logs) {
	    sprintf(s1,"%s.%s",logs[i].filename,ct);
	    logs[i].f=fopen(s1,"a+");
	  }
	  else logs[i].f=fopen(logs[i].filename,"a+");
	}
	if (logs[i].f!=NULL) fputs(out,logs[i].f);
      }
    }
  }
  if ((!backgrd) && (!con_chan) && (!term_z)) printf("%s",out);
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_CHAT) && (dcc[i].u.chat->con_flags&type)) {
      if ((chname[0]=='*') || (dcc[i].u.chat->con_chan[0]=='*') ||
	  (strcasecmp(chname,dcc[i].u.chat->con_chan)==0))
	dprintf(i,"%s",out);
    }
  if ((type&LOG_MISC) && (use_stderr)) {
    vsprintf(s,format,va);
    tprintf(STDERR,"%s\n",s);
  }
  va_end(va);
}

/* flush the logfiles to disk */
void flushlogs()
{
  int i;
  for (i=0; i<MAXLOGS; i++)
    if (logs[i].f!=NULL) fflush(logs[i].f);
}

/***** BOT AND HELPBOT SERVER QUEUES *****/

/* queue a msg on one of the msg queues */
struct msgq *q_msg PROTO3(struct msgq *,qq,int,sock,char *,s)
{
  struct msgq *q; int cnt;
  if (qq==NULL) {
    q=(struct msgq *)nmalloc(sizeof(struct msgq));
    q->sock=sock; q->next=NULL;
    q->msg=(char *)nmalloc(strlen(s)+1);
    strcpy(q->msg,s); return q;
  }
  cnt=0; q=qq; while (q->next!=NULL) { q=q->next; cnt++; }
  if (cnt>maxqmsg) return NULL;   /* return null: did not alter queue */
  q->next=(struct msgq *)nmalloc(sizeof(struct msgq));
  q=q->next; q->sock=sock; q->next=NULL;
  q->msg=(char *)nmalloc(strlen(s)+1);
  strcpy(q->msg,s); return qq;
}

/* use when sending msgs... will spread them out so there's no flooding */
void mprintf(va_alist)
va_dcl
{
  char s[1024]; int sock; char *format; va_list va; struct msgq *q;
  static int warned=0;
  va_start(va); sock=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va); va_end(va);
  q=q_msg(mq,sock,s);
#ifdef EBUG_OUTPUT
  if (s[strlen(s)-1]=='\n')
    s[strlen(s)-1]=0;
  debug1("[!m] %s",s);
#endif
  if (q!=NULL) { mq=q; mtot++; warned=0; }
  else {
    if (!warned) putlog(LOG_MISC,"*","!!! OVER MAXIMUM MSG QUEUE");
    warned=1;
  }
}

/* use when sending help msgs (different queue) */
void hprintf(va_alist)
va_dcl
{
  char s[1024]; int sock; char *format; va_list va; struct msgq *q;
  static int warned=0;
  va_start(va); sock=va_arg(va,int); format=va_arg(va,char *);
  vsprintf(s,format,va); va_end(va);
  q=q_msg(hq,sock,s);
#ifdef EBUG_OUTPUT
  if (s[strlen(s)-1]=='\n')
    s[strlen(s)-1]=0;
  debug1("[!h] %s",s);
#endif
  if (q!=NULL) { hq=q; htot++; warned=0; }
  else {
    if (!warned) putlog(LOG_MISC,"*","!!! OVER MAXIMUM HELP QUEUE");
    warned=1;
  }
}

/* called periodically to shove out another queued item */
void deq_msg()
{
  static which=0;    /* to alternate which queue is pushed */
  struct msgq *q;
  which=(which+1)%2;
  q=(which ? hq : mq);
  if (q==NULL) q=(which ? mq : hq);    /* chosen one is empty? switch off */
  if (q==NULL) return;       /* both queues empty */
  tputs(q->sock,q->msg,strlen(q->msg));
  if (q==mq) { mq=mq->next; mtot--; }
  else { hq=hq->next; htot--; }
  nfree(q->msg); nfree(q);
}

/* clean out the msg queues (like when changing servers) */
void empty_msgq()
{
  struct msgq *q,*qq;
  q=mq; while (q!=NULL) {
    qq=q->next;
    nfree(q->msg); nfree(q);
    q=qq;
  }
  q=hq; while (q!=NULL) {
    qq=q->next;
    nfree(q->msg); nfree(q);
    q=qq;
  }
  mtot=htot=0; mq=hq=NULL;
}

/***** RESYNC BUFFERS *****/

/* create a tandem buffer for 'bot' */
void new_tbuf PROTO1(char *,bot)
{
  int i;
  for (i=0; i<5; i++) if (tbuf[i].bot[0]==0) {
    /* this one is empty */
    strcpy(tbuf[i].bot,bot); tbuf[i].q=NULL;
    tbuf[i].timer=time(NULL);
    putlog(LOG_MISC,"*","Creating resync buffer for %s",bot);
    return;
  }
}

/* flush a certain bot's tbuf */
int flush_tbuf PROTO1(char *,bot)
{
  int i; struct msgq *q;
  for (i=0; i<5; i++) if (strcasecmp(tbuf[i].bot,bot)==0) {
    while (tbuf[i].q!=NULL) {
      q=tbuf[i].q; tbuf[i].q=tbuf[i].q->next;
      nfree(q->msg); nfree(q);
    }
    tbuf[i].bot[0]=0; return 1;
  }
  return 0;
}

/* flush all tbufs older than 15 minutes */
void check_expired_tbufs()
{
  int i; time_t now=time(NULL); struct msgq *q;
  for (i=0; i<5; i++) if (tbuf[i].bot[0]) {
    if (now-tbuf[i].timer > 900) {
      /* EXPIRED */
      while (tbuf[i].q!=NULL) {
	q=tbuf[i].q; tbuf[i].q=tbuf[i].q->next;
	nfree(q->msg); nfree(q);
      }
      putlog(LOG_MISC,"*","Flushing resync buffer for clonebot %s.",
	     tbuf[i].bot);
      tbuf[i].bot[0]=0;
    }
  }
}

/* add stuff to a specific bot's tbuf */
void q_tbuf PROTO2(char *,bot,char *,s)
{
  int i; struct msgq *q;
  for (i=0; i<5; i++) if (strcasecmp(tbuf[i].bot,bot)==0) {
    q=q_msg(tbuf[i].q,0,s); if (q!=NULL) tbuf[i].q=q;
  }
}

/* add stuff to the resync buffers */
void q_resync PROTO1(char *,s)
{
  int i; struct msgq *q;
  for (i=0; i<5; i++) if (tbuf[i].bot[0]) {
    q=q_msg(tbuf[i].q,0,s); if (q!=NULL) tbuf[i].q=q;
  }
}

/* is bot in resync list? */
int can_resync PROTO1(char *,bot)
{
  int i;
  for (i=0; i<5; i++) if (strcasecmp(bot,tbuf[i].bot)==0) return 1;
  return 0;
}

/* dump the resync buffer for a bot */
void dump_resync PROTO2(int,z,char *,bot)
{
  int i; struct msgq *q;
  for (i=0; i<5; i++) if (strcasecmp(bot,tbuf[i].bot)==0) {
    while (tbuf[i].q!=NULL) {
      q=tbuf[i].q; tbuf[i].q=tbuf[i].q->next;
      tprintf(z,"%s",q->msg);
      nfree(q->msg); nfree(q);
    }
    tbuf[i].bot[0]=0; return;
  }
}

/* give status report on tbufs */
void status_tbufs PROTO1(int,idx)
{
  int i,count; struct msgq *q; char s[121];
  s[0]=0;
  for (i=0; i<5; i++) if (tbuf[i].bot[0]) {
    strcat(s,tbuf[i].bot); count=0;
    q=tbuf[i].q; while (q!=NULL) { count++; q=q->next; }
    sprintf(&s[strlen(s)]," (%d), ",count);
  }
  if (s[0]) {
    s[strlen(s)-2]=0;
    dprintf(idx,"Pending sharebot buffers: %s\n",s);
  }
}

/********** STRING SUBSTITUTION **********/

static int cols=0;
static int colsofar=0;
static int blind=0;
static int subwidth=70;
static char *colstr=NULL;

/* add string to colstr */
void subst_addcol PROTO2(char *,s,char *,newcol)
{
  char *p,*q; int i,colwidth;
  if ((newcol[0]) && (newcol[0]!='\377')) colsofar++;
  colstr=nrealloc(colstr,strlen(colstr)+strlen(newcol)+(colstr[0]?2:1));
  if ((newcol[0]) && (newcol[0]!='\377')) {
    if (colstr[0]) strcat(colstr,"\377");
    strcat(colstr,newcol);
  }
  if ((colsofar==cols) || ((newcol[0]=='\377') && (colstr[0]))) {
    colsofar=0; strcpy(s,"     "); colwidth=(subwidth-5)/cols;
    q=colstr; p=strchr(colstr,'\377');
    while (p!=NULL) {
      *p=0; strcat(s,q);
      for (i=strlen(q); i<colwidth; i++) strcat(s," ");
      q=p+1; p=strchr(q,'\377');
    }
    strcat(s,q); nfree(colstr);
    colstr=(char *)nmalloc(1); colstr[0]=0;
  }
}

/* substitute %x codes in help files */
/* %B = bot nickname */
/* %V = version */
/* %C = list of channels i monitor */
/* %E = eggdrop banner */
/* %A = admin line */
/* %T = current time ("14:15") */
/* %N = user's nickname */
/* %{+xy}     require flags to read this section */
/* %{center}  center this line */
/* %{cols=N}  start of columnated section (indented) */
/* %{end}     end of section */
void help_subst PROTO4(char *,s,char *,nick,int,flags,int,isdcc)
{
  char xx[512],sub[161],*p,*q,c; int i,j,center=0; time_t tt;
  if (s==NULL) {
    /* used to reset substitutions */
    blind=0; cols=0; subwidth=70;
    if (colstr!=NULL) { nfree(colstr); colstr=NULL; }
    return;
  }
  strcpy(xx,s); s[0]=0;
  p=strchr(xx,'%');
  while (p!=NULL) {
    c=*(p+1); sub[0]=0; *p=0; if (!blind) strcat(s,xx);
    switch(c) {
    case 'B': strcpy(sub,(isdcc?botnetnick:botname)); break;
    case 'V': strcpy(sub,ver); break;
    case 'C': getchanlist(sub,160); break;
    case 'E': strcpy(sub,version); break;
    case 'A': strcpy(sub,admin); break;
    case 'T': tt=time(NULL); strcpy(sub,ctime(&tt));
      strcpy(sub,&sub[11]); sub[5]=0; break;
    case 'N': strcpy(sub,nick); break;
    case '{':
      q=p; p++; while ((*p!='}') && (*p)) p++;
      if (*p) {
	*p=0; p--; q+=2;
	/* now q is the string and p is where the rest of the fcn expects */
	if (q[0]=='+') {
	  int reqflags=str2flags(q);
	  if (!flags_ok(reqflags,flags)) blind=1;
	  else blind = 0;
	}
	if (q[0]=='-') blind=0;
	if (strcasecmp(q,"end")==0) {
	  blind=0; subwidth=70;
	  if (cols) {
	    subst_addcol(s,"\377"); nfree(colstr); colstr=NULL; cols=0;
	  }
	}
	if (strcasecmp(q,"center")==0) center=1;
	if (strncmp(q,"cols=",5)==0) {
	  char *r;
	  cols=atoi(q+5); colsofar=0;
	  colstr=(char *)nmalloc(1); colstr[0]=0;
	  r=strchr(q+5,'/');
	  if (r!=NULL) subwidth=atoi(r+1);
	}
      }
      else p=q;  /* no } so ignore */
      break;
    case '%': strcpy(sub,"%"); break;
    default: strcpy(sub,"%_"); sub[1]=c; break;
    }
    i=strlen(sub);
    if (!blind) strcat(s,sub);
    if (c!=0) strcpy(xx,p+2); else xx[0]=0;
    p=strchr(xx,'%');
  }
  if (!blind) strcat(s,xx);
  if (strlen(s)>120) s[120]=0;
  if (center) {
    strcpy(xx,s); i=35-(strlen(xx)/2);
    if (i>0) { 
      s[0]=0; for (j=0; j<i; j++) s[j]=' ';
      s[i]=0; strcat(s,xx);
    }
  }
  if (cols) {
    strcpy(xx,s); s[0]=0;
    subst_addcol(s,xx);
  }
}

void showhelp PROTO3(char *,who,char *,file,int,flags)
{
  FILE *f; char s[1024],*p; int lines=0;
  for (p=file; *p!=0; p++) {
    if ((*p==' ') || (*p=='.')) *p='/';
    if (*p=='-') *p='_';
    if (*p=='+') *p='P';
  }
  sprintf(s,"%s%s",helpdir,file); s[256]=0;
  if (!is_file(s)) {
    strcat(s,"/"); strcat(s,file);
    if (!is_file(s)) {
      hprintf(serv,"NOTICE %s :No help available on that.\n",who);
      return;
    }
  }
  f=fopen(s,"r");
  if (f==NULL) {
    hprintf(serv,"NOTICE %s :No help available on that.\n",who);
    return;
  }
  help_subst(NULL,NULL,0,0);   /* clear flags */
  while (!feof(f)) {
    fgets(s,120,f);
    if (!feof(f)) {
      if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
      if (!s[0]) strcpy(s," ");
      help_subst(s,who,flags,0);
      if (s[0]) { hprintf(serv,"NOTICE %s :%s\n",who,s); lines++; }
    }
  }
  fclose(f);
  if (!lines) hprintf(serv,"NOTICE %s :No help available on that.\n",who);
}

void showtext PROTO3(char *,who,char *,file,int,flags)
{
  FILE *f; char s[1024],*p;
  for (p=file; *p!=0; p++) if ((*p==' ') || (*p=='.')) *p='/';
  sprintf(s,"%s%s",textdir,file); s[256]=0;
  f=fopen(s,"r");
  if (f==NULL) return;
  if (!is_file(s)) {
    fclose(f);
    hprintf(serv,"NOTICE %s :'%s' is not a normal file!\n",who,s);
    return;
  }
  help_subst(NULL,NULL,0,0);
  while (!feof(f)) {
    fgets(s,120,f);
    if (!feof(f)) {
      if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
      if (!s[0]) strcpy(s," ");
      help_subst(s,who,flags,0);
      if (s[0]) hprintf(serv,"NOTICE %s :%s\n",who,s);
    }
  }
  fclose(f);
}

void tellhelp PROTO3(int,idx,char *,file,int,flags)
{
  FILE *f; char s[1024],*p; int lines=0;
  for (p=file; *p!=0; p++) {
    if ((*p==' ') || (*p=='.')) *p='/';
    if (*p=='-') *p='_';
    if (*p=='+') *p='P';
  }
  sprintf(s,"%sdcc/%s",helpdir,file); s[256]=0;
  if (!is_file(s)) {
    strcat(s,"/"); strcat(s,file);
    if (!is_file(s)) {
      dprintf(idx,"No help available on that.\n");
      return;
    }
  }
  f=fopen(s,"r");
  if (f==NULL) {
    dprintf(idx,"No help available on that.\n");
    return;
  }
  help_subst(NULL,NULL,0,1);
  while (!feof(f)) {
    fgets(s,120,f);
    if (!feof(f)) {
      if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
      if (!s[0]) strcpy(s," ");
      help_subst(s,dcc[idx].nick,flags,1);
      if (s[0]) { dprintf(idx,"%s\n",s); lines++; }
    }
  }
  if (!lines) dprintf(idx,"No help available on that.\n");
  fclose(f);
}

void telltext PROTO3(int,idx,char *,file,int,flags)
{
  FILE *f; char s[1024],*p;
  for (p=file; *p!=0; p++) if ((*p==' ') || (*p=='.')) *p='/';
  sprintf(s,"%s%s",textdir,file); s[256]=0;
  f=fopen(s,"r");
  if (f==NULL) return;
  if (!is_file(s)) {
    fclose(f);
    dprintf(idx,"### '%s' is not a normal file!\n",s);
    return;
  }
  help_subst(NULL,NULL,0,1);
  while (!feof(f)) {
    fgets(s,120,f);
    if (!feof(f)) {
      if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
      if (!s[0]) strcpy(s," ");
      help_subst(s,dcc[idx].nick,flags,1);
      if (s[0]) dprintf(idx,"%s\n",s);
    }
  }
  fclose(f);
}

/* show motd to dcc chatter */
void show_motd PROTO1(int,idx)
{
  FILE *vv; char s[1024]; int atr;
  atr=get_attr_handle(dcc[idx].nick);
  vv=fopen(motdfile,"r"); if (vv!=NULL) {
    if (!is_file(motdfile)) {
      fclose(vv);
      dprintf(idx,"### MOTD is not a normal file!\n");
      return;
    }
    dprintf(idx,"\n");
    help_subst(NULL,NULL,0,1);
    while (!feof(vv)) {
      fgets(s,120,vv);
      if (!feof(vv)) {
	if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
	if (!s[0]) strcpy(s," ");
	help_subst(s,dcc[idx].nick,atr,1);
	if (s[0]) dprintf(idx,"%s\n",s);
      }
    }
    fclose(vv);
    dprintf(idx,"\n");
  }
}
