/*
 *   EGGDROP compile-time settings
 *
 *   IF YOU ALTER THIS FILE, YOU NEED TO RECOMPILE THE BOT.
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#ifndef _H_EGGDROP
#define _H_EGGDROP

/*
 *   Settings which toggle certain features of the bot to be enabled
 *   or disabled -- use "#define" to enable and "#undef" to disable
 */

/*
 *   Choose a language or dialect for the bot's messages by uncommenting
 *   only ONE of the following #include's:
 */

/* #include "lang/english.h"		 Standard English messages 
 * #include "lang/slang.h"		 Fun slang and rude messages 
 */

#include "lang/english.h"

   /* standalone bot, not connected to any IRC server? ("in limbo") */
   /* this shaves about 55k from the executable on my linux system */
   /* (including the 45k saved by dropping the file system) but will */
   /* make your bot useless for protecting channels or interacting on */
   /* any IRC network. */
#undef NO_IRC

/***********************************************************************/
/***** the 'configure' script should make this next part automatic *****/
/***********************************************************************/

/* handy maximum string lengths */
#define NICKLEN       32     /* thanks to dalnet */
#define UHOSTLEN     161     /* reasonable, i think? */
#define DIRLEN       256     /* paranoia */

/* have to use a weird way to make the compiler error out cos not all
   compilers support #error or error */
#if !HAVE_VSPRINTF
#include "error_you_need_vsprintf_to_compile_eggdrop"
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef STATIC
#if !defined(MODULES_OK) || !defined(HAVE_DLOPEN)
#include "you_can't_compile_with_module_support_on_this_system_try_make_static"
#endif
#endif

#if !defined(STDC_HEADERS)
#include "you_need_to_upgrade_your_compiler_to_a_standard_c_one_mate!"
#endif

/* almost every module needs some sort of time thingy, so... */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef EBUG_OUTPUT
# ifndef EBUG
#  define EBUG
# endif 
#endif

#if !HAVE_RENAME
#define rename movefile
#endif

#if !HAVE_SRANDOM
#define srandom(x) srand(x)
#endif

#if !HAVE_RANDOM
#define random() (rand()/16)
#endif

#if !HAVE_SIGACTION     /* old "weird signals" */
#define sigaction sigvec
#ifndef sa_handler
#define sa_handler sv_handler
#define sa_mask sv_mask
#define sa_flags sv_flags
#endif
#endif

#if !HAVE_SIGEMPTYSET
/* and they probably won't have sigemptyset, dammit */
#define sigemptyset(x) ((*(int *)(x))=0)
#endif

/* handy aliases for memory tracking and core dumps */

#define nmalloc(x) n_malloc((x),__FILE__,__LINE__)
#define nrealloc(x,y) n_realloc((x),(y),__FILE__,__LINE__)
#define nfree(x) n_free((x),__FILE__,__LINE__)

#ifdef EBUG
#define context { cx_ptr=((cx_ptr + 1) & 15); \
                  strcpy(cx_file[cx_ptr],__FILE__); \
                  cx_line[cx_ptr]=__LINE__; }
/* move these here, makes more sense to me :) */
extern int cx_line[16];
extern char cx_file[16][30];
extern int cx_ptr;
#else
#define context { strcpy(cx_file,__FILE__); cx_line=__LINE__; }
extern int cx_line;
extern char cx_file[30];
#endif

#undef malloc
#undef free
#define malloc(x) dont_use_old_malloc(x)
#define free(x) dont_use_old_free(x)

/* IP type */
#if SIZEOF_INT==4
typedef unsigned int IP;
#else
#  if SIZEOF_LONG==4
typedef unsigned long IP;
#  else
#include "cant/find/32bit/type"
#  endif
#endif

/* macro for simplifying patches */
#define PATCH(str) { \
  char *p=strchr(egg_version,'+'); \
  if (p==NULL) p=&egg_version[strlen(egg_version)]; \
  sprintf(p,"+%s",str); \
  egg_numver++; \
  sprintf(&egg_xtra[strlen(egg_xtra)]," %s",str); \
}

#ifdef EBUG
#define debug0(x) putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1) putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2) putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3) putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4) putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)
#else
#define debug0(x) ;
#define debug1(x,a1) ;
#define debug2(x,a1,a2) ;
#define debug3(x,a1,a2,a3) ;
#define debug4(x,a1,a2,a3,a4) ;
#endif

#define flags_eq(req,have) (((have)&(req))==(req))

/***********************************************************************/


/* used to queue a lot of things */
struct eggqueue {
  char *item;
  time_t stamp;
  struct eggqueue *next;
};


/* public structure of all the dcc connections */
struct dcc_table {
   void (*eof) (int);
   void (*activity) (int,char *,int);
   int * timeout_val;
   void (*timeout) ();
   void (*display) (int,char *);
   int (*expmem) (int);
   void (*kill) (int);
   void (*output)  (int,char *);
};

struct userrec;

struct dcc_t {
  long sock;  /* this should be a long to keep 64-bit machines sane */
  IP addr;
  unsigned int port;
  struct userrec *user;
  char nick[NICKLEN];
  char host[UHOSTLEN];
  struct dcc_table * type;
  time_t timeval;        /* use for any timing stuff 
			  - this is used for timeout checking*/
  union {
    struct chat_info *chat;
    struct file_info *file;
    struct edit_info *edit;
    struct xfer_info *xfer;
    struct bot_info *bot;
    struct relay_info *relay;
    struct script_info *script;
    void *other;
  } u;    /* special use depending on type */
};
 
struct chat_info {
  char *away;               /* non-NULL if user is away */
  unsigned long status;     /* status flags */
  int msgs_per_sec;         /* used to stop flooding */
  int con_flags;            /* with console: what to show */
  int strip_flags;          /* what codes to strip (b,r,u,c) */
  char con_chan[81];        /* with console: what channel to view */
  int channel;              /* 0=party line, -1=off */
  struct eggqueue * buffer; /* a buffer of outgoing lines (for .page cmd) */
  int max_line;             /* maximum lines at once */
  int line_count;           /* number of lines sent since last page */
  int current_lines;        /* number of lines total stoerd */
};

struct file_info {
  struct chat_info *chat;
  char dir[121];
};

struct xfer_info {
  char filename[121];
  char dir[121];            /* used when uploads go to the current dir */
  unsigned long length;
  unsigned long sent;
  unsigned long acked;
  char buf[4];              /* you only need 5 bytes! */
  unsigned char sofar;      /* how much of the byte count received */
  char from[NICKLEN];       /* [GET] user who offered the file */
  FILE *f;		    /* pointer to file being sent/received */
};

struct bot_info {
  unsigned long status;
  char version[121];        /* channel/version info */
  char linker[21];          /* who requested this link */
  int numver;
  int x;
  int port;                 /* base port */
};

struct relay_info {
  struct chat_info *chat;
  int sock;
};

struct script_info {
  struct dcc_table * type;
  union {
    struct chat_info *chat;
    struct file_info *file;
    void *other;
  } u;
  char command[121];
};


/* for dcc chat & files: */
#define STAT_ECHO    1      /* echo commands back? */
#define STAT_DENY    2      /* bad username (ignore password & deny access) */
/*#define STAT_XFER    4       has 'x' flag on chat line */
#define STAT_CHAT    8      /* in file-system but may return */
#define STAT_TELNET  16     /* connected via telnet */
#define STAT_PARTY   32     /* only on party line via 'p' flag */
#define STAT_BOTONLY 64     /* telnet on bots-only connect */
#define STAT_USRONLY 128    /* telnet on users-only connect */
#define STAT_PAGE    256    /* page output to the user */

/* for stripping out mIRC codes */
#define STRIP_COLOR  1      /* remove mIRC color codes */
#define STRIP_BOLD   2      /* remove bold codes */
#define STRIP_REV    4      /* remove reverse video codes */
#define STRIP_UNDER  8      /* remove underline codes */
#define STRIP_ANSI   16     /* remove ALL ansi codes */
#define STRIP_ALL    31     /* remove every damn thing! */

/* for dcc bot links: */
#define STAT_PINGED  0x01   /* waiting for ping to return */
#define STAT_SHARE   0x02   /* sharing user data with the bot */
#define STAT_CALLED  0x04   /* this bot called me */
#define STAT_OFFERED 0x08   /* offered her the user file */
#define STAT_SENDING 0x10   /* in the process of sending a user list */
#define STAT_GETTING 0x20   /* in the process of getting a user list */
#define STAT_WARNED  0x40   /* warned him about unleaflike behavior */
#define STAT_LEAF    0x80   /* this bot is a leaf only */

/* bug: changing the order of these will mess up filedb */
#define USER_GLOBAL   0x00000001  /* o   user is +o on all channels */
#define USER_DEOP     0x00000002  /* d   user is global de-op */
#define USER_KICK     0x00000004  /* k   user is global auto-kick */
#define USER_FRIEND   0x00000008  /* f   user is global friend*/
#define USER_MASTER   0x00000010  /* m   user has full bot access */
#define USER_OWNER    0x00000020  /* n   user is the bot owner */
#define USER_FLAG1    0x00000040  /* 1   user-defined flag #1 */
#define USER_FLAG2    0x00000080  /* 2   user-defined flag #2 */
#define USER_FLAG3    0x00000100  /* 3   user-defined flag #3 */
#define USER_FLAG4    0x00000200  /* 4   user-defined flag #4 */
#define USER_FLAG5    0x00000400  /* 5   user-defined flag #5 */
#define USER_FLAG6    0x00000800  /* 6   user-defined flag #6 */
#define USER_FLAG7    0x00001000  /* 7   user-defined flag #7 */
#define USER_FLAG8    0x00002000  /* 8   user-defined flag #8 */
#define USER_FLAG9    0x00004000  /* 9   user-defined flag #9 */
#define USER_FLAG0    0x00008000  /* 0   user-defined flag #10 */
#define USER_JANITOR  0x00400000  /* j   user has file area master */
#define USER_UNSHARED 0x00800000  /* u   not shared with sharebots */
#define USER_XFER     0x01000000  /* x   user has file area access */
#define USER_PARTY    0x02000000  /* p   user has party line access */
#define USER_COMMON   0x04000000  /* c   user is actually a public irc site */
#define USER_BOTMAST  0x08000000  /* B   user is botnet master */
#define USER_MASK    (0xffff003f) /* all non-userdef flags */
#define USER_CHANMASK 0x0000003f  /* flags common to global & chan records */

/*   ????Bcpx ujbarlhs 09876543 21nmfkdo   */
/*   (users)    (bots) (users)  (users)    */
/*   unused letters: egiqtvwyz          */

#define BOT_MASK     (0xffC0ffff)     /* all non-bot flags */

/* flags specifically for bots */
#define BOT_SHARE     0x00010000  /* s   bot shares user files */
#define BOT_HUB       0x00020000  /* h   auto-link to ONE of these bots */
#define BOT_LEAF      0x00040000  /* l   may not link other bots */
#define BOT_REJECT    0x00080000  /* r   automatically reject anywhere */
#define BOT_ALT       0x00100000  /* a   auto-link here if all +h's fail */
#define USER_BOT      0x00200000  /* b   user is a bot (previously 't') */

/*   ???????? ???????s 09876543 21nmfkdo   */

/* channel-specific flags */
#define CHANUSER_OP       0x00000001  /* o   bot will op the user */
#define CHANUSER_DEOP     0x00000002  /* d   make sure user never gets ops */
#define CHANUSER_KICK     0x00000004  /* k   kick user off the channel */
#define CHANUSER_FRIEND   0x00000008  /* f   exempt from revenge */
#define CHANUSER_MASTER   0x00000010  /* m   master of one channel */
#define CHANUSER_OWNER    0x00000020  /* n   owner of one channel */
#define CHANUSER_1        0x00000040  /* 1   user defined */
#define CHANUSER_2        0x00000080  /* 2   user defined */
#define CHANUSER_3        0x00000100  /* 3   user defined */
#define CHANUSER_4        0x00000200  /* 4   user defined */
#define CHANUSER_5        0x00000400  /* 5   user defined */
#define CHANUSER_6        0x00000800  /* 6   user defined */
#define CHANUSER_7        0x00001000  /* 7   user defined */
#define CHANUSER_8        0x00002000  /* 8   user defined */
#define CHANUSER_9        0x00004000  /* 9   user defined */
#define CHANUSER_0        0x00008000  /* 0   user defined */
#define CHANBOT_SHARE     0x00010000  /* s   bot shares user files */
#define CHANUSER_MASK    (0xffff003f) /* all non-use-defined */

/* for detecting floods: */
#define FLOOD_NICK      0
#define FLOOD_PRIVMSG   1
#define FLOOD_NOTICE    2
#define FLOOD_CTCP      3
#define FLOOD_JOIN      4

/* for local console: */
#define STDIN      0
#define STDOUT     1
#define STDERR     2

/* structure for internal logs */
typedef struct {
  char *filename;
  unsigned int mask;       /* what to send to this log */
  char *chname;            /* which channel */
  FILE *f;                 /* existing file */
} log_t;

/* logfile display flags */
#define LOG_MSGS   0x00001  /* m   msgs/notice/ctcps */
#define LOG_PUBLIC 0x00002  /* p   public msg/notice/ctcps */
#define LOG_JOIN   0x00004  /* j   channel joins/parts/etc */
#define LOG_MODES  0x00008  /* k   mode changes/kicks/bans */
#define LOG_CMDS   0x00010  /* c   user dcc or msg commands */
#define LOG_MISC   0x00020  /* o   other misc bot things */
#define LOG_BOTS   0x00040  /* b   bot notices */
#define LOG_RAW    0x00080  /* r   raw server stuff coming in */
#define LOG_FILES  0x00100  /* x   file transfer commands and stats */
#define LOG_LEV1   0x00200  /* 1   user log level */
#define LOG_LEV2   0x00400  /* 2   user log level */
#define LOG_LEV3   0x00800  /* 3   user log level */
#define LOG_LEV4   0x01000  /* 4   user log level */
#define LOG_LEV5   0x02000  /* 5   user log level */
#define LOG_LEV6   0x04000  /* 6   user log level */
#define LOG_LEV7   0x08000  /* 7   user log level */
#define LOG_LEV8   0x10000  /* 8   user log level */
#define LOG_SERV   0x20000  /* s   server information */
#define LOG_DEBUG  0x40000  /* d   debug */
#define LOG_WALL   0x80000  /* w   wallops */
#define LOG_ALL    0xfffff  /* (dump to all logfiles) */


#define FILEDB_HIDE     1
#define FILEDB_UNHIDE   2
#define FILEDB_SHARE    3
#define FILEDB_UNSHARE  4

/* socket flags: */
#define SOCK_UNUSED     0x01    /* empty socket */
#define SOCK_BINARY     0x02    /* do not buffer input */
#define SOCK_LISTEN     0x04    /* listening port */
#define SOCK_CONNECT    0x08    /* connection attempt */
#define SOCK_NONSOCK    0x10    /* used for file i/o on debug */
#define SOCK_STRONGCONN 0x20    /* don't report success until sure */

/* fake idx's for dprintf - these should be rediculously large +ve nums */
#define DP_STDOUT       0x7FF1
#define DP_LOG          0x7FF2
#define DP_SERVER       0x7FF3
#define DP_HELP         0x7FF4

#define NORMAL          0
#define QUICK           1

/* return codes for add_note */
#define NOTE_ERROR      0   /* error */
#define NOTE_OK         1   /* success */
#define NOTE_STORED     2   /* not online; stored */
#define NOTE_FULL       3   /* too many notes stored */
#define NOTE_TCL        4   /* tcl binding caught it */
#define NOTE_AWAY       5   /* away; stored */

/* builtins values for add_builins */
#define BUILTIN_DCC   1
#define BUILTIN_MSG   2
#define BUILTIN_FILES 3
#define BUILTIN_FILT  4
#define BUILTIN_BOT   5
#define STR_PROTECT  2
#define STR_DIR      1

/* it's used in so many places, let's put it here */
typedef int (*Function)();

/* this is used by the net module to keep track of sockets and what's
   queued on them */
typedef struct {
   int sock;
   char flags;
   char *inbuf;
   char *outbuf;
   unsigned long outbuflen;	/* outbuf could be binary data */
} sock_list;
#endif
