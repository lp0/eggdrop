#define MODULE_NAME "share"

#include "../module.h"
#include "../../chan.h"
#include "../../users.h"
#include <varargs.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern char natip[];
extern int noshare;
extern int ban_time;
extern int ignore_time;
extern int maxqmsg;
extern struct userrec * lastuser, *banu, *ignu;
extern int max_dcc;

int passive = 0;
int min_share = 1020000;	/* minimum version I will share with */
int private_owner = 1;
int allow_resync = 0;

static void start_sending_users(int);
static void shareout_but ();
static int flush_tbuf (char * bot);
static int can_resync (char * bot);
static void dump_resync (int z, char * bot);
static void q_resync (char * s, struct chanset_t * chan);
static int write_tmp_userfile (char *,struct userrec *);
static struct userrec *dup_userlist(int);
static void restore_chandata(void);
static void cancel_user_xfer(int);

static char TBUF[1024];
/* store info for sharebots */
struct msgq {
   struct chanset_t * chan;
   char *msg;
   struct msgq *next;
};

static struct tandbuf {
   char bot[10];
   time_t timer;
   struct msgq *q;
} tbuf[5];

/* botnet commands */

static void share_stick (int idx, char * par)
{
   char *host = TBUF, *val = TBUF + 512;
   int yn;

   nsplit(host, par);
   nsplit(val, par);
   yn = atoi(val);
   noshare = 1;
   if (!par[0]) {		/* global ban */
      if (setsticky_ban(host, yn) > 0) {
	 putlog(LOG_CMDS, "*", "%s: stick %s %c", dcc[idx].nick, host, yn ? 'y' : 'n');
	 shareout_but(NULL,idx,"stick %s %d\n", host, yn);
      }
   } else {
      struct chanset_t *chan = findchan(par);
      if ((chan != NULL) && (chan->stat & CHAN_SHARED) &&
	  (get_chanattr_handle(dcc[idx].nick,par) & CHANBOT_SHARE)) 
	if (u_setsticky_ban(chan->bans, host, yn) > 0) {
	   putlog(LOG_CMDS, "*", "%s: stick %s %c %s", dcc[idx].nick, host,
		  yn ? 'y' : 'n', par);
	   shareout_but(chan,idx,"stick %s %d %s\n", host, yn, chan->name);
	   noshare = 0;
	   return;
	}
      putlog(LOG_CMDS,"*","Rejecting invalid sticky ban: %s on %s, %c",
	     host,par,yn?'y':'n');
   }
   noshare = 0;
}

#define CHKSEND if (!(dcc[idx].u.bot->status&STAT_SENDING)) return
#define CHKGET if (!(dcc[idx].u.bot->status&STAT_GETTING)) return
#define CHKSHARE if (!(dcc[idx].u.bot->status&STAT_SHARE)) return

static void share_chpass (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;
   shareout_but(NULL,idx, "chpass %s %s\n", hand, par);
   noshare = 1;
   change_pass_by_handle(hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: newpass %s", dcc[idx].nick, hand);
}

static void share_chhand (int idx, char * par)
{
   char *hand = TBUF;
   int i;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "chhand %s %s\n", hand, par);
   noshare = 1;
   change_handle(hand, par);
   notes_change(-1, hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: handle %s->%s", dcc[idx].nick, hand, par);
   for (i = 0; i < dcc_total; i++)
      if (strcasecmp(hand, dcc[i].nick) == 0) {
	 if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel >= 0)) {
	    chanout2(dcc[i].u.chat->channel, "Nick change: %s -> %s\n",
		     dcc[i].nick, par);
	    modcontext;
	    if (dcc[i].u.chat->channel < 100000) {
	       tandout("part %s %s %d\n", botnetnick, dcc[i].nick, dcc[i].sock);
	       tandout("join %s %s %d %c%d %s\n", botnetnick, par, dcc[i].u.chat->channel,
		       geticon(i), dcc[i].sock, dcc[i].host);
	    }
	 }
	 strcpy(dcc[i].nick, par);
      }
}

static void share_chattr (int idx, char * par)
{
   char *hand = TBUF, *atr = TBUF + 50, *s = TBUF + 512;
   int oatr, natr;
   struct chanset_t *cst;
   CHKSHARE;
   nsplit(hand, par);
   nsplit(atr, par);
   cst = findchan(par);
   if (get_attr_handle(hand) & (USER_UNSHARED|BOT_SHARE))
     return;   
   if (cst && !(cst->stat & CHAN_SHARED))
     return;
   shareout_but(cst,idx, "chattr %s %s %s\n", hand, atr, par);
   noshare = 1;
   if (par[0] && cst) {
      natr = str2chflags(atr);
      if ((cst->stat & CHAN_SHARED) && 
	  (get_chanattr_handle(dcc[idx].nick,par) & CHANBOT_SHARE)) {
	 set_chanattr_handle(hand, par, natr & ~CHANBOT_SHARE);
	 noshare = 0;
	 chflags2str(get_chanattr_handle(hand, par), s);
	 putlog(LOG_CMDS, "*", "%s: chattr %s %s %s", dcc[idx].nick, hand, s, par);
      } else
	putlog(LOG_CMDS, "*", "Rejected info for unshared channel %s from %s",
	       par, dcc[idx].nick);
      return;
   }
   /* don't let bot flags be altered */
   oatr = (get_attr_handle(hand) & ~BOT_MASK);
   natr = str2flags(atr);
   if (private_owner) {
      oatr |= (get_attr_handle(hand) & USER_OWNER);
      natr &= (~USER_OWNER);
   }
   set_attr_handle(hand, (natr & BOT_MASK) | oatr);
   noshare = 0;
   flags2str(get_attr_handle(hand), s);
   putlog(LOG_CMDS, "*", "%s: chattr %s %s", dcc[idx].nick, hand, s);
}

static void share_pls_chrec (int idx, char * par) {
   char user[512];
   struct chanset_t * chan;
   struct userrec * u;
   
   CHKSHARE;
   nsplit(user,par);
   u = get_user_by_handle(userlist,user);
   if (u == NULL) 
     return;
   chan = findchan(par);
   if ((chan == NULL) || !(chan->stat & CHAN_SHARED) ||
       !(get_chanattr_handle(dcc[idx].nick,chan->name) & CHANBOT_SHARE)) {
      putlog(LOG_CMDS, "*", "Rejected info for unshared channel %s from %s",
	     par, dcc[idx].nick);
      return;
   }
   noshare = 1;
   add_chanrec(u,par,0,0);
   shareout_but(chan,idx, "+cr %s %s\n", user, par);
   putlog(LOG_CMDS, "*", "%s: +chrec %s %s", dcc[idx].nick, user, par);
   noshare = 0;
}

static void share_mns_chrec (int idx, char * par) {
   char user[512];
   struct chanset_t * chan;
   struct userrec * u;
   
   CHKSHARE;
   nsplit(user,par);
   u = get_user_by_handle(userlist,user);
   if (u == NULL) 
     return;
   chan = findchan(par);
   if ((chan == NULL) || !(chan->stat & CHAN_SHARED) ||
       !(get_chanattr_handle(dcc[idx].nick,chan->name) & CHANBOT_SHARE)) {
      putlog(LOG_CMDS, "*", "Rejected info for unshared channel %s from %s",
	     par, dcc[idx].nick);
      return;
   }
   noshare = 1;
   del_chanrec(u,par);
   shareout_but(chan,idx, "-cr %s %s\n", user, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: -chrec %s %s", dcc[idx].nick, user, par);
}

static void share_newuser (int idx, char * par)
{
   char *etc = TBUF, *etc2 = TBUF + 41, *etc3 = TBUF + 121;
   CHKSHARE;
   nsplit(etc, par);
   if (get_attr_handle(etc) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "newuser %s %s\n", etc, par);
   noshare = 1;
   etc[40] = 0;
   /* If user already exists, ignore command */
   if (is_user(etc)) {
      noshare = 0;
      return;
   }
   nsplit(etc2, par);
   etc2[80] = 0;
   nsplit(etc3, par);
   userlist = adduser(userlist, etc, etc2, etc3, atoi(par));
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: newuser %s", dcc[idx].nick, etc);
}

static void share_killuser (int idx, char * par)
{
   CHKSHARE;
   noshare = 1;
   /* If user is a share bot, ignore command */
   if (!(get_attr_handle(par) & (BOT_SHARE|USER_UNSHARED))) {
      if (deluser(par)) {
	 shareout_but(NULL,idx, "killuser %s\n", par);
	 putlog(LOG_CMDS, "*", "%s: killuser %s", dcc[idx].nick, par);
      }
   }
   noshare = 0;
}

static void share_pls_upload (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "+upload %s %s\n", hand, par);
   noshare = 1;
   stats_add_upload(hand, atol(par));
   noshare = 0;
   /* no point logging this */
}

static void share_pls_dnload (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "+dnload %s %s\n", hand, par);
   noshare = 1;
   stats_add_dnload(hand, atol(par));
   noshare = 0;
   /* no point logging this either */
}

static void share_pls_host (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "+host %s %s\n", hand, par);
   noshare = 1;
   addhost_by_handle(hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: +host %s %s", dcc[idx].nick, hand, par);
}

static void share_pls_bothost (int idx, char * par)
{
   char *hand = TBUF, *p = TBUF + 512;
   int atr;
   CHKSHARE;
   nsplit(hand, par);
   atr = get_attr_handle(hand);
   if (atr & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "+bothost %s %s\n", hand, par);
   noshare = 1;
   /* add bot to userlist if not there */
   if (is_user(hand)) {
      if (!(atr & USER_BOT)) {
	 noshare = 0;
	 return;		/* ignore */
      }
      addhost_by_handle(hand, par);
   } else {
      makepass(p);
      userlist = adduser(userlist, hand, par, p, USER_BOT);
   }
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: +host %s %s", dcc[idx].nick, hand, par);
}

static void share_mns_host (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & (USER_UNSHARED|BOT_SHARE))
     return;   
   shareout_but(NULL,idx, "-host %s %s\n", hand, par);
   noshare = 1;
   /* If user is a share bot, ignore command */
   delhost_by_handle(hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: -host %s %s", dcc[idx].nick, hand, par);
}

static void share_chemail (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "chemail %s %s\n", hand, par);
   noshare = 1;
   set_handle_email(userlist, hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change email %s", dcc[idx].nick, hand);
}

static void share_chdccdir (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "chdccdir %s %s\n", hand, par);
   noshare = 1;
   set_handle_dccdir(userlist, hand, par);
   noshare = 0;
}

static void share_chcomment (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "chcomment %s %s\n", hand, par);
   noshare = 1;
   set_handle_comment(userlist, hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change comment %s", dcc[idx].nick, hand);
}

static void share_chinfo (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "chinfo %s %s\n", hand, par);
   noshare = 1;
   set_handle_info(userlist, hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change info %s", dcc[idx].nick, hand);
}

static void share_chchinfo (int idx, char * par)
{
   char *hand = TBUF;
   char *chan = TBUF + 512;
   struct chanset_t *cst;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   nsplit(chan, par);
   cst = findchan(chan);
   if (!(cst->stat & CHAN_SHARED) || 
       !(get_chanattr_handle(hand,chan) & CHANBOT_SHARE))  {
      putlog(LOG_CMDS, "*", "Info line change from %s denied.  Channel %s not shared.",
	     dcc[idx].nick, chan);
      return;
   }
   shareout_but(cst,idx, "chchinfo %s %s\n", hand, par);
   noshare = 1;
   set_handle_chaninfo(userlist, hand, chan, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change info %s %s", dcc[idx].nick, chan, hand);
}

static void share_chaddr (int idx, char * par)
{
   char *hand = TBUF;
   int atr = get_attr_handle(hand);
   CHKSHARE;
   modcontext;
   nsplit(hand, par);
   if ((atr & USER_UNSHARED)|| ! (atr & USER_BOT))
     return;   
   shareout_but(NULL,idx, "chaddr %s %s\n", hand, par);
   noshare = 1;
   /* add bot to userlist if not there */
   if (!is_user(hand)) {
      char * p = TBUF+512;
      makepass(p);
      userlist = adduser(userlist, hand, "none", p, USER_BOT);
   }
   set_handle_info(userlist, hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change address %s", dcc[idx].nick, hand);
}

static void share_clrxtra (int idx, char * par)
{
   CHKSHARE;
   if (get_attr_handle(par) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "clrxtra %s\n", par);
   noshare = 1;
   set_handle_xtra(userlist, par, "");
   noshare = 0;
}

static void share_addxtra (int idx, char * par)
{
   char *hand = TBUF;
   CHKSHARE;
   nsplit(hand, par);
   if (get_attr_handle(hand) & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "addxtra %s %s\n", hand, par);
   noshare = 1;
   add_handle_xtra(userlist, hand, par);
   noshare = 0;
}

static void share_mns_ban (int idx, char * par)
{
   CHKSHARE;
   shareout_but(NULL,idx, "-ban %s\n", par);
   putlog(LOG_CMDS, "*", "%s: cancel ban %s", dcc[idx].nick, par);
   noshare = 1;
   delban(par);
   noshare = 0;
}

static void share_mns_banchan (int idx, char * par)
{
   char *chname = TBUF;
   struct chanset_t *chan;
   CHKSHARE;
   nsplit(chname, par);
   chan = findchan(chname);
   if (!chan || !(get_chanattr_handle(dcc[idx].nick,chname)&CHANBOT_SHARE))
      return;
   shareout_but(chan,idx, "-banchan %s\n", par);
   if (chan->stat & CHAN_SHARED) {
      putlog(LOG_CMDS, "*", "%s: cancel ban %s on %s", dcc[idx].nick, par, chname);
      noshare = 1;
      u_delban(chan->bans, par);
      noshare = 0;
   } 
}

static void share_mns_ignore (int idx, char * par)
{
   CHKSHARE;
   shareout_but(NULL,idx, "-ignore %s\n", par);
   putlog(LOG_CMDS, "*", "%s: cancel ignore %s", dcc[idx].nick, par);
   noshare = 1;
   delignore(par);
   noshare = 0;
}

static void share_pls_ban (int idx, char * par)
{
   time_t expire_time;
   char *ban = TBUF, *tm = TBUF + 512, *from = TBUF + 532;
   CHKSHARE;
   shareout_but(NULL,idx, "+ban %s\n", par);
   noshare = 1;
   nsplit(ban, par);
   nsplit(tm, par);
   nsplit(from, par);
   if (from[strlen(from) - 1] == ':')
      from[strlen(from) - 1] = 0;
   putlog(LOG_CMDS, "*", "%s: ban %s (%s: %s)", dcc[idx].nick, ban, from, par);
   /* new format? */
   if (tm[0] == '+') {
      /* time left */
      strcpy(tm, &tm[1]);
      expire_time = (time_t) atol(tm);
      if (expire_time != 0L)
	 expire_time += now;
   } else {
      expire_time = (time_t) atol(tm);
      if (expire_time != 0L)
	 expire_time = (now - expire_time);
   }
   addban(ban, from, par, expire_time);
   noshare = 0;
}

static void share_pls_banchan (int idx, char * par)
{
   time_t expire_time;
   struct chanset_t *chan;
   char *ban = TBUF, *tm = TBUF + 512, *chname = TBUF + 600, *from = TBUF + 700;
   CHKSHARE;
   nsplit(ban, par);
   nsplit(tm, par);
   nsplit(chname, par);
   chan = findchan(chname);
   if (!chan)
      return;
   if (!(chan->stat & CHAN_SHARED) || 
       !(get_chanattr_handle(dcc[idx].nick,chname)&CHANBOT_SHARE)) {
      putlog(LOG_CMDS, "*", "Channel ban %s on %s rejected - channel not shared.",
	    ban, chan);
      return;
   }
   shareout_but(chan,idx, "+banchan %s\n", par);
   nsplit(from, par);
   if (from[strlen(from) - 1] == ':')
      from[strlen(from) - 1] = 0;
   putlog(LOG_CMDS, "*", "%s: ban %s on %s (%s:%s)", dcc[idx].nick, ban, chname,
	  from, par);
   noshare = 1;
   /* new format? */
   if (tm[0] == '+') {
      /* time left */
      strcpy(tm, &tm[1]);
      expire_time = (time_t) atol(tm);
      if (expire_time != 0L)
	 expire_time += now;
   } else {
      expire_time = (time_t) atol(tm);
      if (expire_time != 0L)
	 expire_time = (now - expire_time) + (60 * ban_time);
   }
   u_addban(chan->bans, ban, from, par, expire_time);
   noshare = 0;
}

/* +ignore <host> +<seconds-left> <from> <note> */
static void share_pls_ignore (int idx, char * par)
{
   time_t expire_time;
   char *ign = TBUF, *from = TBUF + 256, *ts = TBUF + 512;
   CHKSHARE;
   shareout_but(NULL,idx, "share +ignore %s\n", par);
   noshare = 1;
   nsplit(ign, par);
   if (par[0] == '+') {
      /* new-style */
      nsplit(ts, par);
      strcpy(ts, &ts[1]);
      if (atoi(ts) == 0)
	 expire_time = 0L;
      else
	 expire_time = now + atoi(ts);
      nsplit(from, par);
      from[10] = 0;
      par[65] = 0;
   } else {
      if (atoi(par) == 0)
	 expire_time = 0L;
      else
	 expire_time = now + (60 * ignore_time) - atoi(par);
      strcpy(from, dcc[idx].nick);
      strcpy(par, "-");
   }
   putlog(LOG_CMDS, "*", "%s: ignore %s (%s: %s)", dcc[idx].nick, ign, from, par);
   addignore(ign, from, par, expire_time);
   noshare = 0;
}

static void share_ufno (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "User file rejected by %s: %s", dcc[idx].nick, par);
   dcc[idx].u.bot->status &= ~STAT_OFFERED;
   if (!(dcc[idx].u.bot->status & STAT_GETTING)) {
      dcc[idx].u.bot->status &= ~STAT_SHARE;
   }
}

static void share_ufyes (int idx, char * par)
{
   modcontext;
   if (dcc[idx].u.bot->status & STAT_OFFERED) {
      dcc[idx].u.bot->status &= ~STAT_OFFERED;
      dcc[idx].u.bot->status |= STAT_SHARE;
      dcc[idx].u.bot->status |= STAT_SENDING;
      start_sending_users(idx);
      putlog(LOG_BOTS, "*", "Sending user file send request to %s", dcc[idx].nick);
   }
}

static void share_userfileq (int idx, char * par)
{
   int ok = 1, i;
   flush_tbuf(dcc[idx].nick);
   if (module_find("transfer", 1, 0) == NULL)
      tprintf(dcc[idx].sock, "share uf-no Transfer module not installed.\n");
   else if (!passive)
      tprintf(dcc[idx].sock, "share uf-no Aggressive mode active.\n");
   else if (!(get_attr_handle(dcc[idx].nick) & BOT_SHARE))
      tprintf(dcc[idx].sock, "share uf-no You are not +s for me.\n");
   else if (min_share > dcc[idx].u.bot->numver)
      tprintf(dcc[idx].sock,
	      "share uf-no Your version is not high enough, need v%d.%d.%d\n",
	      (min_share / 1000000), (min_share / 10000) % 100, (min_share / 100) % 100);
   else {
      for (i = 0; i < dcc_total; i++)
	 if (dcc[i].type == &DCC_BOT)
	    if ((dcc[i].u.bot->status & STAT_SHARE) &&
		(dcc[i].u.bot->status & STAT_GETTING))
	       ok = 0;
      if (!ok)
	 tprintf(dcc[idx].sock, "share uf-no Already downloading a userfile.\n");
      else {
	 tprintf(dcc[idx].sock, "share uf-yes\n");
	 /* set stat-getting to astatic void race condition (robey 23jun96) */
	 dcc[idx].u.bot->status |= STAT_SHARE | STAT_GETTING;
	 putlog(LOG_BOTS, "*", "Downloading user file from %s", dcc[idx].nick);
      }
   }
}

/* ufsend <ip> <port> <length> */
static void share_ufsend (int idx, char * par)
{
   char *ip = TBUF, *port = TBUF + 512;
   int i;
   if (!(dcc[idx].u.bot->status & STAT_SHARE)) {
      tprintf(dcc[idx].sock, "error You didn't ask; you just started sending.\n");
      tprintf(dcc[idx].sock, "error Ask before sending the userfile.\n");
      zapfbot(idx);
      return;
   }
   if (dcc_total == max_dcc) {
      putlog(LOG_MISC, "*", "NO MORE DCC CONNECTIONS -- can't grab userfile");
      tprintf(dcc[idx].sock, "error I can't open a DCC to you; I'm full.\n");
      zapfbot(idx);
      return;
   }
   nsplit(ip, par);
   nsplit(port, par);
   i = dcc_total;
   dcc[i].addr = my_atoul(ip);
   dcc[i].port = atoi(port);
   dcc[i].sock = (-1);
   dcc[i].type = &DCC_FORK_SEND;
   strcpy(dcc[i].nick, "*users");
   strcpy(dcc[i].host, dcc[idx].nick);
   dcc[i].u.other = NULL;
   dcc[i].u.xfer= get_data_ptr(sizeof(struct xfer_info));
   strcpy(dcc[i].u.xfer->filename, ".share.users");
   dcc[i].u.xfer->dir[0] = 0;	/* this dir */
   dcc[i].u.xfer->length = atol(par);
   dcc[i].u.xfer->sent = 0;
   dcc[i].u.xfer->sofar = 0;
   dcc[i].u.xfer->f = fopen(".share.users", "w");
   if (dcc[i].u.xfer->f == NULL) {
      putlog(LOG_MISC, "*", "CAN'T WRITE USERFILE DOWNLOAD FILE!");
      modfree(dcc[i].u.xfer);
      zapfbot(idx);
      return;
   }
   dcc[idx].u.bot->status |= STAT_GETTING;
   dcc_total++;
   /* don't buffer this */
   dcc[i].sock = getsock(SOCK_BINARY);
   if (open_telnet_dcc(dcc[i].sock, ip, port) < 0) {
      putlog(LOG_MISC, "*", "Asynchronous connection failed!");
      tprintf(dcc[idx].sock, "error Can't connect to you!\n");
      lostdcc(i);
      zapfbot(idx);
   }
}

static void share_resyncq (int idx, char * par)
{
   if (!allow_resync) {
      tprintf(dcc[idx].sock, "share resync-no Not permitting resync.\n");
      return;
   } else {
      if (!(get_attr_handle(dcc[idx].nick) & BOT_SHARE))
	tprintf(dcc[idx].sock, "shrare resync-no You are not +s for me.\n");
      else if (can_resync(dcc[idx].nick)) {
	 tprintf(dcc[idx].sock, "share resync!\n");
	 dump_resync(dcc[idx].sock, dcc[idx].nick);
	 dcc[idx].u.bot->status &= ~STAT_OFFERED;
	 dcc[idx].u.bot->status |= STAT_SHARE;
	 putlog(LOG_BOTS, "*", "Resync'd user file with %s", dcc[idx].nick);
      } else if (passive) {
	 tprintf(dcc[idx].sock, "share resync!\n");
	 dcc[idx].u.bot->status &= ~STAT_OFFERED;
	 dcc[idx].u.bot->status |= STAT_SHARE;
	 putlog(LOG_BOTS, "*", "Resyncing user file from %s", dcc[idx].nick);
      } else
	tprintf(dcc[idx].sock, "share resync-no No resync buffer.\n");
   }
}

static void share_resync (int idx, char * par)
{
   if (dcc[idx].u.bot->status & STAT_OFFERED) {
      if (can_resync(dcc[idx].nick)) {
	 dump_resync(dcc[idx].sock, dcc[idx].nick);
	 dcc[idx].u.bot->status &= ~STAT_OFFERED;
	 dcc[idx].u.bot->status |= STAT_SHARE;
	 putlog(LOG_BOTS, "*", "Resync'd user file with %s", dcc[idx].nick);
      }
   }
}

static void share_resync_no (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "Resync refused by %s: %s", dcc[idx].nick, par);
   flush_tbuf(dcc[idx].nick);
   tprintf(dcc[idx].sock, "share userfile?\n");
}

static void share_version (int idx, char * par) {
   if (dcc[idx].u.bot->numver >= min_share) {
      if (get_attr_handle(dcc[idx].nick) & BOT_SHARE) {
	 if (passive) {
	    if (dcc[idx].u.bot->status & STAT_CALLED) {
	       if (can_resync(dcc[idx].nick))
		 tprintf(dcc[idx].sock, "share resync?\n");
	       else
		 tprintf(dcc[idx].sock, "share userfile?\n");
	       dcc[idx].u.bot->status |= STAT_OFFERED;
	    }
	 } else {
	    if (can_resync(dcc[idx].nick))
	      tprintf(dcc[idx].sock, "share resync?\n");
	    else
	      tprintf(dcc[idx].sock, "share userfile?\n");
	    dcc[idx].u.bot->status |= STAT_OFFERED;
	 }
      }
   }
}

static void share_end (int idx, char * par) {
   putlog(LOG_BOTS,"*","Ending sharing with %s, unloading modules.",dcc[idx].nick);
   cancel_user_xfer(-idx);
   dcc[idx].u.bot->status &= 
     ~(STAT_SHARE|STAT_GETTING|STAT_SENDING|STAT_OFFERED);
}

/* these MUST be sorted */
static botcmd_t C_share[]={
  { "+ban", (Function) share_pls_ban },
  { "+banchan", (Function) share_pls_banchan },
  { "+bothost", (Function) share_pls_bothost },
  { "+cr", (Function) share_pls_chrec },
  { "+dnload", (Function) share_pls_dnload },
  { "+host", (Function) share_pls_host },
  { "+ignore", (Function) share_pls_ignore },
  { "+upload", (Function) share_pls_upload },
  { "-ban", (Function) share_mns_ban },
  { "-banchan", (Function) share_mns_banchan },
  { "-cr", (Function) share_mns_chrec },
  { "-host", (Function) share_mns_host },
  { "-ignore", (Function) share_mns_ignore },
  { "addxtra", (Function) share_addxtra },
  { "chaddr", (Function) share_chaddr },
  { "chattr", (Function) share_chattr },
  { "chchinfo", (Function) share_chchinfo },
  { "chcomment", (Function) share_chcomment },
  { "chdccdir", (Function) share_chdccdir },
  { "chemail", (Function) share_chemail },
  { "chhand", (Function) share_chhand },
  { "chinfo", (Function) share_chinfo },
  { "chpass", (Function) share_chpass },
  { "clrxtra", (Function) share_clrxtra },
  { "end", (Function) share_end },
  { "killuser", (Function) share_killuser },
  { "newuser", (Function) share_newuser },
  { "resync!", (Function) share_resync },
  { "resync-no", (Function) share_resync_no },
  { "resync?", (Function) share_resyncq },
  { "stick", (Function) share_stick },
  { "uf-no", (Function) share_ufno },
  { "uf-yes", (Function) share_ufyes },
  { "ufsend", (Function) share_ufsend },
  { "userfile?", (Function) share_userfileq },
  { "version", (Function) share_version },
  { 0, 0 }
};

static void sharein_mod (int idx, char * msg) {
   char code[512];
   int f,i;
   modcontext;
   nsplit(code, msg);
   f = 0;
   i = 0;
   while ((C_share[i].name != NULL) && (!f)) {
      int y = strcasecmp(code, C_share[i].name);
      
      if (y == 0) {
	 /* found a match */
	 (C_share[i].func) (idx, msg);
	 f = 1;
      } else if (y < 0)
	   return;
      i++;
   }
}

static void shareout_mod(va_alist)
va_dcl
{
   int i;
   va_list va;
   char *format;
   char s[601];
   struct chanset_t * chan;
   va_start(va);
   chan = va_arg(va, struct chanset_t *);
   if (!chan || (chan->stat & CHAN_SHARED)) {
      format = va_arg(va, char *);
      strcpy(s,"share ");
      vsprintf(s+6, format, va);
      for (i = 0; i < dcc_total; i++)
	if ((dcc[i].type == &DCC_BOT) &&
	    (dcc[i].u.bot->status & STAT_SHARE) &&
	    (!(dcc[i].u.bot->status & STAT_GETTING)) &&
	    (!(dcc[i].u.bot->status & STAT_SENDING)) &&
	    (!chan || 
	     (get_chanattr_handle(dcc[i].nick,chan->name) & CHANBOT_SHARE)))
	  tputs(dcc[i].sock, s, strlen(s));
      q_resync(s,chan);
   }
   va_end(va);
}

static void shareout_but(va_alist)
va_dcl
{
   int i, x;
   va_list va;
   char *format;
   char s[601];
   struct chanset_t * chan;
   va_start(va);
   chan = va_arg(va, struct chanset_t *);
   x = va_arg(va, int);
   format = va_arg(va, char *);
   strcpy(s,"share ");
   vsprintf(s+6, format, va);
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_BOT) && (i != x) &&
	  (dcc[i].u.bot->status & STAT_SHARE) &&
	  (!(dcc[i].u.bot->status & STAT_GETTING)) &&
	  (!(dcc[i].u.bot->status & STAT_SENDING)) &&
	  (!chan ||
	     (get_chanattr_handle(dcc[i].nick,chan->name) & CHANBOT_SHARE)))
	 tputs(dcc[i].sock, s, strlen(s));
   q_resync(s,chan);
   va_end(va);
}
/***** RESYNC BUFFERS *****/

/* create a tandem buffer for 'bot' */
static void new_tbuf (char * bot)
{
   int i;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0] == 0) {
	 /* this one is empty */
	 strcpy(tbuf[i].bot, bot);
	 tbuf[i].q = NULL;
	 tbuf[i].timer = time(NULL);
	 putlog(LOG_MISC, "*", "Creating resync buffer for %s", bot);
	 return;
      }
}

/* flush a certain bot's tbuf */
static int flush_tbuf (char * bot)
{
   int i;
   struct msgq *q;
   for (i = 0; i < 5; i++)
      if (strcasecmp(tbuf[i].bot, bot) == 0) {
	 while (tbuf[i].q != NULL) {
	    q = tbuf[i].q;
	    tbuf[i].q = tbuf[i].q->next;
	    modfree(q->msg);
	    modfree(q);
	 }
	 tbuf[i].bot[0] = 0;
	 return 1;
      }
   return 0;
}

/* flush all tbufs older than 15 minutes */
static void check_expired_tbufs()
{
   int i;
   struct msgq *q;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0]) {
	 if (now - tbuf[i].timer > 900) {
	    /* EXPIRED */
	    while (tbuf[i].q != NULL) {
	       q = tbuf[i].q;
	       tbuf[i].q = tbuf[i].q->next;
	       modfree(q->msg);
	       modfree(q);
	    }
	    putlog(LOG_MISC, "*", "Flushing resync buffer for clonebot %s.",
		   tbuf[i].bot);
	    tbuf[i].bot[0] = 0;
	 }
      }

}

static struct msgq *q_addmsg (struct msgq * qq, struct chanset_t * chan, char * s)
{
   struct msgq *q;
   int cnt;
   if (qq == NULL) {
      q = (struct msgq *) modmalloc(sizeof(struct msgq));
      q->chan = chan;
      q->next = NULL;
      q->msg = (char *) modmalloc(strlen(s) + 1);
      strcpy(q->msg, s);
      return q;
   }
   cnt = 0;
   q = qq;
   while (q->next != NULL) {
      q = q->next;
      cnt++;
   }
   if (cnt > maxqmsg)
      return NULL;		/* return null: did not alter queue */
   q->next = (struct msgq *) modmalloc(sizeof(struct msgq));
   q = q->next;
   q->chan = chan;
   q->next = NULL;
   q->msg = (char *) modmalloc(strlen(s) + 1);
   strcpy(q->msg, s);
   return qq;
}

/* add stuff to a specific bot's tbuf */
static void q_tbuf (char * bot, char * s, struct chanset_t * chan)
{
   int i;
   struct msgq *q;
   for (i = 0; i < 5; i++)
      if ((strcasecmp(tbuf[i].bot, bot) == 0) && (!chan ||
	 (get_chanattr_handle(tbuf[i].bot,chan->name)&CHANBOT_SHARE))) {
	 q = q_addmsg(tbuf[i].q, chan, s);
	 if (q != NULL)
	   tbuf[i].q = q;
      }
}

/* add stuff to the resync buffers */
static void q_resync (char * s, struct chanset_t * chan)
{
   int i;
   struct msgq *q;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0] && (!chan || 
	  (get_chanattr_handle(tbuf[i].bot,chan->name)&CHANBOT_SHARE))) {
	 q = q_addmsg(tbuf[i].q, chan, s);
	 if (q != NULL)
	    tbuf[i].q = q;
      }
}

/* is bot in resync list? */
static int can_resync (char * bot)
{
   int i;
   for (i = 0; i < 5; i++)
      if (strcasecmp(bot, tbuf[i].bot) == 0)
	 return 1;
   return 0;
}

/* dump the resync buffer for a bot */
static void dump_resync (int z, char * bot)
{
   int i;
   struct msgq *q;
   for (i = 0; i < 5; i++)
      if (strcasecmp(bot, tbuf[i].bot) == 0) {
	 while (tbuf[i].q != NULL) {
	    q = tbuf[i].q;
	    tbuf[i].q = tbuf[i].q->next;
	    tprintf(z, "%s", q->msg);
	    modfree(q->msg);
	    modfree(q);
	 }
	 tbuf[i].bot[0] = 0;
	 return;
      }
}

/* give status report on tbufs */
static void status_tbufs (int idx)
{
   int i, count;
   struct msgq *q;
   char s[121];
   s[0] = 0;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0]) {
	 strcat(s, tbuf[i].bot);
	 count = 0;
	 q = tbuf[i].q;
	 while (q != NULL) {
	    count++;
	    q = q->next;
	 }
	 sprintf(&s[strlen(s)], " (%d), ", count);
      }
   if (s[0]) {
      s[strlen(s) - 2] = 0;
      dprintf(idx, "Pending sharebot buffers: %s\n", s);
   }
}

static int write_tmp_userfile (char * fn, struct userrec * bu)
{
   FILE *f;
   struct userrec *u;
   int ok;
   f = fopen(fn, "w");
   chmod(fn, 0600);		/* make it -rw------- */
   if (f == NULL) {
      putlog(LOG_MISC, "*", USERF_ERRWRITE2);
      return 0;
   }
   fprintf(f, "#3v: %s -- %s -- transmit\n", ver, origbotname);
   ok = 1;
   u = bu;
   while ((u != NULL) && (ok)) {
      ok = write_user(u, f, 1);
      u = u->next;
   }
   ok = write_chanbans(f);
   fclose(f);
   if (!ok) {
      putlog(LOG_MISC, "*", USERF_ERRWRITE2);
      return 0;
   }
   return ok;
}


/* create a copy of the entire userlist (for sending user lists to
   clone bots) -- userlist is reversed in the process, which is OK
   because the receiving bot reverses the list AGAIN when saving */
/* t=1: copy only tandem-bots  --  t=0: copy everything BUT tandem-bots */
static struct userrec *dup_userlist (int t)
{
   struct userrec *u, *u1, *retu, *nu;
   struct eggqueue *q;
   struct chanuserrec *ch;
   nu = retu = NULL;
   u = userlist;
   modcontext;
   noshare = 1;
   while (u != NULL) {
      if (((u->flags & (USER_BOT | USER_UNSHARED)) && (t)) ||
	  (!(u->flags & (USER_BOT | USER_UNSHARED)) && (!t))) {
	 u1 = adduser(NULL,u->handle,"",u->pass, u->flags);
	 if (nu == NULL)
	    nu = retu = u1;
	 else {
	    nu->next = u1;
	    nu = nu->next;
	 }
	 /* u1->next=nu; nu=u1; */
	 nu->upload_k = u->upload_k;
	 nu->uploads = u->uploads;
	 nu->dnload_k = u->dnload_k;
	 nu->dnloads = u->dnloads;
	 q = u->host;
	 nu->host = NULL;
	 while (q != NULL) {
	    nu->host = add_q(q->item, nu->host);
	    q = q->next;
	 }
	 ch = u->chanrec;
	 nu->chanrec = NULL;
	 while (ch != NULL) {
	    struct chanuserrec *z;
	    z = add_chanrec(nu,ch->channel,ch->flags,ch->laston);
	    set_handle_chaninfo(nu,nu->handle,ch->channel,ch->info);
	    ch = ch->next;
	 }
	 set_handle_email(nu,nu->handle,u->email);
	 set_handle_dccdir(nu,nu->handle,u->dccdir);
	 set_handle_comment(nu,nu->handle,u->comment);
	 set_handle_info(nu,nu->handle,u->info);
	 set_handle_xtra(nu,nu->handle,u->xtra);
	 nu->lastonchan = NULL;
	 if (u->lastonchan) {
	    touch_laston(nu, u->lastonchan, u->laston);
	 } else
	    touch_laston(nu, 0, 1);
      }
      u = u->next;
   }
   noshare = 0;
   return retu;
}

static void restore_chandata()
{
   FILE *f;
   struct userrec *tbu = NULL;
   struct chanset_t *cst;
   char s[181], hand[181], code[181];
   modcontext;
   f = fopen(userfile, "r");
   if (f == NULL) {
      putlog(LOG_MISC, "*", "* %s", USERF_BADREREAD);
      return;
   }
   fgets(s, 180, f);
   /* Disregard opening statement.  We already know it should be good */
   while (!feof(f)) {
      fgets(s, 180, f);
      if (!feof(f)) {
	 rmspace(s);
	 if ((s[0] != '#') && (s[0] != ';') && (s[0])) {
	    nsplit(code, s);
	    rmspace(code);
	    rmspace(s);
	    if (strcasecmp(code, "!") == 0) {
	       if ((hand[0]) && (tbu != NULL)) {
		  char chname[181], st[181], fl[181];
		  int flags;
		  time_t last;
		  struct chanuserrec *cr = NULL;
		  nsplit(chname, s);
		  rmspace(chname);
		  rmspace(s);
		  nsplit(st, s);
		  rmspace(st);
		  rmspace(s);
		  nsplit(fl, s);
		  rmspace(fl);
		  rmspace(s);
		  flags = str2chflags(fl);
		  last = (time_t) atol(st);
		  if (defined_channel(chname)) {
		     cst = findchan(chname);
		     if (!(cst->stat & CHAN_SHARED)) {
			cr = get_chanrec(tbu, chname);
			if (cr == NULL) {
			   add_chanrec_by_handle(tbu, hand, chname, 
							flags, last);
			   if (s[0])
			      set_handle_chaninfo(tbu, hand, chname, s);
			} else {
			   cr->flags = flags;
			   cr->laston = last;
			   if (s[0])
			      set_handle_chaninfo(tbu, hand, chname, s);
			}
		     }
		  }
	       }
	    } else if ((strcasecmp(code, "-") == 0) || 
		       (strcasecmp(code, "+") == 0) ||
		       (strcasecmp(code, "*") == 0) || 
		       (strcasecmp(code, "=") == 0) ||
		       (strcasecmp(code, ":") == 0) || 
		       (strcasecmp(code, ".") == 0) ||
		       (strcasecmp(code, "!!") == 0) || 
		       (strcasecmp(code, "::") == 0)) {
	       /* do nothing */
	    } else {
	       strcpy(hand, code);
	       tbu = get_user_by_handle(userlist, hand);
	    }
	 }
      }
   }
   modcontext;
}

/* erase old user list, switch to new one */
static void finish_share (int idx)
{
   struct userrec *u;
   int i, j = -1;
   for (i = 0; i < dcc_total; i++)
      if ((strcasecmp(dcc[i].nick, dcc[idx].host) == 0) &&
	  (dcc[i].type == &DCC_BOT))
	 j = i;
   if (j == -1)
      return;			/* oh well. */
   dcc[j].u.bot->status &= ~STAT_GETTING;
   /* copy the bots over */
   u = dup_userlist(1);
   /* read the rest in */
   if (!readuserfile(dcc[idx].u.xfer->filename, &u)) {
      putlog(LOG_MISC, "*", "%s",USERF_CANTREAD);
      return;
   }
   putlog(LOG_MISC, "*", "%s.", USERF_XFERDONE);
   modcontext;
   clear_userlist(userlist);
   userlist = u;
   modcontext;
   restore_chandata();
   modcontext;
   unlink(dcc[idx].u.xfer->filename);	/* done with you! */
   modcontext;
   reaffirm_owners();		/* make sure my owners are +n */
   modcontext;
   clear_chanlist();
   lastuser = banu = ignu = NULL;
   updatebot(dcc[j].nick,'+',0);
   modcontext;
}

/* begin the user transfer process */
static void start_sending_users (int idx)
{
   struct userrec *u;
   char s[161], s1[64];
   int i = 1;
   struct eggqueue *q;
   struct chanuserrec *ch;
   struct chanset_t *cst;
   
   modcontext;
   sprintf(s, ".share.user%lu", time(NULL));
   updatebot(dcc[idx].nick,'+',0);
   u = dup_userlist(0);		/* only non-bots */
   write_tmp_userfile(s, u);
   clear_userlist(u);
   i = raw_dcc_send(s, "*users", "(users)", s);
   if (i > 0) {			/* abort */
      unlink(s);
      tprintf(dcc[idx].sock, "error %s\n", USERF_CANTSEND);
      putlog(LOG_MISC, "*", "NO MORE DCC CONNECTIONS -- can't send userfile");
      dcc[idx].u.bot->status &= ~STAT_SHARE;
      return;
   }
   dcc[idx].u.bot->status |= STAT_SENDING;
   i = dcc_total - 1;
   strcpy(dcc[i].host, dcc[idx].nick);	/* store bot's nick */
   tprintf(dcc[idx].sock, "share ufsend %lu %d %lu\n", 
	   iptolong(natip[0]?(IP) inet_addr(natip):getmyip()), dcc[i].port,
	   dcc[i].u.xfer->length);
   /* start up a tbuf to queue outgoing changes for this bot until the */
   /* userlist is done transferring */
   new_tbuf(dcc[idx].nick);
   /* immediately, queue bot hostmasks & addresses (jump-start) */
   u = userlist;
   while (u != NULL) {
      if ((u->flags & USER_BOT) && !(u->flags & USER_UNSHARED)) {
	 /* send hostmasks */
	 q = u->host;
	 while (q != NULL) {
	    if (strcmp(q->item, "none") != 0) {
	       sprintf(s, "share +bothost %s %s\n", u->handle, q->item);
	       q_tbuf(dcc[idx].nick, s,NULL);
	    }
	    q = q->next;
	 }
	 /* send address */
	 sprintf(s, "share chaddr %s %s\n", u->handle, u->info);
	 q_tbuf(dcc[idx].nick, s, NULL);
	 /* send user-flags */
	 flags2str((u->flags & BOT_MASK), s1);
	 sprintf(s, "share chattr %s %s\n", u->handle, s1);
	 q_tbuf(dcc[idx].nick, s, NULL);
	 ch = u->chanrec;
	 while (ch) {
	    if (ch->flags) {
	       cst = findchan(ch->channel);
	       if (cst && (cst->stat & CHAN_SHARED)) {
		  chflags2str(ch->flags, s1);
		  sprintf(s, "share chattr %s %s %s\n", u->handle, s1, ch->channel);
		  q_tbuf(dcc[idx].nick, s, cst);
	       }
	    }
	    ch = ch->next;
	 }
      }
      u = u->next;
   }
   /* wish could unlink the file here to avoid possibly leaving it lying */
   /* around, but that messes up NFS clients. */
   modcontext;
}

static void (*def_dcc_bot_kill)(int) = 0;

static void cancel_user_xfer (int idx)
{
   int i, j, k = 0;
   modcontext;
   if (idx < 0) {
      idx = -idx;
      k = 1;
      updatebot(dcc[idx].nick,'-',0);
   }
   flush_tbuf(dcc[idx].nick);
   if (dcc[idx].u.bot->status & STAT_SHARE) {
      if (dcc[idx].u.bot->status & STAT_GETTING) {
	 j = 0;
	 for (i = 0; i < dcc_total; i++)
	    if ((strcasecmp(dcc[i].host, dcc[idx].nick) == 0) &&
		((dcc[i].type == &DCC_SEND) ||
		 (dcc[i].type == &DCC_FORK_SEND)))
	       j = i;
	 if (j != 0) {
	    killsock(dcc[j].sock);
	    dcc[j].sock = (long)dcc[j].type;
	    dcc[j].type = &DCC_LOST;
	    unlink(dcc[j].u.xfer->filename);
	 }
	 putlog(LOG_BOTS, "*", "(Userlist download aborted.)");
      }
      if (dcc[idx].u.bot->status & STAT_SENDING) {
	 j = 0;
	 for (i = 0; i < dcc_total; i++)
	    if ((strcasecmp(dcc[i].host, dcc[idx].nick) == 0) &&
		((dcc[i].type == &DCC_GET) 
		 || (dcc[i].type == &DCC_GET_PENDING)))
	     j = i;
	 if (j != 0) {
	    killsock(dcc[j].sock);
	    dcc[j].sock = (long)dcc[j].type;
	    dcc[j].type = &DCC_LOST;
	    unlink(dcc[j].u.xfer->filename);
	 }
	 putlog(LOG_BOTS, "*", "(Userlist transmit aborted.)");
      }
      if (allow_resync && (!(dcc[idx].u.bot->status & STAT_GETTING)) &&
	  (!(dcc[idx].u.bot->status & STAT_SENDING)))
	 new_tbuf(dcc[idx].nick);
   }
   modcontext;
   if (!k)
     def_dcc_bot_kill(idx);
   modcontext;
}


static tcl_ints my_ints [] = {
   {"passive", &passive },
   {"allow-resync", &allow_resync },
   {"private-owner", &private_owner },
   { 0, 0 }
};

static void cmd_flush (int idx, char * par)
{
   if (!par[0]) {
      dprintf(idx, "Usage: flush <botname>\n");
      return;
   }
   if (flush_tbuf(par))
      dprintf(idx, "Flushed resync buffer for %s\n", par);
   else
      dprintf(idx, "There is no resync buffer for that bot.\n");
}

static cmd_t my_cmds[] = {
  { "flush", "m", (Function)cmd_flush, NULL },
  { 0, 0, 0, 0 }
};

static void read_userfile () {
   int i;
   
   if ((!noshare) && (!passive)) {
      for (i = 0; i < dcc_total; i++)
	if ((dcc[i].type == &DCC_BOT) 
	    && (dcc[i].u.bot->status & STAT_SHARE)) {
	   /* cancel any existing transfers */
	     if (dcc[i].u.bot->status & STAT_SENDING)
	       cancel_user_xfer(-i);
	   tprintf(dcc[i].sock, "share userfile?\n");
	   dcc[i].u.bot->status |= STAT_OFFERED;
	}
   }
}

static char *share_close()
{
   p_tcl_hash_list H_dcc;
   int i;
   
   modcontext;
   module_undepend(MODULE_NAME);
   modcontext;
   putlog(LOG_MISC|LOG_BOTS,"*","Unloaded sharing module, flushing tbuf's...");
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0])
	 flush_tbuf(tbuf[i].bot);
   putlog(LOG_MISC|LOG_BOTS,"*","Sending 'share end' to all sharebots...");
   for (i = 0;i < dcc_total;i++)
     if ((dcc[i].type == &DCC_BOT) 
	 && (dcc[i].u.bot->status & STAT_SHARE)) {
	dprintf(i,"share end\n");
	cancel_user_xfer(-i);
	dcc[i].u.bot->status &= 
	  ~(STAT_SHARE|STAT_GETTING|STAT_SENDING|STAT_OFFERED);
     }
   del_hook(HOOK_SHAREOUT,shareout_mod);
   del_hook(HOOK_SHAREIN,sharein_mod);
   del_hook(HOOK_MINUTELY,check_expired_tbufs);
   del_hook(HOOK_READ_USERFILE,read_userfile);
   DCC_BOT.kill = def_dcc_bot_kill;
   rem_tcl_ints(my_ints);
   H_dcc = find_hash_table("dcc");
   rem_builtins(H_dcc,my_cmds);
   return NULL;
}

static int share_expmem()
{
   int i, tot = 0;
   for (i = 0; i < 5; i++) {
      if (tbuf[i].bot[0]) {
	 struct msgq * q = tbuf[i].q;
	 while (q) {
	    tot += sizeof(struct msgq);
	    tot += strlen(q->msg)+1;
	    q = q->next;
	 }
      }
   }
   return tot;
}

static void share_report (int idx)
{
   modprintf(idx,"   Share module, using %d bytes, %s sharing.\n",share_expmem(),
	     passive? "passive": "aggressive");
   status_tbufs(idx);
}

char *share_start ();
static Function share_table[] =
{
   (Function) share_start,
   (Function) share_close,
   (Function) share_expmem,
   (Function) share_report,
     
   (Function) finish_share,
   (Function) dump_resync,
};

char *share_start ()
{
   p_tcl_hash_list H_dcc;
   int i;
   
   module_register(MODULE_NAME, share_table, 1, 0);
   module_depend(MODULE_NAME, "transfer", 1, 1);
   add_hook(HOOK_SHAREOUT,shareout_mod);
   add_hook(HOOK_SHAREIN,sharein_mod);
   add_hook(HOOK_MINUTELY,check_expired_tbufs);
   add_hook(HOOK_READ_USERFILE,read_userfile);
   for (i = 0; i < 5; i++) {
      tbuf[i].q = NULL;
      tbuf[i].bot[0] = 0;
   }
   def_dcc_bot_kill = DCC_BOT.kill;
   DCC_BOT.kill = cancel_user_xfer;
   add_tcl_ints(my_ints);
   H_dcc = find_hash_table("dcc");
   add_builtins(H_dcc,my_cmds);
   return NULL;
}
