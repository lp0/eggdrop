/*
   mode.c -- handles:
   queueing and flushing mode changes made by the bot
   channel mode changes and the bot's reaction to them
   setting and getting the current wanted channel modes

   dprintf'ized, 12dec95
   multi-channel, 6feb96
   stopped the bot deopping masters and bots in bitch mode, pteron 23Mar97


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
#include "eggdrop.h"
#include "users.h"
#include "chan.h"
#include "proto.h"

extern int serv;
extern char botname[];
extern char botuserhost[];
extern char botuser[];
extern char botserver[];
extern struct chanset_t *chanset;

/* reversing this mode? */
int reversing = 0;
/* number of modes per line to send */
int modesperline = 3;

#define PLUS    1
#define MINUS   2
#define CHOP    4
#define BAN     8
#define VOICE   16


void flush_mode PROTO2(struct chanset_t *, chan, int, pri)
{
#ifndef NO_IRC
   char *p, out[512], post[512];
   int i, ok = 0;
   p = out;
   post[0] = 0;
   if (chan->pls[0])
      *p++ = '+';
   for (i = 0; i < strlen(chan->pls); i++)
      *p++ = chan->pls[i];
   if (chan->mns[0])
      *p++ = '-';
   for (i = 0; i < strlen(chan->mns); i++)
      *p++ = chan->mns[i];
   chan->pls[0] = 0;
   chan->mns[0] = 0;
   ok = 0;
   /* +k or +l ? */
   if (chan->key[0]) {
      if (!ok) {
	 *p++ = '+';
	 ok = 1;
      }
      *p++ = 'k';
      strcat(post, chan->key);
      strcat(post, " ");
   }
   if (chan->limit != (-1)) {
      if (!ok) {
	 *p++ = '+';
	 ok = 1;
      }
      *p++ = 'l';
      sprintf(&post[strlen(post)], "%d ", chan->limit);
   }
   chan->limit = (-1);
   chan->key[0] = 0;
   for (i = 0; i < modesperline; i++)
      if (chan->cmode[i].type & PLUS) {
	 if (!ok) {
	    *p++ = '+';
	    ok = 1;
	 }
	 *p++ = (chan->cmode[i].type & BAN ? 'b' : (chan->cmode[i].type & CHOP ? 'o' : 'v'));
	 strcat(post, chan->cmode[i].op);
	 strcat(post, " ");
	 nfree(chan->cmode[i].op);
	 chan->cmode[i].op = NULL;
      }
   ok = 0;
   /* -k ? */
   if (chan->rmkey[0]) {
      if (!ok) {
	 *p++ = '-';
	 ok = 1;
      }
      *p++ = 'k';
      strcat(post, chan->rmkey);
      strcat(post, " ");
   }
   chan->rmkey[0] = 0;
   for (i = 0; i < modesperline; i++)
      if (chan->cmode[i].type & MINUS) {
	 if (!ok) {
	    *p++ = '-';
	    ok = 1;
	 }
	 *p++ = (chan->cmode[i].type & BAN ? 'b' : (chan->cmode[i].type & CHOP ? 'o' : 'v'));
	 strcat(post, chan->cmode[i].op);
	 strcat(post, " ");
	 nfree(chan->cmode[i].op);
	 chan->cmode[i].op = NULL;
      }
   *p = 0;
   for (i = 0; i < modesperline; i++)
      chan->cmode[i].type = 0;
   if (post[strlen(post) - 1] == ' ')
      post[strlen(post) - 1] = 0;
   if (post[0]) {
      strcat(out, " ");
      strcat(out, post);
   }
   if (out[0]) {
      if (pri == QUICK)
	 tprintf(serv, "MODE %s %s\n", chan->name, out);
      else
	 mprintf(serv, "MODE %s %s\n", chan->name, out);
   }
#endif
}

/* for EVERY channel */
void flush_modes()
{
#ifndef NO_IRC
   struct chanset_t *chan;
   chan = chanset;
   while (chan != NULL) {
      flush_mode(chan, NORMAL);
      chan = chan->next;
   }
#endif
}

/* queue a channel mode change */
void add_mode PROTO4(struct chanset_t *, chan, char, plus, char, mode, char *, op)
{
#ifndef NO_IRC
   int i, type, ok;
   char s[21];
   if ((mode == 'o') || (mode == 'b') || (mode == 'v')) {
      type = (plus == '+' ? PLUS : MINUS) | (mode == 'o' ? CHOP : (mode == 'b' ? BAN : VOICE));
      /* op-type mode change */
      for (i = 0; i < modesperline; i++)
	 if ((chan->cmode[i].type == type) && (chan->cmode[i].op != NULL) &&
	     (strcasecmp(chan->cmode[i].op, op) == 0))
	    return;		/* already in there :- duplicate */
      ok = 0;			/* add mode to buffer */
      for (i = 0; i < modesperline; i++)
	 if ((chan->cmode[i].type == 0) && (!ok)) {
	    chan->cmode[i].type = type;
	    chan->cmode[i].op = (char *) nmalloc(strlen(op) + 1);
	    strcpy(chan->cmode[i].op, op);
	    ok = 1;
	 }
      ok = 0;			/* check for full buffer */
      for (i = 0; i < modesperline; i++)
	 if (chan->cmode[i].type == 0)
	    ok = 1;
      if (!ok)
	 flush_mode(chan, NORMAL);	/* full buffer!  flush modes */
      return;
   }
   /* +k ? store key */
   if ((plus == '+') && (mode == 'k')) {
      strcpy(chan->key, op);
      return;
   }
   /* -k ? store removed key */
   if ((plus == '-') && (mode == 'k')) {
      strcpy(chan->rmkey, op);
      return;
   }
   /* +l ? store limit */
   if ((plus == '+') && (mode == 'l')) {
      chan->limit = atoi(op);
      return;
   }
   /* typical mode changes */
   if (plus == '+')
      strcpy(s, chan->pls);
   else
      strcpy(s, chan->mns);
   if (strchr(s, mode) != NULL)
      return;			/* duplicate */
   if (plus == '+') {
      chan->pls[strlen(chan->pls) + 1] = 0;
      chan->pls[strlen(chan->pls)] = mode;
   } else {
      chan->mns[strlen(chan->mns) + 1] = 0;
      chan->mns[strlen(chan->mns)] = mode;
   }
#endif
}

#ifndef NO_IRC

/**********************************************************************/
/* horrible code to parse mode changes */
/* no, it's not horrible, it just looks that way */

void got_key PROTO5(struct chanset_t *, chan, char *, nick, char *, from,
		    char *, key, int, atr)
{
   int bogus = 0, i;
   set_key(chan, key);
   for (i = 0; i < strlen(key); i++)
      if (((key[i] < 32) || (key[i] == 127)) &&
	  (key[i] != 2) && (key[i] != 31) && (key[i] != 22))
	 bogus = 1;
   if ((bogus) && (strcasecmp(nick, botname) != 0)) {
      putlog(LOG_MODES, chan->name, "Bogus channel key on %s!", chan->name);
      mprintf(serv, "KICK %s %s :bogus channel key\n", chan->name, nick);
   }
   if ((reversing) || (bogus) || ((chan->mode_mns_prot & CHANKEY) &&
				  (!(atr & (USER_MASTER | USER_BOT)))))
      add_mode(chan, '-', 'k', key);
}

void got_op PROTO6(struct chanset_t *, chan, char *, nick, char *, from,
		   char *, who, int, atr1, int, chatr1)
{
   memberlist *m;
   char s[UHOSTLEN];
   int atr, chatr;
   m = ismember(chan, who);
   if (m == NULL) {
      putlog(LOG_MISC, chan->name, "* Mode change on %s for nonexistent %s!",
	     chan->name, who);
      mprintf(serv, "WHO %s\n", who);
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chan->name);
   if ((!me_op(chan)) && (strcasecmp(who, botname) == 0))
      newly_chanop(chan);
   else if ((me_op(chan)) && (strcasecmp(who, botname) != 0) &&
	    (!(m->flags & SENTDEOP)) && (nick[0])) {
      if ((chan->stat & CHAN_BITCH) && (!(atr1 & (USER_MASTER | USER_BOT))) &&
	  (!(chatr1 & CHANUSER_MASTER)) && (!(atr &
		   (USER_GLOBAL | USER_MASTER | USER_BOT))) && (!(chatr &
				     (CHANUSER_OP | CHANUSER_MASTER)))) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= SENTDEOP;
      } else if (((chatr & CHANUSER_DEOP) || (atr & USER_DEOP))
		 && (!(chatr & CHANUSER_OP)) &&
		 (!(atr1 & (USER_MASTER | USER_BOT))) && (!(chatr1 &
						     CHANUSER_MASTER))) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= SENTDEOP;
      } else if (reversing) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= SENTDEOP;
      }
   } else if ((reversing) && (!(m->flags & SENTDEOP)) &&
	      (strcasecmp(who, botname) != 0)) {
      add_mode(chan, '-', 'o', who);
      m->flags |= SENTDEOP;
   }
   if (nick[0] == 0) {		/* server op! */
      int ok = 0;
      if ((atr & (USER_MASTER)) || (chatr & (CHANUSER_MASTER)))
	 ok = 1;
      else if (!(chatr & CHANUSER_DEOP)) {
	 if (!(atr & USER_DEOP) && ((atr & (USER_FRIEND | USER_GLOBAL))
			   || (chatr & (CHANUSER_FRIEND | CHANUSER_OP))))
	    ok = 1;
	 if (chatr & CHANUSER_OP)
	    ok = 1;
      }
      if (!ok && (!(m->flags & CHANOP)) && (!(m->flags & SENTDEOP)) &&
	  (me_op(chan)) && (chan->stat & CHAN_STOPNETHACK)) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= (FAKEOP | SENTDEOP);
      }
   } else
      m->flags &= ~FAKEOP;
   m->flags |= CHANOP;
   m->flags &= ~SENTOP;
}

void got_deop PROTO6(struct chanset_t *, chan, char *, nick, char *, from,
		     char *, who, int, atr1, int, chatr1)
{
   memberlist *m;
   char s[UHOSTLEN], s1[UHOSTLEN], s2[UHOSTLEN];
   int atr, chatr;
   m = ismember(chan, who);
   if (m == NULL) {
      putlog(LOG_MISC, chan->name, "* Mode change on %s for nonexistent %s!",
	     chan->name, who);
      mprintf(serv, "WHO %s\n", who);
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   fixfrom(s);
   sprintf(s1, "%s!%s", nick, from);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chan->name);
   /* deop'd someone on my oplist? */
   if (me_op(chan)) {
      int ok = 1;
      if ((atr & USER_MASTER) || (chatr & CHANUSER_MASTER))
	 ok = 0;
      else if ((atr & (USER_GLOBAL | USER_FRIEND)) && !(chatr & CHANUSER_DEOP))
	 ok = 0;
      else if (chatr & (CHANUSER_OP | CHANUSER_FRIEND))
	 ok = 0;
      if (!ok && (strcasecmp(nick, botname) != 0) &&
	  (strcasecmp(who, nick) != 0) && (m->flags & CHANOP) &&
	  (strcasecmp(who, botname) != 0)) {	/* added 25mar96, robey */
	 /* reop? */
	 if (!(atr1 & (USER_MASTER | USER_BOT)) && (chan->stat & CHAN_PROTECTOPS) &&
	     (!(chan->stat & CHAN_BITCH) || (atr & (USER_MASTER | USER_GLOBAL))
	      || (chatr & (CHANUSER_OP | CHANUSER_MASTER))) && !(chatr & CHANUSER_DEOP) &&
	 /* ^ must be +o or +m to get re-op'd when +bitch is on */
	     !(m->flags & SENTOP)) {
	    add_mode(chan, '+', 'o', who);
	    m->flags |= SENTOP;
	 } else if (reversing) {
	    add_mode(chan, '+', 'o', who);
	    m->flags |= SENTOP;
	 }
	 if (!(atr1 & (USER_MASTER | USER_BOT | USER_FRIEND))
	     && !(chatr1 & (CHANUSER_FRIEND | CHANUSER_MASTER)) &&
	     (chan->stat & CHAN_REVENGE)) {
	    if (nick[0]) {
	       sprintf(s2, "deopped %s", s);
	       take_revenge(chan, s1, s2);	/* punish bad guy */
	    }
	 }
      }
   }
   if (!nick[0])
      putlog(LOG_MODES, chan->name, "TS resync (%s): %s deopped by %s",
	     chan->name, who, from);
   /* check for mass deop */
   if (!(atr1 & (USER_MASTER | USER_BOT)) && !(chatr1 & CHANUSER_FRIEND) &&
       (nick[0]) && (strcasecmp(nick, botname) != 0)) {
      if ((strcasecmp(nick, chan->deopnick) == 0) &&
	  (strcasecmp(who, chan->deopd) != 0)) {
	 time_t tx = time(NULL);
	 strcpy(chan->deopd, who);
	 if (tx - chan->deoptime <= 10) {
	    chan->deops++;
	    if (chan->deops >= 3) {
	       putlog(LOG_MODES, chan->name, "Mass deop on %s by %s", chan->name, s1);
	       mprintf(serv, "KICK %s %s :mass deop, go sit in a corner\n",
		       chan->name, nick);
	       chan->deopnick[0] = 0;
	       chan->deoptime = 0L;
	       chan->deops = 0;
	       chan->deopd[0] = 0;
	    }
	 } else {
	    chan->deoptime = time(NULL);
	    chan->deops = 1;
	    strcpy(chan->deopd, who);
	 }
      } else {
	 strcpy(chan->deopnick, nick);
	 chan->deoptime = time(NULL);
	 chan->deops = 1;
	 strcpy(chan->deopd, who);
      }
   }
   /* having op hides your +v status -- so now that someone's lost ops,
      check to see if they have +v */
   if (!(m->flags & CHANVOICE))
      mprintf(serv, "WHO %s\n", m->nick);
   if ((strcasecmp(who, botname) == 0) && (chan->stat & CHAN_REVENGE)) {
      /* deopped ME!  take revenge */
      if (!(atr1 & (USER_MASTER | USER_FRIEND))
       && !(chatr1 & (CHANUSER_MASTER | CHANUSER_FRIEND)) && (nick[0]) &&
	  (strcasecmp(nick, botname) != 0))
	 take_revenge(chan, s1, "deopped me");
      if (!nick[0])
	 putlog(LOG_MODES, chan->name, "TS resync deopped me on %s :(", chan->name);
      if (chan->need_op[0])
	 do_tcl("need-op", chan->need_op);
   }
   m->flags &= ~(FAKEOP | CHANOP | SENTDEOP);
}

void got_ban PROTO6(struct chanset_t *, chan, char *, nick, char *, from,
		    char *, who, int, atr, int, chatr)
{
   char me[UHOSTLEN], s[UHOSTLEN], s1[UHOSTLEN];
   int check, i, bogus;
   memberlist *m;
   sprintf(me, "%s!%s", botname, botuserhost);
   sprintf(s, "%s!%s", nick, from);
   newban(chan, who, s);
   bogus = 0;
   check = 1;
   if (strcasecmp(nick, botname) != 0) {	/* it's not my ban */
      if ((chan->stat & CHAN_NOUSERBANS) && (!(atr & USER_BOT)) &&
	  (!(atr & USER_MASTER)) && (!(chatr & CHANUSER_MASTER))) {
	 /* no bans made by users */
	 add_mode(chan, '-', 'b', who);
	 return;
      }
      for (i = 0; i < strlen(who); i++)
	 if (((who[i] < 32) || (who[i] == 127)) &&
	     (who[i] != 2) && (who[i] != 22) && (who[i] != 31))
	    bogus = 1;
      if (bogus) {
	 if ((atr & (USER_MASTER | USER_BOT | USER_FRIEND))
	     || (chatr & (CHANUSER_MASTER | CHANUSER_FRIEND))) {
	    /* fix their bogus ban */
	    int ok = 0;
	    strcpy(s1, who);
	    for (i = 0; i < strlen(s1); i++) {
	       if (((s1[i] < 32) || (s1[i] == 127)) &&
		   (s1[i] != 2) && (s1[i] != 22) && (s1[i] != 31))
		  s1[i] = '?';
	       if ((s1[i] != '?') && (s1[i] != '*') && (s1[i] != '!') && (s1[i] != '@'))
		  ok = 1;
	    }
	    add_mode(chan, '-', 'b', who);
	    flush_mode(chan, NORMAL);
	    /* only re-add it if it has something besides wildcards */
	    if (ok)
	       add_mode(chan, '+', 'b', s1);
	 } else {
	    add_mode(chan, '-', 'b', who);
	    mprintf(serv, "KICK %s %s :bogus ban\n", chan->name, nick);
	 }
	 return;
      }
      /* don't enforce a server ban right away -- give channel users a chance */
      /* to remove it, in case it's fake */
      if (!nick[0])
	 check = 0;
      /* does this remotely match against any of our hostmasks? */
      /* just an off-chance... */
      if ((get_attr_host(who) & (USER_MASTER | USER_FRIEND | USER_GLOBAL)) ||
	  (get_chanattr_host(who, chan->name) &
	   (CHANUSER_MASTER | CHANUSER_FRIEND | CHANUSER_OP))) {
	 if (!(atr & (USER_MASTER | USER_BOT))) {
	    reversing = 1;
	    check = 0;
	 }
	 if (get_attr_host(who) & USER_MASTER)
	    check = 0;
      } else if ((wild_match(who, me)) && (me_op(chan))) {
	 reversing = 1;
	 check = 0;
      }
      /* ^ don't really feel like being banned today, thank you! */
      else {
	 /* banning an oplisted person who's on the channel? */
	 m = chan->channel.member;
	 while (m->nick[0]) {
	    sprintf(s1, "%s!%s", m->nick, m->userhost);
	    if (wild_match(who, s1)) {
	       int tatr = get_attr_host(s1), tchatr = get_chanattr_host(s1, chan->name);
	       if ((tatr & (USER_MASTER | USER_GLOBAL | USER_FRIEND)) ||
		   (tchatr & (CHANUSER_MASTER | CHANUSER_FRIEND | CHANUSER_OP))) {
		  /* remove ban on +o/f/m user, unless placed by another +m/b */
		  if (!(atr & (USER_MASTER | USER_BOT))
		      && !(chatr & (CHANUSER_MASTER))) {
		     add_mode(chan, '-', 'b', who);
		     check = 0;
		  }
		  if (tatr & USER_MASTER)
		     check = 0;
	       }
	    }
	    m = m->next;
	 }
      }
      if (check)
	 kick_match_ban(chan, who);
   } else
      kick_match_ban(chan, who);
   /* is it a server ban from nowhere? */
#ifdef BOUNCE_SERVER_BANS
   if ((!nick[0]) && (!equals_ban(who)) && (check))
      add_mode(chan, '-', 'b', who);
   else
#endif
   if (reversing)
      add_mode(chan, '-', 'b', who);
}

void got_unban PROTO6(struct chanset_t *, chan, char *, nick, char *, from,
		      char *, who, int, atr, int, chatr)
{
   int i, bogus;
   killban(chan, who);
   bogus = 0;
   for (i = 0; i < strlen(who); i++)
      if (((who[i] < 32) || (who[i] == 127)) &&
	  (who[i] != 2) && (who[i] != 22) && (who[i] != 31))
	 bogus = 1;
   if ((bogus) && (strcasecmp(nick, botname) != 0) && (!isbanned(chan, who)) &&
       !(atr & (USER_MASTER | USER_BOT | USER_FRIEND | USER_GLOBAL)) &&
       !(chatr & (CHANUSER_MASTER | CHANUSER_OP | CHANUSER_FRIEND))) {
      tprintf(serv, "KICK %s %s :bogus ban\n", chan->name, nick);
      return;
   }
   if (u_sticky_ban(chan->bans, who) || sticky_ban(who)) {
      /* that's a sticky ban! No point in being */
      /* sticky unless we enforce it!! */
      add_mode(chan, '+', 'b', who);
   }
   if (((equals_ban(who)) || (u_equals_ban(chan->bans, who))) &&
       (me_op(chan)) && (!(chan->stat & CHAN_DYNAMICBANS))) {
      /* that's a permban! */
      if ((atr & (USER_BOT | BOT_SHARE)) == (USER_BOT | BOT_SHARE)) {
	 /* sharebot -- do nothing */
      } else if ((atr & (USER_MASTER | USER_GLOBAL))
		 || (chatr & (CHANUSER_MASTER | CHANUSER_OP))) {
	 hprintf(serv, "NOTICE %s :%s is in my permban list.  You need to %s\n",
		 nick, who, "use '-ban' in dcc chat if you want it gone for good.");
      } else
	 add_mode(chan, '+', 'b', who);
   }
   /* there's no need to reverse a -b mode unless it was something i wanted */
   /* if (reversing) add_mode(chan,'+','b',who); */
}

#define modechg(x,y) { \
  char ms[3]; \
  ms[0]=(pos==1)?'+':'-'; ms[1]=y; ms[2]=0; \
  check_tcl_mode(nick,from,hand,chan->name,ms); \
  if (pos==1) chan->channel.mode|=(x); else chan->channel.mode&=~(x); \
  if ((((pos==1)&&(chan->mode_mns_prot&(x))) || \
      ((pos==-1)&&(chan->mode_pls_prot&(x)))) && (!(atr&USER_MASTER)) \
      && (!(chatr&CHANUSER_MASTER))) \
    add_mode(chan,(pos==1)?'-':'+',y,""); \
  else if ((reversing) && ((pos==1)||(chan->mode_pls_prot&(x))) && \
      ((pos==-1)||(chan->mode_mns_prot&(x)))) \
    add_mode(chan,(pos==1)?'-':'+',y,""); \
}

#define cur_mode(z) { \
  if ((pos==1)&&!(chan->mode_cur&(z))) chan->mode_cur|=(z); \
  else if ((pos==-1)&&(chan->mode_cur&(z))) chan->mode_cur&=~(z); \
}

/* a pain in the ass: mode changes */
void gotmode PROTO2(char *, from, char *, msg)
{
   char nick[NICKLEN], hand[10], ch[UHOSTLEN], op[UHOSTLEN], chg[81];
   char s[UHOSTLEN], ms[UHOSTLEN];
   int pos = 0, i, atr, chatr;
   memberlist *m;
   struct chanset_t *chan;
   split(ch, msg);
   nsplit(chg, msg);
   reversing = 0;
   /* usermode changes? */
   if ((ch[0] != '#') && (ch[0] != '&')) {
      if (strcasecmp(ch, botname) == 0) {
	 /* umode +r? */
	 if ((chg[0] == '+') && (strchr(chg, 'r') != NULL)) {
	    putlog(LOG_MISC | LOG_JOIN, "*", "%s has me i-lined (jumping)", botserver);
	    tprintf(serv, "QUIT :i-lines suck\n");
	    killsock(serv);
	    serv = (-1);
	    return;
	 }
      }
      return;
   }
   chan = findchan(ch);
   if (chan == NULL) {
      putlog(LOG_MISC, "*", "Oops, someone joined me to %s ... leaving ...", ch);
      mprintf(serv, "PART %s\n", ch);
      return;
   }
   if (chan->stat & CHANPEND)
      return;			/* not yet! */
   putlog(LOG_MODES, chan->name, "%s: mode change '%s %s' by %s", ch, chg, msg, from);
   get_handle_by_host(hand, from);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chan->name);
   splitnick(nick, from);
   i = 0;
   m = ismember(chan, nick);
   if ((m != NULL) && (me_op(chan))) {
      if (m->flags & FAKEOP) {
	 putlog(LOG_MODES, ch, "Mode change by fake op on %s!  Reversing...", ch);
	 mprintf(serv, "KICK %s %s :abusing ill-gained server ops\n", ch, nick);
	 reversing = 1;
      } else if (!(m->flags & CHANOP)) {
	 putlog(LOG_MODES, ch, "Mode change by non-chanop on %s!  Reversing...", ch);
	 mprintf(serv, "KICK %s %s :abusing desync\n", ch, nick);
	 reversing = 1;
      }
   }
   while (chg[i] != 0) {
      if (chg[i] == '+')
	 pos = 1;
      if (chg[i] == '-')
	 pos = (-1);
      if (chg[i] == 'i') {
	 modechg(CHANINV, 'i');
	 cur_mode(CHANINV);
      }
      if (chg[i] == 'p') {
	 modechg(CHANPRIV, 'p');
	 cur_mode(CHANPRIV);
      }
      if (chg[i] == 's') {
	 modechg(CHANSEC, 's');
	 cur_mode(CHANSEC);
      }
      if (chg[i] == 'm') {
	 modechg(CHANMODER, 'm');
	 cur_mode(CHANMODER);
      }
      if (chg[i] == 't') {
	 modechg(CHANTOPIC, 't');
	 cur_mode(CHANTOPIC);
      }
      if (chg[i] == 'n') {
	 modechg(CHANNOMSG, 'n');
	 cur_mode(CHANNOMSG);
      }
      if (chg[i] == 'a') {
	 modechg(CHANANON, 'a');
	 cur_mode(CHANANON);
      }
      if (chg[i] == 'q') {
	 modechg(CHANQUIET, 'q');
	 cur_mode(CHANQUIET);
      }
      if (chg[i] == 'l') {
	 if (pos == -1) {
	    cur_mode(CHANLIMIT);
	    check_tcl_mode(nick, from, hand, chan->name, "-l");
	    if ((reversing) && (chan->channel.maxmembers != (-1))) {
	       sprintf(s, "%d", chan->channel.maxmembers);
	       add_mode(chan, '+', 'l', s);
	    } else if ((chan->limit_prot != (-1)) && (!(atr & USER_MASTER))
		       && (!(chatr & CHANUSER_MASTER))) {
	       sprintf(s, "%d", chan->limit_prot);
	       add_mode(chan, '+', 'l', s);
	    }
	    chan->channel.maxmembers = (-1);
	 } else {
	    nsplit(op, msg);
	    chan->channel.maxmembers = atoi(op);
	    sprintf(ms, "+l %d", chan->channel.maxmembers);
	    check_tcl_mode(nick, from, hand, chan->name, ms);
	    if ((reversing) ||
		((chan->mode_mns_prot & CHANLIMIT) && !(atr & USER_MASTER)
		 && !(chatr & CHANUSER_MASTER))) {
	       if (chan->channel.maxmembers == 0)
		  add_mode(chan, '+', 'l', "23");
	       add_mode(chan, '-', 'l', "");
	    }
	    if ((chan->limit_prot != chan->channel.maxmembers) &&
		!(atr & USER_MASTER)) {
	       sprintf(s, "%d", chan->limit_prot);
	       add_mode(chan, '+', 'l', s);
	    }
	 }
      }
      if (chg[i] == 'k') {
	 cur_mode(CHANKEY);
	 nsplit(op, msg);
	 sprintf(ms, "%ck %s", (pos == 1) ? '+' : '-', op);
	 check_tcl_mode(nick, from, hand, chan->name, ms);
	 if (pos == 1)
	    got_key(chan, nick, from, op, atr);
	 else {
	    if ((reversing) && (chan->channel.key[0]))
	       add_mode(chan, '+', 'k', chan->channel.key);
	    else if ((chan->key_prot[0]) && (!(atr & USER_MASTER))
		     && (!(chatr & CHANUSER_MASTER)))
	       add_mode(chan, '+', 'k', chan->key_prot);
	    set_key(chan, NULL);
	 }
      }
      if (chg[i] == 'o') {
	 nsplit(op, msg);
	 sprintf(ms, "%co %s", (pos == 1) ? '+' : '-', op);
	 check_tcl_mode(nick, from, hand, chan->name, ms);
	 if (pos == 1)
	    got_op(chan, nick, from, op, atr, chatr);
	 else
	    got_deop(chan, nick, from, op, atr, chatr);
      }
      if (chg[i] == 'v') {
	 nsplit(op, msg);
	 m = ismember(chan, op);
	 if (m == NULL) {
	    putlog(LOG_MISC, chan->name, "* Mode change on %s for nonexistent %s!",
		   chan->name, op);
	    tprintf(serv, "WHO %s\n", op);
	 } else {
	    sprintf(ms, "%cv %s", (pos == 1) ? '+' : '-', op);
	    check_tcl_mode(nick, from, hand, chan->name, ms);
	    if (pos == 1) {
	       m->flags |= CHANVOICE;
	       if (reversing)
		  add_mode(chan, '-', 'v', op);
	    } else {
	       m->flags &= ~CHANVOICE;
	       if (reversing)
		  add_mode(chan, '+', 'v', op);
	    }
	 }
      }
      if (chg[i] == 'b') {
	 nsplit(op, msg);
	 sprintf(ms, "%cb %s", (pos == 1) ? '+' : '-', op);
	 check_tcl_mode(nick, from, hand, chan->name, ms);
	 if (pos == 1)
	    got_ban(chan, nick, from, op, atr, chatr);
	 else
	    got_unban(chan, nick, from, op, atr, chatr);
      }
      i++;
   }
}

#endif				/* !NO_IRC */

void recheck_chanmode PROTO1(struct chanset_t *, chan)
{
#ifndef NO_IRC
   int cur, pls, mns;
   char s[121];
   pls = chan->mode_pls_prot;
   mns = chan->mode_mns_prot;
   cur = chan->mode_cur;
   if (pls & CHANINV && !(cur & CHANINV))
      add_mode(chan, '+', 'i', "");
   else if (mns & CHANINV && cur & CHANINV)
      add_mode(chan, '-', 'i', "");
   if (pls & CHANPRIV && !(cur & CHANPRIV))
      add_mode(chan, '+', 'p', "");
   else if (mns & CHANPRIV && cur & CHANPRIV)
      add_mode(chan, '-', 'p', "");
   if (pls & CHANSEC && !(cur & CHANSEC))
      add_mode(chan, '+', 's', "");
   else if (mns & CHANSEC && cur & CHANSEC)
      add_mode(chan, '-', 's', "");
   if (pls & CHANMODER && !(cur & CHANMODER))
      add_mode(chan, '+', 'm', "");
   else if (mns & CHANMODER && cur & CHANMODER)
      add_mode(chan, '-', 'm', "");
   if (pls & CHANTOPIC && !(cur & CHANTOPIC))
      add_mode(chan, '+', 't', "");
   else if (mns & CHANTOPIC && cur & CHANTOPIC)
      add_mode(chan, '-', 't', "");
   if (pls & CHANNOMSG && !(cur & CHANNOMSG))
      add_mode(chan, '+', 'n', "");
   else if (mns & CHANNOMSG && cur & CHANNOMSG)
      add_mode(chan, '-', 'n', "");
   if (pls & CHANANON && !(cur & CHANANON))
      add_mode(chan, '+', 'a', "");
   else if (mns & CHANANON && cur & CHANANON)
      add_mode(chan, '-', 'a', "");
   if (pls & CHANQUIET && !(cur & CHANQUIET))
      add_mode(chan, '+', 'q', "");
   else if (mns & CHANQUIET && cur & CHANQUIET)
      add_mode(chan, '-', 'q', "");
   if ((chan->limit_prot != (-1)) && !(cur & CHANLIMIT)) {
      sprintf(s, "%d", chan->limit_prot);
      add_mode(chan, '+', 'l', s);
   } else if (mns & CHANLIMIT && cur & CHANLIMIT)
      add_mode(chan, '-', 'l', "");
   if (chan->key_prot[0]) {
      if (strcasecmp(chan->channel.key, chan->key_prot) != 0) {
	 if (chan->channel.key[0]) {
	    add_mode(chan, '-', 'k', chan->channel.key);
	 }
	 add_mode(chan, '+', 'k', chan->key_prot);
      }
   } else if (mns & CHANKEY && cur & CHANKEY)
      add_mode(chan, '-', 'k', chan->channel.key);
#endif
}

/* interpret configfile setting for modes to protect */
#define protmode(x) { \
  chan->mode_pls_prot&=(~(x)); chan->mode_mns_prot&=(~(x)); \
  if (pos==1) chan->mode_pls_prot|=(x); else chan->mode_mns_prot|=(x); \
}

void set_mode_protect PROTO2(struct chanset_t *, chan, char *, set)
{
#ifndef NO_IRC
   int i, pos = 1;
   char s[121], s1[121];
   nsplit(s, set);
   /* clear old modes */
   chan->mode_mns_prot = chan->mode_pls_prot = 0;
   chan->limit_prot = (-1);
   chan->key_prot[0] = 0;
   for (i = 0; i < strlen(s); i++) {
      if (s[i] == '+')
	 pos = 1;
      if (s[i] == '-')
	 pos = (-1);
      if (s[i] == 'i')
	 protmode(CHANINV);
      if (s[i] == 'p')
	 protmode(CHANPRIV);
      if (s[i] == 's')
	 protmode(CHANSEC);
      if (s[i] == 'm')
	 protmode(CHANMODER);
      if (s[i] == 't')
	 protmode(CHANTOPIC);
      if (s[i] == 'n')
	 protmode(CHANNOMSG);
      if (s[i] == 'a')
	 protmode(CHANANON);
      if (s[i] == 'q')
	 protmode(CHANQUIET);
      if (s[i] == 'l') {
	 chan->mode_mns_prot &= (~CHANLIMIT);
	 chan->limit_prot = (-1);
	 if (pos == -1)
	    chan->mode_mns_prot |= CHANLIMIT;
	 else {
	    nsplit(s1, set);
	    if (s1[0])
	       chan->limit_prot = atoi(s1);
	 }
      }
      if (s[i] == 'k') {
	 chan->mode_mns_prot &= (~CHANKEY);
	 chan->key_prot[0] = 0;
	 if (pos == -1)
	    chan->mode_mns_prot |= CHANKEY;
	 else {
	    nsplit(s1, set);
	    if (s1[0])
	       strcpy(chan->key_prot, s1);
	 }
      }
   }
   if (chan->stat & CHANACTIVE)
      recheck_chanmode(chan);
#endif
}

void get_mode_protect PROTO2(struct chanset_t *, chan, char *, s)
{
#ifndef NO_IRC
   char *p = s, s1[121];
   int ok = 0, i, tst;
   s1[0] = 0;
   for (i = 0; i < 2; i++) {
      ok = 0;
      if (i == 0) {
	 tst = chan->mode_pls_prot;
	 if ((tst) || (chan->limit_prot != (-1)) || (chan->key_prot[0]))
	    *p++ = '+';
	 if (chan->limit_prot != (-1)) {
	    *p++ = 'l';
	    sprintf(&s1[strlen(s1)], "%d ", chan->limit_prot);
	 }
	 if (chan->key_prot[0]) {
	    *p++ = 'k';
	    sprintf(&s1[strlen(s1)], "%s ", chan->key_prot);
	 }
      } else {
	 tst = chan->mode_mns_prot;
	 if (tst)
	    *p++ = '-';
      }
      if (tst & CHANINV)
	 *p++ = 'i';
      if (tst & CHANPRIV)
	 *p++ = 'p';
      if (tst & CHANSEC)
	 *p++ = 's';
      if (tst & CHANMODER)
	 *p++ = 'm';
      if (tst & CHANTOPIC)
	 *p++ = 't';
      if (tst & CHANNOMSG)
	 *p++ = 'n';
      if (tst & CHANLIMIT)
	 *p++ = 'l';
      if (tst & CHANKEY)
	 *p++ = 'k';
      if (tst & CHANANON)
	 *p++ = 'a';
      if (tst & CHANQUIET)
	 *p++ = 'q';
   }
   *p = 0;
   if (s1[0]) {
      s1[strlen(s1) - 1] = 0;
      strcat(s, " ");
      strcat(s, s1);
   }
#endif				/* !NO_IRC */
}
