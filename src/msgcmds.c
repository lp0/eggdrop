/*
   msgcmds.c -- handles:
   all commands entered via /MSG

   dprintf'ized, 4feb96
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

/* let unknown users greet us and become known */
int learn_users = 0;
/* new server? */
char newserver[121];
/* new server port? */
int newserverport = 0;
/* new server password? */
char newserverpass[121];
/* enable the info subsystem? */
int use_info = 1;
/* person to send a note to for new users */
char notify_new[121] = "";
/* default user flags for people who say 'hello' */
int default_flags = 0;
extern int quiet_reject;

/* non-irc?  need none of this file then */
#ifndef NO_IRC

extern int serv;
extern char botname[];
extern int use_info;
extern char origbotname[];
extern char botnetnick[];
extern char helpdir[];
extern int make_userfile;
extern char notefile[];
extern int dcc_total;
extern struct dcc_t * dcc;
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern int default_port;

/* msg to xx everyone on the channel's info */
static void show_all_info (char * chname, char * who)
{
   memberlist *m;
   char s[UHOSTLEN], nick[NICKLEN], also[512], info[120];
   int atr;
   struct chanset_t *chan;
   also[0] = 0;
   chan = findchan(chname);
   if (chan == NULL) {
      hprintf(serv, "NOTICE %s :I'm not on channel %s\n", who, chname);
      return;
   }
   m = chan->channel.member;
   while (m->nick[0]) {
      sprintf(s, "%s!%s", m->nick, m->userhost);
      get_handle_by_host(nick, s);
      get_handle_info(nick, info);
      atr = get_attr_handle(nick);
      if (atr & USER_BOT)
	 info[0] = 0;
      if (info[0] == '@')
	 strcpy(info, &info[1]);
      else {
	 get_handle_chaninfo(nick, chname, s);
	 if (s[0] == '@')
	    strcpy(s, &s[1]);
	 if (s[0])
	    strcpy(info, s);
      }
      if (info[0])
	 hprintf(serv, "NOTICE %s :[%9s] %s\n", who, m->nick, info);
      else {
	 if (strcasecmp(m->nick, botname) == 0)
	    hprintf(serv, "NOTICE %s :[%9s] <-- I'm the bot, of course.\n",
		    who, m->nick);
	 else if (atr & USER_BOT) {
	    if (atr & BOT_SHARE)
	       hprintf(serv, "NOTICE %s :[%9s] <-- a twin of me\n", who, m->nick);
	    else
	       hprintf(serv, "NOTICE %s :[%9s] <-- another bot\n", who, m->nick);
	 } else {
	    strcat(also, m->nick);
	    strcat(also, ", ");
	 }
      }
      m = m->next;
   }
   if (also[0]) {
      also[strlen(also) - 2] = 0;
      hprintf(serv, "NOTICE %s :No info: %s\n", who, also);
   }
}

static int msg_hello (char * nick, char * h, char * n, char * p)
{
   char host[161], s[161], s1[161];
   char *p1;
   int common = 0;
   int atr;
   struct chanset_t * chan;
   
   if ((!learn_users) && (!make_userfile))
      return 0;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   atr = get_attr_handle(n);
   if ((n[0] != '*') && !(atr & USER_COMMON)) {
      mprintf(serv, "NOTICE %s :%s, %s.\n", IRC_HI, nick, n);
      return 1;
   }
   if (is_user(nick)) {
      struct flag_record fr = { 0, 0, 0 };
      fr.global = atr;
      showtext(nick, "badhost", &fr);
      return 1;
   }
   sprintf (s, "%s!%s", nick, h);
   if (match_ban(s)) {
      hprintf(serv, "NOTICE %s :%s.\n", nick, IRC_BANNED2);
      return 1;
   }
   if (strlen(nick) > 9) {
      /* crazy dalnet */
      hprintf(serv, "NOTICE %s :%s.\n", nick, IRC_NICKTOOLONG);
      return 1;
   }
   if (atr & USER_COMMON) {
      maskhost(s, host);
      strcpy(s, host);
      sprintf(host, "%s!%s", nick, &s[2]);
      userlist = adduser(userlist, nick, host, "-", default_flags);
      putlog(LOG_MISC, "*", "%s %s (%s) -- %s", 
			IRC_INTRODUCED, nick, host, IRC_COMMONSITE);
      common = 1;
   } else {
      maskhost(s, host);
      if (make_userfile)
	 userlist = adduser(userlist, nick, host, "-", default_flags | USER_MASTER |
			    USER_OWNER);
      else
	 userlist = adduser(userlist, nick, host, "-", default_flags);
      putlog(LOG_MISC, "*", "%s %s (%s)", IRC_INTRODUCED, nick, host);
   }
   for (chan = chanset; chan; chan=chan->next) {
      if (ismember(chan,nick))
	add_chanrec_by_handle(userlist,nick,chan->name,0,0);
   }
   hprintf(serv, IRC_SALUT1, IRC_SALUT1_ARGS);
   hprintf(serv, IRC_SALUT2, IRC_SALUT2_ARGS);
   if (common) {
      hprintf(serv, "NOTICE %s :%s", nick, IRC_SALUT2A);
      hprintf(serv, "NOTICE %s :%s", nick, IRC_SALUT2B);
   }
   if (make_userfile) {
      struct flag_record fr = { default_flags | USER_MASTER | USER_OWNER, 0, 0 };

      hprintf(serv, "NOTICE %s :%s\n", nick, IRC_INITOWNER1);
      showtext(nick, "newbot", &fr);
      putlog(LOG_MISC, "*", IRC_INIT1, IRC_INIT1_ARGS);
      make_userfile = 0;
      write_userfile();
      add_note(nick, origbotname, IRC_INITNOTE, -1, 0);
   } else {
      struct flag_record fr = { default_flags, 0, 0 };
      showtext(nick, "intro", &fr);
   }
   if (notify_new[0]) {
      sprintf(s, IRC_INITINTRO, nick, host);
      strcpy(s1, notify_new);
      while (s1[0]) {
	 p1 = strchr(s1, ',');
	 if (p1 != NULL) {
	    *p1 = 0;
	    p1++;
	    rmspace(p1);
	 }
	 rmspace(s1);
	 add_note(s1, origbotname, s, -1, 0);
	 if (p1 == NULL)
	    s1[0] = 0;
	 else
	    strcpy(s1, p1);
      }
   }
   return 1;
}

static int msg_pass (char * nick, char * host, char * hand, char * par)
{
   char old[512], new[512];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (strcmp(hand, "*") == 0)
      return 1;
   if (get_attr_handle(hand) & (USER_BOT | USER_COMMON))
      return 1;
   split(old, par);
   if (!par[0]) {
      mprintf(serv, "NOTICE %s :%s\n", nick,
	      pass_match_by_handle("-", hand) ? IRC_NOPASS : IRC_PASS);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS?", nick, host, hand);
      return 1;
   }
   if ((!pass_match_by_handle("-", hand)) && (!old[0])) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_EXISTPASS);
      return 1;
   }
   if (!old[0]) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS...", nick, host, hand);
      nsplit(new, par);
      if (strlen(new) > 15)
	 new[15] = 0;
      if (strlen(new) < 4) {
	 mprintf(serv, "NOTICE %s :%s.\n", nick, IRC_PASSFORMAT);
	 return 0;
      }
      change_pass_by_handle(hand, new);
      mprintf(serv, "NOTICE %s :%s '%s'\n", nick, IRC_SETPASS, new);
      return 1;
   }
   if (!pass_match_by_handle(old, hand)) {
      mprintf(serv, "NOTICE %s :%s\n", nick);
      return 1;
   }
   nsplit(new, par);
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS...", nick, host, hand);
   if (strlen(new) > 15)
      new[15] = 0;
   if (strlen(new) < 4) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_PASSFORMAT);
      return 0;
   }
   change_pass_by_handle(hand, new);
   mprintf(serv, "NOTICE %s :%s '%s'.\n", nick, IRC_CHANGEPASS, new);
   return 1;
}

static int msg_ident (char * nick, char * host, char * hand, char * par)
{
   char s[121], s1[121], pass[512], who[NICKLEN];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (get_attr_handle(hand) & USER_BOT)
      return 1;
   if (get_attr_handle(hand) & USER_COMMON) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_FAILCOMMON);
      return 1;
   }
   nsplit(pass, par);
   if (!is_user(nick)) {
      if (strcmp(hand, "*") != 0) {
	 mprintf(serv, IRC_MISIDENT, IRC_MISIDENT_ARGS);
	 return 1;
      }
      if ((!par[0]) || (!is_user(par)))
	 return 1;		/* dunno you */
   }
   strncpy(who, par, NICKLEN);
   who[NICKLEN - 1] = 0;
   if (!par[0])
      strcpy(who, nick);
   /* This could be used as detection... */
   if (strcasecmp(who, origbotname) == 0)
      return 1;
   if (pass_match_by_handle("-", who)) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_NOPASS);
      return 1;
   }
   if (!pass_match_by_handle(pass, who)) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_DENYACCESS);
      return 1;
   }
   if (strcasecmp(hand, who) == 0) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_RECOGNIZED);
      return 1;
   }
   if (strcmp(hand, "*") != 0) {
      mprintf(serv, IRC_MISIDENT, IRC_MISIDENT_ARGS2);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! IDENT %s", nick, host, hand, who);
   sprintf(s, "%s!%s", nick, host);
   maskhost(s, s1);
   hprintf(serv, "NOTICE %s :%s: %s\n", nick, IRC_ADDHOSTMASK, s1);
   addhost_by_handle(who, s1);
   recheck_ops(nick, who);
   return 1;
}

static int msg_email (char * nick, char * host, char * hand, char * par)
{
   char s[161];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (strcasecmp(hand, "*") == 0)
      return 0;
   if (get_attr_handle(hand) & USER_COMMON)
      return 1;
   if (par[0]) {
      get_handle_email(hand, s);
      if (strcasecmp(par, "none") == 0) {
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! EMAIL NONE", nick, host, hand);
	 par[0] = 0;
	 set_handle_email(userlist, hand, par);
	 mprintf(serv, "NOTICE %s :%s\n", nick, IRC_DELMAILADDR);
      } else {
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! EMAIL...", nick, host, hand);
	 set_handle_email(userlist, hand, par);
	 mprintf(serv, "NOTICE %s :%s %s\n", nick, IRC_FIELDCHANGED, par);
      }
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! EMAIL?", nick, host, hand);
   get_handle_email(hand, s);
   if (s[0]) {
      mprintf(serv, "NOTICE %s :%s %s\n", nick, IRC_FIELDCURRENT, s);
      mprintf(serv, "NOTICE %s :%s /msg %s email none\n", 
				nick, IRC_FIELDTOREMOVE, botname);
   } else
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_NOEMAIL);
   return 1;
}

static int msg_info (char * nick, char * host, char * hand, char * par)
{
   char s[121], pass[512], chname[512];
   int locked = 0;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (!use_info)
      return 1;
   if (strcasecmp(hand, "*") == 0)
      return 0;
   if (get_attr_handle(hand) & USER_COMMON)
      return 1;
   if (!pass_match_by_handle("-", hand)) {
      nsplit(pass, par);
      if (!pass_match_by_handle(pass, hand)) {
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed INFO", nick, host, hand);
	 return 1;
      }
   }
   if ((par[0] == '#') || (par[0] == '+') || (par[0] == '&'))
      nsplit(chname, par);
   else
      chname[0] = 0;
   if (par[0]) {
      get_handle_info(hand, s);
      if (s[0] == '@')
	 locked = 1;
      if (chname[0]) {
	 get_handle_chaninfo(hand, chname, s);
	 if (s[0] == '@')
	    locked = 1;
      }
      if (locked) {
	 mprintf(serv, "NOTICE %s :%s\n", nick, IRC_INFOLOCKED);
	 return 1;
      }
      if (strcasecmp(par, "none") == 0) {
	 par[0] = 0;
	 if (chname[0]) {
	    set_handle_chaninfo(userlist, hand, chname, par);
	    mprintf(serv, "NOTICE %s :%s %s.\n", nick, IRC_REMINFOON, chname);
	    putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO %s NONE", nick, host, hand,
		   chname);
	 } else {
	    set_handle_info(userlist, hand, par);
	    mprintf(serv, "NOTICE %s :%s\n", nick, IRC_REMINFO);
	    putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO NONE", nick, host, hand);
	 }
	 return 1;
      }
      if (par[0] == '@')
	 strcpy(par, &par[1]);
      mprintf(serv, "NOTICE %s :%s %s\n", nick, IRC_FIELDCHANGED, par);
      if (chname[0]) {
	 set_handle_chaninfo(userlist, hand, chname, par);
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO %s ...", nick, host, hand, chname);
      } else {
	 set_handle_info(userlist, hand, par);
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO ...", nick, host, hand);
      }
      return 1;
   }
   if (chname[0]) {
      get_handle_chaninfo(hand, chname, s);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO? %s", nick, host, hand, chname);
   } else {
      get_handle_info(hand, s);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INFO?", nick, host, hand);
   }
   if (s[0]) {
      mprintf(serv, "NOTICE %s :%s %s\n", nick, IRC_FIELDCURRENT, s);
      mprintf(serv, "NOTICE %s :%s /msg %s info <pass>%s%s none\n",
	      nick, IRC_FIELDTOREMOVE, botname, chname[0] ? " " : "", chname);
   } else {
      if (chname[0])
	 mprintf(serv, "NOTICE %s :%s %s.\n", nick, IRC_NOINFOON, chname);
      else
	 mprintf(serv, "NOTICE %s :%s\n", nick, IRC_NOINFO);
   }
   return 1;
}

static int msg_who (char * nick, char * host, char * hand, char * par)
{
   struct chanset_t *chan;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (strcasecmp(hand, "*") == 0)
      return 0;
   if (!use_info)
      return 1;
   if (!par[0]) {
      mprintf(serv, "NOTICE %s :%s: /msg %s who <channel>\n", nick,
	      			USAGE, botname);
      return 0;
   }
   chan = findchan(par);
   if (chan == NULL) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_NOMONITOR);
      return 0;
   }
   if ((channel_hidden(chan)) &&
       (!hand_on_chan(chan, hand)) &&
       !(get_attr_handle(hand) & USER_MASTER) &&
       !(get_chanattr_handle(hand, chan->name) & (CHANUSER_OP | CHANUSER_FRIEND))) {
      mprintf(serv, "NOTICE %s :%s\n", nick, IRC_CHANHIDDEN);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! WHO", nick, host, hand);
   show_all_info(chan->name, nick);
   return 1;
}

static int msg_whois (char * nick, char * host, char * hand, char * opar)
{
   time_t tt, tt1 = 0;
   char s[161], s1[81], par[NICKLEN+1];
   int atr, ok;
   struct chanset_t *chan;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (strcasecmp(hand, "*") == 0)
      return 0;
   strncpy(par, opar, NICKLEN);
   par[NICKLEN] = 0;
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! WHOIS %s", nick, host, hand, par);
   if (!is_user(par)) {
      /* no such handle -- maybe it's a nickname of someone on a chan? */
      ok = 0;
      chan = chanset;
      while ((chan != NULL) && (!ok)) {
	 if (ischanmember(chan->name, par)) {
	    sprintf(s, "%s!", par);
	    getchanhost(chan->name, par, &s[strlen(s)]);
	    get_handle_by_host(par, s);
	    ok = 1;
	    if (par[0] == '*')
	       ok = 0;
	    else
	       hprintf(serv, "NOTICE %s :[%s] AKA '%s':\n", nick, opar, par);
	 }
	 chan = chan->next;
      }
      if (!ok) {
	 hprintf(serv, "NOTICE %s :[%s] %s\n", nick, opar, USERF_NOUSERREC);
	 return 1;
      }
   }
   atr = get_attr_handle(par);
   get_handle_info(par, s);
   if (s[0] == '@')
      strcpy(s, &s[1]);
   if ((s[0]) && (!(atr & USER_BOT)))
      hprintf(serv, "NOTICE %s :[%s] %s\n", nick, par, s);
   get_handle_email(par, s);
   if (s[0])
      hprintf(serv, "NOTICE %s :[%s] email: %s\n", nick, par, s);
   ok = 0;
   chan = chanset;
   while (chan != NULL) {
      if (hand_on_chan(chan, par)) {
	 sprintf(s1, "NOTICE %s :[%s] %s %s.", nick, par, 
				IRC_ONCHANNOW, chan->name);
	 ok = 1;
      } else {
	 get_handle_laston(chan->name, par, &tt);
	 if ((tt > tt1) && (!channel_hidden(chan) ||
			    hand_on_chan(chan, hand) ||
		 (get_attr_handle(hand) & (USER_MASTER | USER_GLOBAL)) ||
			    (get_chanattr_handle(hand, chan->name) &
			     (CHANUSER_OP | CHANUSER_FRIEND)))) {
	    tt1 = tt;
	    strcpy(s, ctime(&tt));
	    strcpy(s, &s[4]);
	    s[12] = 0;
	    ok = 1;
	    sprintf(s1, "NOTICE %s :[%s] %s %s on %s", nick, par,
		    		IRC_LASTSEENAT, s, chan->name);
	 }
      }
      chan = chan->next;
   }
   if (!ok)
      sprintf(s1, "NOTICE %s :[%s] %s", nick, par, IRC_NEVERJOINED);
   if (atr & USER_GLOBAL)
      strcat(s1, USER_ISGLOBALOP);
   if (atr & USER_BOT)
      strcat(s1, USER_ISBOT);
   if (atr & USER_MASTER)
      strcat(s1, USER_ISMASTER);
   hprintf(serv, "%s\n", s1);
   return 1;
}

static int msg_help (char * nick, char * host, char * hand, char * par)
{
   int atr;
   char s[121], *p;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   sprintf(s, "%s!%s", nick, host);
   atr = get_attr_host(s);
   if (strcasecmp(hand, "*") == 0) {
     if (quiet_reject != 0) {
      hprintf(serv, 
           "NOTICE %s :%s\n", nick, IRC_DONTKNOWYOU);
      hprintf(serv, "NOTICE %s :/MSG %s hello\n", nick, botname);
     }
     return 0;
   }
   if (helpdir[0]) {
      struct flag_record fr = { atr, 0, 0 };

      if (!par[0])
	 showhelp(nick, "help", &fr);
      else {
	 for (p = par; *p != 0; p++)
	    if ((*p >= 'A') && (*p <= 'Z'))
	       *p += ('a' - 'A');
	 showhelp(nick, par, &fr);
      }
   } else
      hprintf(serv, "NOTICE %s :%s\n", nick, IRC_NOHELP);
   return 1;
}

/* i guess just op them on every channel they're on */
static int msg_op (char * nick, char * host, char * hand, char * par)
{
   struct chanset_t *chan;
   char pass[512], pass2[512];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   nsplit(pass, par);
   if (pass_match_by_handle(pass, hand)) {
      int chatr;
      get_pass_by_handle(hand, pass2);
/* Prevent people from gaining ops when no password set */
      if (strcmp(pass2, "-") == 0) {
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed OP", nick, host, hand);
	 return 1;
      }
      if (par[0]) {
	 if (!active_channel(par)) {
	    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed OP", nick, host, hand);
	    return 1;
	 }
	 chan = findchan(par);
	 if (chan == NULL) {
	    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed OP", nick, host, hand);
	    return 1;
	 }
	 chatr = get_chanattr_handle(hand, chan->name);
	 if ((hand_on_chan(chan, hand)) && (!member_op(chan->name, nick)) &&
	     ((chatr & CHANUSER_OP) ||
	      ((get_attr_handle(hand) & USER_GLOBAL) && !(chatr & CHANUSER_DEOP)))) {
	    add_mode(chan, '+', 'o', nick);
	    putlog(LOG_CMDS, "*", "(%s!%s) !%s! OP %s", nick, host, hand, par);
	 }
	 return 1;
      }
      chan = chanset;
      while (chan != NULL) {
	 chatr = get_chanattr_handle(hand, chan->name);
	 if ((hand_on_chan(chan, hand)) && (!member_op(chan->name, nick)) &&
	     ((chatr & CHANUSER_OP) ||
	      ((get_attr_handle(hand) & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))))
	    add_mode(chan, '+', 'o', nick);
	 chan = chan->next;
      }
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! OP", nick, host, hand);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed OP", nick, host, hand);
   return 1;
}

static int msg_invite (char * nick, char * host, char * hand, char * par)
{
   char pass[512];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (strcmp(hand, "*") == 0)
      return 0;
   nsplit(pass, par);
   if (pass_match_by_handle(pass, hand)) {
      if (findchan(par) == NULL) {
	 mprintf(serv, "NOTICE %s :%s: /MSG %s invite <pass> <channel>\n",
		 nick, USAGE, botname);
	 return 1;
      }
      if (!active_channel(par)) {
	 mprintf(serv, "NOTICE %s :%s: %s\n", nick, par, IRC_NOTONCHAN);
	 return 1;
      }
      mprintf(serv, "INVITE %s %s\n", nick, par);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE %s", nick, host, hand, par);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed INVITE %s", nick, host, hand, par);
   return 1;
}

static int msg_status (char * nick, char * host, char * hand, char * par)
{
   if (strcasecmp(nick, botname) == 0)
      return 1;
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! STATUS", nick, host, hand);
   tell_chan_info(nick);
   return 1;
}

static int msg_memory (char * nick, char * host, char * hand, char * par)
{
   if (strcasecmp(nick, botname) == 0)
      return 1;
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! MEMORY", nick, host, hand);
   tell_mem_status(nick);
   return 1;
}

static int msg_die (char * nick, char * host, char * hand, char * par)
{
   char s[121];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (pass_match_by_handle(par, hand)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! DIE", nick, host, hand);
      tprintf(serv, "NOTICE %s :%s\n", nick, BOT_MSGDIE);
      chatout("*** BOT SHUTDOWN (authorized by %s)\n", hand);
      tandout("chat %s BOT SHUTDOWN (authorized by %s)\n", botnetnick, hand);
      tandout("bye\n");
      tprintf(serv, "QUIT :%s\n", nick);
      write_userfile();
      sleep(1);			/* give the server time to understand */
      sprintf(s, "DEAD BY REQUEST OF %s!%s", nick, host);
      fatal(s, 0);
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed DIE", nick, host, hand);
   return 1;
}

static int msg_rehash (char * nick, char * host, char * hand, char * par)
{
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (pass_match_by_handle(par, hand)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! REHASH", nick, host, hand);
      mprintf(serv, "NOTICE %s :%s\n", nick, USERF_REHASHING);
      rehash();
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed REHASH", nick, host, hand);
   return 1;
}

static int msg_reset (char * nick, char * host, char * hand, char * par)
{
   struct chanset_t *chan;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (par[0]) {
      chan = findchan(par);
      if (chan == NULL) {
	 mprintf(serv, "NOTICE %s :%s: %s\n", nick, par, IRC_NOMONITOR);
	 return 0;
      }
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! RESET %s", nick, host, hand, par);
      mprintf(serv, "NOTICE %s :%s: %s\n", nick, par, IRC_RESETCHAN);
      reset_chan_info(chan);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! RESET ALL", nick, host, hand);
   mprintf(serv, "NOTICE %s :%s\n", nick, IRC_RESETCHAN);
   chan = chanset;
   while (chan != NULL) {
      reset_chan_info(chan);
      chan = chan->next;
   }
   return 1;
}

static int msg_go (char * nick, char * host, char * hand, char * par)
{
   struct chanset_t *chan;
   int ok = 0;
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (!op_anywhere(hand)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed GO (not op)", nick, host, hand);
      return 1;
   }
   if (par[0]) {
      /* specific GO */
      int chatr;
      chan = findchan(par);
      if (chan == NULL)
	 return 0;
      chatr = get_chanattr_handle(hand, chan->name);
      if (!(chatr & CHANUSER_OP) &&
	  !((get_attr_handle(hand) & USER_GLOBAL) && !(chatr & CHANUSER_DEOP))) {
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed GO (not op)", nick, host, hand);
	 return 1;
      }
      if (!me_op(chan)) {
	 tprintf(serv, "PART %s\n", chan->name);
	 putlog(LOG_CMDS, chan->name, "(%s!%s) !%s! GO %s", nick, host, hand, par);
	 return 1;
      }
      putlog(LOG_CMDS, chan->name, "(%s!%s) !%s! failed GO %s (i'm chop)", nick,
	     host, hand, par);
      return 1;
   }
   chan = chanset;
   while (chan != NULL) {
      if (ischanmember(chan->name, nick)) {
	 if (!me_op(chan)) {
	    tprintf(serv, "PART %s\n", chan->name);
	    ok = 1;
	 }
      }
      chan = chan->next;
   }
   if (ok) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! GO", nick, host, hand);
      return 1;
   }
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed GO (i'm chop)", nick, host, hand);
   return 1;
}

static int msg_jump (char * nick, char * host, char * hand, char * par)
{
   char s[512], port[512];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   nsplit(s, par);		/* password */
   if (pass_match_by_handle(s, hand)) {
      if (par[0]) {
	 nsplit(s, par);
	 nsplit(port, par);
	 if (!port[0])
	    sprintf(port, "%d", default_port);
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! JUMP %s %s %s", nick, host, hand, s, port,
		par);
	 strcpy(newserver, s);
	 newserverport = atoi(port);
	 strcpy(newserverpass, par);
      } else
	 putlog(LOG_CMDS, "*", "(%s!%s) !%s! JUMP", nick, host, hand);
      tprintf(serv, "NOTICE %s :%s\n", nick, IRC_JUMP);
      tprintf(serv, "QUIT :changing servers\n");
      killsock(serv);
      serv = (-1);
   } else
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed JUMP", nick, host, hand);
   return 1;
}

/* notes <pass> <func> */
static int msg_notes (char * nick, char * host, char * hand, char * par)
{
   char pwd[512], fcn[512];
   if (strcasecmp(nick, botname) == 0)
      return 1;
   if (hand[0] == '*')
      return 0;
   if (!par[0]) {
      hprintf(serv, "NOTICE %s :%s: NOTES [pass] INDEX\n", nick, USAGE);
      hprintf(serv, "NOTICE %s :       NOTES [pass] TO <nick> <msg>\n", nick);
      hprintf(serv, "NOTICE %s :       NOTES [pass] READ <# or ALL>\n", nick);
      hprintf(serv, "NOTICE %s :       NOTES [pass] ERASE <# or ALL>\n", nick);
      return 1;
   }
   if (!pass_match_by_handle("-", hand)) {
      /* they have a password set */
      nsplit(pwd, par);
      if (!pass_match_by_handle(pwd, hand))
	 return 0;
   }
   nsplit(fcn, par);
   if (strcasecmp(fcn, "INDEX") == 0)
      notes_read(hand, nick, -1, -1);
   else if (strcasecmp(fcn, "READ") == 0) {
      if (strcasecmp(par, "ALL") == 0)
	 notes_read(hand, nick, 0, -1);
      else
	 notes_read(hand, nick, atoi(par), -1);
   } else if (strcasecmp(fcn, "ERASE") == 0) {
      if (strcasecmp(par, "ALL") == 0)
	 notes_del(hand, nick, 0, -1);
      else
	 notes_del(hand, nick, atoi(par), -1);
   } else if (strcasecmp(fcn, "TO") == 0) {
      char to[514];
      int i;
      FILE *f;
      nsplit(to, par);
      if (!par[0]) {
	 hprintf(serv, "NOTICE %s :%s: NOTES [pass] TO <nick> <message>\n",
		 nick, USAGE);
	 return 0;
      }
      if (!is_user(to)) {
	 hprintf(serv, "NOTICE %s :USERF_UNKNOWN\n", nick, USERF_UNKNOWN);
	 return 1;
      }
      for (i = 0; i < dcc_total; i++) {
	 if ((strcasecmp(dcc[i].nick, to) == 0) && ((dcc[i].type == &DCC_CHAT) ||
					   (dcc[i].type == &DCC_FILES))) {
	    int aok = 1;
	    if (dcc[i].type == &DCC_CHAT)
	       if (dcc[i].u.chat->away != NULL)
		  aok = 0;
	    if (dcc[i].type == &DCC_FILES)
	       if (dcc[i].u.file->chat->away != NULL)
		  aok = 0;
	    if (aok) {
	       dprintf(i, "\007%s [%s]: %s\n", hand, BOT_NOTEOUTSIDE, par);
	       hprintf(serv, "NOTICE %s :%s\n", nick, BOT_NOTEDELIV);
	       return 1;
	    }
	 }
      }
      if (notefile[0] == 0) {
	 hprintf(serv, "NOTICE %s :%s\n", nick, BOT_NOTEUNSUPP);
	 return 1;
      }
      f = fopen(notefile, "a");
      if (f == NULL)
	 f = fopen(notefile, "w");
      if (f == NULL) {
	 hprintf(serv, "NOTICE %s :%s", nick, BOT_NOTESERROR1);
	 putlog(LOG_MISC, "*", "* %s", BOT_NOTESERROR2);
	 return 1;
      }
      fprintf(f, "%s %s %lu %s\n", to, hand, time(NULL), par);
      fclose(f);
      hprintf(serv, "NOTICE %s :%s\n", nick, BOT_NOTEDELIV);
      return 1;
   } else
      hprintf(serv, "NOTICE %s :%s INDEX, READ, ERASE, TO\n",
			nick, BOT_NOTEUSAGE);
   putlog(LOG_CMDS, "*", "(%s!%s) !%s! NOTES %s %s", nick, host, hand, fcn,
	  par[0] ? "..." : "");
   return 1;
}

/* MSG COMMANDS */
/* function call should be:
   int msg_cmd("handle","nick","user@host","params");
   function is responsible for any logging
   (return 1 if successful, 0 if not) */
cmd_t C_msg[]={
  { "die", "n", (Function)msg_die, NULL },
  { "email", "", (Function)msg_email, NULL },
  { "go", "", (Function)msg_go, NULL },
  { "hello", "", (Function)msg_hello, NULL },
  { "help", "", (Function)msg_help, NULL },
  { "ident", "", (Function)msg_ident, NULL },
  { "info", "", (Function)msg_info, NULL },
  { "invite", "", (Function)msg_invite, NULL },
  { "jump", "m",(Function) msg_jump, NULL },
  { "memory", "m", (Function)msg_memory, NULL },
  { "notes", "", (Function)msg_notes, NULL },
  { "op", "", (Function)msg_op, NULL },
  { "pass", "", (Function)msg_pass, NULL },
  { "rehash", "m", (Function)msg_rehash, NULL },
  { "reset", "m", (Function)msg_reset, NULL },
  { "status", "m|m", (Function)msg_status, NULL },
  { "who", "", (Function)msg_who, NULL },
  { "whois", "", (Function)msg_whois, NULL },
  { 0, 0, 0, 0 }
};

#endif				/* !NO_IRC */
