/* 
   cmds.c -- handles:
   commands from a user via dcc

   dprintf'ized, 3nov95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "main.h"
#include "chan.h"
#include "users.h"
#include "modules.h"
#include <ctype.h>

extern int serv;
extern char botuserhost[];
extern char newserver[];
extern char newserverpass[];
extern int newserverport;
extern int require_p;
extern int use_info;
extern char origbotname[];
extern char botnetnick[];
extern int dcc_total;
extern struct dcc_t * dcc;
extern char botname[];
extern char botuser[];
extern struct userrec *userlist;
extern char owner[];
extern struct chanset_t *chanset;
extern int make_userfile;
extern int conmask;
extern Tcl_Interp *interp;
extern tcl_timer_t *timer, *utimer;
extern char chanfile[];
extern int gban_total;
extern int default_port;
extern int backgrd;
extern int default_flags;

#ifndef NO_IRC

/* request from a user to kick (over dcc) */
static void user_kick (int idx, char * nick)
{
   memberlist *m;
   char s[512], note[512];
   int atr;
   struct chanset_t *chan;
   if (strchr(nick, ' ') != NULL) {
      split(s, nick);
      strncpy(note, nick, 65);
      note[65] = 0;
      strcpy(nick, s);
   } else
      strcpy(note, "request");
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
      dprintf(idx, "But I don't WANT to kick myself!\n");
      return;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s\n", nick, chan->name);
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   if (get_chanattr_host(s, chan->name) & CHANUSER_MASTER) {
      dprintf(idx, "%s is a %s master.\n", nick, chan->name);
      return;
   }
   atr = get_attr_host(s);
   if (atr & USER_MASTER) {
      dprintf(idx, "%s is a bot master.\n", nick);
      return;
   }
   if (atr & USER_BOT) {
      dprintf(idx, "%s is another channel bot!\n", nick);
      return;
   }
   mprintf(serv, "KICK %s %s :%s\n", chan->name, m->nick, note);
   dprintf(idx, "Okay, done.\n");
}

/* add a user who's on the channel */
static int add_chan_user (char * nick, int idx, char * hand)
{
   memberlist *m;
   char s[121], s1[121];
   struct chanset_t *chan;
   
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "error: invalid console channel\n");
      return 0;
   }
   if (!(chan->stat & CHANACTIVE)) {
      dprintf(idx, "I'm not on %s!\n", chan->name);
      return 0;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s.\n", nick, chan->name);
      return 0;
   }
   hand[9] = 0;
   sprintf(s, "%s!%s", m->nick, m->userhost);
   get_handle_by_host(s1, s);
   if (s1[0] != '*') {
      dprintf(idx, "%s is already known as %s.\n", nick, s1);
      return 0;
   }
   if ((get_attr_handle(s1) & USER_OWNER) &&
       !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (strcmp(dcc[idx].nick, s1) != 0)) {
      dprintf(idx, "You can't add hostmasks to the bot owner.\n");
      return 0;
   }
   maskhost(s, s1);
   if (!is_user(hand)) {
      dprintf(idx, "Added [%s]%s with no password.\n", hand, s1);
      userlist = adduser(userlist, hand, s1, "-", default_flags);
      return 1;
   } else {
      char h[10];
      int atr,chatr;
      
      get_handle_by_host(h, s);
      if (strcmp(h, "*") != 0) {
	 dprintf(idx, "This user already matches for %s!\n", h);
	 return 0;
      }
      dprintf(idx, "Added hostmask %s to %s.\n", s1, hand);
      addhost_by_handle(hand, s1);
      atr = get_attr_handle(hand);
      chatr = get_chanattr_handle(hand,chan->name);
      if (!(m->flags & CHANOP) &&
	  (((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)) ||
	  (chatr & CHANUSER_OP)) && (chan->stat & CHAN_OPONJOIN))
	 add_mode(chan, '+', 'o', m->nick);
      return 1;
   }
}

/* Remove a user who's on the channel */
static int del_chan_user (char * nick, int idx)
{
   memberlist *m;
   char s[121], s1[121];
   struct chanset_t *chan;
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "error: invalid console channel\n");
      return 0;
   }
   if (!(chan->stat & CHANACTIVE)) {
      dprintf(idx, "I'm not on %s!\n", chan->name);
      return 0;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s.\n", nick, chan->name);
      return 0;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   get_handle_by_host(s1, s);
   if (s1[0] == '*') {
      dprintf(idx, "%s is not in a valid user.\n", nick);
      return 0;
   }
   if ((get_attr_handle(s1) & USER_OWNER) &&
       !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (strcmp(dcc[idx].nick, s1) != 0)) {
      dprintf(idx, "You can't delete the bot owner.\n");
      return 0;
   }
   if ((get_attr_handle(s1) & USER_MASTER) && !(get_attr_handle(dcc[idx].nick)
						& USER_MASTER)) {
      dprintf(idx, "You can't delete a bot master.\n");
      return 0;
   }
   if ((get_attr_handle(s1) & USER_BOT) && !(get_attr_handle(dcc[idx].nick)
					     & USER_MASTER)) {
      dprintf(idx, "You can't delete a bot.\n");
      return 0;
   }
   if (deluser(s1)) {
      dprintf(idx, "Deleted %s.\n", s1);
      return 1;
   } else {
      dprintf(idx, "Failed.\n");
      return 0;
   }
}

/* op/deop on the fly per master's request */
static void give_op (char * nick, struct chanset_t * chan, int idx)
{
   memberlist *m;
   char s[121];
   int atr, chatr;
   if (!(chan->stat & CHANACTIVE)) {
      dprintf(idx, "I'm not on %s!\n", chan->name);
      return;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s.\n", nick, chan->name);
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chan->name);
   if ((chatr & CHANUSER_DEOP) ||
       ((atr & USER_DEOP) && !(chatr & CHANUSER_OP))) {
      dprintf(idx, "%s is currently being auto-deopped.\n", m->nick);
      return;
   }
   if ((chan->stat & CHAN_BITCH) && (!(chatr & CHANUSER_OP))
       && !((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))) {
      dprintf(idx, "%s is not a registered op.\n", m->nick);
      return;
   }
   add_mode(chan, '+', 'o', nick);
   dprintf(idx, "Gave op to %s on %s\n", nick, chan->name);
}

static void give_deop (char * nick, struct chanset_t * chan, int idx)
{
   memberlist *m;
   char s[121];
   int atr, chatr;
   if (!(chan->stat & CHANACTIVE)) {
      dprintf(idx, "I'm not on %s!\n", chan->name);
      return;
   }
   m = ismember(chan, nick);
   if (m == NULL) {
      dprintf(idx, "%s is not on %s.\n", nick, chan->name);
      return;
   }
   if (strcasecmp(nick, botname) == 0) {
      dprintf(idx, "I'm not going to deop myself.\n");
      return;
   }
   sprintf(s, "%s!%s", m->nick, m->userhost);
   atr = get_attr_host(s);
   chatr = get_chanattr_host(s, chan->name);
   if ((atr & USER_MASTER) || (chatr & CHANUSER_MASTER)) {
      dprintf(idx, "%s is a master for %s\n", m->nick, chan->name);
      return;
   }
   if ((((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))
	|| (chatr & CHANUSER_OP)) &&
       (!((get_attr_handle(dcc[idx].nick) & USER_MASTER) ||
   (get_chanattr_handle(dcc[idx].nick, chan->name) & CHANUSER_MASTER)))) {
      dprintf(idx, "%s has the op flag for %s\n", m->nick, chan->name);
      return;
   }
   add_mode(chan, '-', 'o', nick);
   dprintf(idx, "Took op from %s on %s\n", nick, chan->name);
}

#endif

/* Do we have any flags that will allow us ops on a channel? */
static int has_op (int idx, char * par)
{
   struct chanset_t *chan;
   int atr, chatr;
   context;
   if (par[0]) {
      chan = findchan(par);
      if (chan == NULL) {
	 dprintf(idx, "No such channel.\n");
	 return 0;
      }
   } else {
      chan = findchan(dcc[idx].u.chat->con_chan);
      if (chan == NULL) {
	 dprintf(idx, "Invalid console channel.\n");
	 return 0;
      }
   }
   atr = get_attr_handle(dcc[idx].nick);
   chatr = get_chanattr_handle(dcc[idx].nick, chan->name);
   if ((atr & (USER_MASTER | USER_OWNER)) ||
       (chatr & (CHANUSER_MASTER | CHANUSER_OWNER | CHANUSER_OP)))
      return 1;
   else if ((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))
      return 1;
   else {
      dprintf(idx, "You are not a channel op on %s.\n", chan->name);
      return 0;
   }
   return 1;
}

#ifndef NO_IRC
static void cmd_act (int idx, char * par)
{
   char chan[512];
   
   if (!par[0]) {
      dprintf(idx, "Usage: act [channel] <action>\n");
      return;
   }
   if ((par[0] == '#') || (par[0] == '&'))
      nsplit(chan,par);
   else
      chan[0] = 0;
   if (!has_op(idx, chan))
      return;
   putlog(LOG_CMDS, "*", "#%s# (%s) act %s", dcc[idx].nick,
	  chan[0]?chan:dcc[idx].u.chat->con_chan, par);
   mprintf(serv, "PRIVMSG %s :\001ACTION %s\001\n", 
	   chan[0]?chan:dcc[idx].u.chat->con_chan,
	   par);
}
#endif

#ifndef NO_IRC
static void cmd_msg (int idx, char * par)
{
   char nick[512];
   
   if (!par[0]) {
      dprintf(idx, "Usage: msg <nick> <message>\n");
   } else {
      nsplit(nick, par);
      putlog(LOG_CMDS, "*", "#%s# msg %s %s", dcc[idx].nick, nick, par);
      mprintf(serv, "PRIVMSG %s :%s\n", nick, par);
   }
}
#endif

#ifndef NO_IRC
static void cmd_say (int idx, char * par)
{
   char chan[512];
   if (!par[0]) {
      dprintf(idx, "Usage: say <message>\n");
      return;
   }
   if ((par[0] == '#') || (par[0] == '&'))
     nsplit(chan,par);
   else
     chan[0] = 0;
   if (!has_op(idx, chan))
      return;
   putlog(LOG_CMDS, "*", "#%s# (%s) say %s", dcc[idx].nick,
	  chan[0]?chan:dcc[idx].u.chat->con_chan, par);
   mprintf(serv, "PRIVMSG %s :%s\n", chan[0]?chan:dcc[idx].u.chat->con_chan,
	   par);
}
#endif

#ifndef NO_IRC
static void cmd_kickban (int idx, char * par)
{
   struct chanset_t *chan;
   char chname[512];
   
   if (!par[0]) {
      dprintf(idx, "Usage: kickban [channel] <nick> [reason]\n");
      return;
   }
   if ((par[0] == '#') || (par[0] == '&'))
     nsplit(chname,par);
   else
     chname[0] = 0;
   if (!has_op(idx, chname))
      return;
   chan = findchan(chname[0]?chname:dcc[idx].u.chat->con_chan);
   if (!me_op(chan)) {
      dprintf(idx, "I can't help you now because I'm not a channel op on %s.\n",
	      chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# (%s) kickban %s", dcc[idx].nick,
	  chan->name, par);
   user_kickban(idx, par);
}
#endif

#ifndef NO_IRC
static void cmd_op (int idx, char * par)
{
   struct chanset_t *chan;
   char nick[512];
   if (!par[0]) {
      dprintf(idx, "Usage: op <nick> [channel]\n");
      return;
   }
   nsplit(nick, par);
   if (par[0]) {
      if (!has_op(idx, par))
	 return;
      chan = findchan(par);
   } else {
      if (!has_op(idx, ""))
	 return;
      chan = findchan(dcc[idx].u.chat->con_chan);
   }
   if (!me_op(chan)) {
      dprintf(idx, "I can't help you now because I'm not a chan op on %s.\n",
	      chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# (%s) op %s %s", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, nick, par);
   give_op(nick, chan, idx);
}
#endif

#ifndef NO_IRC
static void cmd_deop (int idx, char * par)
{
   struct chanset_t *chan;
   char nick[512];
   if (!par[0]) {
      dprintf(idx, "Usage: deop <nick> [channel]\n");
      return;
   }
   nsplit(nick, par);
   if (par[0]) {
      if (!has_op(idx, par))
	 return;
      chan = findchan(par);
   } else {
      if (!has_op(idx, ""))
	 return;
      chan = findchan(dcc[idx].u.chat->con_chan);
   }
   if (!me_op(chan)) {
      dprintf(idx, "I can't help you now because I'm not a channel op on %s.\n",
	      chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# (%s) deop %s %s", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, nick, par);
   give_deop(nick, chan, idx);
}
#endif

#ifndef NO_IRC
static void cmd_kick (int idx, char * par)
{
   struct chanset_t *chan;
   char chname[512];
   
   if (!par[0]) {
      dprintf(idx, "Usage: kick [channel] <nick> [reason]\n");
      return;
   }
   if ((par[0] == '#') || (par[0] == '&'))
     nsplit(chname,par);
   else
     chname[0] = 0;
   if (!has_op(idx, chname))
      return;
   chan = findchan(chname[0]?chname:dcc[idx].u.chat->con_chan);
   if (!me_op(chan)) {
      dprintf(idx, "I can't help you now because I'm not a channel op on %s.\n",
	      chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# (%s) kick %s", dcc[idx].nick,
	  chan->name, par);
   user_kick(idx, par);
}
#endif

#ifndef NO_IRC
static void cmd_invite (int idx, char * par)
{
   struct chanset_t *chan;
   if (!has_op(idx, ""))
      return;
   chan = findchan(dcc[idx].u.chat->con_chan);
   putlog(LOG_CMDS, "*", "#%s# (%s) invite %s", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, par);
   if (!me_op(chan) && (chan->channel.mode & CHANINV)) {
      dprintf(idx, "I'm not chop on %s, so I can't invite anyone.\n", chan->name);
      return;
   }
   if (ischanmember(chan->name, par) && !is_split(chan->name, par)) {
      dprintf(idx, "%s is already on %s!\n", par, chan->name);
      return;
   }
   mprintf(serv, "INVITE %s %s\n", par, chan->name);
   dprintf(idx, "Inviting %s to %s.\n", par, chan->name);
}
#endif

static void cmd_resetbans (int idx, char * par)
{
   struct chanset_t *chan;
   if (!has_op(idx, ""))
      return;
   chan = findchan(dcc[idx].u.chat->con_chan);
   putlog(LOG_CMDS, "*", "#%s# (%s) resetbans", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan);
   dprintf(idx, "Resetting bans on %s...\n", chan->name);
   resetbans(chan);
}

static void cmd_pls_ban (int idx, char * par)
{
   char who[512], note[512], s[UHOSTLEN];
   struct chanset_t *chan;
   char chname[512];
   chname[0] = 0;
   if (!par[0]) {
      dprintf(idx, "Usage: +ban <hostmask> [channel] [reason]\n");
      return;
   }
   nsplit(who, par);
   rmspace(who);
   chan = findchan(dcc[idx].u.chat->con_chan);
   if ((par[0] == '#') || (par[0] == '&')) {
      nsplit(chname, par);
      rmspace(chname);
      if (!has_op(idx, chname))
	 return;
      chan = findchan(chname);
   } else if (!(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
      if (!has_op(idx, ""))
	 return;
      strcpy(chname, dcc[idx].u.chat->con_chan);
   }
   rmspace(par);
   if (!par[0])
      strcpy(note, "request");
   else {
      strncpy(note, par, 65);
      note[65] = 0;
   }
   /* fix missing ! or @ BEFORE checking against myself */
   if (strchr(who, '!') == NULL)
      strcat(s, "!*@*");	/* lame nick ban */
   if (strchr(who, '@') == NULL)
      strcat(s, "@*");		/* brain-dead? */
   sprintf(s, "%s!%s", botname, botuserhost);
   if (wild_match(who, s)) {
      dprintf(idx, "Duh...  I think I'll ban myself today, Marge!\n");
      putlog(LOG_CMDS, "*", "#%s# attempted +ban %s", dcc[idx].nick, who);
      return;
   }
   if (strlen(who) > 70)
      who[70] = 0;
   /* irc can't understand bans longer than that */
   if (chname[0]) {
      u_addban(chan->bans, who, dcc[idx].nick, note, 0L);
      putlog(LOG_CMDS, "*", "#%s# (%s) +ban %s %s (%s)", dcc[idx].nick,
	     dcc[idx].u.chat->con_chan, who, chname, note);
      dprintf(idx, "New %s ban: %s (%s)\n", chname, who, note);
      if (me_op(chan))
	 add_mode(chan, '+', 'b', who);
      recheck_channel(chan);
      return;
   }
   addban(who, dcc[idx].nick, note, 0L);
   putlog(LOG_CMDS, "*", "#%s# (%s) +ban %s (%s)", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, who, note);
   dprintf(idx, "New ban: %s (%s)\n", who, note);
   chan = chanset;
   while (chan != NULL) {
      if (me_op(chan))
	 add_mode(chan, '+', 'b', who);
      recheck_channel(chan);
      chan = chan->next;
   }
}

static void cmd_mns_ban (int idx, char * par)
{
   int i = 0, j, k;
   struct chanset_t *chan;
   char s[UHOSTLEN], ban[512], chname[512];
   chname[0] = 0;
   if (!par[0]) {
      dprintf(idx, "Usage: -ban <hostmask|ban #> [channel]\n");
      return;
   }
   nsplit(ban, par);
   rmspace(ban);
   rmspace(par);
   if (par[0] == '#') {
      strcpy(chname, par);
      if (!has_op(idx, chname))
	 return;
   } else if (!has_op(idx, ""))
      return;
   if (!chname[0] && (get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
      i = delban(ban);
      if (i > 0) {
	 putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s", dcc[idx].nick,
		dcc[idx].u.chat->con_chan, ban);
	 dprintf(idx, "Removed ban: %s\n", ban);
	 chan = chanset;
	 while (chan != NULL) {
	    if (me_op(chan))
	       add_mode(chan, '-', 'b', ban);
	    chan = chan->next;
	 }
	 return;
      }
   }
   /* channel-specific ban? */
   if (!chname[0])
      strcpy(chname, dcc[idx].u.chat->con_chan);
   chan = findchan(chname);
   sprintf(s, "%d", i + atoi(ban));
   j = u_delban(chan->bans, s);
   if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s", dcc[idx].nick,
	     dcc[idx].u.chat->con_chan, s);
      dprintf(idx, "Removed %s channel ban: %s\n", chname, s);
      add_mode(chan, '-', 'b', s);
      return;
   }
   k = u_delban(chan->bans, ban);
   if (k > 0) {
      putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s", dcc[idx].nick,
	     dcc[idx].u.chat->con_chan, ban);
      dprintf(idx, "Removed %s channel ban: %s\n", chname, ban);
      add_mode(chan, '-', 'b', ban);
      return;
   }
   /* okay, not in any ban list -- might be ban on channel */
   if (atoi(ban) > 0) {
      if (kill_chanban(chname, idx, 1 - i - j, atoi(ban))) {
	 putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s [on channel]",
		dcc[idx].nick, dcc[idx].u.chat->con_chan, ban);
	 dprintf(idx, "Not in banlist. Removing match on %s\n", chname);
	 return;
      }
   } else {
      if (kill_chanban_name(chname, idx, ban)) {
	 putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s [on channel]",
		dcc[idx].nick, dcc[idx].u.chat->con_chan, ban);
	 dprintf(idx, "Not in banlist. Removing match on %s\n", chname);
	 return;
      }
   }
}

static void cmd_bans (int idx, char * par)
{
   if (strcasecmp(par, "all") == 0) {
      putlog(LOG_CMDS, "*", "#%s# bans all", dcc[idx].nick);
      tell_bans(idx, 1, "");
   } else {
      putlog(LOG_CMDS, "*", "#%s# bans %s", dcc[idx].nick, par);
      tell_bans(idx, 0, par);
   }
}

#ifndef NO_IRC
static void cmd_channel (int idx, char * par)
{
   if (par[0]) {
      if (!has_op(idx, par))
	 return;
   } else if (!has_op(idx, ""))
      return;
   putlog(LOG_CMDS, "*", "#%s# (%s) channel %s", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, par);
   tell_verbose_chan_info(idx, par);
}
#endif

#ifndef NO_IRC
static void cmd_servers (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# servers", dcc[idx].nick);
   tell_servers(idx);
}

#endif

static void cmd_pls_ignore (int idx, char * par)
{
   char who[512], note[66];
   if (!par[0]) {
      dprintf(idx, "Usage: +ignore <hostmask> [comment]\n");
      return;
   }
   nsplit(who, par);
   who[UHOSTLEN - 1] = 0;
   strncpy(note, par, 65);
   note[65] = 0;
   if (match_ignore(who)) {
      dprintf(idx, "That already matches an existing ignore.\n");
      return;
   }
   dprintf(idx, "Now ignoring: %s (%s)\n", who, note);
   addignore(who, dcc[idx].nick, note, 0L);
   putlog(LOG_CMDS, "*", "#%s# +ignore %s %s", dcc[idx].nick, who, note);
}

static void cmd_mns_ignore (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: -ignore <hostmask>\n");
      return;
   }
   if (delignore(par)) {
      putlog(LOG_CMDS, "*", "#%s# -ignore %s", dcc[idx].nick, par);
      dprintf(idx, "No longer ignoring: %s\n", par);
   } else
      dprintf(idx, "Can't find that ignore.\n");
}

static void cmd_ignores (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# ignores %s", dcc[idx].nick, par);
   tell_ignores(idx, par);
}


#ifndef NO_IRC
static void cmd_adduser (int idx, char * par)
{
   char nick[512];
   struct chanset_t *chan;
   if (!par[0]) {
      dprintf(idx, "Usage: adduser <nick> [handle]\n");
      return;
   }
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "Your console channel is invalid.\n");
      return;
   }
   nsplit(nick, par);
   if (!par[0]) {
      
      if (add_chan_user(nick, idx, nick)) {
	 add_chanrec_by_handle(userlist,nick,dcc[idx].u.chat->con_chan,0,0);
	 putlog(LOG_CMDS, "*", "#%s# adduser %s", dcc[idx].nick, nick);
      }
   } else {
      char * p;
      int ok = 1;
      
      for (p = par;*p;p++)
	if ((*p <= 32) || (*p >= 127))
	 ok = 0;
      if (!ok) 
	dprintf(idx,"You can't have strange characters in a nick.\n");
      else if (strchr("-,+*=:!.@#;$", par[0]) != NULL) 
	dprintf(idx, "You can't start a nick with '%c'.\n",par[0]);
      else if (add_chan_user(nick, idx, par)) {
	 add_chanrec_by_handle(userlist,par,dcc[idx].u.chat->con_chan,0,0);
	 putlog(LOG_CMDS, "*", "#%s# adduser %s %s", dcc[idx].nick, nick, par);
      }
   }
}
#endif

static void cmd_pls_user (int idx, char * par)
{
   char handle[512], host[512];
   if (!par[0]) {
      dprintf(idx, "Usage: +user <handle> <hostmask>\n");
      return;
   }
   nsplit(handle, par);
   nsplit(host, par);
   if (strlen(handle) > 9)
      handle[9] = 0;		/* max len = 9 */
   if (is_user(handle)) {
      dprintf(idx, "Someone already exists by that name.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# +user %s %s", dcc[idx].nick, handle, host);
   userlist = adduser(userlist, handle, host, "-", 0);
   dprintf(idx, "Added %s (%s) with no password or flags.\n", handle, host);
}

#ifndef NO_IRC
static void cmd_deluser (int idx, char * par)
{
   char nick[512];
   struct chanset_t *chan;
   if (!par[0]) {
      dprintf(idx, "Usage: deluser <nick>\n");
      return;
   }
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "Your console channel is invalid.\n");
      return;
   }
   nsplit(nick, par);
   if (!(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (get_attr_handle(par) & USER_OWNER)) {
      dprintf(idx, "Can't remove the bot owner!\n");
      return;
   }
   if (!(get_chanattr_handle(dcc[idx].nick, chan->name) & CHANUSER_OWNER) &&
       (get_chanattr_handle(par, chan->name) & CHANUSER_OWNER) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "Can't remove the channel owner!\n");
      return;
   }
   if (del_chan_user(nick, idx))
      putlog(LOG_CMDS, "*", "#%s# deluser %s", dcc[idx].nick, nick);
}
#endif

static void cmd_mns_user (int idx, char * par)
{
   int atr = get_attr_handle(dcc[idx].nick), atr1 = get_attr_handle(par);
   if (!par[0]) {
      dprintf(idx, "Usage: -user <nick>\n");
      return;
   }
   if (!(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (get_attr_handle(par) & USER_OWNER)) {
      dprintf(idx, "Can't remove the bot owner!\n");
      return;
   }
   if ((get_attr_handle(par) & BOT_SHARE) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "You can't remove shared bots.\n");
      return;
   }
   if ((atr & USER_BOTMAST) && (!(atr & (USER_MASTER | USER_OWNER)))
       && (!(atr1 & USER_BOT))) {
      dprintf(idx, "Can't remove users who aren't bots!\n");
      return;
   }
   if (deluser(par)) {
      putlog(LOG_CMDS, "*", "#%s# -user %s", dcc[idx].nick, par);
      dprintf(idx, "Deleted %s.\n", par);
   } else
      dprintf(idx, "Failed.\n");
}

static void cmd_pls_host (int idx, char * par)
{
   char handle[512], host[512];
   int chatr;
   if (!par[0]) {
      dprintf(idx, "Usage: +host <handle> <newhostmask>\n");
      return;
   }
   nsplit(handle, par);
   nsplit(host, par);
   chatr = get_chanattr_handle(handle, dcc[idx].u.chat->con_chan);
   if (!is_user(handle)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if (ishost_for_handle(handle, host)) {
      dprintf(idx, "That hostmask is already there.\n");
      return;
   }
   if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
       (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER | USER_OWNER))) &&
       (!(get_attr_handle(handle) & USER_BOT))) {
      dprintf(idx, "You can't add hostmasks to non-bots.\n");
      return;
   }
   if (chatr == 0 && (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
      dprintf(idx, "You can't add hostmasks to non-channel users.\n");
      return;
   }
   if ((get_attr_handle(handle) & USER_OWNER) &&
       !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't add hostmasks to the bot owner.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# +host %s %s", dcc[idx].nick, handle, host);
   addhost_by_handle(handle, host);
   dprintf(idx, "Added '%s' to %s\n", host, handle);
}

static void cmd_mns_host (int idx, char * par)
{
   char handle[512], host[512];
   int chatr;
   if (!par[0]) {
      dprintf(idx, "Usage: -host [handle] <hostmask>\n");
      return;
   }
   nsplit(handle, par);
   nsplit(host, par);
   chatr = get_chanattr_handle(handle, dcc[idx].u.chat->con_chan);
   if (!is_user(handle)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if (!host[0]) {
      strcpy(host,handle);
      strcpy(handle,dcc[idx].nick);
   } else if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
	      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER | USER_OWNER))) &&
	      (!(get_attr_handle(handle) & USER_BOT))) {
      dprintf(idx, "You can't remove hostmasks from non-bots.\n");
      return;
   } else if (chatr == 0 && (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
      dprintf(idx, "You can't remove hostmasks from non-channel users.\n");
      return;
   } else if ((get_attr_handle(handle) & BOT_SHARE) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "You can't remove hostmask from a shared bot.\n");
      return;
   } else if ((get_attr_handle(handle) & USER_OWNER) &&
       !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't remove hostmasks from the bot owner.\n");
      return;
   }
   if (delhost_by_handle(handle, host)) {
      putlog(LOG_CMDS, "*", "#%s# -host %s %s", dcc[idx].nick, handle, host);
      dprintf(idx, "Removed '%s' from %s\n", host, handle);
   } else
      dprintf(idx, "Failed.\n");
}

#ifndef NO_IRC
static void cmd_dump (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: dump <server stuff>\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# dump %s", dcc[idx].nick, par);
   mprintf(serv, "%s\n", par);
}
#endif

static void cmd_reset (int idx, char * par)
{
   struct chanset_t *chan;
   int atr, chatr = 0;
   atr = get_attr_handle(dcc[idx].nick);
   if (par[0]) {
      chan = findchan(par);
      if (chan == NULL) {
	 dprintf(idx, "I don't monitor channel %s\n", par);
	 return;
      }
      chatr = get_chanattr_handle(dcc[idx].nick, chan->name);
      if ((!(atr & USER_MASTER)) && (!(chatr & CHANUSER_MASTER))) {
	 dprintf(idx, "You are not a master on %s.\n", chan->name);
	 return;
      }
      putlog(LOG_CMDS, "*", "#%s# reset %s", dcc[idx].nick, par);
      dprintf(idx, "Resetting channel info for %s...\n", par);
      reset_chan_info(chan);
      return;
   }
   if (!(atr & USER_MASTER)) {
      dprintf(idx, "You are not a Bot Master.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# reset all", dcc[idx].nick);
   dprintf(idx, "Resetting channel info for all channels...\n");
   chan = chanset;
   while (chan != NULL) {
      reset_chan_info(chan);
      chan = chan->next;
   }
}

#ifndef NO_IRC
static void cmd_jump (int idx, char * par)
{
   char other[512], port[512];
   if (par[0]) {
      nsplit(other, par);
      nsplit(port, par);
      if (!port[0])
	 sprintf(port, "%d", default_port);
      putlog(LOG_CMDS, "*", "#%s# jump %s %s %s", dcc[idx].nick, other, port, par);
      strcpy(newserver, other);
      newserverport = atoi(port);
      strcpy(newserverpass, par);
   } else
      putlog(LOG_CMDS, "*", "#%s# jump", dcc[idx].nick);
   dprintf(idx, "Jumping servers...\n");
   tprintf(serv, "QUIT :changing servers\n");
   if (serv >= 0)
      killsock(serv);
   serv = (-1);
}
#endif

static void cmd_info (int idx, char * par)
{
   char s[512], chname[512];
   int locked = 0;
   if (!use_info) {
      dprintf(idx, "Info storage is turned off.\n");
      return;
   }
   /* yes, if the default info line is locked, then all the channel ones */
   /* are too. */
   get_handle_info(dcc[idx].nick, s);
   if (s[0] == '@')
      locked = 1;
   if ((par[0] == '#') || (par[0] == '+') || (par[0] == '&')) {
      nsplit(chname, par);
      if (!defined_channel(chname)) {
	 dprintf(idx, "No such channel.\n");
	 return;
      }
      get_handle_chaninfo(dcc[idx].nick, chname, s);
      if (s[0] == '@')
	 locked = 1;
   } else
      chname[0] = 0;
   if (!par[0]) {
      if (s[0] == '@')
	 strcpy(s, &s[1]);
      if (s[0]) {
	 if (chname[0]) {
	    dprintf(idx, "Info on %s: %s\n", chname, s);
	    dprintf(idx, "Use '.info %s none' to remove it.\n", chname);
	 } else {
	    dprintf(idx, "Default info: %s\n", s);
	    dprintf(idx, "Use '.info none' to remove it.\n");
	 }
      } else
	 dprintf(idx, "No info has been set for you.\n");
      putlog(LOG_CMDS, "*", "#%s# info %s", dcc[idx].nick, chname);
      return;
   }
   if ((locked) && !(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
      dprintf(idx, "Your info line is locked.  Sorry.\n");
      return;
   }
   if (strcasecmp(par, "none") == 0) {
      par[0] = 0;
      if (chname[0]) {
	 set_handle_chaninfo(userlist, dcc[idx].nick, chname, par);
	 dprintf(idx, "Removed your info line on %s.\n", chname);
	 putlog(LOG_CMDS, "*", "#%s# info %s none", dcc[idx].nick, chname);
      } else {
	 set_handle_info(userlist, dcc[idx].nick, par);
	 dprintf(idx, "Removed your default info line.\n");
	 putlog(LOG_CMDS, "*", "#%s# info none", dcc[idx].nick);
      }
      return;
   }
   if (par[0] == '@')
      strcpy(par, &par[1]);
   if (chname[0]) {
      set_handle_chaninfo(userlist, dcc[idx].nick, chname, par);
      dprintf(idx, "Your info on %s is now: %s\n", chname, par);
      putlog(LOG_CMDS, "*", "#%s# info %s ...", dcc[idx].nick, chname);
   } else {
      set_handle_info(userlist, dcc[idx].nick, par);
      dprintf(idx, "Your default info is now: %s\n", par);
      putlog(LOG_CMDS, "*", "#%s# info ...", dcc[idx].nick);
   }
}

static void cmd_chinfo (int idx, char * par)
{
   char handle[512], chname[512];
   if (!use_info) {
      dprintf(idx, "Info storage is turned off.\n");
      return;
   }
   nsplit(handle, par);
   if (!handle[0]) {
      dprintf(idx, "Usage: chinfo <handle> [channel] <new-info>\n");
      return;
   }
   if (!is_user(handle)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if ((par[0] == '#') || (par[0] == '+') || (par[0] == '&'))
      nsplit(chname, par);
   else
      chname[0] = 0;
   if (get_attr_handle(handle) & USER_BOT) {
      dprintf(idx, "Useful only for users.\n");
      return;
   }
   if ((get_attr_handle(handle) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't change info for the bot owner.\n");
      return;
   }
   if (chname[0])
      putlog(LOG_CMDS, "*", "#%s# chinfo %s %s %s", dcc[idx].nick, handle, chname, par);
   else
      putlog(LOG_CMDS, "*", "#%s# chinfo %s %s", dcc[idx].nick, handle, par);
   if (strcasecmp(par, "none") == 0)
      par[0] = 0;
   if (chname[0]) {
      set_handle_chaninfo(userlist, handle, chname, par);
      if (par[0] == '@')
	 dprintf(idx, "New info (LOCKED) for %s on %s: %s\n", handle, chname, &par[1]);
      else if (par[0])
	 dprintf(idx, "New info for %s on %s: %s\n", handle, chname, par);
      else
	 dprintf(idx, "Wiped info for %s on %s\n", handle, chname);
   } else {
      set_handle_info(userlist, handle, par);
      if (par[0] == '@')
	 dprintf(idx, "New default info (LOCKED) for %s: %s\n", handle, &par[1]);
      else if (par[0])
	 dprintf(idx, "New default info for %s: %s\n", handle, par);
      else
	 dprintf(idx, "Wiped default info for %s\n", handle);
   }
}

#ifndef NO_IRC
static void cmd_topic (int idx, char * par)
{
   struct chanset_t *chan;
   if (!has_op(idx, ""))
      return;
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (!par[0]) {
      if (chan->channel.topic[0] == 0) {
	 dprintf(idx, "No topic is set for %s\n", chan->name);
	 return;
      } else {
	 dprintf(idx, "The topic for %s is: %s\n", chan->name, chan->channel.topic);
	 return;
      }
   }
   if ((channel_optopic(chan)) && (!me_op(chan))) {
      dprintf(idx, "I'm not a channel op on %s and the channel is +t.\n",
	      chan->name);
      return;
   }
   mprintf(serv, "TOPIC %s :%s\n", chan->name, par);
   dprintf(idx, "Changing topic...\n");
   strcpy(chan->channel.topic, par);
   putlog(LOG_CMDS, "*", "#%s# (%s) topic %s", dcc[idx].nick,
	  dcc[idx].u.chat->con_chan, par);
}
#endif

static void cmd_stick_yn(idx, par, yn)
int idx;
char *par;
int yn;
{
   int i, j;
   struct chanset_t *chan;
   char s[UHOSTLEN];
   if (!par[0]) {
      dprintf(idx, "Usage: %sstick <ban>\n", yn ? "" : "un");
      return;
   }
   i = setsticky_ban(par, yn);
   if (i > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick %s", dcc[idx].nick, yn ? "" : "un", par);
      dprintf(idx, "%stuck: %s\n", yn ? "S" : "Uns", par);
      return;
   }
   /* channel-specific ban? */
   chan = findchan(dcc[idx].u.chat->con_chan);
   if (chan == NULL) {
      dprintf(idx, "Invalid console channel.\n");
      return;
   }
   if (atoi(par))
      sprintf(s, "%d", i + atoi(par));
   else
      strcpy(s, par);
   j = u_setsticky_ban(chan->bans, s, yn);
   if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick %s", dcc[idx].nick, yn ? "" : "un", s);
      dprintf(idx, "%stuck: %s\n", yn ? "S" : "Uns", s);
      return;
   }
   /* well fuxor then. */
   dprintf(idx, "No such ban.\n");
}

static void cmd_stick (int idx, char * par)
{
   cmd_stick_yn(idx, par, 1);
}

static void cmd_unstick (int idx, char * par)
{
   cmd_stick_yn(idx, par, 0);
}

static void cmd_pls_chrec (int idx, char * par)
{
   char nick[512], chn[512];
   struct chanset_t *chan;
   struct userrec *u;
   struct chanuserrec *chanrec;
   context;
   if (!par[0]) {
      dprintf(idx, "Usage: +chrec <User> [channel]\n");
      return;
   }
   nsplit(nick, par);
   u = get_user_by_handle(userlist, nick);
   if (u == NULL) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if (par[0] == 0)
      chan = findchan(dcc[idx].u.chat->con_chan);
   else {
      nsplit(chn, par);
      chan = findchan(chn);
   }
   if (chan == NULL) {
      dprintf(idx, "No such channel.\n");
      return;
   }
   if (!(get_attr_handle(dcc[idx].nick) & USER_MASTER) &&
   !(get_chanattr_handle(dcc[idx].nick, chan->name) & CHANUSER_MASTER)) {
      dprintf(idx, "You have no permission to do that on %s.\n",
	      chan->name);
      return;
   }
   chanrec = get_chanrec(u, chan->name);
   if (chanrec != NULL) {
      dprintf(idx, "User %s already has a channel record for %s.\n", nick, chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# +chrec %s %s", dcc[idx].nick, nick, chan->name);
   add_chanrec(u,chan->name,0,0);
   dprintf(idx, "Added %s channel record for %s.\n", chan->name, nick);
}

static void cmd_mns_chrec (int idx, char * par)
{
   char nick[512], chn[512];
   struct chanset_t *chan;
   struct userrec *u;
   struct chanuserrec *chanrec;
   context;
   if (!par[0]) {
      dprintf(idx, "Usage: -chrec <User> [channel]\n");
      return;
   }
   nsplit(nick, par);
   u = get_user_by_handle(userlist, nick);
   if (u == NULL) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if (par[0] == 0)
      chan = findchan(dcc[idx].u.chat->con_chan);
   else {
      nsplit(chn, par);
      chan = findchan(chn);
   }
   if (chan == NULL) {
      dprintf(idx, "No such channel.\n");
      return;
   }
   if (!(get_attr_handle(dcc[idx].nick) & USER_MASTER) &&
   !(get_chanattr_handle(dcc[idx].nick, chan->name) & CHANUSER_MASTER)) {
      dprintf(idx, "You have no permission to do that on %s.\n",
	      chan->name);
      return;
   }
   chanrec = get_chanrec(u, chan->name);
   if (chanrec == NULL) {
      dprintf(idx, "User %s doesn't have a channel record for %s.\n", nick, chan->name);
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# -chrec %s %s", dcc[idx].nick, nick, chan->name);
   del_chanrec(u, chan->name);
   dprintf(idx, "Removed %s channel record for %s.\n", chan->name, nick);
}

#ifndef NO_IRC

static void cmd_pls_chan (int idx, char * par)
{
   char chname[512], null[] = "";
   if (!par[0]) {
      dprintf(idx, "Usage: +chan <#channel>\n");
      return;
   }
   nsplit(chname, par);
   if (findchan(chname) != NULL) {
      dprintf(idx, "That channel already exists!\n");
      return;
   }
   if (tcl_channel_add(0, chname, null) == TCL_ERROR)
      dprintf(idx, "Invalid channel.\n");
   else
      putlog(LOG_CMDS, "*", "#%s# +chan %s", dcc[idx].nick, chname);
}

static void cmd_mns_chan (int idx, char * par)
{
   char chname[512];
   struct chanset_t *chan;
   int i;
   if (!par[0]) {
      dprintf(idx, "Usage: -chan <#channel>\n");
      return;
   }
   nsplit(chname, par);
   chan = findchan(chname);
   if (chan == NULL) {
      dprintf(idx, "That channel doesnt exist!\n");
      return;
   }
   if (chan->stat & CHANSTATIC) {
      dprintf(idx, "Cannot remove %s, it is not a dynamic channel!.\n",
	      chname);
      return;
   }
   clear_channel(chan, 0);
   freeuser(chan->bans);
   killchanset(chname);
   if (serv >= 0)
      mprintf(serv, "PART %s\n", chname);
   dprintf(idx, "Channel %s removed from the bot.\n", chname);
   dprintf(idx, "This includes any channel specific bans you set.\n");
   putlog(LOG_CMDS, "*", "#%s# -chan %s", dcc[idx].nick, chname);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT)
	  && (strcasecmp(dcc[i].u.chat->con_chan, chname) == 0)) {
	 dprintf(i, "%s is no longer a valid channel, changing your console to '*'\n",
		 chname);
	 strcpy(dcc[i].u.chat->con_chan, "*");
      }
}

static void cmd_chaninfo (int idx, char * par)
{
   char chname[256], work[512];
   struct chanset_t *chan;
   if (!par[0]) {
      strcpy(chname,dcc[idx].u.chat->con_chan);
      if (chname[0] == '*') {
	 dprintf(idx, "You console channel is invalid\n");
	 return;
      }
   } else {
      nsplit(chname, par);
      if (!(get_attr_handle(dcc[idx].nick)& USER_MASTER)
	  && !(get_chanattr_handle(dcc[idx].nick,chname) & CHANUSER_MASTER)) {
	 dprintf(idx,"You dont have access to %s.",chname);
	 return;
      }
   }
   chan = findchan(chname);
   if (chan == NULL) {
      dprintf(idx, "No such channel defined.\n");
      return;
   }
   if (chan->stat & CHANSTATIC)
      dprintf(idx, "Settings for static channel %s\n", chname);
   else
      dprintf(idx, "Settings for dynamic channel %s\n", chname);
   get_mode_protect(chan, work);
   dprintf(idx, "Protect modes (chanmode): %s\n", work[0] ? work : "None");
   if (chan->idle_kick)
      dprintf(idx, "Idle Kick after (idle-kick): %d\n", chan->idle_kick);
   else
      dprintf(idx, "Idle Kick after (idle-kick): DONT!\n");
   /* only bot owners can see/change these (they're TCL commands) */
   if (get_attr_handle(dcc[idx].nick) & USER_OWNER) {
      if (chan->need_op[0])
	 dprintf(idx, "To regain op's (need-op):\n%s\n", chan->need_op);
      if (chan->need_invite[0])
	 dprintf(idx, "To get invite (need-invite):\n%s\n", chan->need_invite);
      if (chan->need_key[0])
	 dprintf(idx, "To get key (need-key):\n%s\n", chan->need_key);
      if (chan->need_unban[0])
	 dprintf(idx, "If Im banned (need-unban):\n%s\n", chan->need_unban);
      if (chan->need_limit[0])
	 dprintf(idx, "When channel full (need-limit):\n%s\n", chan->need_limit);
   }
   dprintf(idx, "Other modes:\n");
   dprintf(idx, "     %cclearbans  %cenforcebans  %cdynamicbans  %cuserbans\n",
	   (chan->stat & CHAN_CLEARBANS) ? '+' : '-',
	   (chan->stat & CHAN_ENFORCEBANS) ? '+' : '-',
	   (chan->stat & CHAN_DYNAMICBANS) ? '+' : '-',
	   (chan->stat & CHAN_NOUSERBANS) ? '-' : '+');
   dprintf(idx, "     %cautoop     %cbitch        %cgreet        %cprotectops\n",
	   (chan->stat & CHAN_OPONJOIN) ? '+' : '-',
	   (chan->stat & CHAN_BITCH) ? '+' : '-',
	   (chan->stat & CHAN_GREET) ? '+' : '-',
	   (chan->stat & CHAN_PROTECTOPS) ? '+' : '-');
   dprintf(idx, "     %cstatuslog  %cstopnethack  %crevenge      %csecret\n",
	   (chan->stat & CHAN_LOGSTATUS) ? '+' : '-',
	   (chan->stat & CHAN_STOPNETHACK) ? '+' : '-',
	   (chan->stat & CHAN_REVENGE) ? '+' : '-',
	   (chan->stat & CHAN_SECRET) ? '+' : '-');
   dprintf(idx, "     %cshared\n", (chan->stat & CHAN_SHARED) ? '+' : '-');
   putlog(LOG_CMDS, "*", "#%s# chaninfo %s", dcc[idx].nick, chname);
}

static void cmd_chanset (int idx, char * par)
{
   char chname[512], work[512], args[512], answer[512];
   char *list[2];
   struct chanset_t *chan;
   if (!par[0]) {
      dprintf(idx, "Usage: chanset [#channel] <settings>\n");
      return;
   }
   if ((par[0] == '&') || (par[0] == '#')) 
     nsplit(chname, par);
   else
     strcpy(chname,dcc[idx].u.chat->con_chan);
   chan = findchan(chname);
   if (chan == NULL) {
      dprintf(idx, "That channel doesnt exist!\n");
      return;
   }
   nsplit(work, par);
   list[0] = work;
   list[1] = args;
   answer[0] = 0;
   while (work[0]) {
      if (work[0] == '+' || work[0] == '-' ||
	  (strcmp(work, "dont-idle-kick") == 0)) {
	 if (strcmp(&(work[1]), "shared") == 0) {
	    dprintf(idx, "You can't change shared settings on the fly\n");
	 } else if (tcl_channel_modify(0, chan, 1, list) == TCL_OK) {
	    strcat(answer, work);
	    strcat(answer, " ");
	 } else
	    dprintf(idx, "Error trying to set %s for %s, invalid mode\n",
		    work, chname);
	 nsplit(work, par);
	 continue;
      }
      /* the rest have an unknow amount of args, so assume the rest of the *
       * line is args. Woops nearly made a nasty little hole here :) we'll *
       * just ignore any non global +n's trying to set the need-commands   */
      if ((strncmp(work, "need-", 5) != 0) ||
	  (get_attr_handle(dcc[idx].nick) & USER_OWNER)) {
	 strcpy(args, par);
	 if (tcl_channel_modify(0, chan, 2, list) == TCL_OK) {
	    strcat(answer, work);
	    strcat(answer, " { ");
	    strcat(answer, par);
	    strcat(answer, " }");
	 } else
	    dprintf(idx, "Error trying to set %s for %s, invalid option\n",
		    work, chname);
      }
      break;
   }
   if (answer[0]) {
      dprintf(idx, "Successfully set modes { %s } on %s.\n",
	      answer, chname);
      putlog(LOG_CMDS, "*", "#%s# chanset %s %s", dcc[idx].nick, chname, answer);
   }
}

static void cmd_chansave (int idx, char * par)
{
   if (!chanfile[0])
      dprintf(idx, "No channel saving file defined.\n");
   else
      dprintf(idx, "Saving all dynamic channel settings.\n");
   putlog(LOG_MISC, "*", "#%s# chansave", dcc[idx].nick);
   write_channels();
}

static void cmd_chanload (int idx, char * par)
{
   if (!chanfile[0])
      dprintf(idx, "No channel saving file defined.\n");
   else
      dprintf(idx, "Reloading all dynamic channel settings.\n");
   putlog(LOG_MISC, "*", "#%s# chanload", dcc[idx].nick);
   read_channels();
}

#endif				/* !NO_IRC */

/* DCC CHAT COMMANDS */
/* function call should be:
   int cmd_whatever(idx,"parameters");
   as with msg commands, function is responsible for any logging
*/
cmd_t C_dcc_irc[]={
  { "+ban", "o|o", (Function)cmd_pls_ban, NULL },
#ifndef NO_IRC
  { "+chan", "n", (Function)cmd_pls_chan, NULL },
#endif
  { "+chrec", "m", (Function)cmd_pls_chrec, NULL },
  { "+host", "Bm|m", (Function)cmd_pls_host, NULL },
  { "+ignore", "m", (Function)cmd_pls_ignore, NULL },
  { "+user", "m", (Function)cmd_pls_user, NULL },
  { "-ban", "o|o", (Function)cmd_mns_ban, NULL },
  { "-bot", "B", (Function)cmd_mns_user, NULL },
#ifndef NO_IRC
  { "-chan", "n", (Function)cmd_mns_chan, NULL },
#endif
  { "-chrec", "m", (Function)cmd_mns_chrec, NULL },
  { "-host", "", (Function)cmd_mns_host, NULL },
  { "-ignore", "m", (Function)cmd_mns_ignore, NULL },
  { "-user", "m", (Function)cmd_mns_user, NULL },
#ifndef NO_IRC
  { "act", "o|o", (Function)cmd_act, NULL },
  { "adduser", "M", (Function)cmd_adduser, NULL },
  { "deluser", "M", (Function)cmd_deluser, NULL },
#endif
  { "bans", "o|o", (Function)cmd_bans, NULL },
#ifndef NO_IRC
  { "chaninfo", "m|m", (Function)cmd_chaninfo, NULL },
  { "chanload", "n|n", (Function)cmd_chanload, NULL },
  { "channel", "o|o", (Function)cmd_channel, NULL },
  { "chanset", "n|n", (Function)cmd_chanset, NULL },
  { "chansave", "n|n", (Function)cmd_chansave, NULL },
#endif
  { "chinfo", "m", (Function)cmd_chinfo, NULL },
#ifndef NO_IRC
  { "deop", "o|o", (Function)cmd_deop, NULL },
  { "dump", "m", (Function)cmd_dump, NULL },
#endif
  { "ignores", "m", (Function)cmd_ignores, NULL },
  { "info", "", (Function)cmd_info, NULL },
#ifndef NO_IRC
  { "invite", "o|o", (Function)cmd_invite, NULL },
  { "jump", "m", (Function)cmd_jump, NULL },
  { "kick", "o|o", (Function)cmd_kick, NULL },
  { "kickban", "o|o", (Function)cmd_kickban, NULL },
  { "msg", "o", (Function)cmd_msg, NULL },
  { "op", "o|o", (Function)cmd_op, NULL },
#endif
  { "quit", "", (Function)0, NULL },
  { "reset", "m|m", (Function)cmd_reset, NULL },
  { "resetbans", "o|o", (Function)cmd_resetbans, NULL },
#ifndef NO_IRC
  { "say", "o|o", (Function)cmd_say, NULL },
  { "servers", "o", (Function)cmd_servers, NULL },
#endif
  { "stick", "o|o", (Function)cmd_stick, NULL },
#ifndef NO_IRC
  { "topic", "o|o", (Function)cmd_topic, NULL },
#endif
  { "unstick", "o|o", (Function)cmd_unstick, NULL },
  { 0, 0, 0, 0 }
};
