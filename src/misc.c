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

#include "main.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <varargs.h>
#include "chan.h"
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

extern int serv;
extern int dcc_total;
extern struct dcc_t * dcc;
extern char helpdir[];
extern char version[];
extern char origbotname[];
extern char admin[];
extern int require_p;
extern int backgrd;
extern int con_chan;
extern int term_z;
extern int use_stderr;
extern char motdfile[];
extern char ver[];
extern char textdir[];
extern int keep_all_logs;
extern char botnetnick[];
extern struct chanset_t *chanset;
extern time_t now;

/* whether or not to display the time with console output */
int shtime = 1;
/* logfiles */
log_t * logs = 0;
/* current maximum log files */
int max_logs = 5;
/* console mask */
int conmask = LOG_MODES | LOG_CMDS | LOG_MISC;
/* disply output to server to LOG_SERVEROUT */
int debug_output = 0;

struct help_list {
   struct help_list * next;
   char * name;
   int type;
};

static struct help_ref {
   char * name;
   struct help_list * first;
   struct help_ref * next;
} * help_list = NULL;

/* expected memory usage */
int expmem_misc()
{
   struct help_ref * current;
   struct help_list * item;
   int tot = 0;
   
   for (current = help_list; current; current = current->next) {
      tot += sizeof(struct help_ref) + strlen(current->name) + 1;
      for (item = current->first;item;item = item->next)
	 tot += sizeof(struct help_list) + strlen(item->name) + 1;
   }
   return tot + (max_logs * sizeof(log_t));
}

void init_misc()
{
   static int last = 0;
   if (max_logs < 1)
     max_logs = 1;
   if (logs)
     logs = nrealloc(logs,max_logs * sizeof(log_t));
   else
     logs = nmalloc(max_logs * sizeof(log_t));
   for (; last < max_logs; last++) {
      logs[last].filename = logs[last].chname = NULL;
      logs[last].mask = 0;
      logs[last].f = NULL;
   }
}

/***** MISC FUNCTIONS *****/

/* low-level stuff for other modules */
static int is_file (char * s)
{
   struct stat ss;
   int i = stat(s, &ss);
   if (i < 0)
      return 0;
   if ((ss.st_mode & S_IFREG) || (ss.st_mode & S_IFLNK))
      return 1;
   return 0;
}

#define upcase(c) (((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c))

/* determine if littles is contained in bigs (ignoring case) */
/* if so: return pointer to the littles in bigs */
/* if not: return NULL */
char *stristr (char * bigs, char * littles)
{
   char *st = bigs, *p, *q;
   while (1) {
      if (!*st)
	 return NULL;
      p = littles;
      q = st;
      while ((*p) && (*q) && (upcase(*p) == upcase(*q))) {
	 p++;
	 q++;
      }
      if ((!*q) && (*p))
	 return NULL;		/* premature end of bigs */
      if (!*p)
	 return st;		/* found it! */
      st++;			/* try again */
   }
}

#if !HAVE_STRCASECMP
/* unixware has no strcasecmp() without linking in a hefty library */
int strcasecmp (char * s1, char * s2)
{
   while ((*s1) && (*s2) && (upcase(*s1) == upcase(*s2))) {
      s1++;
      s2++;
   }
   return upcase(*s1) - upcase(*s2);
}
#endif

int my_strcpy(char * a, char * b) {
   char * c = b;
   while (*b) 
     *a++ = *b++;
   *a = *b;
   return b-c;
}

/* split first word off of rest and put it in first */
void splitc (char * first, char * rest, char divider)
{
   char *p;
   p = strchr(rest, divider);
   if (p == NULL) {
      if ((first != rest) && (first != NULL))
	 first[0] = 0;
      return;
   }
   *p = 0;
   if (first != NULL)
      strcpy(first, rest);
   if (first != rest)
      strcpy(rest, p + 1);
}


char * splitnick ( char ** blah ) {
   char * p = strchr(*blah,'!'), *q = *blah;
   if (p) {
      *p = 0;
      *blah = p + 1;
      return q;
   }
   return "";
}

char * newsplit ( char ** rest ) {
   register char * o, * r;
   
   if (!rest) 
     return *rest = "";
   o = *rest;
   while (*o == ' ')
     o++;
   r = o;
   while (*o && (*o != ' '))
     o++;
   if (*o)
     *o++ = 0;
   else
     *o = 0;
   *rest = o;
   return r;
}

#ifdef EBUG
/* return the index'd word without changing 'rest' */
void stridx (char * first, char * rest, int index)
{
   char s[510];
   int i;
   context;
   strcpy(s, rest);
   for (i = 0; i < index; i++) {
      splitc(first, s, ' ');
      rmspace(s);
   }
}
#endif

/* convert "abc!user@a.b.host" into "*!user@*.b.host"
   or "abc!user@1.2.3.4" into "*!user@1.2.3.*"  */
void maskhost (char * s, char * nw)
{
   char *p, *q, xx[150];
   strcpy(xx, s);
   p = strchr(s, '!');
   if (p != NULL) {
      /* copy username over, quoting '?' and '*' */
      char *dest = xx, *src = p + 1;
      while (*src) {
	 if ((*src == '*') || (*src == '?'))
	    *dest++ = '\\';
	 *dest++ = *src++;
      }
      *dest = 0;
      if (strlen(dest) > 10) {
	 /* truncate */
	 p = strchr(s, '@');
	 if (p != NULL) {
	    if (*(dest + 8) == '\\') {
	       *(dest + 8) = '*';
	       strcpy(dest + 9, p);
	    } else {
	       *(dest + 9) = '*';
	       strcpy(dest + 10, p);
	    }
	 }
      }
   }
   p = strchr(xx, '@');
   if (p != NULL) {
      q = strchr(p, '.');
      if (q == NULL) {
	 /* form xx@yy -> very bizarre */
	 sprintf(nw, "*!%s", xx);
	 return;
      }
      if (strchr(q + 1, '.') == NULL) {
	 /* form xx@yy.com -> don't truncate */
	 sprintf(nw, "*!%s", xx);
	 return;
      }
      if ((xx[strlen(xx) - 1] >= '0') && (xx[strlen(xx) - 1] <= '9')) {
	 /* ip number -> xx@#.#.#.* */
	 q = strrchr(p, '.');
	 if (q != NULL)
	    strcpy(q, ".*");
	 sprintf(nw, "*!%s", xx);
	 return;
      }
      /* form xx@yy.zz.etc.edu or whatever -> xx@*.zz.etc.edu */
      if (q != NULL) {
	 *(p + 1) = '*';
	 strcpy(p + 2, q);
      }
      sprintf(nw, "*!%s", xx);
   } else
      strcpy(nw, "*");
}

/* copy a file from one place to another (possibly erasing old copy) */
/* returns 0 if OK, 1 if can't open original file, 2 if can't open new */
/* file, 3 if original file isn't normal, 4 if ran out of disk space */
int copyfile (char * oldpath, char * newpath)
{
   int fi, fo, x;
   char buf[512];
   struct stat st;
   fi = open(oldpath, O_RDONLY, 0);
   if (fi < 0)
      return 1;
   fstat(fi, &st);
   if (!(st.st_mode & S_IFREG))
      return 3;
   fo = creat(newpath, (int) (st.st_mode & 0777));
   if (fo < 0) {
      close(fi);
      return 2;
   }
   for (x = 1; x > 0;) {
      x = read(fi, buf, 512);
      if (x > 0) {
	 if (write(fo, buf, x) < x) {	/* couldn't write */
	    close(fo);
	    close(fi);
	    unlink(newpath);
	    return 4;
	 }
      }
   }
   close(fo);
   close(fi);
   return 0;
}

int movefile (char * oldpath, char * newpath)
{
   int x = copyfile(oldpath, newpath);
   if (x == 0)
      unlink(oldpath);
   return x;
}

/* dump a potentially super-long string of text */
/* assume prefix 20 chars or less */
void dumplots (int idx, char * prefix, char * data)
{
   char *p = data, *q, *n, c;
   if (!(*data)) {
      dprintf(idx, "%s\n", prefix);
      return;
   }
   while (strlen(p) > 480) {
      q = p + 480;
      /* search for embedded linefeed first */
      n = strchr(p, '\n');
      if ((n != NULL) && (n < q)) {
	 /* great! dump that first line then start over */
	 *n = 0;
	 dprintf(idx, "%s%s\n", prefix, p);
	 *n = '\n';
	 p = n + 1;
      } else {
	 /* search backwards for the last space */
	 while ((*q != ' ') && (q != p))
	    q--;
	 if (q == p)
	    q = p + 480;
	 /* ^ 1 char will get squashed cos there was no space -- too bad */
	 c = *q;
	 *q = 0;
	 dprintf(idx, "%s%s\n", prefix, p);
	 *q = c;
	 p = q + 1;
      }
   }
   /* last trailing bit: split by linefeeds if possible */
   n = strchr(p, '\n');
   while (n != NULL) {
      *n = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *n = '\n';
      p = n + 1;
      n = strchr(p, '\n');
   }
   if (*p)
      dprintf(idx, "%s%s\n", prefix, p);	/* last trailing bit */
}

/* convert an interval (in seconds) to one of: */
/* "19 days ago", "1 day ago", "18:12" */
void daysago (time_t now, time_t then, char * out)
{
   char s[81];
   if (now - then > 86400) {
      int days = (now - then) / 86400;
      sprintf(out, "%d day%s ago", days, (days == 1) ? "" : "s");
      return;
   }
   strcpy(s, ctime(&then));
   s[16] = 0;
   strcpy(out, &s[11]);
}

/* convert an interval (in seconds) to one of: */
/* "in 19 days", "in 1 day", "at 18:12" */
void days (time_t now, time_t then, char * out)
{
   char s[81];
   if (now - then > 86400) {
      int days = (now - then) / 86400;
      sprintf(out, "in %d day%s", days, (days == 1) ? "" : "s");
      return;
   }
   strcpy(out, "at ");
   strcpy(s, ctime(&now));
   s[16] = 0;
   strcpy(&out[3], &s[11]);
}

/* convert an interval (in seconds) to one of: */
/* "for 19 days", "for 1 day", "for 09:10" */
void daysdur (time_t now, time_t then, char * out)
{
   char s[81];
   int hrs, mins;
   if (now - then > 86400) {
      int days = (now - then) / 86400;
      sprintf(out, "for %d day%s", days, (days == 1) ? "" : "s");
      return;
   }
   strcpy(out, "for ");
   now -= then;
   hrs = (int) (now / 3600);
   mins = (int) ((now - (hrs * 3600)) / 60);
   sprintf(s, "%02d:%02d", hrs, mins);
   strcat(out, s);
}

/***** LOGGING *****/

/* log something */
/* putlog(level,channel_name,format,...);  */
void putlog(va_alist)
va_dcl
{
   va_list va;
   int i, type;
   char *format, *chname, s[768], s1[256], *out;
   time_t tt;
   char ct[81];
   va_start(va);
   type = va_arg(va, int);
   chname = va_arg(va, char *);
   format = va_arg(va, char *);
   /* format log entry at offset 8, then i can prepend the timestamp */
   out = &s[8];
#ifdef HAVE_VSNPRINTF
   if (vsnprintf(out, 759, format, va) < 0)
     out[767] = 0;
#else
   vsprintf(out, format, va);
#endif
   tt = now;
   if (keep_all_logs) {
      strcpy(ct, ctime(&tt));
      ct[10] = 0;
      strcpy(ct, &ct[8]);
      ct[7] = 0;
      strcpy(&ct[2], &ct[4]);
      ct[24] = 0;
      strcpy(&ct[5], &ct[22]);
      if (ct[0] == ' ')
	 ct[0] = '0';
   }
   if ((out[0]) && (shtime)) {
      strcpy(s1, ctime(&tt));
      strcpy(s1, &s1[11]);
      s1[5] = 0;
      out = s;
      s[0] = '[';
      strncpy(&s[1], s1, 5);
      s[6] = ']';
      s[7] = ' ';
   }
   strcat(out, "\n");
   if (!use_stderr) {
      for (i = 0; i < max_logs; i++) {
	 if ((logs[i].filename != NULL) && (logs[i].mask & type) &&
	     ((chname[0] == '*') || (logs[i].chname[0] == '*') ||
	      (strcasecmp(chname, logs[i].chname) == 0))) {
	    if (logs[i].f == NULL) {
	       /* open this logfile */
	       if (keep_all_logs) {
		  sprintf(s1, "%s.%s", logs[i].filename, ct);
		  logs[i].f = fopen(s1, "a+");
	       } else
		  logs[i].f = fopen(logs[i].filename, "a+");
	    }
	    if (logs[i].f != NULL)
	       fputs(out, logs[i].f);
	 }
      }
   }
   if ((!backgrd) && (!con_chan) && (!term_z))
      printf("%s", out);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->con_flags & type)) {
	 if ((chname[0] == '*') || (dcc[i].u.chat->con_chan[0] == '*') ||
	     (strcasecmp(chname, dcc[i].u.chat->con_chan) == 0))
	    dprintf(i, "%s", out);
      }
   if ((type & LOG_MISC) && use_stderr) {
      if (shtime)
	out += 8;
      dprintf(DP_STDERR, "%s", s);
   }
   va_end(va);
}

/* flush the logfiles to disk */
void flushlogs()
{
   int i;
   context;
   for (i = 0; i < max_logs; i++)
      if (logs[i].f != NULL)
	 fflush(logs[i].f);
   context;
}

/********** STRING SUBSTITUTION **********/

static int cols = 0;
static int colsofar = 0;
static int blind = 0;
static int subwidth = 70;
static char *colstr = NULL;

/* add string to colstr */
static void subst_addcol (char * s, char * newcol)
{
   char *p, *q;
   int i, colwidth;
   if ((newcol[0]) && (newcol[0] != '\377'))
      colsofar++;
   colstr = nrealloc(colstr, strlen(colstr) + strlen(newcol) + (colstr[0] ? 2 : 1));
   if ((newcol[0]) && (newcol[0] != '\377')) {
      if (colstr[0])
	 strcat(colstr, "\377");
      strcat(colstr, newcol);
   }
   if ((colsofar == cols) || ((newcol[0] == '\377') && (colstr[0]))) {
      colsofar = 0;
      strcpy(s, "     ");
      colwidth = (subwidth - 5) / cols;
      q = colstr;
      p = strchr(colstr, '\377');
      while (p != NULL) {
	 *p = 0;
	 strcat(s, q);
	 for (i = strlen(q); i < colwidth; i++)
	    strcat(s, " ");
	 q = p + 1;
	 p = strchr(q, '\377');
      }
      strcat(s, q);
      nfree(colstr);
      colstr = (char *) nmalloc(1);
      colstr[0] = 0;
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
/* %U = display system name if possible */
/* %{+xy}     require flags to read this section */
/* %{-}       turn of required flag matching only */
/* %{center}  center this line */
/* %{cols=N}  start of columnated section (indented) */
/* %{help=TOPIC} start a section for a particular command */
/* %{end}     end of section */
#define HELP_BUF_LEN 256
#define HELP_BOLD  1
#define HELP_REV   2
#define HELP_UNDER 4
#define HELP_FLASH 8
#define HELP_IRC   16
void help_subst (char * s, char * nick, struct flag_record * flags,
		 int isdcc, char * topic)
{
   char xx[HELP_BUF_LEN+1], sub[161], *current, *q, chr, *writeidx,
     *readidx, *towrite;
   struct chanset_t *chan;
   int i, j, center = 0;
   static int help_flags;
#ifdef HAVE_UNAME
   struct utsname uname_info;
#endif
	  
   if (s == NULL) {
      /* used to reset substitutions */
      blind = 0;
      cols = 0;
      subwidth = 70;
      if (colstr != NULL) {
			nfree(colstr);
			colstr = NULL;
      }
      help_flags = isdcc;
      return;
   }
   strncpy(xx,s,HELP_BUF_LEN);
   xx[HELP_BUF_LEN]=0;
   readidx = xx;
   writeidx = s;
   current = strchr(readidx, '%');
   while (current) {
      /* are we about to copy a chuck to the end of the buffer? 
       * if so return */
      if ((writeidx + (current - readidx)) >= (s + HELP_BUF_LEN)) {
	 strncpy(writeidx,readidx,(s + HELP_BUF_LEN) - writeidx);
	 s[HELP_BUF_LEN] = 0;
	 return;
      }      
      chr = *(current + 1);
      *current = 0;
      if (!blind)    
		  writeidx += my_strcpy(writeidx, readidx);
      towrite = NULL;
      switch (chr) {
	case 'b':
	 if (glob_hilite(*flags)) 
	   if (help_flags & HELP_IRC) {
	      towrite = "\002";
	   } else if (help_flags & HELP_BOLD) {
	      help_flags &= ~HELP_BOLD;
	      towrite = "\033[0m";
	   } else {
	      help_flags |= HELP_BOLD;
	      towrite = "\033[1m";
	   }
	 break;
	case 'v':
	 if (glob_hilite(*flags)) 
	   if (help_flags & HELP_IRC) {
	      towrite = "\026";
	   } else if (help_flags & HELP_REV) {
	      help_flags &= ~HELP_REV;
	      towrite = "\033[0m";
	   } else {
	      help_flags |= HELP_REV;
	      towrite = "\033[7m";
	   }
	 break;
	case '_':
	 if (glob_hilite(*flags)) 
	   if (help_flags & HELP_IRC) {
	      towrite = "\037";
	   } else if (help_flags & HELP_UNDER) {
	      help_flags &= ~HELP_UNDER;
	      towrite = "\033[0m"; 
	   } else {
	      help_flags |= HELP_UNDER;
	      towrite = "\033[4m";
	   }
	 break;
	case 'f':
	 if (glob_hilite(*flags))
	   if (help_flags & HELP_FLASH) {
	      if (help_flags & HELP_IRC) {
		 towrite = "\002\037";
	      } else {
		 towrite = "\033[0m";
	      }
	      help_flags &= ~HELP_FLASH;
	   } else {
	      help_flags |= HELP_FLASH;
	      if (help_flags & HELP_IRC) {
		 towrite = "\037\002";
	      } else {
		 towrite = "\033[5m";
	      }
	   }
	 break;
	case 'U':
#ifdef HAVE_UNAME
	 if (!uname(&uname_info)) {
	    simple_sprintf(sub, "%s %s",uname_info.sysname,
			   uname_info.release);
	    towrite = sub;
	 } else
#endif
	   towrite = "*UNKNOWN*";
	 break;
	case 'B':
	 towrite = (isdcc ? botnetnick : origbotname);
	 break;
	case 'V':
	 towrite = ver;
	 break;
	case 'E':
	 towrite = version;
	 break;
	case 'A':
	 towrite = admin;
	 break;
	case 'T':
	 strcpy(sub, ctime(&now));
	 sub[16] = 0;
	 towrite = sub+11;
	 break;
	case 'N':
	 towrite = strchr(nick,':');
	 if (towrite)
	   towrite++;
	 else
	   towrite = nick;
	 break;
	case 'C':
	 if (!blind) 
	   for (chan = chanset;chan; chan = chan->next) {
	      if ((strlen(chan->name) + writeidx + 2) >=
		  (s + HELP_BUF_LEN)) {
		 strncpy(writeidx,chan->name,(s + HELP_BUF_LEN) - writeidx);
		 s[HELP_BUF_LEN] = 0;
		 return;
	      }
	      writeidx += my_strcpy(writeidx,chan->name);
	      if (chan->next) {
		 *writeidx++ = ',';
		 *writeidx++ = ' ';
	      }
	   }
	 break;
	case '{':
	 q = current;
	 current++;
	 while ((*current != '}') && (*current))
	   current++;
	 if (*current) {
	    *current = 0;
	    current--;
	    q += 2;
	    /* now q is the string and p is where the rest of the fcn expects */
	    if (!strncmp(q, "help=",5)) {
	       if (topic && strcasecmp(q+5,topic))
		 blind |= 2;
	       else
		 blind &= ~2;
	    } else if (!(blind & 2)) {
	       if (q[0] == '+') {
		  struct flag_record fr = {FR_GLOBAL|FR_CHAN,0,0,0,0,0};
		  break_down_flags(q+1,&fr,NULL);
		  if (!flagrec_ok(&fr, flags))
		    blind |= 1;
		  else 
		    blind &= ~1;
	       } else if (q[0] == '-') {
		  blind &= ~1;
	       } else if (strcasecmp(q, "end") == 0) {
		  blind &= ~1;
		  subwidth = 70;
		  if (cols) {
		     subst_addcol(s, "\377");
		     nfree(colstr);
		     colstr = NULL;
		     cols = 0;
		  }
	       } else if (!strcasecmp(q, "center"))
		    center = 1;
	       else if (!strncmp(q, "cols=", 5)) {
		  char * r;
		  cols = atoi(q + 5);
		  colsofar = 0;
		  colstr = (char *) nmalloc(1);
		  colstr[0] = 0;
		  r = strchr(q + 5, '/');
		  if (r != NULL)
		    subwidth = atoi(r + 1);
	       }
	    }
	 } else
	   current = q;		/* no } so ignore */
	 break;
	default:
	 if (!blind) {
	    *writeidx++ = chr;
	    if (writeidx >= (s + HELP_BUF_LEN)) {
	       *writeidx = 0;
	       return;
	    }
	 }
      }
      if (towrite && !blind) {
	 if ((writeidx + strlen(towrite)) >= (s + HELP_BUF_LEN)) {
	    strncpy(writeidx,towrite,(s + HELP_BUF_LEN) - writeidx);
	    s[HELP_BUF_LEN] = 0;
	    return;
	 }
	 writeidx += my_strcpy(writeidx,towrite);
      }
      if (chr) {
	 readidx = current + 2;
	 current = strchr(readidx,'%');
      } else {
	 readidx = current + 1;
	 current = NULL;
      }
   }
   if (!blind) {
      i = strlen(readidx);
      if (i && ((writeidx + i) >= (s + HELP_BUF_LEN))) {
			strncpy(writeidx,readidx,(s + HELP_BUF_LEN) - writeidx);
			s[HELP_BUF_LEN] = 0;
			return;
      }      
      strcpy(writeidx,readidx);
   } else
     *writeidx = 0;
   if (center) {
      strcpy(xx, s);
      i = 35 - (strlen(xx) / 2);
      if (i > 0) {
	 s[0] = 0;
	 for (j = 0; j < i; j++)
	   s[j] = ' ';
	 strcpy(s+i, xx);
      }
   }
   if (cols) {
      strcpy(xx, s);
      s[0] = 0;
      subst_addcol(s, xx);
   }
}

static void scan_help_file (struct help_ref * current, char * filename, 
			    int type) {
   FILE * f;
   char s[HELP_BUF_LEN + 1], *p, *q;
   struct help_list * list;

   if (is_file(filename) && (f = fopen(filename,"r"))) {
      while (!feof(f)) {
	 fgets(s, HELP_BUF_LEN, f);
	 if (!feof(f)) {
	    p = s;
	    while ((q = strstr(p,"%{help="))) {
	       q += 7;
	       if ((p = strchr(q,'}'))) {
		  *p = 0;
		  list = nmalloc(sizeof(struct help_list));
		  list->name = nmalloc(p - q + 1);
		  strcpy(list->name,q);
		  list->next = current->first;
		  list->type = type;
		  current->first = list;
		  p++;
	       } else 
		 p = "";
	    }
	 }
      }
      fclose(f);
   }
}
	       
void add_help_reference (char * file) {
   char s[1024];
   struct help_ref * current;
   
   for (current = help_list; current; current = current->next) 
     if (!strcmp(current->name,file))
       return; /* already exists, can't re-add :P */
   current = nmalloc(sizeof(struct help_ref));
   current->name = nmalloc(strlen(file)+1);
   strcpy(current->name,file);
   current->next = help_list;
   current->first = NULL;
   help_list = current;
   simple_sprintf(s,"%smsg/%s",helpdir, file);
   scan_help_file(current,s,0);
   simple_sprintf(s,"%s%s",helpdir, file);
   scan_help_file(current,s,1);
   simple_sprintf(s,"%sset/%s",helpdir, file);
   scan_help_file(current,s,2);
};

void rem_help_reference (char * file) {
   struct help_ref * current, *last = NULL;
   struct help_list * item;
   
   for (current = help_list; current; last = current, current = current->next) 
     if (!strcmp(current->name,file)) {
	while ((item = current->first)) {
	   current->first = item->next;
	   nfree(item->name);
	   nfree(item);
	}
	nfree(current->name);
	if (last) 
	  last->next = current->next;
	else
	  help_list = current->next;
	nfree(current);
	return;
     }
}

void reload_help_data (void) {
   struct help_ref * current = help_list, *next;
   struct help_list * item;
   
   help_list = NULL;
   while (current) {
      while ((item = current->first)) {
	 current->first = item->next;
	 nfree(item->name);
	 nfree(item);
      }
      add_help_reference(current->name);
      nfree(current->name);
      next = current->next;
      nfree(current);
      current = next;
   }
}  

#ifdef EBUG
void debug_help (int idx) {
   struct help_ref * current;
   struct help_list * item;
   
   for (current = help_list; current; current = current->next) {
      dprintf(idx,"HELP FILE(S): %s\n", current->name);
      for (item = current->first;item;item = item->next) {
	 dprintf(idx,"   %s (%s)\n", item->name, (item->type == 0) ? "msg/" : 
		 (item->type == 1) ? "" : "set/");
      }
   } 
}
#endif

FILE * resolve_help(int dcc, char * file) {
   char s[1024], *p;
   FILE * f;
   struct help_ref * current;
   struct help_list * item;
   
   /* somewhere here goes the eventual substituation */
   if (!(dcc & HELP_TEXT))
     for (current = help_list; current; current = current->next) 
       for (item = current->first; item; item = item->next) 
	 if (!strcmp(item->name,file)) {
	    if (!item->type && !dcc) {
	       simple_sprintf(s,"%smsg/%s",helpdir,current->name);
	       if ((f = fopen(s,"r")))
		 return f;
	    } else if (dcc && item->type) {
	       if (item->type == 1) 
		 simple_sprintf(s,"%s%s",helpdir,current->name);
	       else
		 simple_sprintf(s,"%sset/%s",helpdir,current->name);
	       if ((f = fopen(s,"r")))
		 return f;
	    }
	 }
   for (p = s + simple_sprintf(s, "%s%s", helpdir, dcc ? "" : "msg/");
	*file && (p < s + 1023); file++, p++) {
      switch(*file) {
       case ' ':
       case '.':
	 *p = '/';
	 break;
       case '-':
	 *p = '-';
	 break;
       case '+':
	 *p = 'P';
	 break;
       default:
	 *p = *file;
      }
   }
   *p = 0;
   if (!is_file(s)) {
      strcat(s, "/");
      strcat(s, file);
      if (!is_file(s)) 
	return NULL;
   }
   return fopen(s, "r");
}

void showhelp (char * who, char * file, struct flag_record * flags, int fl)
{
   int lines = 0;
   char s[HELP_BUF_LEN+1];
   FILE * f = resolve_help(fl,file);

   if (f) {
      help_subst(NULL, NULL, 0, HELP_IRC, NULL);	/* clear flags */
      while (!feof(f)) {
	 fgets(s, HELP_BUF_LEN, f);
	 if (!feof(f)) {
	    if (s[strlen(s) - 1] == '\n')
	      s[strlen(s) - 1] = 0;
	    if (!s[0])
	      strcpy(s, " ");
	    help_subst(s, who, flags, 0, file);
	    if (s[0]) {
	       dprintf(DP_HELP, "NOTICE %s :%s\n", who, s);
	       lines++;
	    }
	 }
      }
      fclose(f);
   }
   if (!lines && !(fl & HELP_TEXT))
     dprintf(DP_HELP, "NOTICE %s :%s\n", who, IRC_NOHELP2);
}

void tellhelp (int idx, char * file, struct flag_record * flags, int fl)
{
   char s[HELP_BUF_LEN+1];
   int lines = 0;
   FILE *f = resolve_help(HELP_DCC|fl,file);
   
   if (f) {
      help_subst(NULL, NULL, 0, 
		 (dcc[idx].status & STAT_TELNET) ? 0 : HELP_IRC,  NULL);
      while (!feof(f)) {
	 fgets(s, HELP_BUF_LEN, f);
	 if (!feof(f)) {
	    if (s[strlen(s) - 1] == '\n')
	      s[strlen(s) - 1] = 0;
	    if (!s[0])
	      strcpy(s, " ");
	    help_subst(s, dcc[idx].nick, flags, 1, file);
	    if (s[0]) {
	       dprintf(idx, "%s\n", s);
	       lines++;
	    }
	 }
      }
      fclose(f);
   }
   if (!lines && !(fl & HELP_TEXT))
      dprintf(idx, "%s\n", IRC_NOHELP2);
}

/* substitute vars in a lang text to dcc chatter */
void sub_lang (int idx, char * text)
{
   char s[1024];
   struct flag_record fr = {FR_GLOBAL|FR_CHAN,0,0,0,0,0};
   
   get_user_flagrec(dcc[idx].user,&fr,dcc[idx].u.chat->con_chan);

      help_subst(NULL, NULL, 0, 
		 (dcc[idx].status & STAT_TELNET) ? 0 : HELP_IRC , NULL);
	 strncpy(s, text, 1024);
	    if (s[strlen(s) - 1] == '\n')
	      s[strlen(s) - 1] = 0;
	    if (!s[0])
	      strcpy(s, " ");
	    help_subst(s, dcc[idx].nick, &fr, 1, botnetnick);
	    if (s[0])
	      dprintf(idx, "%s\n", s);
}

/* show motd to dcc chatter */
void show_motd (int idx)
{
   FILE *vv;
   char s[1024];
   struct flag_record fr = {FR_GLOBAL|FR_CHAN,0,0,0,0,0};
   
   get_user_flagrec(dcc[idx].user,&fr,dcc[idx].u.chat->con_chan);
   vv = fopen(motdfile, "r");
   if (vv != NULL) {
      if (!is_file(motdfile)) {
	 fclose(vv);
	 dprintf(idx, "### MOTD %s\n", IRC_NOTNORMFILE);
	 return;
      }
      dprintf(idx, "\n");
      help_subst(NULL, NULL, 0, 
		 (dcc[idx].status & STAT_TELNET) ? 0 : HELP_IRC , NULL);
      while (!feof(vv)) {
	 fgets(s, 120, vv);
	 if (!feof(vv)) {
	    if (s[strlen(s) - 1] == '\n')
	      s[strlen(s) - 1] = 0;
	    if (!s[0])
	      strcpy(s, " ");
	    help_subst(s, dcc[idx].nick, &fr, 1, botnetnick);
	    if (s[0])
	      dprintf(idx, "%s\n", s);
	 }
      }
      fclose(vv);
      dprintf(idx, "\n");
   }
}

