/* 
   userrec.c -- handles:
   add_q() del_q() str2flags() flags2str() str2chflags() chflags2str()
   a bunch of functions to find and change user records
   change and check user (and channel-specific) flags

   dprintf'ized, 10nov95
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
#include "users.h"
#include "chan.h"
#include "modules.h"

extern char botname[];
extern char botuser[];
extern int serv;
extern struct dcc_t * dcc;
extern int dcc_total;
extern char userfile[];
extern char chanfile[];
extern int share_greet;
extern int require_p;
extern struct chanset_t *chanset;
extern char ver[];
extern char origbotname[];
extern int nulluser;
extern time_t now;

/* share greeting info */
int share_greet = 0;
/* don't send out to sharebots */
int noshare = 1;
/* user records are stored here */
struct userrec *userlist = NULL;
/* last accessed user record */
struct userrec *lastuser = NULL;
struct userrec *banu = NULL, *ignu = NULL;
/* hey let's allow the config file to specify the names for flags! */
/* gee wally i like that idea, that could cause LOTS of trouble! */
char flag[11] = "0123456789";
char chanflag[11] = "0123456789";

/* temporary cache accounting */
int cache_hit = 0, cache_miss = 0;


struct eggqueue *add_q (char * ss, struct eggqueue * q)
{
   char s[512];
   struct eggqueue *x;
   char s1[121], *p;
   strcpy(s, ss);
   do {
      p = strchr(s, ',');
      if (p != NULL) {
	 *p = 0;
	 p++;
	 strcpy(s1, p);
      } else
	 s1[0] = 0;
      rmspace(s);
      rmspace(s1);
      x = (struct eggqueue *) nmalloc(sizeof(struct eggqueue));
      x->next = q;
      x->item = (char *) nmalloc(strlen(s) + 1);
      x->stamp = now;
      strcpy(x->item, s);
      strcpy(s, s1);
   } while (s[0]);
   return x;
}

void chg_q (struct eggqueue * q, char * new)
{
   nfree(q->item);
   q->item = (char *) nmalloc(strlen(new) + 1);
   strcpy(q->item, new);
}

struct eggqueue *del_q (char * s, struct eggqueue * q, int * ok)
{
   struct eggqueue *x, *ret, *old;
   x = q;
   ret = q;
   old = q;
   *ok = 0;
   while (x != NULL) {
      if (strcasecmp(x->item, s) == 0) {
	 if (x == ret) {
	    ret = (x->next);
	    nfree(x->item);
	    nfree(x);
	    x = ret;
	 } else {
	    old->next = x->next;
	    nfree(x->item);
	    nfree(x);
	    x = old->next;
	 }
	 *ok = 1;
      } else {
	 old = x;
	 x = x->next;
      }
   }
   return ret;
}

/* memory we should be using */
int expmem_users()
{
   int tot;
   struct userrec *u;
   struct eggqueue *s;
   struct chanset_t *chan;
   struct chanuserrec *ch;
   context;
   tot = 0;
   u = userlist;
   while (u != NULL) {
      if (u->email != NULL)
	 tot += strlen(u->email) + 1;
      if (u->dccdir != NULL)
	 tot += strlen(u->dccdir) + 1;
      if (u->comment != NULL)
	 tot += strlen(u->comment) + 1;
      if (u->info != NULL)
	 tot += strlen(u->info) + 1;
      if (u->xtra != NULL)
	 tot += strlen(u->xtra) + 1;
      if (u->lastonchan != NULL)
	tot += strlen(u->lastonchan) + 1;
      s = u->host;
      while (s != NULL) {
	 if (s->item != NULL)
	    tot += strlen(s->item) + 1;
	 tot += sizeof(struct eggqueue);
	 s = s->next;
      }
      ch = u->chanrec;
      while (ch != NULL) {
	 tot += sizeof(struct chanuserrec);
	 if (ch->info != NULL)
	    tot += strlen(ch->info) + 1;
	 ch = ch->next;
      }
      tot += sizeof(struct userrec);
      u = u->next;
   }
   /* account for each channel's ban-list user */
   chan = chanset;
   while (chan != NULL) {
      if (chan->bans->info != NULL)
	 tot += strlen(chan->bans->info) + 1;
      if (chan->bans->email != NULL)
	 tot += strlen(chan->bans->email) + 1;
      if (chan->bans->dccdir != NULL)
	 tot += strlen(chan->bans->dccdir) + 1;
      if (chan->bans->comment != NULL)
	 tot += strlen(chan->bans->comment) + 1;
      if (chan->bans->xtra != NULL)
	 tot += strlen(chan->bans->xtra) + 1;
      s = chan->bans->host;
      while (s != NULL) {
	 if (s->item != NULL)
	    tot += strlen(s->item) + 1;
	 tot += sizeof(struct eggqueue);
	 s = s->next;
      }
      ch = chan->bans->chanrec;
      while (ch != NULL) {
	 tot += sizeof(struct chanuserrec);
	 if (ch->info != NULL)
	    tot += strlen(ch->info) + 1;
	 ch = ch->next;
      }
      tot += sizeof(struct userrec);
      chan = chan->next;
   }
   return tot;
}

unsigned int str2chflags (char * s)
{
   unsigned int i, f = 0;
   for (i = 0; i < strlen(s); i++) {
      if (s[i] == 'o')
	 f |= CHANUSER_OP;
      if (s[i] == 'd')
	 f |= CHANUSER_DEOP;
      if (s[i] == 'k')
	 f |= CHANUSER_KICK;
      if (s[i] == 'f')
	 f |= CHANUSER_FRIEND;
      if (s[i] == 'm')
	 f |= CHANUSER_MASTER;
      if (s[i] == 'n')
	 f |= CHANUSER_OWNER;
      if (s[i] == 's')
	 f |= CHANBOT_SHARE;
      if (s[i] == '1')
	 f |= CHANUSER_1;
      if (s[i] == '2')
	 f |= CHANUSER_2;
      if (s[i] == '3')
	 f |= CHANUSER_3;
      if (s[i] == '4')
	 f |= CHANUSER_4;
      if (s[i] == '5')
	 f |= CHANUSER_5;
      if (s[i] == '6')
	 f |= CHANUSER_6;
      if (s[i] == '7')
	 f |= CHANUSER_7;
      if (s[i] == '8')
	 f |= CHANUSER_8;
      if (s[i] == '9')
	 f |= CHANUSER_9;
      if (s[i] == '0')
	 f |= CHANUSER_0;
      if (s[i] == chanflag[1])
	 f |= CHANUSER_1;
      if (s[i] == chanflag[2])
	 f |= CHANUSER_2;
      if (s[i] == chanflag[3])
	 f |= CHANUSER_3;
      if (s[i] == chanflag[4])
	 f |= CHANUSER_4;
      if (s[i] == chanflag[5])
	 f |= CHANUSER_5;
      if (s[i] == chanflag[6])
	 f |= CHANUSER_6;
      if (s[i] == chanflag[7])
	 f |= CHANUSER_7;
      if (s[i] == chanflag[8])
	 f |= CHANUSER_8;
      if (s[i] == chanflag[9])
	 f |= CHANUSER_9;
      if (s[i] == chanflag[0])
	 f |= CHANUSER_0;
   }
   return f;
}

unsigned int str2flags (char * s)
{
   unsigned int i, f = 0;
   for (i = 0; i < strlen(s); i++) {
      if (s[i] == 'o')
	 f |= USER_GLOBAL;
      if (s[i] == 'm')
	 f |= USER_MASTER;
      if (s[i] == 'x')
	 f |= USER_XFER;
      if (s[i] == 'b')
	 f |= USER_BOT;
      if (s[i] == 'p')
	 f |= USER_PARTY;
      if (s[i] == 'c')
	 f |= USER_COMMON;
      if (s[i] == 'n')
	 f |= USER_OWNER;
      if (s[i] == 'j')
	 f |= USER_JANITOR;
      if (s[i] == 'u')
	 f |= USER_UNSHARED;
      if (s[i] == 'B')
	 f |= USER_BOTMAST;
      if (s[i] == 'k')
	 f |= USER_KICK;
      if (s[i] == 'd')
	 f |= USER_DEOP;
      if (s[i] == 'f')
	 f |= USER_FRIEND;
      if (s[i] == 's')
	 f |= BOT_SHARE;
      if (s[i] == 'a')
	 f |= BOT_ALT;
      if (s[i] == 'l')
	 f |= BOT_LEAF;
      if (s[i] == 'r')
	 f |= BOT_REJECT;
      if (s[i] == 'h')
	 f |= BOT_HUB;
      if (s[i] == '1')
	 f |= USER_FLAG1;
      if (s[i] == '2')
	 f |= USER_FLAG2;
      if (s[i] == '3')
	 f |= USER_FLAG3;
      if (s[i] == '4')
	 f |= USER_FLAG4;
      if (s[i] == '5')
	 f |= USER_FLAG5;
      if (s[i] == '6')
	 f |= USER_FLAG6;
      if (s[i] == '7')
	 f |= USER_FLAG7;
      if (s[i] == '8')
	 f |= USER_FLAG8;
      if (s[i] == '9')
	 f |= USER_FLAG9;
      if (s[i] == '0')
	 f |= USER_FLAG0;
      if (s[i] == flag[1])
	 f |= USER_FLAG1;
      if (s[i] == flag[2])
	 f |= USER_FLAG2;
      if (s[i] == flag[3])
	 f |= USER_FLAG3;
      if (s[i] == flag[4])
	 f |= USER_FLAG4;
      if (s[i] == flag[5])
	 f |= USER_FLAG5;
      if (s[i] == flag[6])
	 f |= USER_FLAG6;
      if (s[i] == flag[7])
	 f |= USER_FLAG7;
      if (s[i] == flag[8])
	 f |= USER_FLAG8;
      if (s[i] == flag[9])
	 f |= USER_FLAG9;
      if (s[i] == flag[0])
	 f |= USER_FLAG0;
   }
   return f;
}

void chflags2str (int f, char * s)
{
   s[0] = 0;
   if (f & CHANUSER_OP)
      strcat(s, "o");
   if (f & CHANUSER_DEOP)
      strcat(s, "d");
   if (f & CHANUSER_KICK)
      strcat(s, "k");
   if (f & CHANUSER_FRIEND)
      strcat(s, "f");
   if (f & CHANUSER_MASTER)
      strcat(s, "m");
   if (f & CHANUSER_OWNER)
      strcat(s, "n");
   if (f & CHANBOT_SHARE)
      strcat(s, "s");
   if (f & CHANUSER_1) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[1];
   }
   if (f & CHANUSER_2) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[2];
   }
   if (f & CHANUSER_3) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[3];
   }
   if (f & CHANUSER_4) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[4];
   }
   if (f & CHANUSER_5) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[5];
   }
   if (f & CHANUSER_6) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[6];
   }
   if (f & CHANUSER_7) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[7];
   }
   if (f & CHANUSER_8) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[8];
   }
   if (f & CHANUSER_9) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[9];
   }
   if (f & CHANUSER_0) {
      strcat(s, "?");
      s[strlen(s) - 1] = chanflag[0];
   }
   if (s[0] == 0)
      strcpy(s, "-");
}

void flags2str (int f, char * s)
{
   s[0] = 0;
   if (f & USER_GLOBAL)
      strcat(s, "o");
   if (f & USER_MASTER)
      strcat(s, "m");
   if (f & USER_XFER)
      strcat(s, "x");
   if (f & USER_COMMON)
      strcat(s, "c");
   if (f & USER_JANITOR)
      strcat(s, "j");
   if (f & USER_UNSHARED)
      strcat(s, "u");
   if (f & USER_BOTMAST)
      strcat(s, "B");
   if (f & USER_KICK)
      strcat(s, "k");
   if (f & USER_DEOP)
      strcat(s, "d");
   if (f & USER_FRIEND)
      strcat(s, "f");
   if (f & USER_OWNER)
      strcat(s, "n");
   if (f & USER_FLAG1) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[1];
   }
   if (f & USER_FLAG2) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[2];
   }
   if (f & USER_FLAG3) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[3];
   }
   if (f & USER_FLAG4) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[4];
   }
   if (f & USER_FLAG5) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[5];
   }
   if (f & USER_FLAG6) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[6];
   }
   if (f & USER_FLAG7) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[7];
   }
   if (f & USER_FLAG8) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[8];
   }
   if (f & USER_FLAG9) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[9];
   }
   if (f & USER_FLAG0) {
      strcat(s, "?");
      s[strlen(s) - 1] = flag[0];
   }
   if (f & USER_BOT) {
      strcat(s, "b");
      if (f & BOT_SHARE)
	 strcat(s, "s");
      if (f & BOT_ALT)
	 strcat(s, "a");
      if (f & BOT_LEAF)
	 strcat(s, "l");
      if (f & BOT_REJECT)
	 strcat(s, "r");
      if (f & BOT_HUB)
	 strcat(s, "h");
   } else {
      if (f & USER_PARTY)
	 strcat(s, "p");
   }
   if (s[0] == 0)
      strcat(s, "-");
}

int count_users (struct userrec * bu)
{
   int tot = 0;
   struct userrec *u = bu;
   while (u != NULL) {
      tot++;
      u = u->next;
   }
   return tot;
}

/* forgive me :) */
struct userrec *check_dcclist_hand (char * handle)
{
   /* in the future, this will scan the dcclist for cached entries */
   /* for now, pretend it always failed to find it */
   return NULL;
}

struct userrec *get_user_by_handle (struct userrec * bu, char * handle)
{
   struct userrec *u = bu, *ret;
   if (handle == NULL)
      return NULL;
   rmspace(handle);
   if (!handle[0])
      return NULL;
   if ((lastuser != NULL) && (strcasecmp(lastuser->handle, handle) == 0) &&
       (bu == userlist)) {
      cache_hit++;
      return lastuser;
   }
   if ((banu != NULL) && (strcmp(handle, BAN_NAME) == 0) && (bu == userlist)) {
      cache_hit++;
      return banu;
   }
   if ((ignu != NULL) && (strcmp(handle, IGNORE_NAME) == 0) && (bu == userlist)) {
      cache_hit++;
      return ignu;
   }
   if (bu == userlist) {
      ret = check_chanlist_hand(handle);
      if (ret != NULL) {
	 cache_hit++;
	 return ret;
      }
      ret = check_dcclist_hand(handle);
      if (ret != NULL) {
	 cache_hit++;
	 return ret;
      }
      cache_miss++;
   }
   while (u != NULL) {
      if (strcasecmp(u->handle, handle) == 0) {
	 if ((strcmp(handle, BAN_NAME) == 0) && (bu == userlist))
	    banu = u;
	 else if ((strcmp(handle, IGNORE_NAME) == 0) && (bu == userlist))
	    ignu = u;
	 else if (bu == userlist)
	    lastuser = u;
	 return u;
      }
      u = u->next;
   }
   return NULL;
}

void clear_chanrec (struct userrec * u)
{
   struct chanuserrec *ch, *z;
   ch = u->chanrec;
   while (ch != NULL) {
      z = ch;
      ch = ch->next;
      if (z->info != NULL)
	 nfree(z->info);
      nfree(z);
   }
   u->chanrec = NULL;
}

struct chanuserrec *get_chanrec (struct userrec * u, char * chname)
{
   struct chanuserrec *ch = u->chanrec;
   while (ch != NULL) {
      if (strcasecmp(ch->channel, chname) == 0)
	 return ch;
      ch = ch->next;
   }
   return NULL;
}

struct chanuserrec * add_chanrec (struct userrec * u, char * chname,
				  unsigned int flags, time_t laston)
{
   struct chanuserrec *ch;
   ch = (struct chanuserrec *) nmalloc(sizeof(struct chanuserrec));
   ch->next = u->chanrec;
   u->chanrec = ch;
   ch->info = NULL;
   ch->flags = flags;
   ch->laston = laston;
   touch_laston(u, chname, laston);
   strncpy(ch->channel, chname, 40);
   ch->channel[40] = 0;
   if (!noshare && !(u->flags & USER_UNSHARED))
     shareout(findchan(chname),"+cr %s %s\n",u->handle,chname);
   return ch;
}

void add_chanrec_by_handle (struct userrec * bu, char * hand, char * chname,
			    unsigned int flags, time_t laston)
{
   struct userrec *u;
   u = get_user_by_handle(bu, hand);
   if (u == NULL)
      return;
   if (!get_chanrec(u, chname)) 
     add_chanrec(u, chname, flags, laston);
}

int is_user2 (struct userrec * bu, char * handle)
{
   struct userrec *u;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return 0;
   else
      return 1;
}

int is_user (char * handle)
{
   return is_user2(userlist, handle);
}

/* fix capitalization, etc */
void correct_handle (char * handle)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return;
   strcpy(handle, u->handle);
}

void clear_userlist (struct userrec * bu)
{
   struct userrec *u = bu, *v;
   context;
   while (u != NULL) {
      clearq(u->host);
      clear_chanrec(u);
      if (u->email != NULL)
	 nfree(u->email);
      if (u->dccdir != NULL)
	 nfree(u->dccdir);
      if (u->comment != NULL)
	 nfree(u->comment);
      if (u->info != NULL)
	 nfree(u->info);
      if (u->xtra != NULL)
	 nfree(u->xtra);
      if (u->lastonchan != NULL)
	nfree(u->lastonchan);
      v = u->next;
      nfree(u);
      u = v;
   }
   if (userlist == bu) {
      clear_chanlist();
      lastuser = banu = ignu = NULL;
   }
   /* remember to set your userlist to NULL after calling this */
   context;
}

/* find CLOSEST host match */
/* (if "*!*@*" and "*!*@*clemson.edu" both match, use the latter!) */
/* 26feb: CHECK THE CHANLIST FIRST to possibly avoid needless search */
struct userrec *get_user_by_host (char * host)
{
   struct userrec *u = userlist, *ret;
   struct eggqueue *q;
   int cnt, i;
   if (host == NULL)
      return NULL;
   rmspace(host);
   if (!host[0])
      return NULL;
   ret = check_chanlist(host);
   cnt = 0;
   if (ret != NULL) {
      cache_hit++;
      return ret;
   }
   cache_miss++;
   while (u != NULL) {
      q = u->host;
      while (q != NULL) {
	 i = wild_match(q->item, host);
	 if (i > cnt) {
	    ret = u;
	    cnt = i;
	 }
	 q = q->next;
      }
      u = u->next;
   }
   if (ret != NULL) {
      lastuser = ret;
      set_chanlist(host, ret);
   }
   return ret;
}

void get_handle_by_host (char * nick, char * host)
{
   struct userrec *u;
   u = get_user_by_host(host);
   if (u == NULL) {
      nick[0] = '*';
      nick[1] = 0;
      return;
   }
   strcpy(nick, u->handle);
}

struct userrec *get_user_by_equal_host (char * host)
{
   struct userrec *u = userlist;
   struct eggqueue *q;
   while (u != NULL) {
      q = u->host;
      while (q != NULL) {
	 if (strcasecmp(q->item, host) == 0)
	    return u;
	 q = q->next;
      }
      u = u->next;
   }
   return NULL;
}

/* try: pass_match_by_host("-",host)
   will return 1 if no password is set for that host   */
int pass_match_by_host (char * pass, char * host)
{
   struct userrec *u;
   char new[20];
   u = get_user_by_host(host);
   if (u == NULL)
      return 0;
   if (strcmp(u->pass, "-") == 0)
      return 1;
   else if ((pass[0] == '-') || !pass[0])
      return 0;
   if (u->flags & USER_BOT) {
      if (strcmp(u->pass, pass) == 0)
	 return 1;
   } else {
      if (strlen(pass) > 15)
	 pass[15] = 0;
      encrypt_pass(pass, new);
      if (strcmp(u->pass, new) == 0)
	 return 1;
   }
   return 0;
}

int pass_match_by_handle (char * pass, char * handle)
{
   struct userrec *u;
   char new[20];
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   if (strcmp(u->pass, "-") == 0)
      return 1;
   else if ((pass[0] == '-') || !pass[0])
      return 0;
   if (u->flags & USER_BOT) {
      if (strcmp(u->pass, pass) == 0)
	 return 1;
   } else {
      if (strlen(pass) > 15)
	 pass[15] = 0;
      encrypt_pass(pass, new);
      if (strcmp(u->pass, new) == 0)
	 return 1;
   }
   return 0;
}

void get_pass_by_handle (char * handle, char * pass)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      pass[0] = 0;
      return;
   }
   strcpy(pass, u->pass);
   return;
}

int write_user (struct userrec * u, FILE * f, int shr)
{
   char s[181];
   struct eggqueue *q;
   struct chanuserrec *ch;
   struct chanset_t *cst;
   flags2str((u->flags & USER_MASK), s);
   /* for user-created flags that could have any name, depending: */
   if (u->flags & USER_FLAG1)
      strcat(s, "1");
   if (u->flags & USER_FLAG2)
      strcat(s, "2");
   if (u->flags & USER_FLAG3)
      strcat(s, "3");
   if (u->flags & USER_FLAG4)
      strcat(s, "4");
   if (u->flags & USER_FLAG5)
      strcat(s, "5");
   if (u->flags & USER_FLAG6)
      strcat(s, "6");
   if (u->flags & USER_FLAG7)
      strcat(s, "7");
   if (u->flags & USER_FLAG8)
      strcat(s, "8");
   if (u->flags & USER_FLAG9)
      strcat(s, "9");
   if (u->flags & USER_FLAG0)
      strcat(s, "0");
   if (fprintf(f, "%-10s %-19s %-24s /%u %lu %u %lu\n", u->handle, u->pass, s,
	       u->uploads, u->upload_k, u->dnloads, u->dnload_k) == EOF)
      return 0;
   q = u->host;
   s[0] = 0;
   while (q != NULL) {
      if (!s[0]) {
	 strcpy(s, "-         ");
	 strcat(s, q->item);
      } else {
	 if (strlen(s) + strlen(q->item) + 2 > 70) {
	    if (fprintf(f, "%s\n", s) == EOF)
	       return 0;
	    strcpy(s, "-         ");
	    strcat(s, q->item);
	 } else {
	    strcat(s, ", ");
	    strcat(s, q->item);
	 }
      }
      q = q->next;
   }
   if (s[0]) {
      if (fprintf(f, "%s\n", s) == EOF)
	 return 0;
   }
   ch = u->chanrec;
   while (ch != NULL) {
      cst = findchan(ch->channel);
      if ((shr != 1) || ((cst->stat & CHAN_SHARED))) {
	 chflags2str((ch->flags & CHANUSER_MASK), s);
	 /* now fill in user-defined flags */
	 if (ch->flags & CHANUSER_1)
	    strcat(s, "1");
	 if (ch->flags & CHANUSER_2)
	    strcat(s, "2");
	 if (ch->flags & CHANUSER_3)
	    strcat(s, "3");
	 if (ch->flags & CHANUSER_4)
	    strcat(s, "4");
	 if (ch->flags & CHANUSER_5)
	    strcat(s, "5");
	 if (ch->flags & CHANUSER_6)
	    strcat(s, "6");
	 if (ch->flags & CHANUSER_7)
	    strcat(s, "7");
	 if (ch->flags & CHANUSER_8)
	    strcat(s, "8");
	 if (ch->flags & CHANUSER_9)
	    strcat(s, "9");
	 if (ch->flags & CHANUSER_0)
	    strcat(s, "0");
	 if (fprintf(f, "! %-20s %lu %-10s %s\n", ch->channel, ch->laston,
		     s, (ch->info == NULL) ? "" : ch->info) == EOF)
	    return 0;
      }
      ch = ch->next;
   }
   if (u->email != NULL) {
      if (fprintf(f, "+         %s\n", u->email) == EOF)
	 return 0;
   }
   if (u->dccdir != NULL) {
      if (fprintf(f, "*         %s\n", u->dccdir) == EOF)
	 return 0;
   }
   if (u->comment != NULL) {
      if (fprintf(f, "=         %s\n", u->comment) == EOF)
	 return 0;
   }
   if (u->info != NULL) {
      if (fprintf(f, ":         %s\n", u->info) == EOF)
	 return 0;
   }
   if (u->lastonchan) {
      if (fprintf(f, "!!        %lu %s\n", u->laston, u->lastonchan) == EOF)
	 return 0;
   }
   if (u->xtra != NULL) {
      char *p = u->xtra;
      while (strlen(p) > 160) {
	 char *q = p + 160, c;
	 while ((*q != ' ') && (q != p))
	    q--;
	 if (q == p)
	    q = p + 160;
	 c = *q;
	 *q = 0;
	 if (fprintf(f, ".         %s\n", p) == EOF) {
	    *q = c;
	    return 0;
	 }
	 *q = c;
	 if (c == ' ')
	    p = q + 1;
	 else
	    p = q;
      }
      if (fprintf(f, ".         %s\n", p) == EOF)
	 return 0;
   }
   return 1;
}

/* rewrite the entire user file */
void write_userfile()
{
   FILE *f;
   char s[121], s1[81];
   time_t tt;
   struct userrec *u;
   int ok;
   context;
   call_hook(HOOK_USERFILE);
   context;
   /* also write the channel file at the same time */
#ifndef NO_IRC
   if (chanfile[0])
      write_channels();
#endif
   if (userlist == NULL)
      return;			/* no point in saving userfile */
   sprintf(s, "%s~new", userfile);
   f = fopen(s, "w");
   chmod(s, 0600);		/* make it -rw------- */
   if (f == NULL) {
      putlog(LOG_MISC, "*", USERF_ERRWRITE);
      return;
   }
   putlog(LOG_MISC, "*", USERF_WRITING);
   tt = now;
   strcpy(s1, ctime(&tt));
   fprintf(f, "#3v: %s -- %s -- written %s", ver, origbotname, s1);
   /* fprintf(f,"# wrote user file: %s",s1); */
   context;
   ok = 1;
   u = userlist;
   while ((u != NULL) && (ok)) {
      ok = write_user(u, f, 0);
      u = u->next;
   }
   context;
   ok = write_chanbans(f);
   context;
   if (!ok) {
      fclose(f);
      putlog(LOG_MISC, "*", USERF_ERRWRITE);
      return;
   }
   fclose(f);
   unlink(userfile);
   sprintf(s, "%s~new", userfile);
#ifdef RENAME
   rename(s, userfile);
#else
   movefile(s, userfile);
#endif
   context;
}

void change_pass_by_handle (char * handle, char * pass)
{
   struct userrec *u;
   char new[20];
   unsigned char *p;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return;
   if (strlen(pass) > 15)
      pass[15] = 0;
   if ((pass[0] == '-') || (u->flags & USER_BOT))
      strcpy(u->pass, pass);
   else {
      p = (unsigned char *) pass;
      while (*p) {
	 if ((*p <= 32) || (*p == 127))
	    *p = '?';
	 p++;
      }
      encrypt_pass(pass, new);
      strcpy(u->pass, new);
   }
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))))
      shareout(NULL,"chpass %s %s\n", handle, pass);
}

void change_pass_by_host (char * host, char * pass)
{
   struct userrec *u;
   char new[20];
   unsigned char *p;
   u = get_user_by_host(host);
   if (u == NULL)
      return;
   if (strlen(pass) > 15)
      pass[15] = 0;
   if ((pass[0] == '-') || (u->flags & USER_BOT))
      strcpy(u->pass, pass);
   else {
      p = (unsigned char *) pass;
      while (*p++) {
	 if ((*p <= 32) || (*p == 127))
	    *p = '?';
      }
      encrypt_pass(pass, new);
      strcpy(u->pass, new);
   }
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))))
      shareout(NULL,"chpass %s %s\n", u->handle, pass);
}

int change_handle (char * oldh, char * newh)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, oldh);
   if (u == NULL)
      return 0;
   /* nothing that will confuse the userfile */
   if ((newh[1] == 0) && ((newh[0] == '+') || (newh[0] == '*') || (newh[0] == ':') ||
	       (newh[0] == '=') || (newh[0] == '.') || (newh[0] == '-')))
      return 0;
   strcpy(u->handle, newh);
   /* yes, even send bot nick changes now: */
   if ((!noshare) && !(u->flags & USER_UNSHARED))
      shareout(NULL,"chhand %s %s\n", oldh, newh);
   return 1;
}

struct userrec *adduser (struct userrec * bu, char * handle, char * host,
			 char * pass, int flags)
{
   struct userrec *u, *x;
   char s[81];
   u = (struct userrec *) nmalloc(sizeof(struct userrec));
   /* u->next=bu; bu=u; */
   strcpy(u->handle, handle);
   strcpy(u->pass, pass);
   u->host = NULL;
   u->next = NULL;
   u->chanrec = NULL;
   /* strip out commas -- they're illegal */
   if (host[0]) {
      char *p = strchr(host, ',');
      while (p != NULL) {
	 *p = '?';
	 p = strchr(host, ',');
      }
      u->host = add_q(host, u->host);
   } else {
      u->host = add_q("none", u->host);
   }
   u->flags = flags;
   u->email = u->dccdir = u->comment = u->info = NULL;
   u->uploads = u->dnloads = 0;
   u->upload_k = u->dnload_k = 0L;
   u->laston = 0;
   u->lastonchan = 0;
   sprintf(s, "{created %lu}", (unsigned long) now);
   u->xtra = (char *) nmalloc(strlen(s) + 1);
   strcpy(u->xtra, s);
   if (bu == userlist)
      clear_chanlist();
   if ((!noshare) && (handle[0] != '*') && (!(flags & USER_UNSHARED))
       && (bu == userlist) && (!nulluser))
      shareout(NULL,"newuser %s %s %s %d\n", handle, host, pass, flags);
   if (bu == NULL)
      bu = u;
   else {
      if ((bu == userlist) && (lastuser != NULL))
	 x = lastuser;
      else
	 x = bu;
      while (x->next != NULL)
	 x = x->next;
      x->next = u;
      if (bu == userlist)
	 lastuser = u;
   }
   return bu;
}

void freeuser (struct userrec * u)
{
   if (u == NULL)
      return;
   clearq(u->host);
   clear_chanrec(u);
   if (u->email != NULL)
      nfree(u->email);
   if (u->dccdir != NULL)
      nfree(u->dccdir);
   if (u->comment != NULL)
      nfree(u->comment);
   if (u->info != NULL)
      nfree(u->info);
   if (u->xtra != NULL)
      nfree(u->xtra);
   if (u->lastonchan != NULL)
     nfree(u->lastonchan);
   nfree(u);
}

int deluser (char * handle)
{
   struct userrec *u = userlist, *prev = NULL;
   int fnd = 0;
   while ((u != NULL) && (!fnd)) {
      if (strcasecmp(u->handle, handle) == 0)
	 fnd = 1;
      else {
	 prev = u;
	 u = u->next;
      }
   }
   if (!fnd)
      return 0;
   if (prev == NULL)
      userlist = u->next;
   else
      prev->next = u->next;
   if ((!noshare) && (handle[0] != '*') && !(u->flags & USER_UNSHARED))
      shareout(NULL,"killuser %s\n", handle);
   freeuser(u);
   clear_chanlist();
   lastuser = NULL;
   if (strcmp(handle, BAN_NAME) == 0)
      banu = NULL;
   if (strcmp(handle, IGNORE_NAME) == 0)
      ignu = NULL;
   return 1;
}

int delhost_by_handle (char * handle, char * host)
{
   struct userrec *u;
   int i;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   u->host = del_q(host, u->host, &i);
   if (u->host == NULL)
      u->host = add_q("none", u->host);
   if ((!noshare) && (i) && !(u->flags & USER_UNSHARED))
      shareout(NULL,"-host %s %s\n", handle, host);
   clear_chanlist();
   return i;
}

int ishost_for_handle (char * handle, char * host)
{
   struct userrec *u;
   struct eggqueue *q;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   if (u->host == NULL)
      return 0;
   q = u->host;
   while (q != NULL) {
      if (strcasecmp(q->item, host) == 0)
	 return 1;
      q = q->next;
   }
   return 0;
}

void addhost_by_handle2 (struct userrec * bu, char * handle, char * hst)
{
   struct userrec *u;
   int i;
   char *p;
   struct eggqueue *q;
   char host[161];
   u = get_user_by_handle(bu, handle);
   strcpy(host, hst);
   if (u == NULL)
      return;
   if (u->host != NULL)
      if (strcmp(u->host->item, "none") == 0)
	 u->host = del_q("none", u->host, &i);
   p = strchr(host, ',');	/* commas are forbidden */
   while (p != NULL) {
      *p = '?';
      p = strchr(host, ',');
   }
   /* fred1: check for redundant hostmasks with */
   /* controversial "superpenis" algorithm ;) */
   /* I'm surprised Raistlin hasn't gotten involved in this controversy */
   if ((strcasecmp(u->handle, BAN_NAME) != 0) &&
       (strcasecmp(u->handle, IGNORE_NAME) != 0)) {
      q = u->host;
      while (q != NULL) {
	 if (wild_match(host, q->item))
	    q = u->host = del_q(q->item, u->host, &i);
	 else
	    q = q->next;
      }
   }
   u->host = add_q(host, u->host);
}

void addhost_by_handle (char * handle, char * host)
{
   struct userrec *u;
   addhost_by_handle2(userlist, handle, host);
   /* u will be cached, so really no overhead, even tho this looks dumb: */
   u = get_user_by_handle(userlist, handle);
   if ((!noshare) && !(u->flags & USER_UNSHARED)) {
      if (u->flags & USER_BOT)
	 shareout(NULL,"+bothost %s %s\n", handle, host);
      else
	 shareout(NULL,"+host %s %s\n", handle, host);
   }
   clear_chanlist();
}

void get_handle_email (char * handle, char * s)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      s[0] = 0;
      return;
   }
   if (u->email == NULL) {
      s[0] = 0;
      return;
   }
   strcpy(s, u->email);
   return;
}

void get_handle_dccdir (char * handle, char * s)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      s[0] = 0;
      return;
   }
   if (u->dccdir == NULL) {
      s[0] = 0;
      return;
   }
   strcpy(s, u->dccdir);
   return;
}

void get_handle_comment (char * handle, char * s)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      s[0] = 0;
      return;
   }
   if (u->comment == NULL) {
      s[0] = 0;
      return;
   }
   strcpy(s, u->comment);
   return;
}

void get_handle_info (char * handle, char * s)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      s[0] = 0;
      return;
   }
   if (u->info == NULL) {
      s[0] = 0;
      return;
   }
   strcpy(s, u->info);
   return;
}

void get_handle_chaninfo (char * handle, char * chname, char * s)
{
   struct userrec *u;
   struct chanuserrec *ch;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL) {
      s[0] = 0;
      return;
   }
   ch = get_chanrec(u, chname);
   if (ch == NULL) {
      s[0] = 0;
      return;
   }
   if (ch->info == NULL) {
      s[0] = 0;
      return;
   }
   strcpy(s, ch->info);
   return;
}

/* returns possibly infinite-length string, please do not modify it */
char *get_handle_xtra (char * handle)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return NULL;
   if (u->xtra == NULL)
      return NULL;
   return u->xtra;
}

/* max length for these things is now 160 */

void set_handle_email (struct userrec * bu, char * handle, char * email)
{
   struct userrec *u;
   if (email && (strlen(email) > 160))
      email[160] = 0;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->email != NULL)
      nfree(u->email);
   if (email && email[0]) {
      u->email = (char *) nmalloc(strlen(email) + 1);
      strcpy(u->email, email);
   } else
      u->email = NULL;
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))) && (bu == userlist))
     shareout(NULL,"chemail %s %s\n", handle, email?email:"");
}

void set_handle_dccdir (struct userrec * bu, char * handle, char * dir)
{
   struct userrec *u;
   if (dir && (strlen(dir) > 160))
      dir[160] = 0;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->dccdir != NULL)
      nfree(u->dccdir);
   if (dir && dir[0]) {
      u->dccdir = (char *) nmalloc(strlen(dir) + 1);
      strcpy(u->dccdir, dir);
   } else
      u->dccdir = NULL;
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))) && (bu == userlist))
     shareout(NULL,"chdccdir %s %s\n", handle, dir?dir:"");
}

void set_handle_comment (struct userrec * bu, char * handle, char * comment)
{
   struct userrec *u;
   if (comment && (strlen(comment) > 160))
      comment[160] = 0;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->comment != NULL)
      nfree(u->comment);
   if (comment && comment[0]) {
      u->comment = (char *) nmalloc(strlen(comment) + 1);
      strcpy(u->comment, comment);
   } else
      u->comment = NULL;
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))) && (bu == userlist))
     shareout(NULL,"chcomment %s %s\n", handle, comment?comment:"");
}

void set_handle_info (struct userrec * bu, char * handle, char * info)
{
   /* Modified by crisk to allow 8-bit channels to be joined */
   /* Namely, the Hebrew channel on the Undernet. */
   struct userrec *u;
   unsigned char *p;
   if (info) {
      if (strlen(info) > 80)
	info[80] = 0;
      for (p = info; *p;) {
	 if ((*p < 32) || ((*p > 126) && (*p < 224)))
	   strcpy(p, p + 1);
	 else
	   p++;
      }
   }
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->info != NULL)
      nfree(u->info);
   if (info && info[0]) {
      u->info = (char *) nmalloc(strlen(info) + 1);
      strcpy(u->info, info);
   } else
      u->info = NULL;
   if ((!noshare) && (bu == userlist) && !(u->flags & USER_UNSHARED)) {
      if (u->flags & USER_BOT)
	shareout(NULL,"chaddr %s %s\n", handle, info ? info : "");
      else if (share_greet)
	shareout(NULL,"chinfo %s %s\n", handle, info ? info : "");
   }
}

void set_handle_chaninfo (struct userrec * bu, char * handle,
			  char * chname, char * info)
{
   struct userrec *u;
   struct chanuserrec *ch;
   char *p;
   struct chanset_t *cst;
   if (info) {
      if (strlen(info) > 80)
	info[80] = 0;
      for (p = info; *p;) {
	 if ((*p < 32) || (*p == 127))
	   strcpy(p, p + 1);
	 else
	   p++;
      }
   }
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   ch = get_chanrec(u, chname);
   if (ch == NULL)
      return;
   if (ch->info != NULL)
      nfree(ch->info);
   if (info && info[0]) {
      ch->info = (char *) nmalloc(strlen(info) + 1);
      strcpy(ch->info, info);
   } else
      ch->info = NULL;
   cst = findchan(chname);
   if ((!noshare) && (bu == userlist) && 
       !(u->flags & (USER_UNSHARED | USER_BOT))
	&& share_greet){
      shareout(cst,"chchinfo %s %s %s\n", handle, chname, info?info:"");
   }
}

void set_handle_xtra (struct userrec * bu, char * handle, char * xtra)
{
   struct userrec *u;
   char *p, *q;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->xtra != NULL)
      nfree(u->xtra);
   if (xtra && xtra[0]) {
      u->xtra = (char *) nmalloc(strlen(xtra) + 1);
      strcpy(u->xtra, xtra);
   } else
      u->xtra = NULL;
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))) 
       && (bu == userlist)) {
      shareout(NULL,"clrxtra %s\n", handle);
      /* only send 450 at a time */
      if (u->xtra != NULL) {
	 p = u->xtra;
	 while (strlen(p) > 450) {
	    q = p + 450;
	    while ((q != p) && (*q != ' '))
	       q--;
	    if (q == p)
	       q = p + 450;
	    *q = 0;
	    shareout(NULL,"addxtra %s %s\n", handle, p);
	    *q = ' ';
	    p = q + 1;
	 }
	 if (*p)
	    shareout(NULL,"addxtra %s %s\n", handle, p);
      }
   }
}

void add_handle_xtra (struct userrec * bu, char * handle, char * xtra)
{
   struct userrec *u;
   char *p;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (u->xtra == NULL) {
      set_handle_xtra(bu, handle, xtra);
      return;
   }
   if (!xtra[0])
      return;
   p = u->xtra;
   u->xtra = (char *) nmalloc(strlen(xtra) + strlen(p) + 2);
   strcpy(u->xtra, p);
   strcat(u->xtra, " ");
   strcat(u->xtra, xtra);
   nfree(p);
   if ((!noshare) && (!(u->flags & (USER_BOT | USER_UNSHARED))) && (bu == userlist)) {
      shareout(NULL,"addxtra %s %s\n", handle, xtra);
   }
}

int get_attr_handle (char * handle)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   return u->flags;
}

int get_chanattr_handle (char * handle, char * chname)
{
   struct userrec *u;
   struct chanuserrec *ch;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   ch = get_chanrec(u, chname);
   if (ch == NULL)
      return 0;
   return ch->flags;
}

void set_attr_handle (char * handle, unsigned int flags)
{
   struct userrec *u;
   
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return;
   u->flags = flags;
   if ((!noshare) && !(u->flags & USER_UNSHARED)) {
      char s[100];
      flags2str((u->flags & USER_MASK), s);
      /* for user-created flags that could have any name, depending: */
      if (u->flags & USER_FLAG1)
	 strcat(s, "1");
      if (u->flags & USER_FLAG2)
	 strcat(s, "2");
      if (u->flags & USER_FLAG3)
	 strcat(s, "3");
      if (u->flags & USER_FLAG4)
	 strcat(s, "4");
      if (u->flags & USER_FLAG5)
	 strcat(s, "5");
      if (u->flags & USER_FLAG6)
	 strcat(s, "6");
      if (u->flags & USER_FLAG7)
	 strcat(s, "7");
      if (u->flags & USER_FLAG8)
	 strcat(s, "8");
      if (u->flags & USER_FLAG9)
	 strcat(s, "9");
      if (u->flags & USER_FLAG0)
	 strcat(s, "0");
      context;
      shareout(NULL,"chattr %s %s\n", u->handle, s);
      context;
   }
}

void set_chanattr_handle (char * handle, char * chname, unsigned int flags)
{
   struct userrec *u;
   struct chanuserrec *ch;
   struct chanset_t *cst;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return;
   ch = get_chanrec(u, chname);
   if ((ch == NULL) && (defined_channel(chname))) {
      ch = add_chanrec(u, chname, flags, 0L);
   } 
   if (ch == NULL)
     return; 
   ch->flags = flags;
   cst = findchan(chname);
   if ((!noshare) && !(u->flags & USER_UNSHARED)) {
      char s[100];
      chflags2str((ch->flags & CHANUSER_MASK)&~CHANBOT_SHARE, s);
      /* now fill in user-defined flags */
      if (ch->flags & CHANUSER_1)
	 strcat(s, "1");
      if (ch->flags & CHANUSER_2)
	 strcat(s, "2");
      if (ch->flags & CHANUSER_3)
	 strcat(s, "3");
      if (ch->flags & CHANUSER_4)
	 strcat(s, "4");
      if (ch->flags & CHANUSER_5)
	 strcat(s, "5");
      if (ch->flags & CHANUSER_6)
	 strcat(s, "6");
      if (ch->flags & CHANUSER_7)
	 strcat(s, "7");
      if (ch->flags & CHANUSER_8)
	 strcat(s, "8");
      if (ch->flags & CHANUSER_9)
	 strcat(s, "9");
      if (ch->flags & CHANUSER_0)
	 strcat(s, "0");
      if (!s[0])
	strcpy(s,"-");
      shareout(cst,"chattr %s %s %s\n", u->handle, s, chname);
   }
}

void change_chanflags (struct userrec * bu, char * handle, char * chname,
		       unsigned int add, unsigned int remove)
{
   struct userrec *u;
   struct chanuserrec *ch;
   struct chanset_t *chan;
   u = get_user_by_handle(bu, handle);
   if (u == NULL)
      return;
   if (chname[0] == '*') {
      chan = chanset;
      while (chan != NULL) {
	 ch = get_chanrec(u, chan->name);
	 if (ch == NULL)
	    add_chanrec(u, chan->name, add, 0L);
	 else
	    ch->flags = ((ch->flags | add) & (~remove));
	 chan = chan->next;
      }
      return;
   }
   ch = get_chanrec(u, chname);
   if (ch == NULL) {
      if (defined_channel(chname)) {
	 add_chanrec(u, chname, add, 0L);
	 return;
      }
      /* error */
      return;
   }
   ch->flags = ((ch->flags | add) & (~remove));
}

int get_attr_host (char * host)
{
   struct userrec *u;
   u = get_user_by_host(host);
   if (u == NULL)
      return 0;
   return u->flags;
}

int get_chanattr_host (char * host, char * chname)
{
   struct userrec *u;
   struct chanuserrec *ch;
   u = get_user_by_host(host);
   if (u == NULL)
      return 0;
   ch = get_chanrec(u, chname);
   if (ch == NULL)
      return 0;
   return ch->flags;
}

/* do they have +o anywhere? */
int op_anywhere (char * handle)
{
   struct userrec *u;
   struct chanuserrec *ch;
   struct chanset_t *chan;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   if (u->flags & USER_GLOBAL)
      return 1;
   chan = chanset;
   while (chan != NULL) {
      ch = get_chanrec(u, chan->name);
      if ((ch != NULL) && (ch->flags & CHANUSER_OP))
	 return 1;
      chan = chan->next;
   }
   return 0;
}

/* do they have +m anywhere? */
int master_anywhere (char * handle)
{
   struct userrec *u;
   struct chanuserrec *ch;
   struct chanset_t *chan;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   if (u->flags & USER_MASTER)
      return 1;
   chan = chanset;
   while (chan != NULL) {
      ch = get_chanrec(u, chan->name);
      if ((ch != NULL) && (ch->flags & CHANUSER_MASTER))
	 return 1;
      chan = chan->next;
   }
   return 0;
}

/* do they have +n anywhere? */
int owner_anywhere (char * handle)
{
   struct userrec *u;
   struct chanuserrec *ch;
   struct chanset_t *chan;
   u = get_user_by_handle(userlist, handle);
   if (u == NULL)
      return 0;
   if (u->flags & USER_OWNER)
      return 1;
   chan = chanset;
   while (chan != NULL) {
      ch = get_chanrec(u, chan->name);
      if ((ch != NULL) && (ch->flags & CHANUSER_OWNER))
	 return 1;
      chan = chan->next;
   }
   return 0;
}

/* get icon symbol for a user (depending on access level) */
/* (*)owner on any channel  (+)master on any channel (%) botnet master */
/* (@)op on any channel  (-)other  */
char geticon (int idx)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, dcc[idx].nick);
   if (u == NULL)
      return '-';
   if (owner_anywhere(dcc[idx].nick))
      return '*';
   if (master_anywhere(dcc[idx].nick))
      return '+';
   if (get_attr_handle(dcc[idx].nick) & USER_BOTMAST)
      return '%';
   if (op_anywhere(dcc[idx].nick))
      return '@';
   return '-';
}

/* set upload/dnload stats for a user */
void set_handle_uploads (struct userrec * bu, char * hand,
			 unsigned int ups, unsigned long upk)
{
   struct userrec *u = get_user_by_handle(bu, hand);
   if (u == NULL)
      return;
   u->uploads = ups;
   u->upload_k = upk;
}

void set_handle_dnloads (struct userrec * bu, char * hand,
			 unsigned int dns, unsigned long dnk)
{
   struct userrec *u = get_user_by_handle(bu, hand);
   if (u == NULL)
      return;
   u->dnloads = dns;
   u->dnload_k = dnk;
}

void stats_add_upload (char * hand, unsigned long bytes)
{
   struct userrec *u = get_user_by_handle(userlist, hand);
   if (u == NULL)
      return;
   u->uploads++;
   u->upload_k += ((bytes + 512) / 1024);
   if ((!noshare) && !(u->flags & (USER_BOT | USER_UNSHARED)))
      shareout(NULL,"+upload %s %lu\n", hand, bytes);
}

void stats_add_dnload (char * hand, unsigned long bytes)
{
   struct userrec *u = get_user_by_handle(userlist, hand);
   if (u == NULL)
      return;
   u->dnloads++;
   u->dnload_k += ((bytes + 512) / 1024);
   if ((!noshare) && !(u->flags & (USER_BOT | USER_UNSHARED)))
      shareout(NULL,"+dnload %s %lu\n", hand, bytes);
}

void del_chanrec (struct userrec * u, char * chname)
{
   struct chanuserrec *ch, *lst;
   lst = NULL;
   ch = u->chanrec;
   while (ch) {
      if (strcasecmp(chname, ch->channel) == 0) {
	 if (lst == NULL)
	    u->chanrec = ch->next;
	 else
	    lst->next = ch->next;
	 if (ch->info != NULL)
	    nfree(ch->info);
	 nfree(ch);
	 if (!noshare && !(u->flags & USER_UNSHARED))
	   shareout(findchan(chname),"-cr %s %s\n",u->handle,chname);   
	 return;
      }
      lst = ch;
      ch = ch->next;
   }
}

void del_chanrec_by_handle (struct userrec * bu, char * hand, char * chname)
{
   struct userrec *u;
   u = get_user_by_handle(bu, hand);
   if (u == NULL)
      return;
   del_chanrec(u, chname);
}

void touch_laston (struct userrec * u, char * chan, time_t time)
{
   if (time > 1) {
      u->laston = time;
      if (u->lastonchan) 
	nfree(u->lastonchan);
      if (chan) {
	 u->lastonchan = nmalloc(strlen(chan)+1);
	 strcpy(u->lastonchan, chan);
      } else 
	 u->lastonchan = NULL;
   } else if (time == 1) {
      u->laston = 0;
      u->lastonchan = 0;
   }
}

void touch_laston_handle (struct userrec * bu, char * hand, char * chan,
			  time_t time)
{
   struct userrec *u;
   u = get_user_by_handle(bu, hand);
   if (u == NULL)
      return;
   touch_laston(u, chan, time);
}
