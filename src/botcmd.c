/*
 * botcmd.c -- handles: commands that comes across the botnet userfile
 * transfer and update commands from sharebots
 * 
 * dprintf'ized, 10nov95 
 */
/*
 * This file is part of the eggdrop source code copyright (c) 1997 Robey
 * Pointer and is distributed according to the GNU general public license.
 * For full details, read the top of 'main.c' or the file called COPYING
 * that was distributed with this code. 
 */

#include "main.h"
#include "tandem.h"
#include "users.h"
#include "chan.h"
#include "modules.h"

extern tand_t tand[];
extern int tands;
extern char botnetnick[];
extern int dcc_total;
extern struct dcc_t * dcc;
extern char motdfile[];
extern struct userrec *userlist;
extern char ver[];
extern char os[];
extern struct chanset_t *chanset;
extern int serv;
extern char ban_time;
extern char ignore_time;
extern char network[];
extern char admin[];
extern time_t now;
extern int noshare;
extern int remote_boots;

/* static buffer for goofy bot stuff */
static char TBUF[1024];

/* used for 1.0 compatibility: if a join message arrives with no sock#, */
/* i'll just grab the next "fakesock" # (incrementing to assure uniqueness) */
static int fakesock = 2300;

static void fake_alert (int idx)
{
   tprintf(dcc[idx].sock, "chat %s NOTICE: Fake message rejected.\n",
	   botnetnick);
}

/* chan <from> <chan> <text> */
static void bot_chan (int idx, char * par)
{
   char *from = TBUF, *s = TBUF + 512, *p;
   int i, chan;
   nsplit(from, par);
   chan = atoi(par);
   nsplit(NULL, par);
   /* strip annoying control chars */
   for (p = from; *p;) {
      if ((*p < 32) || (*p == 127))
	 strcpy(p, p + 1);
      else
	 p++;
   }
   p = strchr(from, '@');
   if (p == NULL) {
      sprintf(s, "*** (%s) %s", from, par);
      p = from;
   } else {
      sprintf(s, "<%s> %s", from, par);
      *p = 0;
      partyidle(p + 1, from);
      *p = '@';
      p++;
   }
   i = nextbot(p);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   tandout_but(idx, "chan %s %d %s\n", from, chan, par);
   chanout(chan, "%s\n", s);
   if (strchr(from, '@') != NULL)
      check_tcl_chat(from, chan, par);
   else
      check_tcl_bcst(from, chan, par);
}

/* chat <from> <notice>  -- only from bots */
static void bot_chat (int idx, char * par)
{
   char *from = TBUF;
   int i;
   nsplit(from, par);
   if (strchr(from, '@') != NULL) {
      fake_alert(idx);
      return;
   }
   /* make sure the bot is valid */
   i = nextbot(from);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   chatout("*** (%s) %s\n", from, par);
   tandout_but(idx, "chat %s %s\n", from, par);
}

/* actchan <from> <chan> <text> */
static void bot_actchan (int idx, char * par)
{
   char *from = TBUF, *p;
   int i, chan;
   nsplit(from, par);
   p = strchr(from, '@');
   if (p == NULL) {
      /* how can a bot do an action? */
      fake_alert(idx);
      return;
   }
   *p = 0;
   partyidle(p + 1, from);
   *p = '@';
   p++;
   i = nextbot(p);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   chan = atoi(par);
   nsplit(NULL, par);
   chanout(chan, "* %s %s\n", from, par);
   check_tcl_act(from, chan, par);
   tandout_but(idx, "actchan %s %d %s\n", from, chan, par);
}

/* priv <from> <to> <message> */
static void bot_priv (int idx, char * par)
{
   char *from = TBUF, *p, *to = TBUF + 600, *tobot = TBUF + 512;
   int i;
   nsplit(from, par);
   nsplit(tobot, par);
   tobot[40] = 0;
   splitc(to, tobot, '@');
   p = strchr(from, '@');
   if (p != NULL)
      p++;
   else
      p = from;
   i = nextbot(p);
   if ((i != idx) || (!to[0])) {
      fake_alert(idx);
      return;
   }
   if (strcasecmp(tobot, botnetnick) == 0) {	/* for me! */
      if (p == from)
	 add_note(to, from, par, -2, 0);
      else {
	 i = add_note(to, from, par, -1, 0);
	 switch (i) {
	 case NOTE_ERROR:
	    tprintf(dcc[idx].sock, "priv %s %s No such user %s.\n", botnetnick,
		    from, to);
	    break;
	 case NOTE_STORED:
	    tprintf(dcc[idx].sock, "priv %s %s Not online; note stored.\n",
		    botnetnick, from);
	    break;
	 case NOTE_FULL:
	    tprintf(dcc[idx].sock, "priv %s %s Notebox is full, sorry.\n",
		    botnetnick, from);
	    break;
	 case NOTE_AWAY:
	    tprintf(dcc[idx].sock, "priv %s %s %s is away; note stored.\n",
		    botnetnick, from, to);
	    break;
	 case NOTE_TCL:
	    break;		/* do nothing */
	 case NOTE_OK:
	    tprintf(dcc[idx].sock, "priv %s %s Note sent to %s.\n", botnetnick,
		    from, to);
	    break;
	 }
      }
   } else {			/* pass it on */
      i = nextbot(tobot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "priv %s %s@%s %s\n", from, to, tobot, par);
   }
}

static void bot_bye (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "Disconnected from: %s", dcc[idx].nick);
   chatout("*** Disconnected from: %s\n", dcc[idx].nick);
   tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
   tandout_but(idx, "chat %s Disconnected from: %s\n", botnetnick, dcc[idx].nick);
   tprintf(dcc[idx].sock, "*bye\n");
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void remote_tell_info (int z, char * nick)
{
   char s[256], *p, *q;
   struct chanset_t *chan;
   p = (char *) nmalloc(11);
   strcpy(p, "Channels: ");
   chan = chanset;
   while (chan != NULL) {
      if (!(chan->stat & CHAN_SECRET)) {
	 strcpy(s, chan->name);
	 strcat(s, ", ");
	 q = (char *) nmalloc(strlen(p) + strlen(s) + 1);
	 strcpy(q, p);
	 strcat(q, s);
	 nfree(p);
	 p = q;
      }
      chan = chan->next;
   }
   if (strlen(p) > 10) {
      p[strlen(p) - 2] = 0;
      tprintf(z, "priv %s %s %s  (%s)\n", botnetnick, nick, p, ver);
   } else
      tprintf(z, "priv %s %s No channels.  (%s)\n", botnetnick, nick, ver);
   nfree(p);
   if (admin[0])
      tprintf(z, "priv %s %s Admin: %s\n", botnetnick, nick, admin);
}

static void remote_tell_who (int z, char * nick, int chan)
{
   int i, k, ok = 0;
   char s[121];
   time_t tt;
   tt = time(NULL);
   remote_tell_info(z, nick);
   if (chan == 0)
      tprintf(z, "priv %s %s Party line members:  (* = owner, + = master, @ = op)\n",
	      botnetnick, nick);
   else {
      char *cname = get_assoc_name(chan);
      if (cname == NULL)
	 tprintf(z, "priv %s %s People on channel %s%d:  (* = owner, + = master, @ = op)\n",
	    botnetnick, nick, (chan < 100000) ? "" : "*", chan % 100000);
      else
	 tprintf(z, "priv %s %s People on channel '%s' (%s%d):  (* = owner, + = master, @ = op)\n",
		 botnetnick, nick, cname, (chan < 100000) ? "" : "*", chan % 100000);
   }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel == chan) {
	    sprintf(s, "  %c%-10s %s", (geticon(i) == '-' ? ' ' : geticon(i)),
		    dcc[i].nick, dcc[i].host);
	    if (tt - dcc[i].timeval > 300) {
	       unsigned long days, hrs, mins;
	       days = (tt - dcc[i].timeval) / 86400;
	       hrs = ((tt - dcc[i].timeval) - (days * 86400)) / 3600;
	       mins = ((tt - dcc[i].timeval) - (hrs * 3600)) / 60;
	       if (days > 0)
		  sprintf(&s[strlen(s)], " (idle %lud%luh)", days, hrs);
	       else if (hrs > 0)
		  sprintf(&s[strlen(s)], " (idle %luh%lum)", hrs, mins);
	       else
		  sprintf(&s[strlen(s)], " (idle %lum)", mins);
	    }
	    tprintf(z, "priv %s %s %s\n", botnetnick, nick, s);
	    if (dcc[i].u.chat->away != NULL)
	       tprintf(z, "priv %s %s       AWAY: %s\n", botnetnick, nick,
		       dcc[i].u.chat->away);
	 }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	 if (!ok) {
	    ok = 1;
	    tprintf(z, "priv %s %s Bots connected:\n", botnetnick, nick);
	 }
	 tprintf(z, "priv %s %s   %s%c%-10s %s\n", botnetnick, nick,
		 dcc[i].u.bot->status & STAT_CALLED ? "<-" : "->",
		 dcc[i].u.bot->status & STAT_SHARE ? '+' : ' ',
		 dcc[i].nick, dcc[i].u.bot->version);
      }
   ok = 0;
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel != chan) {
	    if (!ok) {
	       ok = 1;
	       tprintf(z, "priv %s %s Other people on the bot:\n", 
					botnetnick, nick);
	    }
	    sprintf(s, "  %c%-10s %s", (geticon(i) == '-' ? ' ' : geticon(i)),
		    dcc[i].nick, dcc[i].host);
	    if (tt - dcc[i].timeval > 300) {
	       k = (tt - dcc[i].timeval) / 60;
	       if (k < 60)
		  sprintf(&s[strlen(s)], " (idle %dm)", k);
	       else
		  sprintf(&s[strlen(s)], " (idle %dh%dm)", k / 60, k % 60);
	    }
	    tprintf(z, "priv %s %s %s\n", botnetnick, nick, s);
	    if (dcc[i].u.chat->away != NULL)
	       tprintf(z, "priv %s %s       AWAY: %s\n", botnetnick, nick,
		       dcc[i].u.chat->away);
	 }
}

/* who <from@bot> <tobot> <chan#> */
static void bot_who (int idx, char * par)
{
   char *p;
   char *from = TBUF, *to = TBUF + 512;
   int i;
   nsplit(from, par);
   p = strchr(from, '@');
   if (p == NULL) {
      sprintf(to, "%s@%s", from, dcc[idx].nick);
      strcpy(from, to);
   }
   nsplit(to, par);
   if (strcasecmp(to, botnetnick) == 0)
      to[0] = 0;		/* (for me) */
   if (to[0]) {			/* pass it on */
      i = nextbot(to);
      if (i >= 0)
	 tprintf(dcc[i].sock, "who %s %s %s\n", from, to, par);
   } else {
      remote_tell_who(dcc[idx].sock, from, atoi(par));
   }
}

static void bot_share (int idx, char * par) {
   sharein(idx,par);
}

static void bot_version (int idx, char * par)
{
   if ((par[0] >= '0') && (par[0] <= '9')) {
      char work[600];
      nsplit(work, par);
      dcc[idx].u.bot->numver = atoi(work);
      updatebot(dcc[idx].nick,'-',work);
   } else 
      dcc[idx].u.bot->numver = 0;
   strcpy(dcc[idx].u.bot->version, par);
   sprintf(TBUF,"version %d",dcc[idx].u.bot->numver);
   bot_share(idx,TBUF);
}

/* who? <from@bot> <chan>  ->  whom <to@bot> <attr><nick> <bot> <host> */
static void bot_whoq (int idx, char * par)
{
   /* ignore old-style 'whom' request */
   putlog(LOG_BOTS, "*", "Outdated 'whom' request from %s (ignoring)",
	  dcc[idx].nick);
}

/* info? <from@bot>   -> send priv */
static void bot_infoq (int idx, char * par)
{
#ifndef NO_IRC
   char s[161];
   struct chanset_t *chan;
   chan = chanset;
   s[0] = 0;
   while (chan != NULL) {
      if (!(chan->stat & CHAN_SECRET)) {
	 strcat(s, chan->name);
	 strcat(s, ", ");
      }
      chan = chan->next;
   }
   if (s[0]) {
      s[strlen(s) - 2] = 0;
      tprintf(dcc[idx].sock, "priv %s %s %s <%s> (%s)\n", botnetnick, par, ver,
	      network, s);
   } else
      tprintf(dcc[idx].sock, "priv %s %s %s <%s> (no channels)\n", botnetnick, par,
	      ver, network);
#else
   tprintf(dcc[idx].sock, "priv %s %s %s\n", botnetnick, par, ver);
#endif
   tandout_but(idx, "info? %s\n", par);
}

/* whom <to@bot> <attr><nick> <bot> <etc> */
static void bot_whom (int idx, char * par)
{
   /* scrap it */
   putlog(LOG_BOTS, "*", "Outdated 'whom' request from %s (ignoring)",
	  dcc[idx].nick);
}

static void bot_ping (int idx, char * par)
{
   tprintf(dcc[idx].sock, "pong\n");
}

static void bot_pong (int idx, char * par)
{
   dcc[idx].u.bot->status &= ~STAT_PINGED;
}

/* link <from@bot> <who> <to-whom> */
static void bot_link (int idx, char * par)
{
   char *from = TBUF, *bot = TBUF + 512, *rfrom = TBUF + 41;
   int i;
   nsplit(from, par);
   nsplit(bot, par);
   from[40] = 0;
   if (strcasecmp(bot, botnetnick) == 0) {
      strcpy(rfrom, from);
      splitc(NULL, rfrom, ':');
      putlog(LOG_CMDS, "*", "#%s# link %s", rfrom, par);
      if (botlink(from, -2, par))
	 tprintf(dcc[idx].sock, "priv %s %s Attempting link to %s ...\n",
		 botnetnick, from, par);
      else
	 tprintf(dcc[idx].sock, "priv %s %s Can't link there.\n", botnetnick,
		 from);
   } else {
      i = nextbot(bot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "link %s %s %s\n", from, bot, par);
   }
}

/* unlink <from@bot> <linking-bot> <undesired-bot> <reason> */
static void bot_unlink (int idx, char * par)
{
   char *from = TBUF, *bot = TBUF + 512, *rfrom = TBUF + 41;
   int i;
   char *p;
   char *undes = TBUF + 550;
   nsplit(from, par);
   nsplit(bot, par);
   nsplit(undes, par);
   from[40] = 0;
   if (strcasecmp(bot, botnetnick) == 0) {
      strcpy(rfrom, from);
      splitc(NULL, rfrom, ':');
      putlog(LOG_CMDS, "*", "#%s# unlink %s (%s)", rfrom, undes, par[0] ? par :
	     "No reason");
      if (botunlink(-2, undes, par[0] ? par : NULL)) {
	 p = strchr(from, '@');
	 if (p != NULL) {
	    /* idx will change after unlink -- get new idx */
	    i = nextbot(p + 1);
	    if (i >= 0)
	       tprintf(dcc[i].sock, "priv %s %s Unlinked %s.\n", botnetnick,
		       from, undes);
	 }
      } else {
	 tandout("unlinked %s\n", undes);	/* just to clear trash
						 * from link lists */
	 p = strchr(from, '@');
	 if (p != NULL) {
	    /* ditto above, about idx */
	    i = nextbot(p + 1);
	    if (i >= 0)
	       tprintf(dcc[i].sock, "priv %s %s Can't unlink %s.\n",
		       botnetnick, from, undes);
	 }
      }
   } else {
      i = nextbot(bot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "unlink %s %s %s %s\n", from, bot, undes, par);
   }
}

/* bot next share? */
static void bot_update (int idx, char * par) 
{
   char * bot = TBUF;
   nsplit(bot,par);
   if (in_chain(bot))
     updatebot(bot,par[0],par[0]?par+1:"");
}

/* newbot next share? */
static void bot_nlinked (int idx, char * par)
{
   char *newbot = TBUF, *next = TBUF + 512, *p;
   int reject = 0, bogus = 0, atr, i;
   nsplit(newbot, par);
   nsplit(next, par);
   if (strlen(newbot) > 9)
      newbot[9] = 0;
   if (strlen(next) > 9)
      next[9] = 0;
   if (!next[0]) {
      putlog(LOG_BOTS, "*", "Invalid eggnet protocol from %s (zapfing)",
	     dcc[idx].nick);
      chatout("*** Disconnected %s (invalid bot)\n", dcc[idx].nick);
      tandout_but(idx, "chat %s Disconnected %s (invalid bot)\n", botnetnick,
		  dcc[idx].nick);
      tprintf(dcc[idx].sock, "error invalid eggnet protocol for 'nlinked'\n");
      reject = 1;
   } else if ((in_chain(newbot)) || (strcasecmp(newbot, botnetnick) == 0)) {
      /* loop! */
      putlog(LOG_BOTS, "*", "Detected loop: disconnecting %s (mutual: %s)",
	     dcc[idx].nick, newbot);
      chatout("*** Loop (%s): disconnected %s\n", newbot, dcc[idx].nick);
      tandout_but(idx, "chat %s Loop (%s): disconnected %s\n", botnetnick, newbot,
		  dcc[idx].nick);
      tprintf(dcc[idx].sock, "error Loop (%s)\n", newbot);
      reject = 1;
   } 
   if (!reject) {
      for (p = newbot; *p; p++)
	 if ((*p < 32) || (*p == 127))
	    bogus = 1;
      i = nextbot(next);
      if (i != idx)
	 bogus = 1;
   }
   if (bogus) {
      putlog(LOG_BOTS, "*", "Bogus link notice from %s!  (%s -> %s)", dcc[idx].nick,
	     next, newbot);
      chatout("*** Bogus link notice: disconnecting %s\n", dcc[idx].nick);
      tandout_but(idx, "chat %s Bogus link notice: disconnecting %s\n",
		  botnetnick, dcc[idx].nick);
      tprintf(dcc[idx].sock, "error Bogus link notice (%s -> %s)\n", next, newbot);
      reject = 1;
   }
   atr = get_attr_handle(dcc[idx].nick);
   if (atr & BOT_LEAF) {
      putlog(LOG_BOTS, "*", "Disconnecting leaf %s  (linked to %s)", dcc[idx].nick,
	     newbot);
      chatout("*** Illegal link by leaf %s (to %s): disconnecting\n",
	      dcc[idx].nick, newbot);
      tandout_but(idx, "chat %s Illegal link by leaf %s (to %s): disconnecting\n",
		  botnetnick, dcc[idx].nick, newbot);
      tprintf(dcc[idx].sock, "error You are supposed to be a leaf!\n");
      reject = 1;
   }
   if (reject) {
      tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
      tprintf(dcc[idx].sock, "bye\n");
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   addbot(newbot, dcc[idx].nick, next, par);
   tandout_but(idx, "nlinked %s %s %s\n", newbot, next, par);
   check_tcl_link(newbot, next);
   if (get_attr_handle(newbot) & BOT_REJECT) {
      tprintf(dcc[idx].sock, "reject %s %s\n", botnetnick, newbot);
      putlog(LOG_BOTS, "*", "Rejecting bot %s from %s", newbot, dcc[idx].nick);
   }
}

static void bot_linked (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "Older bot detected (unsupported)");
   chatout("*** Disconnected %s (outdated)\n", dcc[idx].nick);
   tandout_but(idx, "chat %s Disconnected %s (outdated)\n", botnetnick,
	       dcc[idx].nick);
   tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void bot_unlinked (int idx, char * par)
{
   int i;
   char bot[512];
   nsplit(bot, par);
   i = nextbot(bot);
   if ((i >= 0) && (i != idx))	/* bot is NOT downstream along idx, so
				   * BOGUS! */
      fake_alert(idx);
   else if (i >= 0) {		/* valid bot downstream of idx */
      rembot(bot, dcc[idx].nick);
      unvia(idx, bot);
      tandout_but(idx, "unlinked %s\n", bot);
   }				/* otherwise it's not even a valid bot, so just ignore! */
}

static void bot_trace (int idx, char * par)
{
   char *from = TBUF, *dest = TBUF + 512;
   int i;
   /* trace <from@bot> <dest> <chain:chain..> */
   nsplit(from, par);
   nsplit(dest, par);
   if (strcasecmp(dest, botnetnick) == 0) {
      tprintf(dcc[idx].sock, "traced %s %s:%s\n", from, par, botnetnick);
   } else {
      i = nextbot(dest);
      if (i >= 0)
	 tprintf(dcc[i].sock, "trace %s %s %s:%s\n", from, dest, par, botnetnick);
   }
}

static void bot_traced (int idx, char * par)
{
   char *to = TBUF, *ss = TBUF + 512, *p;
   int i, sock;
   /* traced <to@bot> <chain:chain..> */
   nsplit(to, par);
   p = strchr(to, '@');
   if (p == NULL)
      p = to;
   else {
      *p = 0;
      p++;
   }
   if (strcasecmp(p, botnetnick) == 0) {
      time_t t = 0,now = time(NULL);
      char * p = par;
      splitc(ss, to, ':');
      if (ss[0])
	 sock = atoi(ss);
      else
	 sock = (-1);
      if (par[0] == ':') {
	 t = atoi(par+1);
	 p = strchr(par+1,':');
	 if (p)
	   p++;
	 else 
	   p = par+1;
      }
      for (i = 0; i < dcc_total; i++)
	 if ((dcc[i].type == &DCC_CHAT) 
	     && (strcasecmp(dcc[i].nick, to) == 0) &&
	     ((sock == (-1)) || (sock == dcc[i].sock)))
	  if (t) {
	    dprintf(i, "Trace result -> %s (%lu secs)\n", p, now - t);
	  } else
	    dprintf(i, "Trace result -> %s\n", p);
   } else {
      i = nextbot(p);
      if (i >= 0)
	 tprintf(dcc[i].sock, "traced %s@%s %s\n", to, p, par);
   }
}

/* reject <from> <bot> */
static void bot_reject (int idx, char * par)
{
   char *from = TBUF, *who = TBUF + 81, *destbot = TBUF + 41, *p;
   int i;
   nsplit(from, par);
   from[40] = 0;
   p = strchr(from, '@');
   if (p == NULL)
      p = from;
   else
      p++;
   i = nextbot(p);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   if (strchr(par, '@') == NULL) {
      /* rejecting a bot */
      i = nextbot(par);
      if (i < 0) {
	 tprintf(dcc[idx].sock, "priv %s %s Can't reject %s (doesn't exist)\n",
		 botnetnick, from, par);
      } else if (strcasecmp(dcc[i].nick, par) == 0) {
	 /* i'm the connection to the rejected bot */
	 putlog(LOG_BOTS, "*", "%s rejected %s", from, dcc[i].nick);
	 tprintf(dcc[i].sock, "bye\n");
	 tandout_but(i, "unlinked %s\n", dcc[i].nick);
	 tandout_but(i, "chat %s Disconnected %s (rejected by %s)\n", botnetnick,
		     dcc[i].nick, from);
	 chatout("*** Disconnected %s (rejected by %s)\n", dcc[i].nick, from);
	 killsock(dcc[i].sock);
	 lostdcc(i);
      } else {
	 if (i < 0)
	    tandout_but(idx, "reject %s %s\n", from, par);
	 else
	    tprintf(dcc[i].sock, "reject %s %s\n", from, par);
      }
   } else {			/* rejecting user */
      nsplit(destbot, par);
      destbot[40] = 0;
      splitc(who, destbot, '@');
      if (strcasecmp(destbot, botnetnick) == 0) {
	 /* kick someone here! */
	 int ok = 0;
	 if (remote_boots == 1) {
	    p = strchr(from, '@');
	    if (p == NULL)
	      p = from;
	    else
	      p++;
	    if (!(get_attr_handle(p) & BOT_SHARE)) {
	       add_note(from, botnetnick, "Remote boots are not allowed.", -1, 0);
	       return;
	    }
	 } else if (remote_boots > 1) {
	    for (i = 0; (i < dcc_total) && (!ok); i++)
	      if ((strcasecmp(who, dcc[i].nick) == 0) && 
		  (dcc[i].type == &DCC_CHAT)) {
		 int atr = get_attr_handle(dcc[i].nick);
		 if (atr & USER_OWNER) {
		    add_note(from, botnetnick, "Can't boot the bot owner.", -1, 0);
		    return;
		 }
		 do_boot(i, from, par);
		 ok = 1;
		 putlog(LOG_CMDS, "*", "#%s# boot %s (%s)", from, dcc[i].nick, par);
	      }
	 } else {
	    tprintf(dcc[idx].sock, "priv %s %s Remote boots are not allowed.\n",
		    botnetnick, from);
	    ok = ok;
	 }
      } else {
	 i = nextbot(destbot);
	 if (i < 0)
	    tandout_but(idx, "reject %s %s@%s %s\n", from, who, destbot, par);
	 else
	    tprintf(dcc[i].sock, "reject %s %s@%s %s\n", from, who, destbot, par);
      }
   }
}

static void bot_thisbot (int idx, char * par)
{
   if (strcasecmp(par, dcc[idx].nick) != 0) {
      putlog(LOG_BOTS, "*", "Wrong bot: wanted %s, got %s", dcc[idx].nick, par);
      tprintf(dcc[idx].sock, "bye\n");
      tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
      tandout_but(idx, "chat %s Disconnected %s (imposter)\n", botnetnick,
		  dcc[idx].nick);
      chatout("*** Disconnected %s (imposter)\n", dcc[idx].nick);
      unvia(idx, dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   if (get_attr_handle(par) & BOT_LEAF)
      dcc[idx].u.bot->status |= STAT_LEAF;
   /* set capitalization the way they want it */
   noshare = 1;
   change_handle(dcc[idx].nick, par);
   noshare = 0;
   strcpy(dcc[idx].nick, par);
}

static void bot_handshake (int idx, char * par)
{
   /* only set a new password if no old one exists */
   if (pass_match_by_handle("-", dcc[idx].nick)) {
      change_pass_by_handle(dcc[idx].nick, par);
      if ((dcc[idx].u.bot->status & STAT_SHARE) &&
	  (!(dcc[idx].u.bot->status & STAT_GETTING)) &&
	  (!(dcc[idx].u.bot->status & STAT_SENDING))) {
	 noshare = 1;
	 change_pass_by_handle(botnetnick, par);
	 noshare = 0;
	 /* i have no idea what that is supposed to accomplish ^ robey
	  * 18dec96 */
      }
   }
}

static void bot_trying (int idx, char * par)
{
   tandout_but(idx, "trying %s\n", par);
   /* currently ignore */
}

static void bot_end_trying (int idx, char * par)
{
   tandout_but(idx, "*trying %s\n", par);
   /* currently ignore */
}

/* used to send a direct msg from Tcl on one bot to Tcl on another 
 * zapf <frombot> <tobot> <code [param]>   */
static void bot_zapf (int idx, char * par)
{
   char *from = TBUF, *to = TBUF + 512;
   int i;
   nsplit(from, par);
   nsplit(to, par);
   i = nextbot(from);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   if (strcasecmp(to, botnetnick) == 0) {
      /* for me! */
      char opcode[512];
      nsplit(opcode, par);
      check_tcl_bot(from, opcode, par);
      return;
   }
   i = nextbot(to);
   if (i >= 0)
      tprintf(dcc[i].sock, "zapf %s %s %s\n", from, to, par);
}

/* used to send a global msg from Tcl on one bot to every other bot 
 * zapf-broad <frombot> <code [param]> */
static void bot_zapfbroad (int idx, char * par)
{
   char *from = TBUF, *opcode = TBUF + 512;
   int i;
   nsplit(from, par);
   nsplit(opcode, par);
   i = nextbot(from);
   if (i != idx) {
      fake_alert(idx);
      return;
   }
   check_tcl_bot(from, opcode, par);
   tandout_but(idx, "zapf-broad %s %s %s\n", from, opcode, par);
}

/* show motd to someone */
static void bot_motd (int idx, char * par)
{
   FILE *vv;
   char *s = TBUF, *who = TBUF + 512, *p;
   int i;
   struct flag_record fr = { USER_BOT, 0, 0 };
   nsplit(who, par);
   if ((!par[0]) || (strcasecmp(par, botnetnick) == 0)) {
      p = strchr(who, ':');
      if (p == NULL)
	 p = who;
      else
	 p++;
      putlog(LOG_CMDS, "*", "#%s# motd", p);
      vv = fopen(motdfile, "r");
      if (vv != NULL) {
	 tprintf(dcc[idx].sock, "priv %s %s --- MOTD file:\n", botnetnick, who);
	 help_subst(NULL, NULL, 0, 1);
	 while (!feof(vv)) {
	    fgets(s, 120, vv);
	    if (!feof(vv)) {
	       if (s[strlen(s) - 1] == '\n')
		  s[strlen(s) - 1] = 0;
	       if (!s[0])
		  strcpy(s, " ");
	       help_subst(s, who, &fr, 1);
	       if (s[0])
		  tprintf(dcc[idx].sock, "priv %s %s %s\n", botnetnick, who, s);
	    }
	 }
	 fclose(vv);
      } else
	 tprintf(dcc[idx].sock, "priv %s %s No MOTD file. :(\n", botnetnick,
		 who);
   } else {
      /* pass it on */
      i = nextbot(par);
      if (i >= 0)
	 tprintf(dcc[i].sock, "motd %s %s\n", who, par);
   }
}

extern void (*do_bot_assoc) (int, char *);
/* assoc [link-flag] <chan#> <name> */
/* link-flag is Y if botlinking */
static void bot_assoc (int idx, char * par)
{
   context;
   do_bot_assoc(idx, par);
}

/* filereject <bot:filepath> <sock:nick@bot> <reason...> */
static void bot_filereject (int idx, char * par)
{
   char *path = TBUF, *tobot = TBUF + 512, *to = TBUF + 542, *p;
   int i;
   nsplit(path, par);
   nsplit(tobot, par);
   splitc(to, tobot, '@');
   if (strcasecmp(tobot, botnetnick) == 0) {	/* for me! */
      p = strchr(to, ':');
      if (p != NULL) {
	 *p = 0;
	 for (i = 0; i < dcc_total; i++) {
	    if (dcc[i].sock == atoi(to))
	       dprintf(i, "FILE TRANSFER REJECTED (%s): %s\n", path, par);
	 }
	 *p = ':';
      }
      /* no ':'? malformed */
      putlog(LOG_FILES, "*", "%s rejected: %s", path, par);
   } else {			/* pass it on */
      i = nextbot(tobot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "filereject %s %s@%s %s\n", path, to, tobot, par);
   }
}

/* filreq <sock:nick@bot> <bot:file> */
static void bot_filereq (int idx, char * par)
{
   char *from = TBUF, *tobot = TBUF + 41;
   int i;
   nsplit(from, par);
   splitc(tobot, par, ':');
   if (strcasecmp(tobot, botnetnick) == 0) {	/* for me! */
      /* process this */
      module_entry *fs = module_find("filesys", 1, 1);
      if (fs == NULL)
	 tprintf(dcc[idx].sock, "priv %s %s I have no file system to grab files from.\n",
		 botnetnick, from);
      else {
	 Function f = fs->funcs[FILESYS_REMOTE_REQ];
	 f(idx, from, par);
      }
   } else {			/* pass it on */
      i = nextbot(tobot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "filereq %s %s:%s\n", from, tobot, par);
   }
}

/* filesend <bot:path> <sock:nick@bot> <IP#> <port> <size> */
static void bot_filesend (int idx, char * par)
{
   char *botpath = TBUF, *nick = TBUF + 512, *tobot = TBUF + 552, *sock = TBUF + 692;
   int i;
   char *nfn;
   nsplit(botpath, par);
   nsplit(tobot, par);
   splitc(nick, tobot, '@');
   splitc(sock, nick, ':');
   if (strcasecmp(tobot, botnetnick) == 0) {	/* for me! */
      nfn = strrchr(botpath, '/');
      if (nfn == NULL) {
	 nfn = strrchr(botpath, ':');
	 if (nfn == NULL)
	    nfn = botpath;	/* that's odd. */
	 else
	    nfn++;
      } else
	 nfn++;
      /* send it to 'nick' as if it's from me */
      mprintf(serv, "PRIVMSG %s :\001DCC SEND %s %s\001\n", nick, nfn, par);
   } else {
      i = nextbot(tobot);
      if (i >= 0)
	 tprintf(dcc[i].sock, "filesend %s %s:%s@%s %s\n", botpath, sock, nick,
		 tobot, par);
   }
}

static void bot_error (int idx, char * par)
{
   putlog(LOG_MISC | LOG_BOTS, "*", "%s: %s", dcc[idx].nick, par);
}

/* join <bot> <nick> <chan> <flag><sock> <from> */
static void bot_join (int idx, char * par)
{
   char *bot = TBUF, *nick = TBUF + 20, *x = TBUF + 40, *y = TBUF + 60,
   *from = TBUF + 70;
   struct userrec * u;
   
   int i, sock;
   nsplit(bot, par);
   bot[9] = 0;
   nsplit(nick, par);
   nick[9] = 0;
   nsplit(x, par);
   x[10] = 0;
   nsplit(y, par);		/* only first char matters */
   if (!y[0]) {
      y[0] = '-';
      sock = 0;
   } else
      sock = atoi(&y[1]);
   /* 1.1 bots always send a sock#, even on a channel change 
    * so if sock# is 0, this is from an old bot and we must tread softly 
    * grab old sock# if there is one, otherwise make up one */
   if (sock == 0)
      sock = partysock(bot, nick);
   if (sock == 0)
      sock = fakesock++;
   strncpy(from, par, 40);
   from[40] = 0;
   i = nextbot(bot);
   if (i != idx)
      return;			/* garbage sent by 1.0g bot */
   u=get_user_by_handle(userlist,nick);
   if (u!=NULL) {
      char xbot[128];
      sprintf(xbot,"@%s",bot);
      touch_laston(u,xbot,now);
   }
   addparty(bot, nick, atoi(x), y[0], sock, from);
   tandout_but(idx, "join %s %s %d %c%d %s\n", bot, nick, atoi(x), y[0], sock, from);
   check_tcl_chjn(bot, nick, atoi(x), y[0], sock, from);
}

/* part <bot> <nick> <sock> [etc..] */
static void bot_part (int idx, char * par)
{
   char *bot = TBUF, *nick = TBUF + 20, *etc = TBUF + 40;
   struct userrec * u;
   int sock;
   nsplit(bot, par);
   bot[9] = 0;
   nsplit(nick, par);
   nick[9] = 0;
   nsplit(etc, par);
   sock = atoi(etc);
   if (sock == 0)
      sock = partysock(bot, nick);
   u=get_user_by_handle(userlist,nick);
   if (u!=NULL) {
      char xbot[128];
      sprintf(xbot,"@%s",bot);
      touch_laston(u,xbot,now);
   }
   check_tcl_chpt(bot, nick, sock);
   remparty(bot, sock);
   tandout_but(idx, "part %s %s %d %s\n", bot, nick, sock, par);
}

/* away <bot> <sock> <message> */
static void bot_away (int idx, char * par)
{
   char *bot = TBUF, *etc = TBUF + 20;
   int sock;
   nsplit(bot, par);
   bot[9] = 0;
   nsplit(etc, par);
   sock = atoi(etc);
   if (sock == 0)
      sock = partysock(bot, etc);
   partystat(bot, sock, PLSTAT_AWAY, 0);
   partyaway(bot, sock, par);
   tandout_but(idx, "away %s %d %s\n", bot, sock, par);
}

/* unaway <bot> <sock> */
static void bot_unaway (int idx, char * par)
{
   char *bot = TBUF, *etc = TBUF + 20;
   int sock;
   nsplit(bot, par);
   bot[9] = 0;
   nsplit(etc, par);
   sock = atoi(etc);
   if (sock == 0)
      sock = partysock(bot, etc);
   partystat(bot, sock, 0, PLSTAT_AWAY);
   tandout_but(idx, "unaway %s %d %s\n", bot, sock, par);
}

/* (a courtesy info to help during connect bursts) */
/* idle <bot> <sock> <#secs> */
static void bot_idle (int idx, char * par)
{
   char *bot = TBUF, *etc = TBUF + 20;
   int sock;
   nsplit(bot, par);
   bot[9] = 0;
   nsplit(etc, par);
   sock = atoi(etc);
   if (sock == 0)
      sock = partysock(bot, etc);
   partysetidle(bot, sock, atoi(par));
   tandout_but(idx, "idle %s %d %s\n", bot, sock, par);
}

static void bot_ufno (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "User file rejected by %s: %s", dcc[idx].nick, par);
   dcc[idx].u.bot->status &= ~STAT_OFFERED;
   if (!(dcc[idx].u.bot->status & STAT_GETTING))
      dcc[idx].u.bot->status &= ~STAT_SHARE;
}

static void bot_old_userfile (int idx, char * par) 
{
   putlog(LOG_BOTS, "*", "Old style share request by %s",dcc[idx].nick);
   tprintf(dcc[idx].sock, "uf-no Antiquated sharing request\n");
}

/* BOT COMMANDS */
/* function call should be:
 * int bot_whatever(idx,"parameters");
 *
 * SORT these, dcc_bot uses a shortcut which requires them sorted 
*/
botcmd_t C_bot[]={
  { "*trying", (Function) bot_end_trying },
  { "actchan", (Function) bot_actchan },
  { "assoc", (Function) bot_assoc },
  { "away", (Function) bot_away },
  { "bye", (Function) bot_bye },
  { "chan", (Function) bot_chan },
  { "chat", (Function) bot_chat },
  { "error", (Function) bot_error },
  { "filereject", (Function) bot_filereject },
  { "filereq", (Function) bot_filereq },
  { "filesend", (Function) bot_filesend },
  { "handshake", (Function) bot_handshake },
  { "idle", (Function) bot_idle },
  { "info?", (Function) bot_infoq },
  { "join", (Function) bot_join },
  { "link", (Function) bot_link },
  { "linked", (Function) bot_linked },
  { "motd", (Function) bot_motd },
  { "nlinked", (Function) bot_nlinked },
  { "part", (Function) bot_part },
  { "ping", (Function) bot_ping },
  { "pong", (Function) bot_pong },
  { "priv", (Function) bot_priv },
  { "reject", (Function) bot_reject },
  { "share", (Function) bot_share },
  { "thisbot", (Function) bot_thisbot },
  { "trace", (Function) bot_trace },
  { "traced", (Function) bot_traced },
  { "trying", (Function) bot_trying },
  { "uf-no", (Function) bot_ufno },
  { "unaway", (Function) bot_unaway },
  { "unlink", (Function) bot_unlink },
  { "unlinked", (Function) bot_unlinked },
  { "update", (Function) bot_update },
  { "userfile?", (Function) bot_old_userfile },
  { "version", (Function) bot_version },
  { "who", (Function) bot_who },
  { "who?", (Function) bot_whoq },
  { "whom", (Function) bot_whom },
  { "zapf", (Function) bot_zapf },
  { "zapf-broad", (Function) bot_zapfbroad },
  { 0, 0 }
};
