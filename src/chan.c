/* 
   chan.c -- handles:
   almost everything to do with channel manipulation
   telling channel status
   'who' response
   user kickban, kick, op, deop
   idle kicking

   dprintf'ized, 27oct95
   multi-channel, 8feb96
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "main.h"
#if HAVE_GETRUSAGE
#include <sys/resource.h>
#if HAVE_SYS_RUSAGE_H
#include <sys/rusage.h>
#endif
#endif
#include "users.h"
#include "chan.h"

extern char botuserhost[];
extern char botuser[];
extern char version[];
extern int serv;
extern char botchan[];
extern char botserver[];
extern time_t online_since;
extern int backgrd;
extern int con_chan;
extern int term_z;
extern int botserverport;
extern int learn_users;
extern int mtot, htot;
extern int dcc_total;
extern struct dcc_t * dcc;
extern int use_info;
extern int passive;
extern char ver[];
extern int ban_time;
extern struct userrec *userlist;
extern int cache_hit, cache_miss;
extern int default_flags;
extern int clearbans;
extern int waiting_for_awake;
extern char newbotname[];
extern time_t trying_server;
extern time_t server_online;
extern int server_lag;
extern struct chanset_t *chanset;
extern int maxqmsg;
extern char initserver[];
extern struct eggqueue * serverlist;
extern char altnick[];
extern int curserv;
extern int default_port;
extern int raw_binds;
extern int serverror_quit;
extern int use_console_r;

/* bot's nickname */
char botname[NICKLEN + 1];
/* bot's intended nickname (might have had to switch if nick was in use) */
char origbotname[NICKLEN + 1];
/* if # servers on our side of a split falls below this, jump servers */
/* 0 = don't use this */
int min_servs = 0;
/* admin info */
char admin[121];
/* strict hostname matching (don't strip ~) */
int strict_host = 0;
/* don't update server list */
int strict_servernames = 0;
/* time to wait before re-display users info line */
int wait_info = 180;

/* dump status info out to dcc */
void tell_verbose_status (int idx, int showchan)
{
   char s[256], s1[121], s2[81];
   int i, j;
   time_t now, hr, min;
#ifndef NO_IRC
   struct chanset_t *chan;
   char *p, *q;
#endif
#if HAVE_GETRUSAGE
   struct rusage ru;
#else
#if HAVE_CLOCK
   clock_t cl;
#endif
#endif
   i = count_users(userlist);
   dprintf(idx, "I am %s, running %s:  %d user%s (mem: %uk)\n", botname, ver,
	   i, i == 1 ? "" : "s", (int) (expected_memory() / 1024));
   if (admin[0])
      dprintf(idx, "Admin: %s\n", admin);
#ifdef NO_IRC
   dprintf(idx, "Floating in limbo (no IRC interaction)\n");
#else
   p = (char *) nmalloc(11);
   strcpy(p, "Channels: ");
   chan = chanset;
   while (chan != NULL) {
      strcpy(s, chan->name);
      if (!(chan->stat & CHANACTIVE))
	 strcat(s, " (trying)");
      else if (chan->stat & CHANPEND)
	 strcat(s, " (pending)");
      else if (!me_op(chan))
	 strcat(s, " (want ops!)");
      strcat(s, ", ");
      q = (char *) nmalloc(strlen(p) + strlen(s) + 1);
      strcpy(q, p);
      strcat(q, s);
      nfree(p);
      p = q;
      chan = chan->next;
   }
   if (strlen(p) > 10) {
      p[strlen(p) - 2] = 0;
      dprintf(idx, "%s\n", p);
   }
   nfree(p);
   if (showchan) {
      int atr =0;
      if (idx != DP_STDOUT)
	atr = get_attr_handle(dcc[idx].nick)&USER_MASTER;
      chan = chanset;
      while (chan != NULL) {
	 if ((idx != DP_STDOUT) && (atr ||
	     (get_chanattr_handle(dcc[idx].nick,chan->name) & CHANUSER_MASTER)))
	   {
	      s[0] = 0;
	      if (chan->stat & CHAN_GREET)
		strcat(s, "greet, ");
	      if (chan->stat & CHAN_OPONJOIN)
		strcat(s, "auto-op, ");
	      if (chan->stat & CHAN_BITCH)
		strcat(s, "bitch, ");
	      if (s[0])
		s[strlen(s) - 2] = 0;
	      if (!s[0])
		strcpy(s, "lurking");
	      get_mode_protect(chan, s2);
	      if (chan->stat & CHANACTIVE)
		dprintf(idx, "%-10s: %2d member%c, enforcing \"%s\"  (%s)\n", chan->name,
			chan->channel.members, chan->channel.members == 1 ? ' ' : 's', s2, s);
	      else
		dprintf(idx, "%-10s: (inactive), enforcing \"%s\"  (%s)\n", chan->name,
			s2, s);
	   }
	 chan = chan->next;
      }
   }
#endif				/* NO_IRC */
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	 if (dcc[i].u.bot->status & STAT_GETTING) {
	    for (j = 0; j < dcc_total; j++)
	       if ((dcc[j].type == &DCC_SEND) &&
		   (strcasecmp(dcc[j].host, dcc[i].nick) == 0)) {
		  dprintf(idx, "Downloading userlist from %s (%d%% done)\n",
			  dcc[i].nick, (int) (100.0 * ((float) dcc[j].u.xfer->sent) /
				       ((float) dcc[j].u.xfer->length)));
	       }
	 }
	 if (dcc[i].u.bot->status & STAT_SENDING) {
	    for (j = 0; j < dcc_total; j++) {
	       if ((dcc[j].type == &DCC_GET) &&
		   (strcasecmp(dcc[j].host, dcc[i].nick) == 0)) {
		  dprintf(idx, "Sending userlist to %s (%d%% done)\n",
			  dcc[i].nick, (int) (100.0 * ((float) dcc[j].u.xfer->sent) /
				       ((float) dcc[j].u.xfer->length)));
	       }
	       if ((dcc[j].type == &DCC_GET_PENDING) &&
		   (strcasecmp(dcc[j].host, dcc[i].nick) == 0)) {
		  dprintf(idx, "Sending userlist to %s (waiting for connect)\n",
			  dcc[i].nick);
	       }
	    }
	 }
      }
#ifndef NO_IRC
   s[0] = 0;
   if (!trying_server) {
      daysdur(time(NULL), server_online, s1);
      sprintf(s, "(connected %s)", s1);
      if ((server_lag) && !(waiting_for_awake)) {
	 sprintf(s1, " (lag: %ds)", server_lag);
	 if (server_lag == (-1))
	    sprintf(s1, " (bad pong replies)");
	 strcat(s, s1);
      }
   }
   dprintf(idx, "Server %s:%d %s\n", botserver, botserverport, trying_server ?
	   "(trying)" : s);
   if (mtot)
      dprintf(idx, "Server queue is %d%% full.\n",
	      (int) ((float) (mtot * 100.0) / (float) maxqmsg));
   if (htot)
      dprintf(idx, "Help queue is %d%% full.\n",
	      (int) ((float) (htot * 100.0) / (float) maxqmsg));
#endif
   now = time(NULL);
   now -= online_since;
   s[0] = 0;
   if (now > 86400) {
      /* days */
      sprintf(s, "%d day", (int) (now / 86400));
      if ((int) (now / 86400) >= 2)
	 strcat(s, "s");
      strcat(s, ", ");
      now -= (((int) (now / 86400)) * 86400);
   }
   hr = (time_t) ((int) now / 3600);
   now -= (hr * 3600);
   min = (time_t) ((int) now / 60);
   sprintf(&s[strlen(s)], "%02d:%02d", (int) hr, (int) min);
   s1[0] = 0;
   if (backgrd)
      strcpy(s1, "background");
   else {
      if (term_z)
	 strcpy(s1, "terminal mode");
      else if (con_chan)
	 strcpy(s1, "status mode");
      else
	 strcpy(s1, "logdump mode");
   }
#if HAVE_GETRUSAGE
   getrusage(RUSAGE_SELF, &ru);
   hr = (int) ((ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) / 60);
   min = (int) ((ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) - (hr * 60));
   sprintf(s2, "CPU %02d:%02d", (int) hr, (int) min);	/* actally min/sec */
#else
#if HAVE_CLOCK
   cl = (clock() / CLOCKS_PER_SEC);
   hr = (int) (cl / 60);
   min = (int) (cl - (hr * 60));
   sprintf(s2, "CPU %02d:%02d", (int) hr, (int) min);	/* actually min/sec */
#else
   sprintf(s2, "CPU ???");
#endif
#endif
   dprintf(idx, "Online for %s  (%s)  %s  cache hit %4.1f%%\n", s, s1, s2,
       100.0 * ((float) cache_hit) / ((float) (cache_hit + cache_miss)));
   if (learn_users)
      strcpy(s1, "learn users");
   else
      strcpy(s1, "none");
   dprintf(idx, "Mode(s): %s.\n", s1);
}

/* what the channel's mode CURRENTLY is */
char *getchanmode (struct chanset_t * chan)
{
   static char s[121];
   int atr, i;
   s[0] = '+';
   i = 1;
   atr = chan->channel.mode;
   if (atr & CHANINV)
      s[i++] = 'i';
   if (atr & CHANPRIV)
      s[i++] = 'p';
   if (atr & CHANSEC)
      s[i++] = 's';
   if (atr & CHANMODER)
      s[i++] = 'm';
   if (atr & CHANTOPIC)
      s[i++] = 't';
   if (atr & CHANNOMSG)
      s[i++] = 'n';
   if (atr & CHANANON)
      s[i++] = 'a';
   if (chan->channel.key[0])
      s[i++] = 'k';
   if (chan->channel.maxmembers > -1)
      s[i++] = 'l';
   s[i] = 0;
   if (chan->channel.key[0])
      sprintf(&s[strlen(s)], " %s", chan->channel.key);
   if (chan->channel.maxmembers > -1)
      sprintf(&s[strlen(s)], " %d", chan->channel.maxmembers);
   return s;
}

/* dump channel info out to dcc */
void tell_verbose_chan_info (int idx, char * chname)
{
#ifndef NO_IRC
   char handle[20], s[121], s1[121], atrflag, chanflag;
   struct chanset_t *chan;
   int i, atr, chatr;
   time_t now;
   memberlist *m;
#endif
#ifdef NO_IRC
   dprintf(idx, "This bot runs without IRC interaction.\n");
#else
   if (!chname[0])
      chan = findchan(dcc[idx].u.chat->con_chan);
   else
      chan = findchan(chname);
   if (chan == NULL) {
      dprintf(idx, "Not active on channel %s\n", chname);
      return;
   }
   now = time(NULL);
   strcpy(s, getchanmode(chan));
   if (chan->stat & CHANPEND)
      sprintf(s1, "Processing channel %s", chan->name);
   else if (chan->stat & CHANACTIVE)
      sprintf(s1, "Channel %s", chan->name);
   else
      sprintf(s1, "Desiring channel %s", chan->name);
   dprintf(idx, "%s, %d member%s, mode %s:\n", s1, chan->channel.members,
	   chan->channel.members == 1 ? "" : "s", s);
   if (chan->channel.topic[0])
      dprintf(idx, "Channel Topic: %s\n", chan->channel.topic);
   m = chan->channel.member;
   i = 0;
   if (chan->stat & CHANACTIVE) {
      dprintf(idx, "(n = ower, m = master, o = op, d = deop, b = bot)\n");
      dprintf(idx, " NICKNAME  HANDLE    JOIN   IDLE  USER@HOST\n");
      while (m->nick[0]) {
	 if (m->joined > 0) {
	    strcpy(s, ctime(&(m->joined)));
	    if ((time(NULL) - (m->joined)) > 86400) {
	       strcpy(s1, &s[4]);
	       strcpy(s, &s[8]);
	       strcpy(&s[2], s1);
	       s[5] = 0;
	    } else {
	       strcpy(s, &s[11]);
	       s[5] = 0;
	    }
	 } else
	    strcpy(s, " --- ");
	 if (m->user == NULL) {
	    sprintf(s1, "%s!%s", m->nick, m->userhost);
	    get_handle_by_host(handle, s1);	/* <- automagically updates m->user */
	 }
	 if (m->user == NULL)
	    atr = 0;
	 else {
	    strcpy(handle, m->user->handle);
	    atr = m->user->flags;
	 }
	 chatr = get_chanattr_handle(handle, chan->name);
	 /* determine status char to use */
	 if (atr & USER_BOT)
	    atrflag = 'b';
	 else if ((atr & USER_OWNER) || (chatr & CHANUSER_OWNER))
	    atrflag = 'n';
	 else if ((atr & USER_MASTER) || (chatr & CHANUSER_MASTER))
	    atrflag = 'm';
	 else if ((chatr & CHANUSER_OP) ||
		  ((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)))
	    atrflag = 'o';
	 else if ((chatr & CHANUSER_DEOP) ||
		  ((atr & USER_DEOP) && !(chatr & CHANUSER_OP)))
	    atrflag = 'd';
	 else
	    atrflag = ' ';
	 if (m->flags & CHANOP)
	    chanflag = '@';
	 else if (m->flags & CHANVOICE)
	    chanflag = '+';
	 else
	    chanflag = ' ';
	 if (m->split > 0)
	    dprintf(idx, "%c%-9s %-9s %s %c     <- netsplit, %lus\n", chanflag,
		    m->nick, handle, s, atrflag, now - (m->split));
	 else if (strcmp(m->nick, botname) == 0)
	    dprintf(idx, "%c%-9s %-9s %s %c     <- it's me!\n", chanflag, m->nick,
		    handle, s, atrflag);
	 else {
	    /* determine idle time */
	    if (now - (m->last) > 86400)
	       sprintf(s1, "%2lud", ((now - (m->last)) / 86400));
	    else if (now - (m->last) > 3600)
	       sprintf(s1, "%2luh", ((now - (m->last)) / 3600));
	    else if (now - (m->last) > 180)
	       sprintf(s1, "%2lum", ((now - (m->last)) / 60));
	    else
	       strcpy(s1, "   ");
	    dprintf(idx, "%c%-9s %-9s %s %c %s  %s\n", chanflag, m->nick, handle,
		    s, atrflag, s1, m->userhost);
	 }
	 if (m->flags & FAKEOP)
	    dprintf(idx, "    (FAKE CHANOP GIVEN BY SERVER)\n");
	 if (m->flags & SENTOP)
	    dprintf(idx, "    (pending +o -- I'm lagged)\n");
	 if (m->flags & SENTDEOP)
	    dprintf(idx, "    (pending -o -- I'm lagged)\n");
	 if (m->flags & SENTKICK)
	    dprintf(idx, "    (pending kick)\n");
	 m = m->next;
      }
   }
   dprintf(idx, "End of channel info.\n");
#endif				/* NO_IRC */
}

/* given a [nick!]user@host, place a quick ban on them on a chan */
static char *quickban (struct chanset_t * chan, char * uhost)
{
   char s1[512], *pp;
   int i;
   static char s[512];
   strcpy(s, uhost);
   maskhost(s, s1);
   /* detect long username (10-char username can't add the '*') */
   i = 0;
   pp = strchr(uhost, '!');
   if (pp == NULL)
      pp = uhost;
   else
      pp++;
   while ((*pp) && (*pp != '@')) {
      pp++;
      i++;
   }
   if (i > 9)
      strcpy(s, s1);
   else
      sprintf(s, "*!*%s", &s1[2]);	/* gotta add that extra '*' */
   add_mode(chan, '+', 'b', s);
   flush_mode(chan, QUICK);
   return s;
}

/* things to do when i just became a chanop: */
void recheck_channel (struct chanset_t * chan)
{
   memberlist *m;
   char s[UHOSTLEN], hand[10];
   int chatr, atr;
   /* okay, sort through who needs to be deopped. */
   m = chan->channel.member;
   while (m->nick[0]) {
      sprintf(s, "%s!%s", m->nick, m->userhost);
      get_handle_by_host(hand, s);
      chatr = get_chanattr_handle(hand, chan->name);
      atr = get_attr_handle(hand);
      /* ignore myself */
      if ((newbotname[0]) && (strcasecmp(m->nick, newbotname) == 0)) {	/* skip */
      } else if ((!newbotname[0]) && (strcasecmp(m->nick, botname) == 0)) {
	 /* skip */
      } else {
	 if ((m->flags & CHANOP) && ((chatr & CHANUSER_DEOP) ||
			((atr & USER_DEOP) && !(chatr & CHANUSER_OP))) &&
	     ((chan->stat & CHAN_BITCH) &&
	      (!(chatr & CHANUSER_OP) &&
	       !((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)))))
	    add_mode(chan, '-', 'o', m->nick);
	 if ((!(m->flags & CHANOP)) &&
	     ((chatr & CHANUSER_OP) ||
	      ((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))) &&
	     (chan->stat & CHAN_OPONJOIN))
	    add_mode(chan, '+', 'o', m->nick);
	 if ((chan->stat & CHAN_ENFORCEBANS) &&
	     ((match_ban(s)) || (u_match_ban(chan->bans, s))))
	    refresh_ban_kick(chan, s, m->nick);
	 /* ^ will use the ban comment */
	 else if ((chatr & CHANUSER_KICK) || (atr & USER_KICK)) {
	    quickban(chan, m->userhost);
	    get_handle_comment(hand, s);
	    if (!s[0])
	       mprintf(serv, "KICK %s %s :...and thank you for playing.\n",
		       chan->name, m->nick);
	    else
	       mprintf(serv, "KICK %s %s :%s\n", chan->name, m->nick, s);
	 }
      }
      m = m->next;
   }
   recheck_bans(chan);
   recheck_chanmode(chan);
}

void tell_chanbans (struct chanset_t * chan, int idx, int k, char * match)
{
   banlist *b = chan->channel.ban;
   char s[UHOSTLEN], s1[UHOSTLEN], fill[256];
   time_t now;
   int min, sec;
   now = time(NULL);
   if (chan->stat & CHANACTIVE) {
      while (b->ban[0]) {
	 if ((!equals_ban(b->ban)) && (!u_equals_ban(chan->bans, b->ban))) {
	    strcpy(s, b->who);
	    splitnick(s1, s);
	    if (s1[0])
	       sprintf(fill, "%s (%s!%s)", b->ban, s1, s);
	    else if (strcasecmp(s, "existent") == 0)
	       sprintf(fill, "%s (%s)", b->ban, s);
	    else
	       sprintf(fill, "%s (server %s)", b->ban, s);
	    if (b->timer != 0) {
	       min = (now - b->timer) / 60;
	       sec = (now - b->timer) - (min * 60);
	       sprintf(s, " (active %02d:%02d)", min, sec);
	       strcat(fill, s);
	    }
	    if ((!match[0]) || (wild_match(match, b->ban)))
	       dprintf(idx, "* [%3d] %s\n", k, fill);
	    k++;
	 }
	 b = b->next;
      }
   }
   if (k == 1)
      dprintf(idx, "(There are no bans, permanent or otherwise.)\n");
}

int kill_chanban (char * chname, int idx, int k, int which)
{
   banlist *b;
   struct chanset_t *chan;
   if (k == 0)
      k = 1;
   chan = findchan(chname);
   if (chan == NULL)
      return 0;
   b = chan->channel.ban;
   while (b->ban[0]) {
      if ((!equals_ban(b->ban)) && (!u_equals_ban(chan->bans, b->ban))) {
	 if (k == which) {
	    add_mode(chan, '-', 'b', b->ban);
	    dprintf(idx, "Removed ban on '%s'.\n", b->ban);
	 }
	 k++;
      }
      b = b->next;
   }
   if (k <= which) {
      dprintf(idx, "No such ban.\n");
      return 0;
   } else
      return 1;
}

int kill_chanban_name (char * chname, int idx, char * which)
{
   banlist *b;
   struct chanset_t *chan;
   chan = findchan(chname);
   if (chan == NULL)
      return 0;
   b = chan->channel.ban;
   while (b->ban[0]) {
      if (strcasecmp(b->ban, which) == 0) {
	 add_mode(chan, '-', 'b', b->ban);
	 dprintf(idx, "Removed ban on '%s'.\n", b->ban);
	 return 1;
      }
      b = b->next;
   }
   dprintf(idx, "No such ban.\n");
   return 0;
}

/* log the channel members */
void log_chans()
{
   banlist *b;
   memberlist *m;
   struct chanset_t *chan;
   int chops, bans;
   chan = chanset;
   while (chan != NULL) {
      if ((chan->stat & CHANACTIVE) && (chan->stat & CHAN_LOGSTATUS)) {
	 m = chan->channel.member;
	 chops = 0;
	 while (m->nick[0]) {
	    if (m->flags & CHANOP)
	       chops++;
	    m = m->next;
	 }
	 b = chan->channel.ban;
	 bans = 0;
	 while (b->ban[0]) {
	    bans++;
	    b = b->next;
	 }
	 putlog(LOG_MISC, chan->name, "%-10s: %2d member%c (%2d chop%c), %2d ban%c %s",
		chan->name, chan->channel.members, chan->channel.members == 1 ? ' ' : 's',
		chops, chops == 1 ? ' ' : 's', bans, bans == 1 ? ' ' : 's', me_op(chan) ? "" :
		"(not op'd)");
      }
      chan = chan->next;
   }
}

/* dump channel info out to nick */
void tell_chan_info (char * nick)
{
   char s[256], *p, *q;
   int i;
   struct chanset_t *chan;
   mprintf(serv, "NOTICE %s :I am %s, running %s.\n", nick, botname, version);
   if (admin[0])
      mprintf(serv, "NOTICE %s :Admin: %s\n", nick, admin);
   p = (char *) nmalloc(11);
   strcpy(p, "Channels: ");
   chan = chanset;
   while (chan != NULL) {
      strcpy(s, chan->name);
      if (!(chan->stat & CHANACTIVE))
	 strcat(s, " (trying)");
      else if (chan->stat & CHANPEND)
	 strcat(s, " (pending)");
      else if (!me_op(chan))
	 strcat(s, " (want ops!)");
      strcat(s, ", ");
      q = (char *) nmalloc(strlen(p) + strlen(s) + 1);
      strcpy(q, p);
      strcat(q, s);
      nfree(p);
      p = q;
      chan = chan->next;
   }
   if (strlen(p) > 10) {
      p[strlen(p) - 2] = 0;
      mprintf(serv, "NOTICE %s :%s\n", nick, p);
   }
   nfree(p);
   i = count_users(userlist);
   mprintf(serv, "NOTICE %s :%d user%s  (mem: %uk)\n", nick, i, i == 1 ? "" : "s",
	   (int) (expected_memory() / 1024));
}

#ifndef NO_IRC

/* got 324: mode status */
/* <server> 324 <to> <channel> <mode> */
static void got324 (char * from, char * msg)
{
   int i = 1;
   char *p, *q, chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL) {
      putlog(LOG_MISC, "*", "Hmm, mode info from a channel I'm not on: %s",
	     chname);
      mprintf(serv, "PART %s\n", chname);
      return;
   }
   if (chan->mode_cur != 0)
      chan->mode_cur = 0;
   while (msg[i] != 0) {
      if (msg[i] == 'i') {
	 chan->channel.mode |= CHANINV;
	 chan->mode_cur |= CHANINV;
      }
      if (msg[i] == 'p') {
	 chan->channel.mode |= CHANPRIV;
	 chan->mode_cur |= CHANPRIV;
      }
      if (msg[i] == 's') {
	 chan->channel.mode |= CHANSEC;
	 chan->mode_cur |= CHANSEC;
      }
      if (msg[i] == 'm') {
	 chan->channel.mode |= CHANMODER;
	 chan->mode_cur |= CHANMODER;
      }
      if (msg[i] == 't') {
	 chan->channel.mode |= CHANTOPIC;
	 chan->mode_cur |= CHANTOPIC;
      }
      if (msg[i] == 'n') {
	 chan->channel.mode |= CHANNOMSG;
	 chan->mode_cur |= CHANNOMSG;
      }
      if (msg[i] == 'a') {
	 chan->channel.mode |= CHANANON;
	 chan->mode_cur |= CHANANON;
      }
      if (msg[i] == 'q') {
	 chan->channel.mode |= CHANQUIET;
	 chan->mode_cur |= CHANQUIET;
      }
      if (msg[i] == 'k') {
	 chan->mode_cur |= CHANKEY;
	 p = strchr(msg, ' ');
	 p++;
	 q = strchr(p, ' ');
	 if (q != NULL) {
	    *q = 0;
	    set_key(chan, p);
	    strcpy(p, q + 1);
	 } else {
	    set_key(chan, p);
	    *p = 0;
	 }
      }
      if (msg[i] == 'l') {
	 chan->mode_cur |= CHANLIMIT;
	 p = strchr(msg, ' ');
	 p++;
	 q = strchr(p, ' ');
	 if (q != NULL) {
	    *q = 0;
	    chan->channel.maxmembers = atoi(p);
	    strcpy(p, q + 1);
	 } else {
	    chan->channel.maxmembers = atoi(p);
	    *p = 0;
	 }
      }
      i++;
   }
}

/* got a 352: who info! */
static void got352 (char * from, char * msg)
{
   char userhost[UHOSTLEN], nick[NICKLEN], s[UHOSTLEN], hand[10];
   memberlist *m;
   int waschanop, atr, chatr;
   struct chanset_t *chan;
   split(NULL, msg);
   split(s, msg);
   chan = findchan(s);
   if (chan == NULL)
      return;
   split(userhost, msg);
   strcat(userhost, "@");
   split(&userhost[strlen(userhost)], msg);
   split(NULL, msg);
   split(nick, msg);
   sprintf(s, "%s!%s", nick, userhost);
   fixfrom(s);
   splitnick(nick, s);
   strcpy(userhost, s);
   split(s, msg);
   m = ismember(chan, nick);
   if (m == NULL) {
      m = newmember(chan);
      m->joined = m->split = 0L;
      m->flags = 0;
      m->last = time(NULL);
   }
   strcpy(m->nick, nick);
   strcpy(m->userhost, userhost);
   m->user = NULL;
   if (strcasecmp(nick, botname) == 0)
      strcpy(botuserhost, userhost);
   waschanop = me_op(chan);
   if (strchr(s, '@') != NULL)
      m->flags |= CHANOP;
   else
      m->flags &= ~CHANOP;
   if (strchr(s, '+') != NULL)
      m->flags |= CHANVOICE;
   else
      m->flags &= ~CHANVOICE;
   if ((strcasecmp(nick, botname) == 0) && (!waschanop) && (me_op(chan)))
      newly_chanop(chan);
   if ((strcasecmp(nick, botname) == 0) && (any_ops(chan)) && (!me_op(chan))) {
      if (chan->need_op[0])
	 do_tcl("need-op", chan->need_op);
   }
   sprintf(s, "%s!%s", nick, userhost);
   fixfrom(s);
   m->user = get_user_by_host(s);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chan->name);
   /* are they a chanop & Im a chanop too */
   if ((m->flags & CHANOP) && me_op(chan) &&
       /* are they a channel de-op */
       ((chatr & CHANUSER_DEOP) ||
	/* or a global de-op without channel op */
       ((atr & USER_DEOP) && !(chatr & CHANUSER_OP)) ||
	/* or is it bitch mode & their not an op anywhere */
       ((chan->stat & CHAN_BITCH) && !((atr & USER_GLOBAL) ||
       (chatr & CHANUSER_OP)))) &&
       /* and of course it's not me */
       (strcasecmp(nick, botname) != 0))
      add_mode(chan, '-', 'o', nick);
   if (((match_ban(s)) || (u_match_ban(chan->bans, s))) &&
       (strcasecmp(nick, botname) != 0) && (me_op(chan)) &&
       (chan->stat & CHAN_ENFORCEBANS))
      mprintf(serv, "KICK %s %s :banned\n", chan->name, nick);
   else if (((chatr & CHANUSER_KICK) || (atr & USER_KICK))
	    && (strcasecmp(nick, botname) != 0) &&
	    (me_op(chan))) {
      get_handle_by_host(hand, s);
      get_handle_comment(hand, userhost);
      quickban(chan, s);
      if (userhost[0])
	 mprintf(serv, "KICK %s %s :%s\n", chan->name, nick, userhost);
      else
	 mprintf(serv, "KICK %s %s :...and thank you for playing.\n", chan->name,
		 nick);
   }
}

/* got 315: end of who */
/* <server> 315 <to> <chan> :End of /who */
static void got315 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   /* may have left the channel before the who info came in */
   if (!(chan->stat & CHANPEND))
      return;
   /* finished getting who list, can now be considered officially ON CHANNEL */
   chan->stat |= CHANACTIVE;
   chan->stat &= ~CHANPEND;
   /* am *I* on the channel now? if not, well shit. */
   if (!ismember(chan, botname)) {
      putlog(LOG_MISC | LOG_JOIN, chan->name, "Oops, I'm not really on %s",
	     chan->name);
      clear_channel(chan, 1);
      chan->stat &= ~CHANACTIVE;
      mprintf(serv, "JOIN %s %s\n", chan->name, chan->key_prot);
   }
   /* do not check for i-lines here. */
}

/* got 367: ban info */
/* <server> 367 <to> <chan> <ban> [placed-by] [timestamp] */
static void got367 (char * from, char * msg)
{
   char s[UHOSTLEN], ban[81], who[UHOSTLEN], chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   nsplit(ban, msg);
   nsplit(who, msg);
   /* extended timestamp format? */
   if (who[0])
      newban(chan, ban, who);
   else
      newban(chan, ban, "existent");
   sprintf(s, "%s!%s", botname, botuserhost);
   if ((wild_match(ban, s)) && (me_op(chan)))
      add_mode(chan, '-', 'b', ban);
   if (((get_attr_host(ban) & (USER_GLOBAL | USER_MASTER)) ||
      (get_chanattr_host(ban, chname) & (CHANUSER_OP | CHANUSER_MASTER)))
       && (me_op(chan)))
      add_mode(chan, '-', 'b', ban);
   /* these will be flushed by 368: end of ban info */
}

/* got 368: end of ban list */
/* <server> 368 <to> <chan> :etc */
static void got368 (char * from, char * msg)
{
   struct chanset_t *chan;
   char chname[81];
   /* ok, now add bans that i want, which aren't set yet */
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->stat & CHAN_CLEARBANS)
      resetbans(chan);
   else {
      kill_bogus_bans(chan);
      recheck_bans(chan);
   }
   /* if i sent a mode -b on myself (deban) in got367, either */
   /* resetbans() or recheck_bans() will flush that */
}

/* got 251: lusers */
/* <server> 251 <to> :there are 2258 users and 805 invisible on 127 servers */
static void got251 (char * from, char * msg)
{
   int i;
   char servs[20];
   split(NULL, msg);
   fixcolon(msg);		/* NOTE!!! If servlimit is not set or is 0 */
   for (i = 0; i < 8; i++)
      split(NULL, msg);		/* lusers IS NOT SENT AT ALL!! */
   split(servs, msg);
   if (strncmp(msg, "servers", 7) != 0)
      return;			/* was invalid format */
   i = atoi(servs);
   if (min_servs == 0)
      return;			/* no minimum limit on servers */
   if (i < min_servs) {
      putlog(LOG_SERV, "*", "Jumping servers (need %d servers, only have %d)",
	     min_servs, i);
      tprintf(serv, "QUIT :changing servers\n");
   }
}

/* too many channels */
static void got405 (char * from, char * msg)
{
   putlog(LOG_MISC, "*", "I'm on too many channels.");
}

/* got 471: can't join channel, full */
static void got471 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   putlog(LOG_JOIN, chname, "Can't join %s (full)", chname);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->need_limit[0])
      do_tcl("need-limit", chan->need_limit);
}

/* got 473: can't join channel, invite only */
static void got473 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   putlog(LOG_JOIN, chname, "Can't join %s (+i)", chname);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->need_invite[0])
      do_tcl("need-invite", chan->need_invite);
}

/* got 474: can't join channel, banned */
static void got474 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   putlog(LOG_JOIN, chname, "Can't join %s (banned)", chname);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->need_unban[0])
      do_tcl("need-unban", chan->need_unban);
}

/* got 442: not on channel */
static void got442 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   putlog(LOG_MISC, chname, "Server says I'm not on %s :(", chname);
   clear_channel(chan, 1);
   chan->stat &= ~CHANACTIVE;
   mprintf(serv, "JOIN %s %s\n", chan->name, chan->key_prot);
}

/* got 475: can't goin channel, bad key */
static void got475 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   putlog(LOG_JOIN, chname, "Can't join %s (bad key)", chname);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->need_key[0])
      do_tcl("need-key", chan->need_key);
}

/* got invitation */
static void gotinvite (char * from, char * msg)
{
   char nick[NICKLEN];
   struct chanset_t *chan;
   static char lastnick[NICKLEN] = "*";
   static time_t lastinv = (time_t) 0L;
   split(NULL, msg);
   fixcolon(msg);
   splitnick(nick, from);
   /* come on.  more than one invite from someone within 30 seconds? */
   /* just ignore them -- don't even log it. */
   if ((strcasecmp(lastnick, nick) != 0) && (time(NULL) - lastinv < 30))
      putlog(LOG_MISC, "*", "%s!%s invited me to %s", nick, from, msg);
   strcpy(lastnick, nick);
   lastinv = time(NULL);
   chan = findchan(msg);
   if (chan == NULL)
      return;			/* some channel i don't want */
   if (chan->stat & (CHANPEND | CHANACTIVE)) {
      hprintf(serv, "NOTICE %s :I'm already here.\n", nick);
      return;
   }
   mprintf(serv, "JOIN %s %s\n", chan->name, chan->key_prot);
}

/* topic change */
static void gottopic (char * from, char * msg)
{
   char nick[NICKLEN], handle[10], s[UHOSTLEN], chname[81];
   memberlist *m;
   struct chanset_t *chan;
   split(chname, msg);
   fixcolon(msg);
   splitnick(nick, from);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   putlog(LOG_JOIN, chname, "Topic changed on %s by %s!%s: %s", chname, nick, from,
	  msg);
   m = ismember(chan, nick);
   if (m != NULL)
      m->last = time(NULL);
   sprintf(s, "%s!%s", nick, from);
   get_handle_by_host(handle, s);
   strncpy(chan->channel.topic, msg, 255);
   chan->channel.topic[255] = 0;
   check_tcl_topc(nick, from, handle, chname, msg);
}

/* 331: no current topic for this channel */
/* <server> 331 <to> <chname> :etc */
static void got331 (char * from, char * msg)
{
   char chname[81];
   struct chanset_t *chan;
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   chan->channel.topic[0] = 0;
   check_tcl_topc("*", "*", "*", chname, "");
}

/* 332: topic on a channel i've just joined */
/* <server> 332 <to> <chname> :topic goes here */
static void got332 (char * from, char * msg)
{
   struct chanset_t *chan;
   char chname[81];
   split(NULL, msg);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   fixcolon(msg);
   strcpy(chan->channel.topic, msg);
   check_tcl_topc("*", "*", "*", chname, msg);
}

void do_embedded_mode (struct chanset_t * chan, char * nick,
		       memberlist * m, char * mode)
{
#ifndef NO_IRC
   char s[81];
   while (*mode) {
      switch (*mode) {
      case 'o':
	 sprintf(s, "+o %s", nick);
	 check_tcl_mode(botserver, "", "*", chan->name, s);
	 got_op(chan, botserver, "", nick, 0, 0);
	 break;
      case 'v':
	 sprintf(s, "+v %s", nick);
	 check_tcl_mode(botserver, "", "*", chan->name, s);
	 m->flags |= CHANVOICE;
	 break;
      }
      mode++;
   }
#endif
}

/* join */
static void gotjoin (char * from, char * chname)
{
   char s[UHOSTLEN], s1[UHOSTLEN], handle[10], nick[NICKLEN], *p, *newmode;
   time_t tt;
   int i, j, atr, chatr;
   struct chanset_t *chan;
   memberlist *m;
   fixcolon(chname);
   /* ircd 2.9 sometimes does '#chname^Gmodes' when returning from splits */
   newmode = NULL;
   p = strchr(chname, 7);
   if (p != NULL) {
      *p = 0;
      newmode = (++p);
   }
   chan = findchan(chname);
   if (chan == NULL) {
      putlog(LOG_MISC, "*", "joined %s but didn't want to, dammit", chname);
      mprintf(serv, "PART %s\n", chname);
      return;
   }
   if (chan->stat & CHANPEND)
      return;
   detect_flood(from, chan, FLOOD_JOIN, 1);
   /* grab last time joined before we update it */
   get_handle_by_host(handle, from);
   get_handle_laston(chname, handle, &tt);
   splitnick(nick, from);
   if ((!(chan->stat & CHANACTIVE)) && (strcasecmp(botname, nick) != 0) &&
       (strcasecmp(newbotname, nick) != 0)) {
      /* uh, what?!  i'm on the channel?! */
      putlog(LOG_MISC, chname,
	  "confused bot: guess I'm on %s and didn't realize it", chname);
      chan->stat |= CHANACTIVE;
      chan->stat &= ~CHANPEND;
      reset_chan_info(chan);
      return;
   }
   m = ismember(chan, nick);
   if (m != NULL) {
      /* already on channel?!? */
      if (m->split == 0)
	 killmember(chan, nick);
      else if (strcasecmp(m->userhost, from) != 0)
	 killmember(chan, nick);
      else {
	 check_tcl_rejn(nick, from, handle, chan->name);
	 m->split = 0;
	 m->last = time(NULL);
	 m->flags = 0;		/* clean slate, let server refresh */
	 set_handle_laston(chname, handle, time(NULL));
	 if ((m->flags & CHANOP) && (me_op(chan)) 
	     && (chan->stat & CHAN_OPONJOIN))
	    add_mode(chan, '+', 'o', nick);
	 if (newmode) {
	    putlog(LOG_JOIN, chname, "%s (%s) returned to %s (with +%s).", nick,
		   from, chname, newmode);
	    do_embedded_mode(chan, nick, m, newmode);
	 } else
	    putlog(LOG_JOIN, chname, "%s (%s) returned to %s.", nick, from, chname);
	 return;
      }
   }
   m = newmember(chan);
   m->joined = time(NULL);
   m->split = 0L;
   m->flags = 0;
   m->last = time(NULL);
   strcpy(m->nick, nick);
   strcpy(m->userhost, from);
   sprintf(s, "%s!%s", nick, from);
   m->user = NULL;
   m->user = get_user_by_host(s);
   check_tcl_join(nick, from, handle, chname);
   set_handle_laston(chname, handle, time(NULL));
   if (newmode)
      do_embedded_mode(chan, nick, m, newmode);
   if (strcasecmp(nick, botname) == 0) {
      /* it was me joining! */
      if (newmode)
	 putlog(LOG_JOIN | LOG_MISC, chname, "%s joined %s (with +%s).",
		nick, chname, newmode);
      else
	 putlog(LOG_JOIN | LOG_MISC, chname, "%s joined %s.", nick, chname);
      chan->stat |= CHANPEND;
      mprintf(serv, "MODE %s\n", chname);
      mprintf(serv, "TOPIC %s\n", chname);
      mprintf(serv, "WHO %s\n", chname);
      mprintf(serv, "MODE %s +b\n", chname);
      /* can't get ban list till i'm opped :( */
      /* ^ this is no longer true, but it's harmless */
      /*   ^ it's not so harmless, it makes us flood off :/ 041397 -poptix */
   } else {
      if (newmode)
	 putlog(LOG_JOIN, chname, "%s (%s) joined %s (with +%s).", nick,
		from, chname, newmode);
      else
	 putlog(LOG_JOIN, chname, "%s (%s) joined %s.", nick, from, chname);
      atr = get_attr_host(s);
      chatr = get_chanattr_host(s, chname);
      for (i = 0; i < strlen(s); i++)
	 if ((unsigned char) s[i] < 32) {
	    mprintf(serv, "KICK %s %s :bogus username\n", chname, nick);
	    return;
	 }
      if (me_op(chan)) {
	 if ((((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))
	      || (chatr & CHANUSER_OP))
	     && (chan->stat & CHAN_OPONJOIN))
	    add_mode(chan, '+', 'o', nick);
	 if (strcasecmp(nick, botname) != 0) {
	    if ((match_ban(s)) || (u_match_ban(chan->bans, s)))
	       refresh_ban_kick(chan, s, nick);
	    else if ((chatr & CHANUSER_KICK) || (atr & USER_KICK)) {
	       quickban(chan, s);
	       get_handle_comment(handle, s1);
	       if (s1[0] && (s1[0] != '@'))
		  mprintf(serv, "KICK %s %s :%s\n", chname, nick, s1);
	       else
		  mprintf(serv, "KICK %s %s :...and don't come back.\n", chname, nick);
	    }
	 }
      }
      /* don't re-display greeting if they've been on the channel recently */
      if ((chan->stat & CHAN_GREET) && (use_info) && (time(NULL) - tt > wait_info))
	 showinfo(chan, handle, nick);
      i = num_notes(handle);
      for (j = 0; j < dcc_total; j++)
	 if ((dcc[j].type == &DCC_CHAT) && (strcasecmp(dcc[j].nick, handle) == 0))
	    i = 0;		/* they already know they have notes */
      if (i) {
	 hprintf(serv, "NOTICE %s :You have %d note%s waiting on %s.\n",
		 nick, i, i == 1 ? "" : "s", botname);
	 hprintf(serv, "NOTICE %s :For a list, /MSG %s NOTES [pass] INDEX\n",
		 nick, botname);
      }
   }
}

/* part */
static void gotpart (char * from, char * chname)
{
   char nick[NICKLEN], hand[10], oldfrom[UHOSTLEN];
   struct chanset_t *chan;
   fixcolon(chname);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->stat & CHANPEND)
      return;
   strcpy(oldfrom, from);
   get_handle_by_host(hand, from);
   splitnick(nick, from);
   fixcolon(chname);
   if (!(chan->stat & CHANACTIVE)) {
      /* whoa! */
      putlog(LOG_MISC, chname,
	  "confused bot: guess I'm on %s and didn't realize it", chname);
      chan->stat |= CHANACTIVE;
      chan->stat &= ~CHANPEND;
      reset_chan_info(chan);
   }
   check_tcl_part(nick, from, hand, chname);
   update_laston(chname, oldfrom);
   killmember(chan, nick);
   putlog(LOG_JOIN, chname, "%s (%s) left %s.", nick, from, chname);
   /* if it was me, all hell breaks loose: */
   if (strcasecmp(nick, botname) == 0) {
      clear_channel(chan, 1);
      chan->stat &= ~(CHANACTIVE | CHANPEND);
      mprintf(serv, "JOIN %s %s\n", chan->name, chan->key_prot);
   } else
      check_lonely_channel(chan);
}

/* kick */
static void gotkick (char * from, char * msg)
{
   char nick[NICKLEN], whodid[NICKLEN], chname[81], s[UHOSTLEN], s1[UHOSTLEN];
   char hand[10];
   memberlist *m;
   struct chanset_t *chan;
   int atr, atr1, chatr, chatr1;
   time_t now = time(NULL);
   split(chname, msg);
   chan = findchan(chname);
   if (chan == NULL)
      return;
   if (chan->stat & CHANPEND)
      return;			/* not ready yet */
   split(nick, msg);
   fixcolon(msg);
   splitnick(whodid, from);
   sprintf(s, "%s!%s", whodid, from);
   m = ismember(chan, whodid);
   if (m != NULL)
      m->last = time(NULL);
   get_handle_by_host(hand, s);
   check_tcl_kick(whodid, from, hand, chname, nick, msg);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chname);
   /* check for masskick */
   if (!(atr & (USER_FRIEND | USER_MASTER))
       && !(chatr & (CHANUSER_FRIEND | CHANUSER_MASTER)) &&
       (strcasecmp(botname, whodid) != 0) &&
       (strcasecmp(chan->kicknick, whodid) == 0)) {
      if (now - chan->kicktime <= 10) {
	 chan->kicks++;
	 if (chan->kicks >= 3) {	/* masskick */
	    tprintf(serv, "KICK %s %s :mass kick, go sit in a corner\n", chname,
		    whodid);
	    chan->kicks = 0;
	    chan->kicknick[0] = 0;
	    chan->kicktime = 0L;
	 }
      } else {
	 chan->kicks = 1;
	 chan->kicktime = time(NULL);
      }
   } else {
      strcpy(chan->kicknick, whodid);
      chan->kicktime = time(NULL);
      chan->kicks = 1;
   }
   /* kicking an oplisted person?  KICK THEM. */
   m = ismember(chan, nick);
   if (m != NULL) {
      sprintf(s1, "%s!%s", m->nick, m->userhost);
      atr1 = get_attr_host(s1);
      chatr1 = get_chanattr_host(s1, chname);
      update_laston(chname, s1);
      /* revenge on, kicker was not bot, kicker was not kickee, kickee was  *
       * oplisted, and kicker was not (whew!) You thought it was bad before *
       * look at it now with channel specific stuff fully acounted for      */
      if (((chatr1 & (CHANUSER_OP | CHANUSER_MASTER))
	   || (atr & (USER_GLOBAL | USER_MASTER)))
	  && (chan->stat & CHAN_REVENGE) &&
	  (strcasecmp(whodid, botname) != 0) &&
	  !(chatr & (CHANUSER_OP | CHANUSER_MASTER)) &&
	  !(atr & (USER_GLOBAL | USER_MASTER)) &&
	  (strcasecmp(whodid, nick) != 0))
	 tprintf(serv, "KICK %s %s :don't kick my friends, bud\n", chname, whodid);
      putlog(LOG_MODES, chname, "%s kicked from %s by %s: %s", s1, chname, s, msg);
   }
   if (strcasecmp(nick, botname) == 0) {
      chan->stat &= ~(CHANACTIVE | CHANPEND);
      mprintf(serv, "JOIN %s %s\n", chan->name, chan->key_prot);
      clear_channel(chan, 1);
      if ((chan->stat & CHAN_REVENGE) && !(atr & (USER_FRIEND | USER_MASTER)) &&
	  !(chatr & (CHANUSER_FRIEND | CHANUSER_MASTER))) {
	 take_revenge(chan, s, "kicked me off the channel");
	 /* ^put the kicker on the deop list : revenge */
      }
   } else
      killmember(chan, nick);
   if (strcasecmp(nick, botname) != 0)
      check_lonely_channel(chan);
}

/* nick change */
static void gotnick (char * ffrom, char * msg)
{
   char nick[NICKLEN], hand[10], from[UHOSTLEN], s[UHOSTLEN], s1[UHOSTLEN];
   memberlist *m, *mm;
   struct chanset_t *chan;
   strcpy(from, ffrom);
   fixfrom(from);
   get_handle_by_host(hand, from);
   strcpy(s, from);
   splitnick(nick, from);
   fixcolon(msg);
   chan = chanset;
   while (chan != NULL) {
      m = ismember(chan, nick);
      if (m != NULL) {
	 putlog(LOG_JOIN, chan->name, "Nick change: %s -> %s", nick, msg);
	 m->last = time(NULL);
	 if (strcasecmp(nick, msg) != 0) {
	    /* not just a capitalization change */
	    mm = ismember(chan, msg);
	    if (mm != NULL) {
	       /* someone on channel with old nick?! */
	       if (mm->split)
		  putlog(LOG_JOIN, chan->name, "Possible future nick collision: %s",
			 mm->nick);
	       else
		  putlog(LOG_MISC, chan->name, "* Bug: nick change to existing nick");
	       killmember(chan, mm->nick);
	    }
	 }
	 check_tcl_nick(nick, from, hand, chan->name, msg);
	 /* banned? */
	 /* compose a nick!user@host for the new nick */
	 sprintf(s1, "%s!%s", msg, from);
	 if ((chan->stat & CHAN_ENFORCEBANS) && (!(m->flags & SENTKICK)) &&
	     ((match_ban(s1)) || (u_match_ban(chan->bans, s1))))
	    refresh_ban_kick(chan, s1, nick);
	 strcpy(m->nick, msg);
      }
      chan = chan->next;
   }
   if ((strcasecmp(nick, botname) == 0) || 
       (strcasecmp(nick, newbotname) == 0)) {
      /* regained nick! */
      strncpy(botname, msg, NICKLEN);
      botname[NICKLEN] = 0;
      newbotname[0] = 0;
      waiting_for_awake = 0;
      putlog(LOG_SERV | LOG_MISC, "*", "Regained nickname '%s'.", msg);
   }
   clear_chanlist();		/* cache is meaningless now */
}

/* WALLOPS: oper's nuisance */
static void gotwall (char * from, char * msg)
{
   char nick[NICKLEN];
   char *p;
   int r;
   context;
   fixcolon(msg);
   p = strchr(from, '!');
   if ((p != NULL) && (p == strrchr(from, '!'))) {
      splitnick(nick, from);
      r = check_tcl_wall(nick, msg);
      if (r == 0)
	 putlog(LOG_WALL, "*", "!%s(%s)! %s", nick, from, msg);
   } else {
      r = check_tcl_wall(from, msg);
      if (r == 0)
	 putlog(LOG_WALL, "*", "!%s! %s", from, msg);
   }
   return;
}

/* signoff, similar to part */
static void gotquit (char * from, char * msg)
{
   char nick[NICKLEN], hand[10],oldfrom[UHOSTLEN];
   int split = 0;
   memberlist *m;
   char *p;
   struct chanset_t *chan;
   get_handle_by_host(hand, from);
   strcpy(oldfrom,from);
   splitnick(nick, from);
   fixcolon(msg);
   /* Fred1: instead of expensive wild_match on signoff, quicker method */
   /* determine if signoff string matches "%.% %.%", and only one space */
   p = strchr(msg, ' ');
   if ((p != NULL) && (p == strrchr(msg, ' '))) {
      char *z1, *z2;
      *p = 0;
      z1 = strchr(p + 1, '.');
      z2 = strchr(msg, '.');
      if ((z1 != NULL) && (z2 != NULL) && (*(z1 + 1) != 0) && (z1 - 1 != p) &&
	  (z2 + 1 != p) && (z2 != msg)) {
	 /* server split, or else it looked like it anyway */
	 /* (no harm in assuming) */
	 split = 1;
      } else
	 *p = ' ';
   }
   chan = chanset;
   while (chan != NULL) {
      m = ismember(chan, nick);
      update_laston(chan->name, oldfrom);
      if (m != NULL) {
	 if (split) {
	    m->split = time(NULL);
	    check_tcl_splt(nick, from, hand, chan->name);
	    putlog(LOG_JOIN, chan->name, "%s (%s) got netsplit.", nick, from);
	 } else {
	    check_tcl_sign(nick, from, hand, chan->name, msg);
	    putlog(LOG_JOIN, chan->name, "%s (%s) left irc: %s", nick, from, msg);
	    killmember(chan, nick);
	    check_lonely_channel(chan);
	 }
      }
      chan = chan->next;
   }
}

void check_for_split()
{
   /* called once a minute... but if we're the only one on the   *
    * channel, we only wanna send out "lusers" once every 5 mins */
   static count = 4;
   int ok = 0;
   struct chanset_t *chan;
   if (min_servs == 0)
      return;
   chan = chanset;
   while (chan != NULL) {
      if ((chan->stat & CHANACTIVE) && (chan->channel.members == 1))
	 ok = 1;
      chan = chan->next;
   }
   if (!ok)
      return;
   count++;
   if (count >= 5) {
      mprintf(serv, "LUSERS\n");
      count = 0;
   }
}

/* 001: welcome to IRC (use it to fix the server name) */
static void got001 (char * from, char * msg)
{
   struct eggqueue *x;
   int i;
   char s[121], s1[121], srv[121];
   struct chanset_t *chan;
   /* ok...param #1 of 001 = what server thinks my nick is */
   fixcolon(msg);
   strncpy(botname,msg,NICKLEN);
   botname[NICKLEN]=0;
   /* init-server */
   if (initserver[0])
      do_tcl("init-server", initserver);
   x = serverlist;
   if (x == NULL)
      return;			/* uh, no server list */
   /* below makes a mess of DEBUG_OUTPUT can we do something else? */
   mprintf(serv, "JOIN ");
   chan = chanset;
   while (chan != NULL) {
      mprintf(serv, "%s,", chan->name);
      chan->stat &= ~(CHANACTIVE | CHANPEND);
      chan->mode_cur = 0;
      chan = chan->next;
   }
   chan = chanset;
   while (chan != NULL) {
      mprintf(serv, " %s", chan->key_prot);
      chan = chan->next;
   }
   mprintf(serv, "\n");
   if (strict_servernames == 1) {
     if (strcasecmp(from, botserver) != 0) {
        putlog(LOG_MISC, "*", "(%s claims to be %s; updating server list)",
	     botserver, from);
        for (i = curserv; i > 0 && x != NULL; i--)
	   x = x->next;
        if (x == NULL) {
	   putlog(LOG_MISC, "*", "Invalid server list!");
	   return;
        }
        strcpy(s, x->item);
        splitc(srv, s, ':');
        if (!srv[0]) {
	   strcpy(srv, s);
	   sprintf(s, "%d", default_port);
        }
        sprintf(s1, "%s:%s", from, s);
        nfree(x->item);
        x->item = (char *) nmalloc(strlen(s1) + 1);
        strcpy(x->item, s1);
        strcpy(botserver, from);
     } else {
        putlog(LOG_MISC, "*", "(%s claime to be %s, server list not changed",
		botserver, from);
     }
   }
}

/* given <in> (raw stuff from server), pull off who it's from & the code */
static void parsemsg (char * in, char * from, char * code, char * params)
{
   char *p;
   from[0] = 0;
   if (in[0] == ':') {
      strcpy(in, &in[1]);
      p = strchr(in, ' ');
      if (p == NULL) {
	 from[0] = params[0] = 0;
	 strcpy(code, in);
	 return;
      }
      strcpy(params, p + 1);
      *p = 0;
      strcpy(from, in);
      *p = ' ';
      p++;
      strcpy(in, p);
   }
   p = strchr(in, ' ');
   if (p == NULL) {
      strcpy(code, in);
      params[0] = 0;
      return;
   }
   *p = 0;
   strcpy(code, in);
   *p = ' ';
   strcpy(params, p + 1);
}
/* ping from server */
static void gotpong (char * from, char * msg)
{
   split(NULL, msg);
   fixcolon(msg);		/* scrap server name */
   waiting_for_awake = 0;
   server_lag = time(NULL) - my_atoul(msg);
   if (server_lag > 99999) {
      /* bogus */
      server_lag = (-1);
   }
}

/* 302 : USERHOST to be used at a later date in a tcl command mebbe */
static void got302 (char * from, char * msg)
{
/*  char userhost[UHOSTLEN],nick[NICKLEN];
   int i,oper=0,away=0;
   context;
   split(NULL,msg); fixcolon(msg); strcpy(s,msg);
   * now we have to interpret this shit *
   * <nick>['*']'='<'+'|'-'><hostname> *
   for (i=0; i<strlen(s); i++) {
   if (s[i]==61) s[i]=' ';
   if (s[i]==42) oper=1;
   if (s[i]==43) away=1;
   }
   split(nick,s); strcpy(userhost,&s[1]);
   if (oper) nick[strlen(nick)-1]=0;

   I had specific uses for what I put here but I 
   thought I would throw the above in for starters
 */
}

/* trace failed! meaning my nick is not in use!
   206 (undernet)
   401 (other non-efnet)
   402 (Efnet)
 */
void trace_fail (char * from, char * msg)
{
   debug1("trace_fail - %s\n", msg);
   if (strcasecmp(botname, origbotname) == 0)
      return;
   putlog(LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
   strcpy(newbotname, botname);	/* save, just in case */
   strcpy(botname, origbotname);
   tprintf(serv, "NICK %s\n", botname);
}

/* 432 : bad nickname */
static void got432 (char * from, char * msg)
{
   putlog(LOG_MISC, "*", IRC_BADBOTNICK);
   /* make random nick. */
   if (!newbotname[0]) {	/* if it's due to an attempt to change nicks .. */
      strcpy(newbotname, botname);	/* store it, just in case it's raist playing */
      makepass(botname);
      botname[NICKLEN] = 0;	/* raist sux :P */
      tprintf(serv, "NICK %s\n", botname);
   } else {			/* go back to old nick */
      strcpy(botname, newbotname);
      newbotname[0] = 0;
   }
}

/* 433 : nickname in use */
/* change nicks till we're acceptable or we give up */
static void got433 (char * from, char * msg)
{
   char c, *oknicks = "^-_\\[]`", *p;
   /* could be futile attempt to regain nick: */
   if (newbotname[0]) {
      tprintf(serv, "NICK %s\n", newbotname);
      strcpy(botname, newbotname);
      newbotname[0] = 0;
      return;
   }
   /* alternate nickname defined? */
   if ((altnick[0]) && (strcasecmp(altnick, botname) != 0)) {
      strcpy(botname, altnick);
   }
   /* if alt nickname failed, drop thru to here */
   else {
      c = botname[strlen(botname) - 1];
      p = strchr(oknicks, c);
      if (((c >= '0') && (c <= '9')) || (p != NULL)) {
	 if (p == NULL) {
	    if (c == '9')
	       botname[strlen(botname) - 1] = oknicks[0];
	    else
	       botname[strlen(botname) - 1] = c + 1;
	 } else {
	    p++;
	    if (!*p)
	       botname[strlen(botname) - 1] = 'a' + random() % 26;
	    else
	       botname[strlen(botname) - 1] = (*p);
	 }
      } else {
	 if (strlen(botname) == NICKLEN)
	    botname[strlen(botname) - 1] = '0';
	 else {
	    botname[strlen(botname) + 1] = 0;
	    botname[strlen(botname)] = '0';
	 }
      }
   }
   putlog(LOG_MISC, "*", IRC_BOTNICKINUSE, botname);
   tprintf(serv, "NICK %s\n", botname);
}

/* 437 : nickname juped (Euronet) */
static void got437 (char * from, char * msg)
{
   char s[512];
   struct chanset_t *chan;
   split(NULL, msg);
   split(s, msg);
   if (strchr("#&+", s[0]) != NULL) {
      chan = findchan(s);
      if (chan != NULL) {
	 if (chan->stat & CHANACTIVE) {
	    putlog(LOG_MISC, "*", IRC_CANTCHANGENICK, s);
	    got433(from, NULL);
	 } else {
	    putlog(LOG_MISC, "*", IRC_CHANNELJUPED, s);
	 }
      }
   } else {
      putlog(LOG_MISC, "*", IRC_BOTNICKJUPED);
      got433(from, NULL);
   }
}

/* 451 : Not registered */
static void got451 (char * from, char * msg)
{
/* usually if we get this then we really fucked up somewhere
   or this is a non-standard server, so we log it and kill the socket
   hoping the next server will work :) -poptix */
   putlog(LOG_MISC, "*", IRC_NOTREGISTERED1, from);
   tprintf(serv, "QUIT :%s", IRC_NOTREGISTERED2);
   if (serv >= 0)
      killsock(serv);
   serv = (-1);
}

/* error notice */
static void goterror (char * from, char * msg)
{
   fixcolon(msg);
   putlog(LOG_SERV | LOG_MSGS, "*", "-ERROR from server- %s", msg);
   if (serverror_quit) {
      putlog(LOG_BOTS, "*", "Disconnecting from server.");
      killsock(serv);
      serv = (-1);   /* they're gonna disconnect anyway :) */
   }
}

void server_activity (char * buf) {
   char from[121], code[520], msg[520],s[520];
   if (trying_server) {
      putlog(LOG_SERV, "*", "Connected to %s", botserver);
      server_online = time(NULL);
      trying_server = 0;
      waiting_for_awake = 0;
   }
   strcpy(s, buf);
   parsemsg(buf, from, code, msg);
   fixfrom(from);
   if (use_console_r) {
      if ((strcmp(code, "PRIVMSG") == 0) || (strcmp(code, "NOTICE") == 0)) {
	 if (!match_ignore(from))
	   putlog(LOG_RAW, "*", "[@] %s", s);
      } else
	putlog(LOG_RAW, "*", "[@] %s", s);
   }
   context;
   if (raw_binds && check_tcl_raw(from, code, msg)) {	/* nothing */
   } else if (strcmp(code, "PRIVMSG") == 0)
	gotmsg(from, msg, match_ignore(from));
   else if (strcmp(code, "NOTICE") == 0)
     gotnotice(from, msg, match_ignore(from));
   else if (strcmp(code, "MODE") == 0)
     gotmode(from, msg);
   else if (strcmp(code, "JOIN") == 0)
     gotjoin(from, msg);
   else if (strcmp(code, "PART") == 0)
     gotpart(from, msg);
   else if (strcmp(code, "ERROR") == 0)
     goterror(from, msg);
   else if (strcmp(code, "PONG") == 0)
     gotpong(from, msg);
   else if (strcmp(code, "WALLOPS") == 0)
     gotwall(from, msg);
   else if (strcmp(code, "001") == 0)
     got001(from, msg);
   else if (strcmp(code, "206") == 0)
     trace_fail(from, msg);	/* undernet */
   else if (strcmp(code, "251") == 0)
     got251(from, msg);
   else if (strcmp(code, "302") == 0)
     got302(from, msg);
   else if (strcmp(code, "315") == 0)
     got315(from, msg);
   else if (strcmp(code, "324") == 0)
     got324(from, msg);
   else if (strcmp(code, "331") == 0)
     got331(from, msg);
   else if (strcmp(code, "332") == 0)
     got332(from, msg);
   else if (strcmp(code, "352") == 0)
     got352(from, msg);
   else if (strcmp(code, "367") == 0)
     got367(from, msg);
   else if (strcmp(code, "368") == 0)
     got368(from, msg);
   else if (strcmp(code, "401") == 0)
     trace_fail(from, msg);	/* other */
   else if (strcmp(code, "402") == 0)
     trace_fail(from, msg);	/* efnet */
   else if (strcmp(code, "405") == 0)
     got405(from, msg);
   else if (strcmp(code, "432") == 0)
     got432(from, msg);
   else if (strcmp(code, "433") == 0)
     got433(from, msg);
   else if (strcmp(code, "437") == 0)
     got437(from, msg);
   else if (strcmp(code, "442") == 0)
     got442(from, msg);
   else if (strcmp(code, "451") == 0)
     got451(from, msg);
   else if (strcmp(code, "471") == 0)
     got471(from, msg);
   else if (strcmp(code, "473") == 0)
     got473(from, msg);
   else if (strcmp(code, "474") == 0)
     got474(from, msg);
   else if (strcmp(code, "475") == 0)
     got475(from, msg);
   else if (strcmp(code, "QUIT") == 0)
     gotquit(from, msg);
   else if (strcmp(code, "NICK") == 0) {
      detect_flood(from, NULL, FLOOD_NICK, 0);
      gotnick(from, msg);
   } else if (strcmp(code, "KICK") == 0)
	gotkick(from, msg);
   else if (strcmp(code, "INVITE") == 0)
     gotinvite(from, msg);
   else if ((strcmp(code, "375") == 0) || 
	    (strcmp(code, "376") == 0) ||
	    (strcmp(code, "372") == 0));	/* ignore motd */
   else if (strcmp(code, "TOPIC") == 0)
     gottopic(from, msg);
   else if (strcmp(code, "PING") == 0) {
      fixcolon(msg);
      tprintf(serv, "PONG :%s\n", msg);
   }
}
/* request from a user to kickban (over dcc) */
void user_kickban (int idx, char * nick)
{
   memberlist *m;
   char s[512], note[512], *s1;
   int atr, atr1, chatr;
   struct chanset_t *chan;
   note[0] = 0;
   if (strchr(nick, ' ') != NULL) {
      split(s, nick);
      strncpy(note, nick, 65);
      note[65] = 0;
      strcpy(nick, s);
   }
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "error: invalid console channel\n");
      return;
   }
   if (!(chan->stat & CHANACTIVE)) {
      dprintf(idx, "I'm not on %s right now!\n", chan->name);
      return;
   }
   if (strcasecmp(nick, botname) == 0) {
      dprintf(idx, "You're trying to pull a Run?\n");
      return;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s\n", nick, chan->name);
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   atr = get_attr_host(s);
   atr1 = get_attr_handle(dcc[idx].nick);
   chatr = get_chanattr_host(s, chan->name);
   if (!(atr1 & USER_OWNER)) {
      if (((chatr & CHANUSER_OP) || ((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)))
	  && (!(atr1 & USER_MASTER))) {
	 dprintf(idx, "%s is a legal op.\n", nick);
	 return;
      }
      if (chatr & CHANUSER_MASTER) {
	 dprintf(idx, "%s is a %s master.\n", nick, chan->name);
	 return;
      }
      if (atr & USER_MASTER) {
	 dprintf(idx, "%s is a bot master.\n", nick);
	 return;
      }
      if (atr & USER_BOT) {
	 dprintf(idx, "%s is another channel bot!\n", nick);
	 return;
      }
   }
   if (m->flags & CHANOP)
      add_mode(chan, '-', 'o', m->nick);
   s1 = quickban(chan, m->userhost);
   mprintf(serv, "KICK %s %s :%s\n", chan->name, m->nick, note);
   u_addban(chan->bans, s1, dcc[idx].nick, note, time(NULL) + (60 * ban_time));
   dprintf(idx, "Okay, done.\n");
}

#endif /* NO_IRC */
