/* 
   cmds.c -- handles:
   commands from a user via dcc
   (split in 2, this portion contains no-irc commands)
 * 
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
#include "modules.h"
#include "chan.h"
#include <ctype.h>

extern int serv;
extern int require_p;
extern char origbotname[];
extern char botnetnick[];
extern int dcc_total;
extern struct dcc_t * dcc;
extern struct userrec *userlist;
extern char owner[];
extern struct chanset_t *chanset;
extern int make_userfile;
extern int conmask;
extern Tcl_Interp *interp;
extern tcl_timer_t *timer, *utimer;
extern int do_restart;
extern int noshare;
extern int backgrd;
extern time_t now;
extern int remote_boots;

/* here to prevent export */
/* add hostmask to a bot's record if possible */
static int add_bot_hostmask (int idx, char * nick)
{
   struct chanset_t *chan;
   memberlist *m;
   char s[UHOSTLEN], s1[UHOSTLEN];
   chan = chanset;
   while (chan != NULL) {
      if (chan->stat & CHANACTIVE) {
	 m = ismember(chan, nick);
	 if (m != NULL) {
	    sprintf(s, "%s!%s", m->nick, m->userhost);
	    get_handle_by_host(s1, s);
	    if (s1[0] != '*') {
	       dprintf(idx, "(Can't add userhost for %s because it matches %s)\n",
		       nick, s1);
	       return 0;
	    }
	    maskhost(s, s1);
	    dprintf(idx, "(Added hostmask for %s from %s)\n", nick, chan->name);
	    addhost_by_handle(m->nick, s1);
	    return 1;
	 }
      }
      chan = chan->next;
   }
   return 0;
}

static void tell_who (int idx, int chan)
{
   int i, k, ok = 0, atr;
   char s[121];
   time_t tt;
   atr = get_attr_handle(dcc[idx].nick);
   tt = time(NULL);
   if (chan == 0)
      dprintf(idx, "Party line members:  (* = owner, + = master, @ = op)\n");
   else {
      char *cname = get_assoc_name(chan);
      if (cname == NULL)
	 dprintf(idx, 
	   "People on channel %s%d:  (* = owner, + = master, @ = op)\n",
		 (chan < 100000) ? "" : "*", chan % 100000);
      else
	 dprintf(idx, 
	   "People on channel '%s' (%s%d):  (* = owner, + = master, @ = op)\n",
		 cname, (chan < 100000) ? "" : "*", chan % 100000);
   }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT)
	 if (dcc[i].u.chat->channel == chan) {
	    if (atr & USER_OWNER) {
	       sprintf(s, "  [%.2d]  %c%-10s %s", 
			i, (geticon(i) == '-' ? ' ' : geticon(i)),
		    	dcc[i].nick, dcc[i].host);
	    } else {
	       sprintf(s, "  %c%-10s %s", 
			(geticon(i) == '-' ? ' ' : geticon(i)),
		    	dcc[i].nick, dcc[i].host);
	    }
	    if (atr & USER_MASTER) {
	       if (dcc[i].u.chat->con_flags)
		  sprintf(&s[strlen(s)], " (con:%s)", 
			masktype(dcc[i].u.chat->con_flags));
	    }
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
	    dprintf(idx, "%s\n", s);
	    if (dcc[i].u.chat->away != NULL)
	       dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
	 }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	 if (!ok) {
	    ok = 1;
	    dprintf(idx, "Bots connected:\n");
	 }
	 strcpy(s, ctime(&dcc[i].timeval));
	 strcpy(s, &s[1]);
	 s[9] = 0;
	 strcpy(s, &s[7]);
	 s[2] = ' ';
	 strcpy(&s[7], &s[10]);
	 s[12] = 0;
	 if (atr & USER_OWNER) {
	    dprintf(idx, "  [%.2d]  %s%c%-10s (%s) %s\n", 
		    dcc[i].sock, dcc[i].u.bot->status & STAT_CALLED ?  "<-" : "->", 
		    dcc[i].u.bot->status & STAT_SHARE ? '+' : ' ', 
		    dcc[i].nick, s, dcc[i].u.bot->version);
	 } else {
	    dprintf(idx, "  %s%c%-10s (%s) %s\n", 
		    dcc[i].u.bot->status & STAT_CALLED ?  "<-" : "->", 
		    dcc[i].u.bot->status & STAT_SHARE ? '+' : ' ', 
		    dcc[i].nick, s, dcc[i].u.bot->version);
	 }
      }
   ok = 0;
   for (i = 0; i < dcc_total; i++) {
      if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel != chan)) {
	 if (!ok) {
	    ok = 1;
	    dprintf(idx, "Other people on the bot:\n");
	 }
	 if (atr & USER_OWNER) {
	    sprintf(s, "  [%.2d]  %c%-10s ", 
		i, (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
	 } else {
	    sprintf(s, "  %c%-10s ", 
		(geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
	 }
	 if (atr & USER_MASTER) {
	    if (dcc[i].u.chat->channel < 0)
	       strcat(s, "(-OFF-) ");
	    else if (dcc[i].u.chat->channel == 0)
	       strcat(s, "(party) ");
	    else
	       sprintf(&s[strlen(s)], "(%5d) ", dcc[i].u.chat->channel);
	 }
	 strcat(s, dcc[i].host);
	 if (atr & USER_MASTER) {
	    if (dcc[i].u.chat->con_flags)
	       sprintf(&s[strlen(s)], " (con:%s)", 
			masktype(dcc[i].u.chat->con_flags));
	 }
	 if (tt - dcc[i].timeval > 300) {
	    k = (tt - dcc[i].timeval) / 60;
	    if (k < 60)
	       sprintf(&s[strlen(s)], " (idle %dm)", k);
	    else
	       sprintf(&s[strlen(s)], " (idle %dh%dm)", k / 60, k % 60);
	 }
	 dprintf(idx, "%s\n", s);
	 if (dcc[i].u.chat->away != NULL)
	    dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
      }
      if ((atr & USER_MASTER) && (dcc[i].type == &DCC_FILES)) {
	 if (!ok) {
	    ok = 1;
	    dprintf(idx, "Other people on the bot:\n");
	 }
	 if (atr & USER_OWNER) {
	    sprintf(s, "  [%.2d]  %c%-10s (files) %s", 
			i, dcc[i].u.file->chat->status & STAT_CHAT ?  '+' : ' ',
			dcc[i].nick, dcc[i].host);
	 } else {
	    sprintf(s, "  %c%-10s (files) %s", 
			dcc[i].u.file->chat->status & STAT_CHAT ?  '+' : ' ', 
			dcc[i].nick, dcc[i].host);
	 }
	 dprintf(idx, "%s\n", s);
      }
   }
}

static void cmd_botinfo (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# botinfo", dcc[idx].nick);
   tandout("info? %d:%s@%s\n", dcc[idx].sock, dcc[idx].nick, botnetnick);
}

static void cmd_whom (int idx, char * par)
{
   if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, "You have chat turned off.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# whom %s", dcc[idx].nick, par);
   if (!par[0])
      answer_local_whom(idx, dcc[idx].u.chat->channel);
   else if (par[0] == '*')
      answer_local_whom(idx, -1);
   else {
      int chan = atoi(par);
      if (par[0] == '*') {
	 if ((par[1] < '0' || par[1] > '9')) {
	    chan = get_assoc(par + 1);
	    if (chan < 0) {
	       dprintf(idx, "No such channel.\n");
	       return;
	    }
	 } else
	    chan = 100000 + atoi(par + 1);
	 if (chan < 100000 || chan > 199999) {
	    dprintf(idx, "Channel # out of range : Local channels are *0 - *99999\n");
	    return;
	 }
      } else {
	 if ((par[0] < '0') || (par[0] > '9')) {
	    chan = get_assoc(par);
	    if (chan < 0) {
	       dprintf(idx, "No such channel.\n");
	       return;
	    }
	 }
	 if ((chan < 0) || (chan > 99999)) {
	    dprintf(idx, "Channel # out of range: must be 0-99999\n");
	    return;
	 }
      }
      answer_local_whom(idx, chan);
   }
}

static void cmd_me (int idx, char * par)
{
   int i;
   if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, "You have chat turned off.\n");
      return;
   }
   if (!par[0]) {
      dprintf(idx, "Usage: me <action>\n");
      return;
   }
   if (dcc[idx].u.chat->away != NULL)
      not_away(idx);
   for (i = 0; i < dcc_total; i++) {
      if ((dcc[i].type == &DCC_CHAT) &&
	  (dcc[i].u.chat->channel == dcc[idx].u.chat->channel) &&
	  ((i != idx) || (dcc[i].u.chat->status & STAT_ECHO)))
	 dprintf(i, "* %s %s\n", dcc[idx].nick, par);
      if (dcc[i].type == &DCC_BOT)
	 if (dcc[idx].u.chat->channel < 100000)
	    tprintf(dcc[i].sock, "actchan %s@%s %d %s\n", dcc[idx].nick, botnetnick,
		    dcc[idx].u.chat->channel, par);
   }
   check_tcl_act(dcc[idx].nick, dcc[idx].u.chat->channel, par);
}

static void cmd_motd (int idx, char * par)
{
   int i;
   if (par[0]) {
      putlog(LOG_CMDS, "*", "#%s# motd %s", dcc[idx].nick, par);
      if (strcasecmp(par, botnetnick) == 0)
	 show_motd(idx);
      else {
	 i = nextbot(par);
	 if (i < 0)
	    dprintf(idx, "That bot isn't connected.\n");
	 else
	    tprintf(dcc[i].sock, "motd %d:%s@%s %s\n", dcc[idx].sock,
		    dcc[idx].nick, botnetnick, par);
      }
   } else {
      putlog(LOG_CMDS, "*", "#%s# motd", dcc[idx].nick);
      show_motd(idx);
   }
}

void cmd_note (int idx, char * par)
{
   char handle[512], *p;
   int echo;
   if (!par[0]) {
      dprintf(idx, "Usage: note <to-whom> <message>\n");
      return;
   }
   /* could be file system user */
   nsplit(handle, par);
   echo = (dcc[idx].type == &DCC_CHAT) ? (dcc[idx].u.chat->status & STAT_ECHO) :
       (dcc[idx].u.file->chat->status & STAT_ECHO);
   p = strchr(handle, ',');
   while (p != NULL) {
      *p = 0;
      p++;
      add_note(handle, dcc[idx].nick, par, idx, echo);
      strcpy(handle, p);
      p = strchr(handle, ',');
   }
   add_note(handle, dcc[idx].nick, par, idx, echo);
}

static void cmd_away (int idx, char * par)
{
   if (strlen(par) > 60)
      par[60] = 0;
   set_away(idx, par);
}

static void cmd_newpass (int idx, char * par)
{
   char new[512];
   if (!par[0]) {
      dprintf(idx, "Usage: newpass <newpassword>\n");
      return;
   }
   nsplit(new, par);
   if (strlen(new) > 16)
      new[16] = 0;
   if (strlen(new) < 4) {
      dprintf(idx, "Please use at least 4 characters.\n");
      return;
   }
   change_pass_by_handle(dcc[idx].nick, new);
   putlog(LOG_CMDS, "*", "#%s# newpass...", dcc[idx].nick);
   dprintf(idx, "Changed password to '%s'\n", new);
}

static void cmd_bots (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# bots", dcc[idx].nick);
   tell_bots(idx);
}

static void cmd_bottree (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# bottree", dcc[idx].nick);
   tell_bottree(idx,0);
}

static void cmd_vbottree (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# vbottree", dcc[idx].nick);
   tell_bottree(idx,1);
}

static void cmd_help (int idx, char * par)
{
   struct flag_record fr = {0,0,0};
   
   fr.global = get_attr_handle(dcc[idx].nick);
   fr.chan = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.chat->con_chan);
   if (par[0]) {
      putlog(LOG_CMDS, "*", "#%s# help %s", dcc[idx].nick, par);
      tellhelp(idx, par, &fr);
   } else {
      putlog(LOG_CMDS, "*", "#%s# help", dcc[idx].nick);
      if ((fr.global & (USER_GLOBAL | USER_MASTER | USER_BOTMAST)) 
	  || (fr.chan & (CHANUSER_OP | CHANUSER_MASTER)))
	 tellhelp(idx, "help", &fr);
      else
	 tellhelp(idx, "helpparty", &fr);
   }
}

static void cmd_addlog (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: addlog <message>\n");
      return;
   }
   dprintf(idx, "Placed entry in the log file.\n");
   putlog(LOG_MISC, "*", "%s: %s", dcc[idx].nick, par);
}

static void cmd_who (int idx, char * par)
{
   int i;
   if (par[0]) {
      if (dcc[idx].u.chat->channel < 0) {
	 dprintf(idx, "You have chat turned off.\n");
	 return;
      }
      putlog(LOG_CMDS, "*", "#%s# who %s", dcc[idx].nick, par);
      if (strcasecmp(par, botnetnick) == 0)
	 tell_who(idx, dcc[idx].u.chat->channel);
      else {
	 i = nextbot(par);
	 if (i < 0) {
	    dprintf(idx, "That bot isn't connected.\n");
	 } else if (dcc[idx].u.chat->channel > 99999)
	    dprintf(idx, "You are on a local channel\n");
	 else
	    tprintf(dcc[i].sock, "who %d:%s@%s %s %d\n", dcc[idx].sock,
	       dcc[idx].nick, botnetnick, par, dcc[idx].u.chat->channel);
      }
   } else {
      putlog(LOG_CMDS, "*", "#%s# who", dcc[idx].nick);
      if (dcc[idx].u.chat->channel < 0)
	 tell_who(idx, 0);
      else
	 tell_who(idx, dcc[idx].u.chat->channel);
   }
}

static void cmd_whois (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: whois <handle>\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# whois %s", dcc[idx].nick, par);
   tell_user_ident(idx, par, (get_attr_handle(dcc[idx].nick) & USER_MASTER));
}

static void cmd_match (int idx, char * par)
{
   int start = 1, limit = 20;
   char s[512], s1[512], chname[512];
   if (!par[0]) {
      dprintf(idx, "Usage: match <nick/host>\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# match %s", dcc[idx].nick, par);
   nsplit(s, par);
   if ((par[0] == '#') || (par[0] == '+') || (par[0] == '&'))
      nsplit(chname, par);
   else
      chname[0] = 0;
   if (atoi(par) > 0) {
      split(s1, par);
      if (atoi(s1) > 0)
	 start = atoi(s1);
      limit = atoi(par);
   }
   tell_users_match(idx, s, start, limit, (get_attr_handle(dcc[idx].nick) &
					   USER_MASTER), chname);
}

static void cmd_status (int idx, char * par)
{
   int atr = 0;
   if (strcasecmp(par, "all") == 0) {
      atr = get_attr_handle(dcc[idx].nick);
      if (!(atr & USER_MASTER)) {
	 dprintf(idx, "YOu do not have Bot Master priveleges.\n");
	 return;
      }
      putlog(LOG_CMDS, "*", "#%s# status all", dcc[idx].nick);
      tell_verbose_status(idx, 1);
      tell_mem_status_dcc(idx);
      dprintf(idx, "\n");
      tell_settings(idx);
   } else {
      putlog(LOG_CMDS, "*", "#%s# status", dcc[idx].nick);
      tell_verbose_status(idx, 1);
      tell_mem_status_dcc(idx);
   }
}

static void cmd_dccstat (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# dccstat", dcc[idx].nick);
   tell_dcc(idx);
}

static void cmd_boot (int idx, char * par)
{
   int i, files = 0, ok = 0;
   char who[512];
   if (!par[0]) {
      dprintf(idx, "Usage: boot nick[@bot]\n");
      return;
   }
   nsplit(who, par);
   if (strchr(who, '@') != NULL) {
      char whonick[512];
      splitc(whonick, who, '@');
      whonick[20] = 0;
      if (strcasecmp(who, botnetnick) == 0) {
	 cmd_boot(idx, whonick);
	 return;
      }
      if (remote_boots > 1) {
	 i = nextbot(who);
	 if (i < 0) {
	    dprintf(idx, "No such bot connected.\n");
	    return;
	 }
	 tprintf(dcc[i].sock, "reject %s@%s %s@%s %s\n", dcc[idx].nick, botnetnick,
		 whonick, who, par[0] ? par : dcc[idx].nick);
	 putlog(LOG_MISC, "*", "#%s# boot %s@%s (%s)", dcc[idx].nick, whonick, who,
		par[0] ? par : dcc[idx].nick);
      } else 
	dprintf(idx, "Remote boots are disabled here.\n");
      return;
   }
   for (i = 0; i < dcc_total; i++)
      if ((strcasecmp(dcc[i].nick, who) == 0) && (!ok) &&
	  ((dcc[i].type == &DCC_CHAT) || (dcc[i].type == &DCC_FILES))) {
	 if ((get_attr_handle(who) & USER_OWNER) &&
	     (strcasecmp(dcc[idx].nick, who) != 0)) {
	    dprintf(idx, "Can't boot the bot owner.\n");
	    return;
	 }
	 if ((get_attr_handle(who) & USER_MASTER) &&
	     (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
	    dprintf(idx, "Can't boot a bot master.\n");
	    return;
	 }
	 files = (dcc[i].type == &DCC_FILES);
	 if (files)
	    dprintf(idx, "Booted %s from the file section.\n", dcc[i].nick);
	 else
	    dprintf(idx, "Booted %s from the bot.\n", dcc[i].nick);
	 putlog(LOG_CMDS, "*", "#%s# boot %s %s", dcc[idx].nick, who, par);
	 do_boot(i, dcc[idx].nick, par);
	 ok = 1;
      }
   if (!ok)
      dprintf(idx, "Who?  No such person on the party line.\n");
}

static void cmd_console (int idx, char * par)
{
   char nick[512], s[2], s1[512];
   int dest = 0, i, ok = 0, pls, md, atr, chatr;
   if (!par[0]) {
      dprintf(idx, "Your console is %s: %s (%s)\n", dcc[idx].u.chat->con_chan,
	      masktype(dcc[idx].u.chat->con_flags),
	      maskname(dcc[idx].u.chat->con_flags));
      return;
   }
   atr = get_attr_handle(dcc[idx].nick);
   strcpy(s1, par);
   split(nick, par);
   if ((nick[0]) && (nick[0] != '#') && (nick[0] != '&') &&
       (atr & (USER_MASTER | USER_OWNER))) {
      for (i = 0; i < dcc_total; i++)
	 if ((strcasecmp(nick, dcc[i].nick) == 0) && (dcc[i].type == &DCC_CHAT) && (!ok)) {
	    ok = 1;
	    dest = i;
	 }
      if (!ok) {
	 dprintf(idx, "No such user on the party line!\n");
	 return;
      }
      nick[0] = 0;
   } else
      dest = idx;
   if (!nick[0])
      nsplit(nick, par);
   while (nick[0]) {
      if ((nick[0] == '#') || (nick[0] == '&') || (nick[0] == '*')) {
	 if ((nick[0] != '*') && (findchan(nick) == NULL)) {
	    dprintf(idx, "Invalid console channel: %s\n", nick);
	    return;
	 }
	 chatr = get_chanattr_handle(dcc[idx].nick, nick);
	 if ((!(chatr & (CHANUSER_OP | CHANUSER_MASTER)))
	     && (!(atr & USER_MASTER)) &&
	     !((atr & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))) {
	    dprintf(idx, "You don't have op or master access to channel %s\n", nick);
	    return;
	 }
	 strncpy(dcc[dest].u.chat->con_chan, nick, 80);
	 dcc[dest].u.chat->con_chan[80] = 0;
      } else {
	 pls = 1;
	 if ((nick[0] != '+') && (nick[0] != '-'))
	    dcc[dest].u.chat->con_flags = 0;
	 for (i = 0; i < strlen(nick); i++) {
	    if (nick[i] == '+')
	       pls = 1;
	    else if (nick[i] == '-')
	       pls = (-1);
	    else {
	       s[0] = nick[i];
	       s[1] = 0;
	       md = logmodes(s);
	       if ((dest == idx) && !(atr & USER_MASTER) && pls) {
		  if (get_chanattr_handle(dcc[idx].nick, dcc[dest].u.chat->con_chan)
		      & CHANUSER_MASTER)
		     md &= ~(LOG_FILES | LOG_LEV1 | LOG_LEV2 |
			     LOG_LEV3 | LOG_LEV4 | LOG_LEV5 | LOG_LEV6 |
			     LOG_LEV7 | LOG_LEV8 | LOG_DEBUG);
		  else
		     md &= ~(LOG_MISC | LOG_CMDS | LOG_FILES | LOG_LEV1 | LOG_LEV2 |
			     LOG_LEV3 | LOG_LEV4 | LOG_LEV5 | LOG_LEV6 | LOG_LEV7 | LOG_LEV8 |
			     LOG_WALL | LOG_DEBUG);
	       }
	       if (!(atr & USER_OWNER) && pls) {
		  md &= LOG_RAW;
	       }
	       if (pls == 1)
		  dcc[dest].u.chat->con_flags |= md;
	       else
		  dcc[dest].u.chat->con_flags &= ~md;
	    }
	 }
      }
      nsplit(nick, par);
   }
   putlog(LOG_CMDS, "*", "#%s# console %s", dcc[idx].nick, s1);
   if (dest == idx) {
      dprintf(idx, "Set your console to %s: %s (%s)\n",
	      dcc[idx].u.chat->con_chan,
	      masktype(dcc[idx].u.chat->con_flags),
	      maskname(dcc[idx].u.chat->con_flags));
   } else {
      dprintf(idx, "Set console of %s to %s: %s (%s)\n", dcc[dest].nick,
	      dcc[dest].u.chat->con_chan,
	      masktype(dcc[dest].u.chat->con_flags),
	      maskname(dcc[dest].u.chat->con_flags));
      dprintf(dest, "%s set your console to %s: %s (%s)\n", dcc[idx].nick,
	      dcc[dest].u.chat->con_chan,
	      masktype(dcc[dest].u.chat->con_flags),
	      maskname(dcc[dest].u.chat->con_flags));
   }
}

static void cmd_pls_bot (int idx, char * par)
{
   char handle[512], addr[512];
   if (!par[0]) {
      dprintf(idx, "Usage: +bot <handle> <address:port#>\n");
      return;
   }
   nsplit(handle, par);
   nsplit(addr, par);
   if (strlen(handle) > 9)
      handle[9] = 0;		/* max len = 9 */
   if (is_user(handle)) {
      dprintf(idx, "Someone already exists by that name.\n");
      return;
   }
   if (strlen(addr) > 60)
      addr[60] = 0;
   if (strchr(addr, ':') == NULL)
      strcat(addr, ":3333");
   putlog(LOG_CMDS, "*", "#%s# +bot %s %s", dcc[idx].nick, handle, addr);
   userlist = adduser(userlist, handle, "none", "-", USER_BOT);
   set_handle_info(userlist, handle, addr);
   dprintf(idx, "Added bot '%s' with address '%s' and no password.\n",
	   handle, addr);
   if (!add_bot_hostmask(idx, handle))
      dprintf(idx, "You'll want to add a hostmask if this bot will ever %s",
	      "be on any channels that I'm on.\n");
}

static void cmd_chnick (int idx, char * par)
{
   char hand[512];
   int i;
   split(hand, par);
   if ((!hand[0]) || (!par[0])) {
      dprintf(idx, "Usage: chnick <oldnick> <newnick>\n");
      return;
   }
   if (strlen(par) > 9)
      par[9] = 0;
   for (i = 0; i < strlen(par); i++)
      if ((par[i] <= 32) || (par[i] >= 127) || (par[i] == '@'))
	 par[i] = '?';
   if (strchr("-,+*=:!.@#;$", par[0]) != NULL) {
      dprintf(idx, "Bizarre quantum forces prevent nicknames from starting with %c\n", par[0]);
      return;
   }
   if ((is_user(par)) && (strcasecmp(hand, par) != 0)) {
      dprintf(idx, "Already a user %s.\n", par);
      return;
   }
   if ((strcasecmp(par, origbotname) == 0) || (strcasecmp(par, botnetnick) == 0)) {
      dprintf(idx, "Hey! That's MY name!\n", par);
      return;
   }
   if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
       (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER | USER_OWNER))) &&
       (!(get_attr_handle(hand) & USER_BOT))) {
      dprintf(idx, "You can't change nick for non-bots.\n");
      return;
   }
   if ((get_attr_handle(hand) & BOT_SHARE) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "You can't change shared bot's nick.\n");
      return;
   }
   if ((get_attr_handle(hand) & USER_OWNER) &&
       (strcasecmp(dcc[idx].nick, hand) != 0)) {
      dprintf(idx, "Can't change the bot owner's handle.\n");
      return;
   }
   if (change_handle(hand, par)) {
      notes_change(idx, hand, par);
      putlog(LOG_CMDS, "*", "#%s# chnick %s %s", dcc[idx].nick, hand, par);
      dprintf(idx, "Changed.\n");
      for (i = 0; i < dcc_total; i++) {
	 if ((strcasecmp(dcc[i].nick, hand) == 0) && (dcc[i].type != &DCC_BOT)) {
	    char s[10];
	    strcpy(s, dcc[i].nick);
	    strcpy(dcc[i].nick, par);
	    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel >= 0)) {
	       chanout2(dcc[i].u.chat->channel, "Nick change: %s -> %s\n", s, par);
	       if (dcc[i].u.chat->channel < 100000) {
		  context;
		  tandout("part %s %s %d\n", botnetnick, s, dcc[i].sock);
		  tandout("join %s %s %d %c%d %s\n", botnetnick, par,
			  dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	       }
	    }
	 }
      }
   } else
      dprintf(idx, "Failed.\n");
}

static void cmd_nick (int idx, char * par)
{
   int i;
   char icon;
   if (!par[0]) {
      dprintf(idx, "Usage: nick <new-handle>\n");
      return;
   }
   if (strlen(par) > 9)
      par[9] = 0;
   for (i = 0; i < strlen(par); i++)
      if ((par[i] <= 32) || (par[i] >= 127) || (par[i] == '@'))
	 par[i] = '?';
   if (strchr("-,+*=:!.@#;$", par[0]) != NULL) {
      dprintf(idx, "Bizarre quantum forces prevent nicknames from starting with %c\n", par[0]);
      return;
   }
   if ((is_user(par)) && (strcasecmp(dcc[idx].nick, par) != 0)) {
      dprintf(idx, "Somebody is already using %s.\n", par);
      return;
   }
   if ((strcasecmp(par, origbotname) == 0) || (strcasecmp(par, botnetnick) == 0)) {
      dprintf(idx, "Hey!  That MY name!\n", par);
      return;
   }
   icon = geticon(idx);
   if (change_handle(dcc[idx].nick, par)) {
      notes_change(idx, dcc[idx].nick, par);
      putlog(LOG_CMDS, "*", "#%s# nick %s", dcc[idx].nick, par);
      dprintf(idx, "Okay, changed.\n");
      if (dcc[idx].u.chat->channel >= 0) {
	 chanout2(dcc[idx].u.chat->channel, "Nick change: %s -> %s\n",
		  dcc[idx].nick, par);
	 context;
	 if (dcc[idx].u.chat->channel < 100000) {
	    tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
	    tandout("join %s %s %d %c%d %s\n", botnetnick, par,
	    dcc[idx].u.chat->channel, icon, dcc[idx].sock, dcc[idx].host);
	 }
      }
      for (i = 0; i < dcc_total; i++)
	 if ((idx != i) && (strcasecmp(dcc[idx].nick, dcc[i].nick) == 0))
	    strcpy(dcc[i].nick, par);
      strcpy(dcc[idx].nick, par);
   } else
      dprintf(idx, "Failed.\n");
}

static void cmd_chpass (int idx, char * par)
{
   char handle[512], new[512];
   if (!par[0]) {
      dprintf(idx, "Usage: chpass <handle> [password]\n");
      return;
   }
   nsplit(handle, par);
   if (!par[0]) {
      if (!is_user(handle)) {
	 dprintf(idx, "No such user.\n");
	 return;
      }
      if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER | USER_OWNER))) &&
	  (!(get_attr_handle(handle) & USER_BOT))) {
	 dprintf(idx, "You can't change password for non-bots.\n");
	 return;
      }
      if ((get_attr_handle(handle) & BOT_SHARE) &&
	  (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
	 dprintf(idx, "You can't change shared bot's password.\n");
	 return;
      }
      if ((get_attr_handle(handle) & USER_OWNER) &&
	  (strcasecmp(handle, dcc[idx].nick) != 0)) {
	 dprintf(idx, "Can't change the bot owner's password.\n");
	 return;
      }
      putlog(LOG_CMDS, "*", "#%s# chpass %s [nothing]", dcc[idx].nick, handle);
      change_pass_by_handle(handle, "-");
      dprintf(idx, "Removed password.\n");
      return;
   }
   if (!is_user(handle)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
       (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER | USER_OWNER))) &&
       (!(get_attr_handle(handle) & USER_BOT))) {
      dprintf(idx, "You can't change password for non-bots.\n");
      return;
   }
   if ((get_attr_handle(handle) & BOT_SHARE) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "You can't change shared bot's password.\n");
      return;
   }
   if ((get_attr_handle(handle) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't change the bot owner's password.\n");
      return;
   }
   nsplit(new, par);
   if (strlen(new) > 16)
      new[16] = 0;
   if (strlen(new) < 4) {
      dprintf(idx, "Please use at least 4 characters.\n");
      return;
   }
   change_pass_by_handle(handle, new);
   putlog(LOG_CMDS, "*", "#%s# chpass %s [something]", dcc[idx].nick, handle);
   dprintf(idx, "Changed password.\n");
}

static void cmd_chaddr (int idx, char * par)
{
   char handle[512], addr[512];
   if (!par[0]) {
      dprintf(idx, "Usage: chaddr <botname> <address:botport#/userport#>\n");
      return;
   }
   nsplit(handle, par);
   nsplit(addr, par);
   if (!(get_attr_handle(handle) & USER_BOT)) {
      dprintf(idx, "Useful only for tandem bots.\n");
      return;
   }
   if ((get_attr_handle(handle) & BOT_SHARE) &&
       (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx, "You can't change shared bot's address.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# chaddr %s %s", dcc[idx].nick, handle, addr);
   dprintf(idx, "Changed bot's address.\n");
   set_handle_info(userlist, handle, addr);
}

static void cmd_comment (int idx, char * par)
{
   char handle[512];
   if (!par[0]) {
      dprintf(idx, "Usage: comment <handle> <newcomment>\n");
      return;
   }
   nsplit(handle, par);
   if (!is_user(handle)) {
      dprintf(idx, "No such user!\n");
      return;
   }
   if ((get_attr_handle(handle) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't change comment on the bot owner.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# comment %s %s", dcc[idx].nick, handle, par);
   if (strcasecmp(par, "none") == 0) {
      dprintf(idx, "Okay, comment blanked.\n");
      set_handle_comment(userlist, handle, "");
      return;
   }
   dprintf(idx, "Changed comment.\n");
   set_handle_comment(userlist, handle, par);
}

static void cmd_email (int idx, char * par)
{
   char s[161];
   if (!par[0]) {
      putlog(LOG_CMDS, "*", "#%s# email", dcc[idx].nick);
      get_handle_email(dcc[idx].nick, s);
      if (s[0]) {
	 dprintf(idx, "Your email address is: %s\n", s);
	 dprintf(idx, "(You can remove it with '.email none')\n");
      } else
	 dprintf(idx, "You have no email address set.\n");
      return;
   }
   if (strcasecmp(par, "none") == 0) {
      dprintf(idx, "Removed your email address.\n");
      set_handle_email(userlist, dcc[idx].nick, "");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# email %s", dcc[idx].nick, par);
   dprintf(idx, "Your email address: %s\n", par);
   set_handle_email(userlist, dcc[idx].nick, par);
}

static void cmd_chemail (int idx, char * par)
{
   char handle[512];
   if (!par[0]) {
      dprintf(idx, "Usage: chemail <handle> [address]\n");
      return;
   }
   nsplit(handle, par);
   if (!is_user(handle)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if ((get_attr_handle(handle) & USER_OWNER) &&
       (strcasecmp(handle, dcc[idx].nick) != 0)) {
      dprintf(idx, "Can't change email address of the bot owner.\n");
      return;
   }
   if (!par[0]) {
      putlog(LOG_CMDS, "*", "#%s# chemail %s", dcc[idx].nick, handle);
      dprintf(idx, "Wiped email for %s\n", handle);
      set_handle_email(userlist, handle, "");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# chemail %s %s", dcc[idx].nick, handle, par);
   dprintf(idx, "Changed email for %s to: %s\n", handle, par);
   set_handle_email(userlist, handle, par);
}

static void cmd_restart (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# restart", dcc[idx].nick);
#ifdef STATIC
   dprintf(idx,"You can not .restart a statically linked bot.\n");
#else
   if (!backgrd) {
      dprintf(idx, "You can not .restart a bot when running -n (due to tcl)\n");
      return;
   }
   dprintf(idx, "Restarting.\n");
   if (make_userfile) {
      putlog(LOG_MISC, "*", "Uh, guess you don't need to create a new userfile.");
      make_userfile = 0;
   }
   write_userfile();
   putlog(LOG_MISC, "*", "Restarting ...");
   wipe_timers(interp, &utimer);
   wipe_timers(interp, &timer);
   do_restart = 1;
#endif
}

static void cmd_rehash (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# rehash", dcc[idx].nick);
   dprintf(idx, "Rehashing.\n");
   if (make_userfile) {
      putlog(LOG_MISC, "*", "Uh, guess you don't need to create a new userfile.");
      make_userfile = 0;
   }
   write_userfile();
   putlog(LOG_MISC, "*", "Rehashing ...");
   rehash();
}

static void cmd_reload (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# reload", dcc[idx].nick);
   dprintf(idx, "Reloading user file...\n");
   reload();
}

static void cmd_die (int idx, char * par)
{
   char s[512];
   putlog(LOG_CMDS, "*", "#%s# die %s", dcc[idx].nick, par);
   if (par[0]) {
      chatout("*** BOT SHUTDOWN (%s: %s)\n", dcc[idx].nick, par);
      tandout("chat %s BOT SHUTDOWN (%s: %s)\n", botnetnick, dcc[idx].nick, par);
      tprintf(serv, "QUIT :%s\n", par);
   } else {
      chatout("*** BOT SHUTDOWN (authorized by %s)\n", dcc[idx].nick);
      tandout("chat %s BOT SHUTDOWN (authorized by %s)\n", botnetnick,
	      dcc[idx].nick);
      tprintf(serv, "QUIT :%s\n", dcc[idx].nick);
   }
   tandout("bye\n");
   write_userfile();
   sleep(3);			/* give the server time to understand */
   sprintf(s, "DIE BY %s!%s (%s)", dcc[idx].nick, dcc[idx].host, par[0] ? par :
	   "request");
   fatal(s, 0);
}

static void cmd_debug (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# debug", dcc[idx].nick);
   debug_mem_to_dcc(idx);
}

static void cmd_simul (int idx, char * par)
{
   char nick[512];
   int i, ok = 0;
   nsplit(nick, par);
   if (!par[0]) {
      dprintf(idx, "Usage: simul <nick> <text>");
      return;
   }
   for (i = 0; i < dcc_total; i++)
      if ((strcasecmp(nick, dcc[i].nick) == 0) && (!ok) &&
	  ((dcc[i].type == &DCC_CHAT) || (dcc[i].type == &DCC_FILES))) {
	 putlog(LOG_CMDS, "*", "#%s# simul %s %s", dcc[idx].nick, nick, par);
	 dcc_activity(dcc[i].sock, par, strlen(par));
	 ok = 1;
      }
   if (!ok)
      dprintf(idx, "No such user on the party line.\n");
}

static void cmd_link (int idx, char * par)
{
   char s[512];
   int i;
   if (!par[0]) {
      dprintf(idx, "Usage: link [some-bot] <new-bot>\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# link %s", dcc[idx].nick, par);
   if (strchr(par, ' ') == NULL)
      botlink(dcc[idx].nick, idx, par);
   else {
      split(s, par);
      i = nextbot(s);
      if (i < 0) {
	 dprintf(idx, "No such bot online.\n");
	 return;
      }
      tprintf(dcc[i].sock, "link %d:%s@%s %s %s\n", dcc[idx].sock, dcc[idx].nick,
	      botnetnick, s, par);
   }
}

static void cmd_unlink (int idx, char * par)
{
   int i;
   char bot[200];
   if (!par[0]) {
      dprintf(idx, "Usage: unlink <bot> [reason]\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# unlink %s", dcc[idx].nick, par);
   nsplit(bot, par);
   i = nextbot(bot);
   if (i < 0) {
      botunlink(idx, bot, par);
      return;
   }
   /* if we're directly connected to that bot, just do it */
   if (strcasecmp(dcc[i].nick, bot) == 0)
      botunlink(idx, bot, par);
   else {
      tprintf(dcc[i].sock, "unlink %d:%s@%s %s %s %s\n", dcc[idx].sock,
	      dcc[idx].nick, botnetnick, lastbot(bot), bot, par);
   }
}

static void cmd_relay (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: relay <bot>\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# relay %s", dcc[idx].nick, par);
   tandem_relay(idx, par, 0);
}

static void cmd_save (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# save", dcc[idx].nick);
   dprintf(idx, "Saving user file...\n");
   write_userfile();
}

static void cmd_trace (int idx, char * par)
{
   int i;
   if (!par[0]) {
      dprintf(idx, "Usage: trace <botname>\n");
      return;
   }
   if (strcasecmp(par, botnetnick) == 0) {
      dprintf(idx, "That's me!  Hiya! :)\n");
      return;
   }
   i = nextbot(par);
   if (i < 0) {
      dprintf(idx, "Unreachable bot.\n");
      return;
   }
   putlog(LOG_CMDS, "*", "#%s# trace %s", dcc[idx].nick, par);
   tprintf(dcc[i].sock, "trace %d:%s@%s %s :%d:%s\n", dcc[idx].sock, 
	   dcc[idx].nick, botnetnick, par, now,botnetnick);
}

static void cmd_binds (int idx, char * par)
{
   putlog(LOG_CMDS, "*", "#%s# binds %s", dcc[idx].nick, par);
   tell_binds(idx,par);
}

static void cmd_banner (int idx, char * par)
{
   char s[540];
   int i;
   if (!par[0]) {
      dprintf(idx, "Usage: banner <message>\n");
      return;
   }
   sprintf(s, "\007\007### Botwide:[%s] %s\n", dcc[idx].nick, par);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_CHAT) || (dcc[i].type == &DCC_FILES))
	 dprintf(i, s);
}

/* after messing with someone's user flags, make sure the dcc-chat flags
   are set correctly */
int check_dcc_attrs(char *hand, int flags, int oatr)
{
   int i, stat;
/* if it matches someone in the owner list, make sure he/she has +n */
   if (owner[0]) {
      char *p, s[512];
      strcpy(s, owner);
      p = strchr(s, ',');
      while (p != NULL) {
	 *p = 0;
	 rmspace(s);
	 if (strcasecmp(hand, s) == 0)
	    flags |= USER_OWNER;
	 strcpy(s, p + 1);
	 p = strchr(s, ',');
      }
      rmspace(s);
      if (strcasecmp(hand, s) == 0)
	 flags |= USER_OWNER;
      set_attr_handle(hand, flags);
   }
   for (i = 0; i < dcc_total; i++) {
      if (((dcc[i].type == &DCC_CHAT) || (dcc[i].type == &DCC_FILES)) &&
	  (strcasecmp(hand, dcc[i].nick) == 0)) {
	 if (dcc[i].type == &DCC_CHAT)
	    stat = dcc[i].u.chat->status;
	 else
	    stat = dcc[i].u.file->chat->status;
	 if ((dcc[i].type == &DCC_CHAT) && ((flags &
					    (USER_GLOBAL | USER_MASTER | USER_OWNER | USER_BOTMAST)) != (oatr &
	     (USER_GLOBAL | USER_MASTER | USER_OWNER | USER_BOTMAST)))) {
	    tandout("part %s %s %d\n", botnetnick, dcc[i].nick, dcc[i].sock);
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[i].nick,
	    dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	 }
	 if ((oatr & USER_MASTER) && !(flags & USER_MASTER)) {
	    dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS | LOG_RAW | LOG_FILES |
			      LOG_LEV1 | LOG_LEV2 | LOG_LEV3 | LOG_LEV4 |
			      LOG_LEV5 | LOG_LEV6 | LOG_LEV7 | LOG_LEV8 |
					  LOG_WALL | LOG_DEBUG);
	    if (master_anywhere(hand))
	       dcc[i].u.chat->con_flags |= (LOG_MISC | LOG_CMDS);
	    dprintf(i, "*** POOF! ***\n");
	    dprintf(i, "You are no longer a master on this bot.\n");
	 }
	 if (!(oatr & USER_MASTER) && (flags & USER_MASTER)) {
	    dcc[i].u.chat->con_flags |= conmask;
	    dprintf(i, "*** POOF! ***\n");
	    dprintf(i, "You are now a master on this bot.\n");
	 }
	 if (!(oatr & USER_BOTMAST) && (flags & USER_BOTMAST)) {
	    dprintf(i, "### POOF! ###\n");
	    dprintf(i, "You are now a botnet master on this bot.\n");
	 }
	 if ((oatr & USER_BOTMAST) && !(flags & USER_BOTMAST)) {
	    dprintf(i, "### POOF! ###\n");
	    dprintf(i, "You are no longer a botnet master on this bot.\n");
	 }
	 if (!(oatr & USER_OWNER) && (flags & USER_OWNER)) {
	    dprintf(i, "@@@ POOF! @@@\n");
	    dprintf(i, "You are now an OWNER of this bot.\n");
	 }
	 if ((oatr & USER_OWNER) && !(flags & USER_OWNER)) {
	    dprintf(i, "@@@ POOF! @@@\n");
	    dprintf(i, "You are no longer an owner of this bot.\n");
	 }
	 if ((stat & STAT_PARTY) && (flags & USER_GLOBAL))
	    stat &= ~STAT_PARTY;
	 if (!(stat & STAT_PARTY) && !(flags & USER_GLOBAL) && !(flags & USER_MASTER))
	    stat |= STAT_PARTY;
	 if ((stat & STAT_CHAT) && !(flags & USER_PARTY) && !(flags & USER_MASTER) &&
	     (!(flags & USER_GLOBAL) || require_p))
	    stat &= ~STAT_CHAT;
	 if ((dcc[i].type == &DCC_FILES) && !(stat & STAT_CHAT) &&
	     ((flags & USER_MASTER) || (flags & USER_PARTY) ||
	      ((flags & USER_GLOBAL) && !require_p)))
	    stat |= STAT_CHAT;
	 if (dcc[i].type == &DCC_CHAT)
	    dcc[i].u.chat->status = stat;
	 else
	    dcc[i].u.file->chat->status = stat;
	 /* check if they no longer have access to wherever they are */
	 /* DON'T kick someone off the party line just cos they lost +p */
	 /* (pinvite script removes +p after 5 mins automatically) */
	 if ((dcc[i].type == &DCC_FILES) && !(flags & USER_XFER) &&
	     !(flags & USER_MASTER)) {
	    dprintf(i, "-+- POOF! -+-\n");
	    dprintf(i, "You no longer have file area access.\n\n");
	    putlog(LOG_MISC, "*", "DCC user [%s]%s removed from file system",
		   dcc[i].nick, dcc[i].host);
	    if (dcc[i].u.file->chat->status & STAT_CHAT) {
	       struct chat_info *ci;
	       ci = dcc[i].u.file->chat;
	       nfree(dcc[i].u.file);
	       dcc[i].u.chat = ci;
	       dcc[i].u.chat->status &= (~STAT_CHAT);
	       dcc[i].type = &DCC_CHAT;
	       if (dcc[i].u.chat->channel >= 0) {
		  chanout2(dcc[i].u.chat->channel, "%s has returned.\n", dcc[i].nick);
		  context;
		  if (dcc[i].u.chat->channel < 100000)
		     tandout("unaway %s %d\n", botnetnick, dcc[i].sock);
	       }
	    } else {
	       killsock(dcc[i].sock);
	       dcc[i].sock = (long)dcc[i].type;
	       dcc[i].type = &DCC_LOST;
	       tandout("part %s %s %d\n", botnetnick, dcc[i].nick, dcc[i].sock);
	    }
	 }
      }
      if ((dcc[i].type == &DCC_BOT) && (strcasecmp(hand, dcc[i].nick) == 0)) {
	 if ((dcc[i].u.bot->status & STAT_LEAF) && !(flags & BOT_LEAF))
	    dcc[i].u.bot->status &= ~(STAT_LEAF | STAT_WARNED);
	 if (!(dcc[i].u.bot->status & STAT_LEAF) && (flags & BOT_LEAF))
	    dcc[i].u.bot->status |= STAT_LEAF;
      }
   }
   return flags;
}

/* some flags are mutually exclusive -- this roots them out */
int sanity_check(int atr)
{
   if ((atr & BOT_HUB) && (atr & BOT_ALT))
      atr &= ~BOT_ALT;
   if (atr & BOT_REJECT) {
      if (atr & BOT_SHARE)
	 atr &= ~(BOT_SHARE | BOT_REJECT);
      if (atr & BOT_HUB)
	 atr &= ~(BOT_HUB | BOT_REJECT);
      if (atr & BOT_ALT)
	 atr &= ~(BOT_ALT | BOT_REJECT);
   }
   if (atr & USER_BOT) {
      if (atr & (USER_PARTY | USER_MASTER | USER_COMMON | USER_OWNER))
	 atr &= ~(USER_PARTY | USER_MASTER | USER_COMMON | USER_OWNER);
   } else {
      if (atr & (BOT_LEAF | BOT_REJECT | BOT_ALT | BOT_SHARE | BOT_HUB))
	 atr &= ~(BOT_LEAF | BOT_REJECT | BOT_ALT | BOT_SHARE | BOT_HUB);
   }
   if ((atr & USER_GLOBAL) && (atr & USER_DEOP))
      atr &= ~(USER_GLOBAL | USER_DEOP);
   /* can't be owner without also being master */
   if (atr & USER_OWNER)
      atr |= USER_MASTER;
   /* master implies botmaster, op & friend */
   if (atr & USER_MASTER)
     atr |= USER_BOTMAST|USER_GLOBAL|USER_FRIEND;
   /* can't be botnet master without party-line access */
   if (atr & USER_BOTMAST)
      atr |= USER_PARTY;
   return atr;
}

int check_dcc_chanattrs(char *hand, char *chname, int chflags, int ochatr)
{
   int i, chatr, found = 0;
   struct chanset_t *chan;
   chan = chanset;
   for (i = 0; i < dcc_total; i++) {
      if (((dcc[i].type == &DCC_CHAT) || (dcc[i].type == &DCC_FILES)) &&
	  (strcasecmp(hand, dcc[i].nick) == 0)) {
	 if ((dcc[i].type == &DCC_CHAT) && ((chflags &
	   (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER)) != (ochatr &
		    (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER)))) {
	    tandout("part %s %s %d\n", botnetnick, dcc[i].nick, dcc[i].sock);
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[i].nick,
	    dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	 }
	 if ((ochatr & CHANUSER_MASTER) && !(chflags & CHANUSER_MASTER)) {
	    if (!(get_attr_handle(hand) & USER_MASTER))
	       dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS);
	    dprintf(i, "*** POOF! ***\n");
	    dprintf(i, "You are no longer a master on %s.\n", chname);
	 }
	 if (!(ochatr & CHANUSER_MASTER) && (chflags & CHANUSER_MASTER)) {
	    dcc[i].u.chat->con_flags |= conmask;
	    if (!(get_attr_handle(hand) & USER_MASTER))
	       dcc[i].u.chat->con_flags &= ~(LOG_LEV1 | LOG_LEV2 | LOG_LEV3 | LOG_LEV4 |
			      LOG_LEV5 | LOG_LEV6 | LOG_LEV7 | LOG_LEV8 |
			     LOG_RAW | LOG_DEBUG | LOG_WALL | LOG_FILES);
	    dprintf(i, "*** POOF! ***\n");
	    dprintf(i, "You are now a master on %s.\n", chname);
	 }
	 if (!(ochatr & CHANUSER_OWNER) && (chflags & CHANUSER_OWNER)) {
	    dprintf(i, "@@@ POOF! @@@\n");
	    dprintf(i, "You are now an OWNER of %s.\n", chname);
	 }
	 if ((ochatr & CHANUSER_OWNER) && !(chflags & CHANUSER_OWNER)) {
	    dprintf(i, "@@@ POOF! @@@\n");
	    dprintf(i, "You are no longer an owner of %s.\n", chname);
	 }
	 if (((ochatr & (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER)) &&
	      (!(chflags & (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER)))) ||
	 ((chflags & (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER)) &&
	  (!(ochatr & (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER))))) {
	    while (chan != NULL && found == 0) {
	       chatr = get_chanattr_handle(dcc[i].nick, chan->name);
	       if (chatr & (CHANUSER_OP | CHANUSER_MASTER | CHANUSER_OWNER))
		  found = 1;
	       else
		  chan = chan->next;
	    }
	    if (chan == NULL)
	       chan = chanset;
	    strcpy(dcc[i].u.chat->con_chan, chan->name);
	 }
      }
   }
   return chflags;
}


/* sanity check on channel attributes */
int chan_sanity_check(int chatr, int atr)
{
   if ((chatr & CHANUSER_OP) && (chatr & CHANUSER_DEOP))
      chatr &= ~(CHANUSER_OP | CHANUSER_DEOP);
   /* can't be channel owner without also being channel master */
   if (chatr & CHANUSER_OWNER)
      chatr |= CHANUSER_MASTER;
   /* master implies friend & op */
   if (chatr & CHANUSER_MASTER)
     chatr |= CHANUSER_OP|CHANUSER_FRIEND;
   if (!(atr & USER_BOT))
     chatr &= ~CHANBOT_SHARE;
   return chatr;
}

static void cmd_chattr (int idx, char * par)
{
   char hand[512], s[21], chg[512];
   struct chanset_t *chan;
   int i, pos, f, atr, oatr, chatr = 0, ochatr = 0, own, chown = 0;
   if (!par[0]) {
      dprintf(idx, "Usage: chattr <handle> [changes [channel]]\n");
      return;
   }
   nsplit(hand, par);
   if ((hand[0] == '*') || (!is_user(hand))) {
      dprintf(idx, "No such user!\n");
      return;
   }
   chg[0] = 0;
   own = (get_attr_handle(dcc[idx].nick) & USER_OWNER);
   oatr = atr = get_attr_handle(hand);
   pos = 1;
   if (par[0]) {
      /* make changes */
      nsplit(chg, par);
      if (par[0]) {
	 if ((!defined_channel(par)) && (par[0] != '*')) {
	    dprintf(idx, "No channel record for %s\n", par);
	    return;
	 }
      }
      if (!par[0] && 
               (!(get_attr_handle(dcc[idx].nick) & 
                           (USER_MASTER | USER_BOTMAST | USER_OWNER)))
               && (!(get_chanattr_handle(dcc[idx].nick, par) & 
                           (CHANUSER_MASTER|CHANUSER_OWNER)))) {
	 dprintf(idx, "You do not have Bot Master privileges.\n");
	 return;
      }
      if (!(get_chanattr_handle(dcc[idx].nick, par) & 
                           (CHANUSER_MASTER|CHANUSER_OWNER)) &&
               !(get_attr_handle(dcc[idx].nick) & 
                           (USER_MASTER | USER_BOTMAST | USER_OWNER))) {
	 dprintf(idx, 
           "You do not have channel master privileges for channel %s\n", 
           par);
	 return;
      }
      if (par[0]) {
	 if (par[0] == '*') {
	    chown = 0;
	    ochatr = chatr = get_chanattr_handle(hand, chanset->name);
	 } else {
	    chown = (get_chanattr_handle(dcc[idx].nick, par) & CHANUSER_OWNER);
	    ochatr = chatr = get_chanattr_handle(hand, par);
	 }
      }
      for (i = 0; i < strlen(chg); i++) {
	 if (chg[i] == '+')
	    pos = 1;
	 else if (chg[i] == '-')
	    pos = 0;
	 else {
	    s[1] = 0;
	    s[0] = chg[i];
	    if (par[0]) {
	       /* channel-specific */
	       f = str2chflags(s);
	       if (!own && !chown)
		  f &= ~(CHANUSER_MASTER | CHANUSER_OWNER);
	       /* only owners and channel owners can +/- channel masters 
                  and channel owners */
	       if (!own ) 
		  f &= ~(CHANBOT_SHARE);
	       change_chanflags(userlist, hand, par, (pos ? f : 0), 
                                      (pos ? 0 : f));
	       /* FIXME: other stuff here? */
	    } else {
	       f = str2flags(s);
	       /* nobody can modify the +b bot flag */
	       f &= ~USER_BOT;
	       if (!own)
		  f &= (~(USER_OWNER | USER_MASTER | USER_BOTMAST | BOT_SHARE));
	       /* only owner can do +/- owner,master,share,botnet master */
	       atr = pos ? (atr | f) : (atr & ~f);
	    }
	 }
      }
      if (!par[0]) {
	 noshare = 1;
	 atr = sanity_check(atr);
	 atr = check_dcc_attrs(hand, atr, oatr);
	 noshare = 0;
	 set_attr_handle(hand, atr);
      }
   }
   if (par[0])
      putlog(LOG_CMDS, "*", "#%s# chattr %s %s %s", 
                                      dcc[idx].nick, hand, chg, par);
   else if (chg[0])
      putlog(LOG_CMDS, "*", "#%s# chattr %s %s", dcc[idx].nick, hand, chg);
   else
      putlog(LOG_CMDS, "*", "#%s# chattr %s", dcc[idx].nick, hand);

   /* get current flags and display them */

   if (!par[0]) {
      flags2str(atr, s);
      dprintf(idx, "Flags for %s are now +%s\n", hand, s);
      chan = chanset;
      while (chan != NULL) {
	 recheck_channel(chan);
	 chan = chan->next;
      }
   } else if (par[0] == '*') {
      /* every channel */
      chan = chanset;
      while (chan != NULL) {
	 chatr = get_chanattr_handle(hand, chan->name);
	 chatr = chan_sanity_check(chatr,atr);
	 if (chan == chanset)
	    chatr = check_dcc_chanattrs(hand, chan->name, chatr, ochatr);
	 chflags2str(chatr, s);
	 dprintf(idx, "Flags for %s are now +%s %s\n", hand, s, chan->name);
	 set_chanattr_handle(hand, chan->name, chatr);
	 recheck_channel(chan);
	 chan = chan->next;
      }
   } else {
      chatr = get_chanattr_handle(hand, par);
      chatr = chan_sanity_check(chatr,atr);
      chatr = check_dcc_chanattrs(hand, par, chatr, ochatr);
      chflags2str(chatr, s);
      dprintf(idx, "Flags for %s are now +%s %s\n", hand, s, par);
      set_chanattr_handle(hand, par, chatr);
      recheck_channel(findchan(par));
   }
}

static void cmd_notes (int idx, char * par)
{
   char fcn[512];
   if (!par[0]) {
      dprintf(idx, "Usage: notes index\n");
      dprintf(idx, "       notes read <# or ALL>\n");
      dprintf(idx, "       notes erase <# or ALL>\n");
      return;
   }
   nsplit(fcn, par);
   if (strcasecmp(fcn, "index") == 0)
      notes_read(dcc[idx].nick, "", -1, idx);
   else if (strcasecmp(fcn, "read") == 0) {
      if (strcasecmp(par, "all") == 0)
	 notes_read(dcc[idx].nick, "", 0, idx);
      else
	 notes_read(dcc[idx].nick, "", atoi(par), idx);
   } else if (strcasecmp(fcn, "erase") == 0) {
      if (strcasecmp(par, "all") == 0)
	 notes_del(dcc[idx].nick, "", 0, idx);
      else
	 notes_del(dcc[idx].nick, "", atoi(par), idx);
   } else
      dprintf(idx, "Function must be one of INDEX, READ, or ERASE.\n");
   putlog(LOG_CMDS, "*", "#%s# notes %s %s", dcc[idx].nick, fcn, par);
}

static void cmd_chat (int idx, char * par)
{
   int newchan, oldchan;
   if (strcasecmp(par, "off") == 0) {
      /* turn chat off */
      if (dcc[idx].u.chat->channel < 0)
	 dprintf(idx, "You weren't in chat anyway!\n");
      else {
	 dprintf(idx, "Leaving chat mode...\n");
	 check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock);
	 chanout2(dcc[idx].u.chat->channel, "%s left the party line.\n",
		  dcc[idx].nick);
	 context;
	 if (dcc[idx].u.chat->channel < 100000)
	    tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
      }
      dcc[idx].u.chat->channel = (-1);
   } else {
      if (par[0] == '*') {
	 if (((par[1] < '0') || (par[1] > '9'))) {
	    if (par[1] == 0)
	       newchan = 0;
	    else
	       newchan = get_assoc(par + 1);
	    if (newchan < 0) {
	       dprintf(idx, "No channel by the name.\n");
	       return;
	    }
	 } else
	    newchan = 100000 + atoi(par + 1);
	 if (newchan < 100000 || newchan > 199999) {
	    dprintf(idx, "Channel # out of range: local channels must be *0-*99999\n");
	    return;
	 }
      } else {
	 if (((par[0] < '0') || (par[0] > '9')) && (par[0])) {
	    if (strcasecmp(par, "on") == 0)
	       newchan = 0;
	    else
	       newchan = get_assoc(par);
	    if (newchan < 0) {
	       dprintf(idx, "No channel by that name.\n");
	       return;
	    }
	 } else
	    newchan = atoi(par);
	 if ((newchan < 0) || (newchan > 99999)) {
	    dprintf(idx, "Channel # out of range: must be between 0 and 99999.\n");
	    return;
	 }
      }
      /* if coming back from being off the party line, make sure they're
         not away */
      if ((dcc[idx].u.chat->channel < 0) && (dcc[idx].u.chat->away != NULL))
	 not_away(idx);
      if (dcc[idx].u.chat->channel == newchan) {
	 if (newchan == 0)
	    dprintf(idx, "You're already on the party line!\n");
	 else
	    dprintf(idx, "You're already on channel %s%d!\n",
		    (newchan < 100000) ? "" : "*", newchan % 100000);
      } else {
	 oldchan = dcc[idx].u.chat->channel;
	 if (oldchan >= 0)
	    check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock);
	 if (oldchan == 0) {
	    chanout2(0, "%s left the party line.\n", dcc[idx].nick);
	    context;
	 } else if (oldchan > 0) {
	    chanout2(oldchan, "%s left the channel.\n", dcc[idx].nick);
	    context;
	 }
	 dcc[idx].u.chat->channel = newchan;
	 if (newchan == 0) {
	    dprintf(idx, "Entering the party line...\n");
	    chanout2(0, "%s joined the party line.\n", dcc[idx].nick);
	    context;
	 } else {
	    dprintf(idx, "Joining channel '%s'...\n", par);
	    chanout2(newchan, "%s joined the channel.\n", dcc[idx].nick);
	    context;
	 }
	 check_tcl_chjn(botnetnick, dcc[idx].nick, newchan, geticon(idx),
			dcc[idx].sock, dcc[idx].host);
	 if (newchan < 100000)
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick, newchan,
		    geticon(idx), dcc[idx].sock, dcc[idx].host);
	 else if (oldchan < 100000)
	    tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
      }
   }
}

static void cmd_echo (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Echo is currently %s.\n", dcc[idx].u.chat->status & STAT_ECHO ?
	      "on" : "off");
      return;
   }
   if (strcasecmp(par, "on") == 0) {
      dprintf(idx, "Echo turned on.\n");
      dcc[idx].u.chat->status |= STAT_ECHO;
      return;
   }
   if (strcasecmp(par, "off") == 0) {
      dprintf(idx, "Echo turned off.\n");
      dcc[idx].u.chat->status &= ~STAT_ECHO;
      return;
   }
   dprintf(idx, "Usage: echo <on/off>\n");
}

static void cmd_fries (int idx, char * par)
{
   dprintf(idx, "* %s juliennes some fries for you.\n", botnetnick);
   dprintf(idx, "Enjoy!\n");
}

static void cmd_beer (int idx, char * par)
{
   dprintf(idx, "* %s throws you a cold brew.\n", botnetnick);
   dprintf(idx, "It's Miller time. :)\n");
}

/* hell, everyone else does it, here's mine :) */
static void cmd_coke (int idx, char * par)
{
   dprintf(idx, "* %s opens the fridge, draws out an ice cold bottle of coke.\n", botnetnick);
   dprintf(idx, "Time to Chill, with the Real Thing :)\n");
}

int stripmodes (char * s)
{
   int i;
   int res = 0;
   for (i = 0; i < strlen(s); i++)
      switch (tolower(s[i])) {
      case 'b':
	 res |= STRIP_BOLD;
	 break;
      case 'c':
	 res |= STRIP_COLOR;
	 break;
      case 'r':
	 res |= STRIP_REV;
	 break;
      case 'u':
	 res |= STRIP_UNDER;
	 break;
      case 'a':
	 res |= STRIP_ANSI;
	 break;
      case '*':
	 res |= STRIP_ALL;
	 break;
      }
   return res;
}

char *stripmasktype(int x)
{
   static char s[20];
   char *p = s;
   if (x & STRIP_BOLD)
      *p++ = 'b';
   if (x & STRIP_COLOR)
      *p++ = 'c';
   if (x & STRIP_REV)
      *p++ = 'r';
   if (x & STRIP_UNDER)
      *p++ = 'u';
   if (x & STRIP_ANSI)
      *p++ = 'a';
   *p = 0;
   return s;
}

static char *stripmaskname(int x)
{
   static char s[161];
   s[0] = 0;
   if (x & STRIP_BOLD)
      strcat(s, "bold, ");
   if (x & STRIP_COLOR)
      strcat(s, "color, ");
   if (x & STRIP_REV)
      strcat(s, "reverse, ");
   if (x & STRIP_UNDER)
      strcat(s, "underline, ");
   if (x & STRIP_ANSI)
      strcat(s, "ansi, ");
   if (!s[0])
      strcpy(s, "none, ");
   s[strlen(s) - 2] = 0;
   return s;
}

static void cmd_strip (int idx, char * par)
{
   char nick[512], s[2], s1[512];
   int dest = 0, i, pls, md, ok = 0, atr;
   if (!par[0]) {
      dprintf(idx, "Your current strip settings are: %s (%s)\n",
	      stripmasktype(dcc[idx].u.chat->strip_flags),
	      stripmaskname(dcc[idx].u.chat->strip_flags));
      return;
   }
   strcpy(s1, par);
   split(nick, par);
   atr = get_attr_handle(dcc[idx].nick);
   if ((nick[0]) && (nick[0] != '+') && (nick[0] != '-') &&
       (atr & USER_MASTER)) {
      for (i = 0; i < dcc_total; i++)
	 if ((strcasecmp(nick, dcc[i].nick) == 0) && (dcc[i].type == &DCC_CHAT) &&
	     (!ok)) {
	    ok = 1;
	    dest = i;
	 }
      if (!ok) {
	 dprintf(idx, "No such user on the party line!\n");
	 return;
      }
      nick[0] = 0;
   } else
      dest = idx;
   if (!nick[0])
      nsplit(nick, par);
   while (nick[0]) {
      pls = 1;
      if ((nick[0] != '+') && (nick[0] != '-'))
	 dcc[dest].u.chat->strip_flags = 0;
      for (i = 0; i < strlen(nick); i++) {
	 if (nick[i] == '+')
	    pls = 1;
	 else if (nick[i] == '-')
	    pls = (-1);
	 else {
	    s[0] = nick[i];
	    s[1] = 0;
	    md = stripmodes(s);
	    if (pls == 1)
	       dcc[dest].u.chat->strip_flags |= md;
	    else
	       dcc[dest].u.chat->strip_flags &= ~md;
	 }
      }
      nsplit(nick, par);
   }
   putlog(LOG_CMDS, "*", "#%s# strip %s", dcc[idx].nick, s1);
   if (dest == idx) {
      dprintf(idx, "Your strip settings are: %s (%s)\n",
	      stripmasktype(dcc[idx].u.chat->strip_flags),
	      stripmaskname(dcc[idx].u.chat->strip_flags));
   } else {
      dprintf(idx, "Strip setting for %s: %s (%s)\n", dcc[dest].nick,
	      stripmasktype(dcc[dest].u.chat->strip_flags),
	      stripmaskname(dcc[dest].u.chat->strip_flags));
      dprintf(dest, "%s set your strip settings to: %s (%s)\n", dcc[idx].nick,
	      stripmasktype(dcc[dest].u.chat->strip_flags),
	      stripmaskname(dcc[dest].u.chat->strip_flags));
   }
}

static void cmd_su (int idx, char * par)
{
   int atr = get_attr_handle(dcc[idx].nick);
   if (strlen(par) == 0) {
      dprintf(idx, "Usage: su <user>\n");
      return;
   }
   if (!is_user(par)) {
      dprintf(idx, "No such user.\n");
      return;
   }
   if (get_attr_handle(par) & USER_BOT) {
      dprintf(idx, "Can't su to a bot... then again, why would you wanna?\n");
      return;
   }
   if (pass_match_by_handle("-", par)) {
      dprintf(idx, "No password set for user. You may not .su to them\n");
      return;
   }
   correct_handle(par);
   putlog(LOG_CMDS, "*", "#%s# su %s", dcc[idx].nick, par);
   if (!(atr & USER_OWNER) || (get_attr_handle(par) & USER_OWNER)) {
      if (dcc[idx].u.chat->channel < 100000) {
	 tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
      }
      chanout2(dcc[idx].u.chat->channel, "%s left the party line.\n",
	       dcc[idx].nick);
      context;
      /* store the old nick in the away section, for weenies who can't get
       * their password right ;) */
      if (dcc[idx].u.chat->away != NULL)
	 nfree(dcc[idx].u.chat->away);
      dcc[idx].u.chat->away = n_malloc(strlen(dcc[idx].nick) + 1, "dccutil.c", 9999);
      strcpy(dcc[idx].u.chat->away, dcc[idx].nick);
      strcpy(dcc[idx].nick, par);
      dprintf(idx, "Enter password for %s\n", par);
      dcc[idx].type = &DCC_CHAT_PASS;
      return;
   }
   if (atr & USER_OWNER) {
      if (dcc[idx].u.chat->channel < 100000) {
	 tandout("part %s %s %d\n", botnetnick, dcc[idx].nick, dcc[idx].sock);
      }
      chanout2(dcc[idx].u.chat->channel, "%s left the party line.\n", dcc[idx].nick);
      context;
      dprintf(idx, "Setting your username to %s.\n", par);
      strcpy(dcc[idx].nick, par);
      if (dcc[idx].u.chat->channel < 100000) {
	 tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
		 dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock,
		 dcc[idx].host);
      }
      chanout2(dcc[idx].u.chat->channel, "%s has joined the party line.\n", dcc[idx].nick);
      context;
      return;
   }
}

static void cmd_fixcodes (int idx, char * par)
{
   if (dcc[idx].u.chat->status & STAT_ECHO) {
      dcc[idx].u.chat->status = STAT_TELNET;
      dprintf(idx, "Turned on telnet codes\n");
      putlog(LOG_CMDS, "*", "#%s# fixcodes (telnet on)", dcc[idx].nick);
      return;
   }
   if (dcc[idx].u.chat->status & STAT_TELNET) {
      dcc[idx].u.chat->status = STAT_ECHO;
      dprintf(idx, "Turned off telnet codes\n");
      putlog(LOG_CMDS, "*", "#%s# fixcodes (telnet off)", dcc[idx].nick);
      return;
   }
}

static void cmd_page (int idx, char * par)
{
   int a;
   if (!par[0]) {
      if (dcc[idx].u.chat->status & STAT_PAGE) {
	 dprintf(idx, "Currently paging outputs to %d lines.\n",
		 dcc[idx].u.chat->max_line);
      } else
	 dprintf(idx, "You dont have paging on.\n");
      return;
   }
   a = atoi(par);
   if (strcasecmp(par, "off") == 0 || (a == 0 && par[0] == 0)) {
      dcc[idx].u.chat->status &= ~STAT_PAGE;
      dcc[idx].u.chat->max_line = 1;	/* flush_lines needs this */
      while (dcc[idx].u.chat->buffer != NULL)
	 flush_lines(idx);
      dprintf(idx, "Paging turned off.\n");
      putlog(LOG_CMDS, "*", "#%s# page off", dcc[idx].nick);
      return;
   }
   if (a > 0) {
      dprintf(idx, "Paging turned on, stopping every %d lines.\n", a);
      dcc[idx].u.chat->status |= STAT_PAGE;
      dcc[idx].u.chat->max_line = a;
      dcc[idx].u.chat->line_count = 0;
      dcc[idx].u.chat->current_lines = 0;
      putlog(LOG_CMDS, "*", "#%s# page %d", dcc[idx].nick, a);
      return;
   }
   dprintf(idx, "Usage: page <off or #>\n");
}

#ifdef EBUG
static int check_cmd (int idx, char * msg)
{
   char s[512];
   context;
   stridx(s, msg, 1);
   if (strcasecmp(s, "chpass") == 0) {
      stridx(s, msg, 3);
      if (s[0])
	 return 1;
   }
   return 0;
}
#endif

/* evaluate a Tcl command, send output to a dcc user */
static void cmd_tcl (int idx, char * msg)
{
   int code;
#ifdef EBUG
   int i = 0;
   char s[512];
   context;
   if (msg[0])
      i = check_cmd(idx, msg);
   if (i) {
      stridx(s, msg, 2);
      if (s[0])
	 debug1("tcl: evaluate (.tcl): chpass %s [something]", s);
      else
	 debug1("tcl: evaluate (.tcl): %s", msg);
   } else
      debug1("tcl: evaluate (.tcl): %s", msg);
#endif
   context;
   set_tcl_vars();
   context;
   code = Tcl_GlobalEval(interp, msg);
   context;
   if (code == TCL_OK)
      dumplots(idx, "Tcl: ", interp->result);
   else
      dumplots(idx, "TCL error: ", interp->result);
   context;
   /* refresh internal vars */
}

/* perform a 'set' command */
static void cmd_set (int idx, char * msg)
{
   int code;
   char s[512];
   putlog(LOG_CMDS, "*", "#%s# set %s", dcc[idx].nick, msg);
   set_tcl_vars();
   strcpy(s, "set ");
   strcat(s, msg);
   if (!msg[0]) {
      strcpy(s, "info globals");
      Tcl_Eval(interp, s);
      dumplots(idx, "global vars: ", interp->result);
      return;
   }
   code = Tcl_Eval(interp, s);
   if (code == TCL_OK) {
      if (strchr(msg, ' ') == NULL)
	 dumplots(idx, "currently: ", interp->result);
      else
	 dprintf(idx, "Ok, set.\n");
   } else
      dprintf(idx, "Error: %s\n", interp->result);
}

static void cmd_modulestat (int idx, char * par)
{
   context;
   putlog(LOG_CMDS, "*", "#%s# modulestat", dcc[idx].nick);
   if (par && par[0]) {
      module_entry * m = module_find(par,0,0);
      if (!m) {
	 dprintf(idx,"%s.\n", MOD_NOSUCH);
      } else if (!m->funcs || !m->funcs[MODCALL_REPORT]){
	 dprintf(idx,"%s %s.", MOD_NOINFO, par);
      } else {
	 m->funcs[MODCALL_REPORT](idx);
      }
   }
   do_module_report(idx);
}

static void cmd_loadmodule (int idx, char * par)
{
   const char *p;

   context;
   if (!par[0]) {
      dprintf(idx, "%s: loadmodule <module>\n", USAGE);
   } else {
      p = module_load(par);
      if (p != NULL)
	 dprintf(idx, "%s: %s %s\n", par, MOD_LOADERROR, p);
      else {
	 putlog(LOG_CMDS, "*", "#%s# loadmodule %s", dcc[idx].nick, par);
	 dprintf(idx, "%s %s\n", MOD_LOADED, par);
      }
   }
   context;
}

static void cmd_unloadmodule (int idx, char * par)
{
   char *p;

   context;
   if (!par[0]) {
      dprintf(idx, "%s: unloadmodule <module>\n", USAGE);
   } else {
      p = module_unload(par,dcc[idx].nick);
      if (p != NULL)
	 dprintf(idx, "%s %s: %s\n", MOD_UNLOADERROR, par, p);
      else {
	 putlog(LOG_CMDS, "*", "#%s# unloadmodule %s", dcc[idx].nick, par);
	 dprintf(idx, "%s %s\n", MOD_UNLOADED, par);
      }
   }
}

/* DCC CHAT COMMANDS */
/* function call should be:
   int cmd_whatever(idx,"parameters");
   as with msg commands, function is responsible for any logging
*/
cmd_t C_dcc[]={
  { "+bot", "B", (Function)cmd_pls_bot, NULL },
  { "addlog", "Bo|o", (Function)cmd_addlog, NULL },
  { "away", "", (Function)cmd_away, NULL },
  { "banner", "m", (Function)cmd_banner, NULL },
  { "beer", "", (Function)cmd_beer, NULL },
  { "binds", "m", (Function)cmd_binds, NULL },
  { "boot", "B", (Function)cmd_boot, NULL },
  { "botinfo", "", (Function)cmd_botinfo, NULL },
  { "bots", "", (Function)cmd_bots, NULL },
  { "bottree", "", (Function)cmd_bottree, NULL },
  { "chaddr", "B", (Function)cmd_chaddr, NULL },
  { "chat", "", (Function)cmd_chat, NULL },
  { "chattr", "Bm|m", (Function)cmd_chattr, NULL },
  { "chemail", "m", (Function)cmd_chemail, NULL },
  { "chnick", "B", (Function)cmd_chnick, NULL },
  { "chpass", "B", (Function)cmd_chpass, NULL },
  { "coke", "", (Function)cmd_coke, NULL },
  { "comment", "m", (Function)cmd_comment, NULL },
  { "console", "o|o", (Function)cmd_console, NULL },
  { "dccstat", "B", (Function)cmd_dccstat, NULL },
  { "debug", "m", (Function)cmd_debug, NULL },
  { "die", "n", (Function)cmd_die, NULL },
  { "echo", "", (Function)cmd_echo, NULL },
  { "email", "", (Function)cmd_email, NULL },
  { "fixcodes", "", (Function)cmd_fixcodes, NULL },
  { "fries", "", (Function)cmd_fries, NULL },
  { "help", "", (Function)cmd_help, NULL },
  { "link", "B", (Function)cmd_link, NULL },
  { "loadmodule", "n", (Function)cmd_loadmodule, NULL },
  { "match", "o|o", (Function)cmd_match, NULL },
  { "me", "", (Function)cmd_me, NULL },
  { "modulestat", "m", (Function)cmd_modulestat, NULL },
  { "motd", "", (Function)cmd_motd, NULL },
  { "newpass", "", (Function)cmd_newpass, NULL },
  { "nick", "", (Function)cmd_nick, NULL },
  { "note", "", (Function)cmd_note, NULL },
  { "notes", "", (Function)cmd_notes, NULL },
  { "page", "", (Function)cmd_page, NULL },
  { "quit", "", (Function)0, NULL },
  { "rehash", "m", (Function)cmd_rehash, NULL },
  { "relay", "B", (Function)cmd_relay, NULL },
  { "reload", "m|m", (Function)cmd_reload, NULL },
  { "restart", "m", (Function)cmd_restart, NULL },
  { "save", "m|m", (Function)cmd_save, NULL },
  { "set", "n", (Function)cmd_set, NULL },
  { "simul", "n", (Function)cmd_simul, NULL },
  { "status", "m|m", (Function)cmd_status, NULL },
  { "strip", "", (Function)cmd_strip, NULL },
  { "su", "", (Function)cmd_su, NULL },
  { "tcl", "n", (Function)cmd_tcl, NULL },
  { "trace", "", (Function)cmd_trace, NULL },
  { "unlink", "B", (Function)cmd_unlink, NULL },
  { "unloadmodule", "n", (Function)cmd_unloadmodule, NULL },
  { "vbottree", "", (Function)cmd_vbottree, NULL },
  { "who", "", (Function)cmd_who, NULL },
  { "whois", "o|o", (Function)cmd_whois, NULL },
  { "whom", "", (Function)cmd_whom, NULL },
  { 0, 0, 0, 0 }
};

