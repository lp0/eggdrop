/* stuff common to chan.c & mode.c */
/* users.h needs to be loaded too */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/
#ifndef _H_CHAN
#define _H_CHAN

typedef struct memstruct {
  char nick[NICKLEN];        /* "dalnet" allows 30 */
  char userhost[UHOSTLEN];
  time_t joined;
  unsigned char flags;
  time_t split;         /* in case they were just netsplit */
  time_t last;          /* for measuring idle time */
  struct userrec *user;
  struct memstruct *next;
} memberlist;

#define CHANOP     1    /* channel +o */
#define CHANVOICE  2    /* channel +v */
#define FAKEOP     4    /* op'd by server */
#define SENTOP     8    /* a mode +o was already sent out for this user */
#define SENTDEOP   16   /* a mode -o was already sent out for this user */
#define SENTKICK   32   /* a kick was already sent out for this user */

typedef struct banstruct {
  char *ban;
  char *who;
  time_t timer;
  struct banstruct *next;
} banlist;

/* for every channel i join */
struct chan_t {
  memberlist *member;
  banlist *ban;
  char topic[256];
  char *key;
  unsigned short int mode;
  int maxmembers;
  int members;
};

#define CHANINV    0x0001  /* +i */
#define CHANPRIV   0x0002  /* +p */
#define CHANSEC    0x0004  /* +s */
#define CHANMODER  0x0008  /* +m */
#define CHANTOPIC  0x0010  /* +t */
#define CHANNOMSG  0x0020  /* +n */
#define CHANLIMIT  0x0040  /* -l */  /* used only for protecting modes */
#define CHANKEY    0x0080  /* -k */  /* used only for protecting modes */
#define CHANANON   0x0100  /* +a */  /* irc 2.9 */
#define CHANQUIET  0x0200  /* +q */  /* irc 2.9 */

/* for every channel i'm supposed to be active on */
struct chanset_t {
  struct chan_t channel;    /* current information */
  char name[81];
  char need_op[121];
  char need_key[121];
  char need_limit[121];
  char need_unban[121];
  char need_invite[121];
  int stat;
  int idle_kick;
  struct userrec *bans;     /* temporary channel bans */
  /* desired channel modes: */
  int mode_cur;             /* current chan modes */
  int mode_pls_prot;        /* modes to enforce */
  int mode_mns_prot;        /* modes to reject */
  int limit_prot;           /* desired limit */
  char key_prot[121];       /* desired password */
  /* queued mode changes: */
  char pls[21];             /* positive mode changes */
  char mns[21];             /* negative mode changes */
  char key[81];             /* new key to set */
  char rmkey[81];           /* old key to remove */
  int limit;                /* new limit to set */
  struct {
    char *op;
    char type;
  } cmode[6];  /* parameter-type mode changes - */
  /* detect mass-deop */
  char deopnick[NICKLEN];   /* last person to deop */
  char deopd[NICKLEN];      /* last person deop'd (must change) */
  time_t deoptime;          /* start time of a deop chain */
  int deops;                /* how many deops in this chain */
  /* detect mass-kick */
  char kicknick[NICKLEN];   /* last person to kick */
  time_t kicktime;          /* start time of a kick chain */
  int kicks;                /* how many kicks in this chain */
  struct chanset_t *next;
};

/* behavior modes for the channel */
#define CHAN_CLEARBANS      0x0001    /* clear bans on join */
#define CHAN_ENFORCEBANS    0x0002    /* kick people who match channel bans */
#define CHAN_DYNAMICBANS    0x0004    /* only activate bans when needed */
#define CHAN_NOUSERBANS     0x0008    /* don't let non-bots place bans */
#define CHAN_OPONJOIN       0x0010    /* op +o people as soon as they join */
#define CHAN_BITCH          0x0020    /* be a tightwad with ops */
#define CHAN_GREET          0x0040    /* greet people with their info line */
#define CHAN_PROTECTOPS     0x0080    /* re-op any +o people who get deop'd */
#define CHAN_LOGSTATUS      0x0100    /* log channel status every 5 mins */
#define CHAN_STOPNETHACK    0x0200    /* deop netsplit hackers */
#define CHAN_REVENGE        0x0400    /* get revenge on bad people */
#define CHAN_SECRET         0x0800    /* don't advertise channel on botnet */

#define CHANACTIVE  0x1000  /* like i'm actually on the channel and stuff */
#define CHANPEND    0x2000  /* just joined; waiting for end of WHO list */
#define CHANFLAGGED 0x4000  /* flagged during rehash for delete */
#define CHANSTATIC  0x8000  /* channels that are NOT dynamic */
#define CHAN_SHARED  0x10000 /* channel is being shared */


/* prototypes */
memberlist *ismember();
memberlist *newmember();
struct chanset_t *findchan();
struct chanset_t *newchanset();

#endif
