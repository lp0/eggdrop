/*
   chanprog.c -- handles:
   rmspace()
   maintaining the server list
   revenge punishment
   timers, utimers
   telling the current programmed settings
   initializing a lot of stuff and loading the tcl scripts

   dprintf'ized, 1nov95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

/* config file format changed 27jan94 (Tcl outdates that) */

#include "main.h"
#include <pwd.h>
#include "users.h"
#include "chan.h"
#include "modules.h"
extern module_entry *module_list;

extern int serv;
extern int shtime;
extern int op_on_join;
extern int min_servs;
extern int curserv;
extern char botname[];
extern char origbotname[];
extern char botnetnick[];
extern char botuser[];
extern char bothost[];
extern char botrealname[];
extern char botserver[];
extern char configfile[];
extern char motdfile[];
extern char admin[];
extern char userfile[];
extern char helpdir[];
extern char initserver[];
extern char notify_new[];
extern char tempdir[];
extern char ctcp_version[];
extern char ctcp_finger[];
extern char ctcp_userinfo[];
extern char textdir[];
extern char owner[];
extern char firewall[];
extern char altnick[];
extern int botserverport;
extern int dcc_total;
extern int use_stderr;
extern int learn_users;
extern int flood_thr;
extern int flood_time;
extern int flood_pub_thr;
extern int flood_pub_time;
extern int flood_join_thr;
extern int flood_join_time;
extern int flood_ctcp_thr;
extern int flood_ctcp_time;
extern int use_info;
extern int passive;
extern int require_p;
extern int conmask;
extern int default_flags;
extern int keep_all_logs;
extern int ban_time;
extern int ignore_time;
extern int make_userfile;
extern int upload_to_cd;
extern int dcc_limit;
extern int never_give_up;
extern int allow_new_telnets;
extern int keepnick;
extern int dcc_block;
extern int dcc_maxsize;
extern int dcc_users;
extern int firewallport;
extern struct dcc_t * dcc;
extern log_t * logs;
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern Tcl_Interp *interp;
extern char network[];
extern int protect_readonly;
extern int use_console_r;
extern int max_logs;

/* used when rehash-ing */
char oldnick[NICKLEN + 1] = "";
int nulluser = 0;
/* where to store notes */
char notefile[121];
/* old-style queue, still used by server list */
struct eggqueue *serverlist = NULL;
/* timers (minutely) and utimers (secondly) */
tcl_timer_t *timer = NULL, *utimer = NULL;
/* next timer of any sort will have this number */
unsigned long timer_id = 1;
/* for setting static flag of channels read from config */
int setstatic = 0;
/* default server connection port */
int default_port = 6667;


/* remove space characters from beginning and end of string */
/* (more efficent by Fred1) */
void rmspace (char * s)
{
#define whitespace(c) ( ((c)==32) || ((c)==9) || ((c)==13) || ((c)==10) )
   char *p;
   /* wipe end of string */
   for (p = s + strlen(s) - 1; ((whitespace(*p)) && (p >= s)); p--);
   if (p != s + strlen(s) - 1)
      *(p + 1) = 0;
   for (p = s; ((whitespace(*p)) && (*p)); p++);
   if (p != s)
      strcpy(s, p);
}

/* memory we should be using */
int expmem_chanprog()
{
   int tot;
   struct eggqueue *s = serverlist;
   tcl_timer_t *t;
   context;
   tot = 0;
   while (s != NULL) {
      tot += strlen(s->item) + 1;
      tot += sizeof(struct eggqueue);
      s = s->next;
   }
   t = timer;
   while (t != NULL) {
      tot += sizeof(tcl_timer_t);
      tot += strlen(t->cmd) + 1;
      t = t->next;
   }
   t = utimer;
   while (t != NULL) {
      tot += sizeof(tcl_timer_t);
      tot += strlen(t->cmd) + 1;
      t = t->next;
   }
   return tot;
}

/* add someone to a queue */
/* new server to the list */
void add_server (char * ss)
{
   char s[512];
   struct eggqueue *x, *z, *q = serverlist;
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
      x->next = NULL;
      x->item = (char *) nmalloc(strlen(s) + 1);
      strcpy(x->item, s);
      if (q == NULL)
	 q = x;
      else {
	 z = q;
	 while (z->next != NULL)
	    z = z->next;
	 z->next = x;
      }
      s[0] = 0;
      strcpy(s, s1);
   } while (s[0]);
   serverlist = q;
}

/* clear out a list */
void clearq (struct eggqueue * xx)
{
   struct eggqueue *x, *x1;
   x = xx;
   while (x != NULL) {
      x1 = x->next;
      nfree(x->item);
      nfree(x);
      x = x1;
   }
}

/* set botserver to the next available server */
/* -> if (*ptr == -1) then jump to that particular server */
void next_server (int * ptr, char * serv, int * port, char * pass)
{
   struct eggqueue *x = serverlist;
   int ok = 1, i;
   char s[121];
   if (x == NULL)
      return;
   /* -1  -->  go to specified server */
   if (*ptr == (-1)) {
      char sv[121];
      int p;
      ok = 0;
      i = 0;
      while ((x != NULL) && (!ok)) {
	 strcpy(s, x->item);
	 splitc(sv, s, ':');
	 if (!sv[0]) {
	    strcpy(sv, s);
	    p = default_port;
	 } else {
	    p = atoi(s);
	 }
	 if ((strcasecmp(sv, serv) == 0) && (p == *port))
	    ok = 1;
	 else {
	    x = x->next;
	    i++;
	 }
      }
      if (ok) {
	 *ptr = i;
	 return;
      }				/* requested server is valid */
      /* gotta add it : */
      sprintf(s, "%s:%d", serv, *port);
      if (pass[0]) {
	 strcat(s, ":");
	 strcat(s, pass);
      }
      add_server(s);
      *ptr = i;
      return;
   }
   /* find where i am and boogey */
   i = (*ptr);
   while ((i > 0) && (x != NULL)) {
      x = x->next;
      i--;
   }
   if (x != NULL) {
      x = x->next;
      (*ptr)++;
   }				/* go to next server */
   if (x == NULL) {
      x = serverlist;
      *ptr = 0;
   }				/* start over at the beginning */
   pass[0] = 0;
   strcpy(s, x->item);
   splitc(serv, s, ':');
   if (!serv[0]) {
      strcpy(serv, s);
      *port = default_port;
   } else {
      char xs[121];
      *port = atoi(s);
      splitc(xs, s, ':');
      if (xs[0])
	 strcpy(pass, s);
   }
   return;
}

/* show server list, and point out which one the bot is on */
void tell_servers (int idx)
{
   struct eggqueue *x = serverlist;
   int i, sp;
   char s[141], sv[121];
   if (x == NULL) {
      dprintf(idx, "No servers.\n");
      return;
   }
   dprintf(idx, "My server list:\n");
   i = 0;
   while (x != NULL) {
      strcpy(s, x->item);
      splitc(sv, s, ':');
      if (!sv[0]) {
	 strcpy(sv, s);
	 sp = default_port;
      } else
	 sp = atoi(s);
      sprintf(s, "  %s:%d", sv, sp);
      if (i == curserv)
	 strcat(s, "   <- I am here.");
      dprintf(idx, "%s\n", s);
      x = x->next;
      i++;
   }
}

/* revenge tactic: person did something bad, if they are oplisted,
   remove them from the op list otherwise, deop them */
void take_revenge (struct chanset_t * chan, char * who, char * reason)
{
   char nick[NICKLEN], s[UHOSTLEN], s1[UHOSTLEN], ct[81], hand[10];
   int i, chatr, atr;
   time_t tm;
   get_handle_by_host(hand, who);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chan->name);
   if ((chatr & CHANUSER_FRIEND) || (atr & USER_FRIEND)) {
      putlog(LOG_MISC, "*", "%s is a friend (%s)", who, reason);
      return;			/* argh! */
   }
   if (chatr & CHANUSER_OP) {
      set_chanattr_handle(hand, chan->name, chatr & (~CHANUSER_OP));
      putlog(LOG_MISC, "*", "No longer opping %s[%s] (%s)", hand, who, reason);
      recheck_channels();
      return;
   }
   if ((match_ban(who)) || (u_match_ban(chan->bans, who))) {
      /* what more can we do? */
      return;
   }
   /* get current time into a string */
   tm = time(NULL);
   strcpy(ct, ctime(&tm));
   ct[10] = 0;
   ct[16] = 0;
   strcpy(ct, &ct[8]);
   strcpy(&ct[2], &ct[3]);
   strcpy(&ct[7], &ct[10]);
   if ((chatr & CHANUSER_DEOP) || (atr & USER_DEOP)) {
      /* this is out of control: BAN THEM */
      putlog(LOG_MISC, "*", "Now banning %s (%s)", who, reason);
      strcpy(s1, who);
      splitnick(nick, s1);
      maskhost(s1, s);
      strcpy(s1, "*!*");
      strcat(s1, &s[2]);	/* add extra * for ban */
      sprintf(s, "(%s) %s", ct, reason);
      u_addban(chan->bans, s1, origbotname, s, time(NULL) + (60 * ban_time));
      if (me_op(chan))
	 add_mode(chan, '+', 'b', s1);
      recheck_channels();
      return;
   }
   if (hand[0] != '*') {
      /* in the user list already, cool :) */
      set_chanattr_handle(hand, chan->name, chatr | CHANUSER_DEOP);
      sprintf(s, "(%s) %s", ct, reason);
      putlog(LOG_MISC, "*", "Now deopping %s[%s] (%s)", hand, who, s);
      recheck_channels();
      return;
   }
   strcpy(s1, who);
   splitnick(nick, s1);
   maskhost(s1, s);
   while (is_user(nick)) {
      if (strncmp(nick, "bad", 3) == 0) {
	 i = atoi(&nick[3]);
	 sprintf(nick, "bad%d", i + 1);
      } else
	 strcpy(nick, "bad1");
   }
   userlist = adduser(userlist, nick, s, "-", 0);
   set_chanattr_handle(nick, chan->name, CHANUSER_DEOP);
   sprintf(s, "(%s) %s (%s)", ct, reason, who);
   set_handle_comment(userlist, nick, s);
   putlog(LOG_MISC, "*", "Now deopping %s (%s)", who, reason);
   recheck_channels();
}

int logmodes (char * s)
{
   int i;
   int res = 0;
   for (i = 0; i < strlen(s); i++)
      switch (s[i]) {
      case 'm':
      case 'M':
	 res |= LOG_MSGS;
	 break;
      case 'p':
      case 'P':
	 res |= LOG_PUBLIC;
	 break;
      case 'j':
      case 'J':
	 res |= LOG_JOIN;
	 break;
      case 'k':
      case 'K':
	 res |= LOG_MODES;
	 break;
      case 'c':
      case 'C':
	 res |= LOG_CMDS;
	 break;
      case 'o':
      case 'O':
	 res |= LOG_MISC;
	 break;
      case 'b':
      case 'B':
	 res |= LOG_BOTS;
	 break;
      case 'r':
      case 'R':
	 res |= use_console_r ? LOG_RAW : 0; 
	 break;
      case 'w':
      case 'W':
	 res |= LOG_WALL;
	 break;
      case 'x':
      case 'X':
	 res |= LOG_FILES;
	 break;
      case 's':
      case 'S':
	 res |= LOG_SERV;
	 break;
      case 'd':
      case 'D':
	 res |= LOG_DEBUG;
	 break;
      case '1':
	 res |= LOG_LEV1;
	 break;
      case '2':
	 res |= LOG_LEV2;
	 break;
      case '3':
	 res |= LOG_LEV3;
	 break;
      case '4':
	 res |= LOG_LEV4;
	 break;
      case '5':
	 res |= LOG_LEV5;
	 break;
      case '6':
	 res |= LOG_LEV6;
	 break;
      case '7':
	 res |= LOG_LEV7;
	 break;
      case '8':
	 res |= LOG_LEV8;
	 break;
      case '*':
	 res |= LOG_ALL;
	 break;
      }
   return res;
}

char *masktype (int x)
{
   static char s[20];
   char *p = s;
   if (x & LOG_MSGS)
      *p++ = 'm';
   if (x & LOG_PUBLIC)
      *p++ = 'p';
   if (x & LOG_JOIN)
      *p++ = 'j';
   if (x & LOG_MODES)
      *p++ = 'k';
   if (x & LOG_CMDS)
      *p++ = 'c';
   if (x & LOG_MISC)
      *p++ = 'o';
   if (x & LOG_BOTS)
      *p++ = 'b';
   if ((x & LOG_RAW) && use_console_r)
      *p++ = 'r';
   if (x & LOG_FILES)
      *p++ = 'x';
   if (x & LOG_SERV)
      *p++ = 's';
   if (x & LOG_DEBUG)
      *p++ = 'd';
   if (x & LOG_WALL)
      *p++ = 'w';
   if (x & LOG_LEV1)
      *p++ = '1';
   if (x & LOG_LEV2)
      *p++ = '2';
   if (x & LOG_LEV3)
      *p++ = '3';
   if (x & LOG_LEV4)
      *p++ = '4';
   if (x & LOG_LEV5)
      *p++ = '5';
   if (x & LOG_LEV6)
      *p++ = '6';
   if (x & LOG_LEV7)
      *p++ = '7';
   if (x & LOG_LEV8)
      *p++ = '8';
   *p = 0;
   return s;
}

char *maskname (int x)
{
   static char s[161];
   s[0] = 0;
   if (x & LOG_MSGS)
      strcat(s, "msgs, ");
   if (x & LOG_PUBLIC)
      strcat(s, "public, ");
   if (x & LOG_JOIN)
      strcat(s, "joins, ");
   if (x & LOG_MODES)
      strcat(s, "kicks/modes, ");
   if (x & LOG_CMDS)
      strcat(s, "cmds, ");
   if (x & LOG_MISC)
      strcat(s, "misc, ");
   if ((x & LOG_BOTS) && use_console_r)
      strcat(s, "bots, ");
   if (x & LOG_RAW)
      strcat(s, "raw, ");
   if (x & LOG_FILES)
      strcat(s, "files, ");
   if (x & LOG_SERV)
      strcat(s, "server, ");
   if (x & LOG_DEBUG)
      strcat(s, "debug, ");
   if (x & LOG_WALL)
      strcat(s, "wallops, ");
   if (x & LOG_LEV1)
      strcat(s, "level 1, ");
   if (x & LOG_LEV2)
      strcat(s, "level 2, ");
   if (x & LOG_LEV3)
      strcat(s, "level 3, ");
   if (x & LOG_LEV4)
      strcat(s, "level 4, ");
   if (x & LOG_LEV5)
      strcat(s, "level 5, ");
   if (x & LOG_LEV6)
      strcat(s, "level 6, ");
   if (x & LOG_LEV7)
      strcat(s, "level 7, ");
   if (x & LOG_LEV8)
      strcat(s, "level 8, ");
   if (!s[0])
      strcpy(s, "none, ");
   s[strlen(s) - 2] = 0;
   return s;
}

/* show all internal state variables */
void tell_settings (int idx)
{
   char s[256];
   int i;
#ifndef NO_IRC
   struct chanset_t *chan;
#endif
   dprintf(idx, "%s!%s@%s (%s)\n", origbotname, botuser, bothost, botrealname);
   if (strcmp(botnetnick, origbotname) != 0)
      dprintf(idx, "Botnet: %s\n", botnetnick);
   if (firewall[0])
      dprintf(idx, "Firewall: %s, port %d\n", firewall, firewallport);
#ifndef NO_IRC
   /* channels */
   chan = chanset;
   while (chan != NULL) {
      dprintf(idx, "Channel %s:  ", chan->name);
      get_mode_protect(chan, s);
      if (s[0])
	 dprintf(idx, " (forcing mode: %s)", s);
      s[0] = 0;
      if (chan->stat & CHAN_CLEARBANS)
	 strcat(s, "clear-bans ");
      if (chan->stat & CHAN_ENFORCEBANS)
	 strcat(s, "enforce-bans ");
      if (chan->stat & CHAN_DYNAMICBANS)
	 strcat(s, "dynamic-bans ");
      if (chan->stat & CHAN_NOUSERBANS)
	 strcat(s, "forbid-user-bans ");
      if (chan->stat & CHAN_OPONJOIN)
	 strcat(s, "op-on-join ");
      if (chan->stat & CHAN_BITCH)
	 strcat(s, "bitch ");
      if (chan->stat & CHAN_GREET)
	 strcat(s, "greet ");
      if (chan->stat & CHAN_PROTECTOPS)
	 strcat(s, "protect-ops ");
      if (chan->stat & CHAN_LOGSTATUS)
	 strcat(s, "log-status ");
      if (chan->stat & CHAN_REVENGE)
	 strcat(s, "revenge ");
      if (chan->stat & CHAN_STOPNETHACK)
	 strcat(s, "stopnethack ");
      if (chan->stat & CHAN_SECRET)
	 strcat(s, "secret ");
      if (chan->stat & CHAN_SHARED)
	 strcat(s, "shared ");
      if (!(chan->stat & CHANSTATIC))
	 strcat(s, "dynamic ");
      dprintf(idx, "\n   Options: %s\n", s);
      s[0] = 0;
      if (chan->need_op[0])
	 dprintf(idx, "   To get ops I do: %s\n", chan->need_op);
      if (chan->need_invite[0])
	 dprintf(idx, "   To get invited I do: %s\n", chan->need_invite);
      if (chan->need_limit[0])
	 dprintf(idx, "   To get the channel limit up'd I do: %s\n", chan->need_limit);
      if (chan->need_unban[0])
	 dprintf(idx, "   To get unbanned I do: %s\n", chan->need_unban);
      if (chan->need_key[0])
	 dprintf(idx, "   To get the channel key I do: %s\n", chan->need_key);
      if (chan->idle_kick)
	 dprintf(idx, "   Kicking idle users after %d min\n", chan->idle_kick);
      chan = chan->next;
   }
#endif
   dprintf(idx, "Userfile: %s   Motd: %s\n", userfile, motdfile);
   if (notefile[0])
      dprintf(idx, "Notes can be stored, in: %s\n", notefile);
   else
      dprintf(idx, "Notes can not be stored.\n");
#ifndef NO_IRC
   if (min_servs)
      dprintf(idx, "Requiring a net of at least %d server(s)\n", min_servs);
   if (initserver[0])
      dprintf(idx, "On connect, I do: %s\n", initserver);
   dprintf(idx, "Flood is: %d msg/%ds, %d pub/%ds, %d join(nick)/%ds, %d ctcp/%ds\n",
    flood_thr, flood_time, flood_pub_thr, flood_pub_time, flood_join_thr,
	   flood_join_time, flood_ctcp_thr, flood_ctcp_time);
   dprintf(idx, "Bans last %d mins, ignores last %d mins\n", ban_time,
	   ignore_time);
#endif
   dprintf(idx, "Help dir (%s), temp dir (%s), text dir (%s)\n", helpdir, tempdir,
	   textdir);
   flags2str(default_flags, s);
   dprintf(idx, "New users get flags [%s], notify: %s\n", s, notify_new);
   if (owner[0])
      dprintf(idx, "Permanent owner(s): %s\n", owner);
   for (i = 0; i < max_logs; i++)
      if (logs[i].filename != NULL) {
	 dprintf(idx, "Logfile #%d: %s on %s (%s: %s)\n", i + 1, logs[i].filename,
	 logs[i].chname, masktype(logs[i].mask), maskname(logs[i].mask));
      }
   do_module_report(idx);
}

void reaffirm_owners()
{
   char *p, s[121];
   /* make sure default owners are +n */
   if (owner[0]) {
      strcpy(s, owner);
      p = strchr(s, ',');
      while (p != NULL) {
	 *p = 0;
	 rmspace(s);
	 set_attr_handle(s, sanity_check(get_attr_handle(s) | USER_OWNER));
	 strcpy(s, p + 1);
	 p = strchr(s, ',');
      }
      rmspace(s);
      if (s[0])
	 set_attr_handle(s, sanity_check(get_attr_handle(s) | USER_OWNER));
   }
}

void chanprog()
{
   int i;
   strcpy(botrealname, "eggdrop bot");
   admin[0] = 0;
   helpdir[0] = 0;
   initserver[0] = 0;
   textdir[0] = 0;
   notefile[0] = 0;
   tempdir[0] = 0;
   for (i = 0; i < max_logs; i++) {
      if (logs[i].filename != NULL) {
	 nfree(logs[i].filename);
	 logs[i].filename = NULL;
      }
      if (logs[i].chname != NULL) {
	 nfree(logs[i].chname);
	 logs[i].chname = NULL;
      }
      if (logs[i].f != NULL) {
	 fclose(logs[i].f);
	 logs[i].f = NULL;
      }
      logs[i].mask = 0;
   }
   conmask = 0;
   /* turn off read-only variables (make them write-able) for rehash */
   protect_readonly = 0;
   /* let's make sure when adding channels to not shareout a null user */
   nulluser = 1;
   /* set static flag for channels added from config file NOT chanfile */
   setstatic = 1;
   /* now read it */
   context;
   if (!readtclprog(configfile))
      fatal("CONFIG FILE NOT FOUND", 0);
   /* We should be safe now */
   setstatic = 0;
   nulluser = 0;
   context;
   protect_readonly = 1;
   strncpy(botname, origbotname, NICKLEN);
   botname[NICKLEN] = 0;
#ifndef NO_IRC
   read_channels();
   if (chanset == NULL)
      fatal("NO CHANNELS DEFINED.", 0);
#endif
   if (!botname[0])
      fatal("NO BOT NAME.", 0);
   if (!userfile[0])
      fatal("NO USER FILE.", 0);
   if (!botuser[0]) {
      /* get this user's real username */
      int uid = getuid();
      struct passwd *pwd;
      pwd = getpwuid(uid);
      if (pwd == NULL)
	 strcpy(botuser, "UNKNOWN");
      else
	 strcpy(botuser, pwd->pw_name);
   }
   if ((int) getuid() == 0) {
      /* perhaps you should make it run something innocent here ;) */
      printf("\n\nWARNING! You are running eggdrop as root!\n");
   }
   if (!readuserfile(userfile, &userlist)) {
      if (!make_userfile)
	 fatal("USER FILE NOT FOUND!  (try 'eggdrop -m' to make one)", 0);
      printf("\n\nSTARTING BOT IN USERFILE CREATION MODE.\n");
#ifdef NO_IRC
      printf("Telnet to the bot and enter 'NEW' as your nickname.\n");
#else
      printf("Go to IRC and:  /msg %s hello\n", botname);
#endif
      printf("This will make the bot recognize you as the master.\n\n");
   } else if (make_userfile)
      fatal("USERFILE ALREADY EXISTS (drop the '-m')", 0);
   context;
#ifndef NO_IRC
   if (serverlist == NULL)
      fatal("NO SERVER.", 0);
#endif
   if (helpdir[0])
      if (helpdir[strlen(helpdir) - 1] != '/')
	 strcat(helpdir, "/");
   if (tempdir[0])
      if (tempdir[strlen(tempdir) - 1] != '/')
	 strcat(tempdir, "/");
   if (textdir[0]) {
      if (textdir[strlen(textdir) - 1] != '/')
	 strcat(textdir, "/");
   } else
      strcpy(textdir, helpdir);
   if (!botnetnick[0]) {
      strncpy(botnetnick, origbotname, 9);
      botnetnick[9] = 0;
   }
#ifdef NO_IRC
   strcpy(network, "[limbo]");
#endif
   context;
   /* test tempdir: it's vital */
   {
      FILE *f;
      char s[161];
      sprintf(s, "%s.test.file", tempdir);
      f = fopen(s, "w");
      if (f == NULL)
	 fatal("CAN'T WRITE TO TEMP DIR.", 0);
      fclose(f);
      unlink(s);
   }
   context;
   reaffirm_owners();
}

/* reload the user file from disk */
void reload()
{
   FILE *f;
   f = fopen(userfile, "r");
   if (f == NULL) {
      putlog(LOG_MISC, "*", "Can't reload user file!");
      return;
   }
   fclose(f);
   clear_userlist(userlist);
   userlist = NULL;
   if (!readuserfile(userfile, &userlist))
      fatal("USER FILE IS MISSING!", 0);
   context;
   reaffirm_owners();
   call_hook(HOOK_READ_USERFILE);
}

void rehash()
{
#ifndef NO_IRC
   struct chanset_t *chan;
   chan = chanset;
   while (chan != NULL) {
      chan->stat |= CHANFLAGGED;
      /* flag will be cleared as the channels are re-added by the config file */
      /* any still flagged afterwards will be removed */
      if (chan->stat & CHANSTATIC)
	 chan->stat &= ~CHANSTATIC;
      /* flag is added to channels read from config file */
      chan = chan->next;
   }
#endif
   /* clear out the programming */
   clear_userlist(userlist);
   userlist = NULL;
   clearq(serverlist);
   serverlist = NULL;
   strcpy(oldnick, botname);
   chanprog();
#ifndef NO_IRC
   if ((strcasecmp(oldnick, botname) != 0) && (strcasecmp(oldnick, altnick) != 0)
       && (oldnick[0])) {
      /* change botname back, don't be premature */
      strcpy(botname, oldnick);
      tprintf(serv, "NICK %s\n", origbotname);
   }
   /* change botname back incase we were using altnick previous to rehash */
   else if (oldnick[0])
      strcpy(botname, oldnick);
   if (initserver[0])
      do_tcl("init-server", initserver);
   /* remove any extra channels */
   chan = chanset;
   while (chan != NULL) {
      if (chan->stat & CHANFLAGGED) {
	 putlog(LOG_MISC, "*", "No longer supporting channel %s", chan->name);
	 if (serv >= 0)
	    mprintf(serv, "PART %s\n", chan->name);
	 clear_channel(chan, 0);
	 freeuser(chan->bans);
	 killchanset(chan->name);
	 chan = chanset;
      } else
	 chan = chan->next;
   }
   /* update stuff, as if we just got control of a channel again: */
   chan = chanset;
   while (chan != NULL) {
      recheck_channel(chan);
      chan = chan->next;
   }
#endif
}

/* brief venture into timers */

/* add a timer */
unsigned long add_timer (tcl_timer_t ** stack, int elapse, char * cmd,
			 unsigned long prev_id)
{
   tcl_timer_t *old = (*stack);
   *stack = (tcl_timer_t *) nmalloc(sizeof(tcl_timer_t));
   (*stack)->next = old;
   (*stack)->mins = elapse;
   (*stack)->cmd = (char *) nmalloc(strlen(cmd) + 1);
   strcpy((*stack)->cmd, cmd);
   /* if it's just being added back and already had an id, */
   /* don't create a new one */
   if (prev_id > 0)
      (*stack)->id = prev_id;
   else
      (*stack)->id = timer_id++;
   return (*stack)->id;
}

/* remove a timer, by id */
int remove_timer (tcl_timer_t ** stack, unsigned long id)
{
   tcl_timer_t *mark = *stack, *old;
   int ok = 0;
   *stack = NULL;
   while (mark != NULL) {
      if (mark->id != id)
	 add_timer(stack, mark->mins, mark->cmd, mark->id);
      else
	 ok++;
      old = mark;
      mark = mark->next;
      nfree(old->cmd);
      nfree(old);
   }
   return ok;
}

/* check timers, execute the ones that have expired */
static void do_check_timers (tcl_timer_t ** stack)
{
   tcl_timer_t *mark = *stack, *old;
   Tcl_DString ds;
   int argc, i;
   char **argv;
   /* new timers could be added by a Tcl script inside a current timer */
   /* so i'll just clear out the timer list completely, and add any */
   /* unexpired timers back on */
   context;
   *stack = NULL;
   while (mark != NULL) {
      if (mark->mins > 0)
	 mark->mins--;
      if (mark->mins == 0) {
	 int code;
	 set_tcl_vars();
	 Tcl_DStringInit(&ds);
	 if (Tcl_SplitList(interp, mark->cmd, &argc, &argv) != TCL_OK) {
	    putlog(LOG_MISC, "*", "(Timer) Error for '%s': %s", mark->cmd,
		   interp->result);
	 } else {
	    for (i = 0; i < argc; i++)
	       Tcl_DStringAppendElement(&ds, argv[i]);
	    n_free(argv, "", 0);
	    code = Tcl_Eval(interp, Tcl_DStringValue(&ds));
	    /* code=Tcl_Eval(interp,mark->cmd); */
	    Tcl_DStringFree(&ds);
	    if (code != TCL_OK)
	       putlog(LOG_MISC, "*", "(Timer) Error for '%s': %s", mark->cmd,
		      interp->result);
	 }
      } else
	 add_timer(stack, mark->mins, mark->cmd, mark->id);
      context;
      old = mark;
      mark = mark->next;
      nfree(old->cmd);
      nfree(old);
   }
}

void check_timers()
{
   do_check_timers(&timer);
}

void check_utimers()
{
   do_check_timers(&utimer);
}

/* wipe all timers */
void wipe_timers (Tcl_Interp * irp, tcl_timer_t ** stack)
{
   tcl_timer_t *mark = *stack, *old;
   while (mark != NULL) {
      old = mark;
      mark = mark->next;
      remove_timer(stack, old->id);
   }
   *stack = NULL;
}

/* return list of timers */
void list_timers (Tcl_Interp * irp, tcl_timer_t * stack)
{
   tcl_timer_t *mark = stack;
   char mins[10], id[20], *argv[3], *x;
   while (mark != NULL) {
      sprintf(mins, "%u", mark->mins);
      sprintf(id, "timer%lu", mark->id);
      argv[0] = mins;
      argv[1] = mark->cmd;
      argv[2] = id;
      x = Tcl_Merge(3, argv);
      Tcl_AppendElement(irp, x);
      n_free(x, "", 0);
      mark = mark->next;
   }
}
