#define MODULE_NAME "share"
#include "../module.h"
#include "../../chan.h"
#include "../../users.h"
#include <varargs.h>
#include "../transfer.mod/transfer.h"
#include "../channels.mod/channels.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

static const int min_share = 1029900;	/* minimum version I will share with */
static int private_owner = 1;
static int allow_resync = 0;
static struct flag_record fr = {0,0,0,0,0,0};
static int resync_time = 900;

static Function * global = NULL, * transfer_funcs = NULL, * channels_funcs = NULL;

static void start_sending_users(int);
static void shareout_but ();
static int flush_tbuf (char * bot);
static int can_resync (char * bot);
static void dump_resync (int idx);
static void q_resync (char * s, struct chanset_t * chan);
static void cancel_user_xfer(int, void *);

/* store info for sharebots */
struct share_msgq {
   struct chanset_t * chan;
   char *msg;
   struct share_msgq *next;
};

static struct tandbuf {
   char bot[HANDLEN+1];
   time_t timer;
   struct share_msgq *q;
} tbuf[5];

/* botnet commands */

static void share_stick (int idx, char * par)
{
   char *host, *val;
   int yn;

   host = newsplit(&par);
   val = newsplit(&par);
   yn = atoi(val);
   noshare = 1;
   if (!par[0]) {		/* global ban */
      if (u_setsticky_ban(NULL,host, yn) > 0) {
	 putlog(LOG_CMDS, "*", "%s: stick %s %c", dcc[idx].nick, host, yn ? 'y' : 'n');
	 shareout_but(NULL,idx,"s %s %d\n", host, yn);
      }
   } else {
      struct chanset_t *chan = findchan(par);
      struct chanuserrec * cr;
      
      if ((chan != NULL) && channel_shared(chan) &&
	  (cr = get_chanrec(dcc[idx].user,par)) && (cr->flags & BOT_SHARE)) 
	if (u_setsticky_ban(chan, host, yn) > 0) {
	   putlog(LOG_CMDS, "*", "%s: stick %s %c %s", dcc[idx].nick, host,
		  yn ? 'y' : 'n', par);
	   shareout_but(chan,idx,"s %s %d %s\n", host, yn, chan->name);
	   noshare = 0;
	   return;
	}
      putlog(LOG_CMDS,"*","Rejecting invalid sticky ban: %s on %s, %c",
	     host,par,yn?'y':'n');
   }
   noshare = 0;
}

#define CHKSEND if (!(dcc[idx].status&STAT_SENDING)) return
#define CHKGET if (!(dcc[idx].status&STAT_GETTING)) return
#define CHKSHARE if (!(dcc[idx].status&STAT_SHARE)) return

static void share_chhand (int idx, char * par)
{
   char *hand;
   int i;
   struct userrec * u;
   
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist,hand);
   if (!u || (u->flags & USER_UNSHARED))
     return;   
   shareout_but(NULL,idx, "h %s %s\n", hand, par);
   noshare = 1;
   i = change_handle(u, par);
   noshare = 0;
   if (i)
     putlog(LOG_CMDS, "*", "%s: handle %s->%s", dcc[idx].nick, hand, par);
}

static void share_chattr (int idx, char * par)
{
   char *hand, *atr, s[100];
   struct chanset_t *cst;
   struct userrec * u;
   struct flag_record fr2;
   int bfl;
   module_entry * me;
   
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist,hand);
   if (!u || (u->flags & USER_UNSHARED) || 
       ((u->flags & USER_BOT) && (bot_flags(u) & BOT_SHARE)))
     return;   
   atr = newsplit(&par);
   cst = findchan(par);
   if (cst && !channel_shared(cst))
     return;
   if (!(dcc[idx].status & STAT_GETTING))
     shareout_but(cst,idx, "a %s %s %s\n", hand, atr, par);
   noshare = 1;
   if (par[0] && cst) {
      get_user_flagrec(dcc[idx].user,&fr,par);
      if (fr.chan & BOT_SHARE) { 
	 fr2.match = FR_CHAN;
	 break_down_flags(atr,&fr,0);
	 get_user_flagrec(u,&fr2,par);
	 fr.chan = (fr2.chan & BOT_AGGRESSIVE) | (fr.chan & ~BOT_AGGRESSIVE);
	 set_user_flagrec(u,&fr,par);
	 noshare = 0;
	 build_flags(s,&fr,0);
	 if (!(dcc[idx].status & STAT_GETTING))
	   putlog(LOG_CMDS, "*", "%s: chattr %s %s %s",
		  dcc[idx].nick, hand, s, par);
	 if ((me = module_find("irc",1,1))) {
	    Function * func = me->funcs;
	    (func[15])(cst,0);
	 }
      } else
	putlog(LOG_CMDS, "*", "Rejected info for unshared channel %s from %s",
	       par, dcc[idx].nick);
      return;
   }
   /* don't let bot flags be altered */
   fr.match= FR_GLOBAL;
   break_down_flags(atr,&fr,0);
   bfl = fr.global & USER_BOT;
   if (private_owner) 
      fr.global = (fr.global & ~USER_OWNER) | (u->flags & USER_OWNER);
   fr.global |= bfl;
   set_user_flagrec(u,&fr,0);
   noshare = 0;
   build_flags(s,&fr,0);
   fr.match = FR_CHAN;
   if (!(dcc[idx].status & STAT_GETTING))
     putlog(LOG_CMDS, "*", "%s: chattr %s %s", dcc[idx].nick, hand, s);
   if ((me = module_find("irc",1,1))) {
      Function * func = me->funcs;
      for (cst = chanset;cst;cst = cst->next)
	(func[15])(cst,0);
   }
}

static void share_pls_chrec (int idx, char * par) {
   char *user;
   struct chanset_t * chan;
   struct userrec * u;
   
   CHKSHARE;
   user = newsplit(&par);
   u = get_user_by_handle(userlist,user);
   if (!u) 
     return;
   chan = findchan(par);
   fr.match = FR_CHAN;
   get_user_flagrec(dcc[idx].user,&fr,par);
   if (!chan || !channel_shared(chan) ||
       !(fr.chan & BOT_SHARE)) {
      putlog(LOG_CMDS, "*", "Rejected info for unshared channel %s from %s",
	     par, dcc[idx].nick);
      return;
   }
   noshare = 1;
   shareout_but(chan,idx, "+cr %s %s\n", user, par);
   if (get_chanrec(u,par))
     return;
   add_chanrec(u,par);
   putlog(LOG_CMDS, "*", "%s: +chrec %s %s", dcc[idx].nick, user, par);
   noshare = 0;
}

static void share_mns_chrec (int idx, char * par) {
   char *user;
   struct chanset_t * chan;
   struct userrec * u;
   
   CHKSHARE;
   user = newsplit(&par);
   u = get_user_by_handle(userlist,user);
   if (u == NULL) 
     return;
   chan = findchan(par);
   fr.match = FR_CHAN;
   get_user_flagrec(dcc[idx].user,&fr,par);
   if ((chan == NULL) || !channel_shared(chan) ||
       !(fr.chan & BOT_SHARE)) {
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
   char *etc, *etc2, *etc3;
   struct userrec * u;
   
   CHKSHARE;
   etc = newsplit(&par);
   u = get_user_by_handle(userlist,etc);
   if (u && (u->flags & USER_UNSHARED))
     return;
   shareout_but(NULL,idx, "n %s %s\n", etc, par);
   /* If user already exists, ignore command */
   if (u)
      return;
   noshare = 1;
   if (strlen(etc) > HANDLEN)
     etc[HANDLEN] = 0;
   etc2 = newsplit(&par);
   etc3 = newsplit(&par);
   fr.match = FR_GLOBAL;
   break_down_flags(par,&fr,NULL);
   userlist = adduser(userlist, etc, etc2, etc3, fr.global);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: newuser %s", dcc[idx].nick, etc);
}

static void share_killuser (int idx, char * par)
{
   struct userrec * u = get_user_by_handle(userlist,par);
   
   CHKSHARE;
   /* If user is a share bot, ignore command */
   if (u && !(u->flags & USER_UNSHARED) &&
       !((u->flags & USER_BOT) && (bot_flags(u) & BOT_SHARE))) {
      noshare = 1;
      if (deluser(par)) {
	 shareout_but(NULL,idx, "k %s\n", par);
	 putlog(LOG_CMDS, "*", "%s: killuser %s", dcc[idx].nick, par);
      }
      noshare = 0;
   }
}

static void share_pls_host (int idx, char * par)
{
   char *hand;
   struct userrec * u;
   
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist,hand);
   if (u->flags & USER_UNSHARED)
     return;   
   shareout_but(NULL,idx, "+h %s %s\n", hand, par);
   set_user(&USERENTRY_HOSTS,u,par);
   putlog(LOG_CMDS, "*", "%s: +host %s %s", dcc[idx].nick, hand, par);
}

static void share_pls_bothost (int idx, char * par)
{
   char *hand, p[32];
   struct userrec * u;
   
   context;
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist,hand);
   if (u && (u->flags & USER_UNSHARED))
     return;   
   if (!(dcc[idx].status & STAT_GETTING))
     shareout_but(NULL,idx, "+bh %s %s\n", hand, par);
   /* add bot to userlist if not there */
   if (u) {
      if (!(u->flags & USER_BOT))
	 return;		/* ignore */
      set_user(&USERENTRY_HOSTS,u,par);
   } else {
      makepass(p);
      userlist = adduser(userlist, hand, par, p, USER_BOT);
   }
   if (!(dcc[idx].status & STAT_GETTING))
     putlog(LOG_CMDS, "*", "%s: +bothost %s %s", dcc[idx].nick, hand, par);
}

static void share_mns_host (int idx, char * par)
{
   char *hand;
   struct userrec * u;

   context;
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist,hand);
   if (!u || (u->flags & USER_UNSHARED) ||
       ((u->flags & USER_BOT) && (bot_flags(u) & BOT_SHARE)))
     return;   
   shareout_but(NULL,idx, "-h %s %s\n", hand, par);
   noshare = 1;
   /* If user is a share bot, ignore command */
   delhost_by_handle(hand, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: -host %s %s", dcc[idx].nick, hand, par);
}

static void share_change (int idx, char * par)
{
   char * key, *hand;
   struct userrec * u;
   struct user_entry_type * uet;
   struct user_entry * e;
   
   CHKSHARE;
   key = newsplit(&par);
   hand = newsplit(&par);
   u = get_user_by_handle(userlist, hand);
   if (u && (u->flags & USER_UNSHARED))
     return;   
   uet = find_entry_type(key);
   if (!uet) { /* if it's not a supported type, forget it */
      putlog(LOG_CMDS,"*","Ignore ch %s from %s (unknown type)",
	     key,dcc[idx].nick);
      return;
   }
   if (!(dcc[idx].status & STAT_GETTING))
     shareout_but(NULL,idx, "c %s %s %s\n",key, hand, par);
   noshare = 1;
   if (!u && (uet == &USERENTRY_BOTADDR)) {
      char pass[30];
      makepass(pass);
      userlist = adduser(userlist, hand, "none", pass, USER_BOT);
      u = get_user_by_handle(userlist, hand);
   } else if (!u)
	return;
   if (uet->got_share) {
      if (!(e = find_user_entry(uet,u))) {
	 e = user_malloc(sizeof(struct user_entry));
	 e->type = uet;
	 e->name = NULL;
	 e->u.list = NULL;
	 list_insert((&(u->entries)),e);
      }
      uet->got_share(u,e,par,idx);
      if (!e->u.list) {
	 list_delete((struct list_type **)&(u->entries),(struct list_type *)e);
	 nfree(e);
      }
   }
   noshare = 0;
}

static void share_chchinfo (int idx, char * par)
{
   char *hand, *chan;
   struct chanset_t *cst;
   struct userrec * u;
   
   CHKSHARE;
   hand = newsplit(&par);
   u = get_user_by_handle(userlist, hand);
   if (u && (u->flags & USER_UNSHARED))
     return;   
   chan = newsplit(&par);
   cst = findchan(chan);
   fr.match = FR_CHAN;
   get_user_flagrec(dcc[idx].user,&fr,chan);
   if (!channel_shared(cst) || !(fr.chan & BOT_SHARE)) {
      putlog(LOG_CMDS, "*", 
	     "Info line change from %s denied.  Channel %s not shared.",
	     dcc[idx].nick, chan);
      return;
   }
   shareout_but(cst,idx, "chchinfo %s %s\n", hand, par);
   noshare = 1;
   set_handle_chaninfo(userlist, hand, chan, par);
   noshare = 0;
   putlog(LOG_CMDS, "*", "%s: change info %s %s", dcc[idx].nick, chan, hand);
}

static void share_mns_ban (int idx, char * par)
{
   CHKSHARE;
   shareout_but(NULL,idx, "-b %s\n", par);
   putlog(LOG_CMDS, "*", "%s: cancel ban %s", dcc[idx].nick, par);
   noshare = 1;
   u_delban(NULL,par,1);
   noshare = 0;
}

static void share_mns_banchan (int idx, char * par)
{
   char *chname;
   struct chanset_t *chan;
   
   CHKSHARE;
   chname = newsplit(&par);
   chan = findchan(chname);
   fr.match = FR_CHAN;
   get_user_flagrec(dcc[idx].user,&fr,chname);
   if (!chan || !(fr.chan&BOT_SHARE))
     return;
   shareout_but(chan,idx, "-bc %s %s\n", chname, par);
   if (channel_shared(chan)) {
      putlog(LOG_CMDS, "*", "%s: cancel ban %s on %s", dcc[idx].nick, par, chname);
      noshare = 1;
      u_delban(chan, par,1);
      noshare = 0;
   } 
}

static void share_mns_ignore (int idx, char * par)
{
   CHKSHARE;
   shareout_but(NULL,idx, "-i %s\n", par);
   putlog(LOG_CMDS, "*", "%s: cancel ignore %s", dcc[idx].nick, par);
   noshare = 1;
   delignore(par);
   noshare = 0;
}

static void share_pls_ban (int idx, char * par)
{
   time_t expire_time;
   char *ban, *tm, *from;
   int flags = 0;
   
   CHKSHARE;
   shareout_but(NULL,idx, "+b %s\n", par);
   noshare = 1;
   ban = newsplit(&par);
   tm = newsplit(&par);
   from = newsplit(&par);
   if (strchr(from,'s'))
     flags |= BANREC_STICKY;
   if (strchr(from,'p'))
     flags |= BANREC_PERM;
   from = newsplit(&par);
   expire_time = (time_t) atoi(tm);
   if (expire_time != 0L)
     expire_time += now;
   u_addban(NULL,ban, from, par, expire_time,flags);
   putlog(LOG_CMDS, "*", "%s: global ban %s (%s:%s)", dcc[idx].nick, ban,
	  from, par);
   noshare = 0;
}

static void share_pls_banchan (int idx, char * par)
{
   time_t expire_time;
   int flags = 0;
   struct chanset_t *chan;
   char *ban, *tm, *chname, *from;

   CHKSHARE;
   ban = newsplit(&par);
   tm = newsplit(&par);
   chname = newsplit(&par);
   chan = findchan(chname);
   fr.match = FR_CHAN;
   get_user_flagrec(dcc[idx].user,&fr,chname);
   if (!chan || !channel_shared(chan) || !(fr.chan&BOT_SHARE)) {
      putlog(LOG_CMDS, "*", 
	     "Channel ban %s on %s rejected - channel not shared.",
	    ban, chan);
      return;
   }
   shareout_but(chan,idx, "+bc %s %s %s %s\n", ban, tm, chname, par);
   from = newsplit(&par);
   if (strchr(from,'s'))
     flags |= BANREC_STICKY;
   if (strchr(from,'p'))
     flags |= BANREC_PERM;
   from = newsplit(&par);
   putlog(LOG_CMDS, "*", "%s: ban %s on %s (%s:%s)", dcc[idx].nick, ban, chname,
	  from, par);
   noshare = 1;
   expire_time = (time_t) atoi(tm);
   if (expire_time != 0L)
     expire_time += now;
   u_addban(chan, ban, from, par, expire_time,flags);
   noshare = 0;
}

/* +ignore <host> +<seconds-left> <from> <note> */
static void share_pls_ignore (int idx, char * par)
{
   time_t expire_time;
   char *ign, *from, *ts;
   
   CHKSHARE;
   shareout_but(NULL,idx, "+i %s\n", par);
   noshare = 1;
   ign = newsplit(&par);
   ts = newsplit(&par);
   if (atoi(ts) == 0)
     expire_time = 0L;
   else
     expire_time = now + atoi(ts);
   from = newsplit(&par);
   if (strchr(from,'p'))
     expire_time = 0;
   from = newsplit(&par);
   if (strlen(from) > HANDLEN+1)
     from[HANDLEN+1] = 0;
   par[65] = 0;
   putlog(LOG_CMDS, "*", "%s: ignore %s (%s: %s)", 
	  dcc[idx].nick, ign, from, par);
   addignore(ign, from, par, expire_time);
   noshare = 0;
}

static void share_ufno (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "User file rejected by %s: %s", dcc[idx].nick, par);
   dcc[idx].status &= ~STAT_OFFERED;
   if (!(dcc[idx].status & STAT_GETTING)) {
      dcc[idx].status &= ~(STAT_SHARE|STAT_AGGRESSIVE);
   }
}

static void share_ufyes (int idx, char * par)
{
   context;
   if (dcc[idx].status & STAT_OFFERED) {
      dcc[idx].status &= ~STAT_OFFERED;
      dcc[idx].status |= STAT_SHARE;
      dcc[idx].status |= STAT_SENDING;
      start_sending_users(idx);
      putlog(LOG_BOTS, "*", "Sending user file send request to %s", dcc[idx].nick);
   }
}

static void share_userfileq (int idx, char * par)
{
   int ok = 1, i, bfl = bot_flags(dcc[idx].user);
   flush_tbuf(dcc[idx].nick);
   if (bfl & BOT_AGGRESSIVE)
     dprintf(idx, "s un I have you marked for Agressive sharing.\n");
   else if (!(bfl & BOT_PASSIVE))
     dprintf(idx, "s un You are not marked for sharing with me.\n");
   else if (min_share > dcc[idx].u.bot->numver)
     dprintf(idx,
	     "s un Your version is not high enough, need v%d.%d.%d\n",
	     (min_share / 1000000), (min_share / 10000) % 100, 
	     (min_share / 100) % 100);
   else {
      for (i = 0; i < dcc_total; i++)
	 if (dcc[i].type->flags & DCT_BOT) {
	    if ((dcc[i].status & STAT_SHARE) &&
		(dcc[i].status & STAT_AGGRESSIVE) && (i != idx))
	      ok = 0;
	 }
      if (!ok)
	 dprintf(idx, "s un Already sharing sharing.\n");
      else {
	 dprintf(idx, "s uy\n");
	 /* set stat-getting to astatic void race condition (robey 23jun96) */
	 dcc[idx].status |= STAT_SHARE | STAT_GETTING | STAT_AGGRESSIVE;
	 putlog(LOG_BOTS, "*", "Downloading user file from %s", dcc[idx].nick);
      }
   }
}

				 
/* ufsend <ip> <port> <length> */
static void share_ufsend (int idx, char * par)
{
   char *ip, *port;
   int i,sock;
   FILE * f;
   
   if (!(b_status(idx) & STAT_SHARE)) {
      dprintf(idx, "s e You didn't ask; you just started sending.\n");
      dprintf(idx, "s e Ask before sending the userfile.\n");
      zapfbot(idx);
      return;
   }
   if (dcc_total == max_dcc) {
      putlog(LOG_MISC, "*", "NO MORE DCC CONNECTIONS -- can't grab userfile");
      dprintf(idx, "s e I can't open a DCC to you; I'm full.\n");
      zapfbot(idx);
      return;
   }
   f = fopen(".share.users", "w");
   if (f == NULL) {
      putlog(LOG_MISC, "*", "CAN'T WRITE USERFILE DOWNLOAD FILE!");
      zapfbot(idx);
      return;
   }
   sock = getsock(SOCK_BINARY);
   ip = newsplit(&par);
   port = newsplit(&par);
   if (open_telnet_dcc(sock, ip, port) < 0) {
      putlog(LOG_MISC, "*", "Asynchronous connection failed!");
      dprintf(idx, "s e Can't connect to you!\n");
      zapfbot(idx);
   }
   i = new_dcc(&DCC_FORK_SEND,sizeof(struct xfer_info));
   dcc[i].addr = my_atoul(ip);
   dcc[i].port = atoi(port);
   dcc[i].sock = (-1);
   dcc[i].type = &DCC_FORK_SEND;
   strcpy(dcc[i].nick, "*users");
   strcpy(dcc[i].host, dcc[idx].nick);
   strcpy(dcc[i].u.xfer->filename, ".share.users");
   dcc[i].u.xfer->length = atoi(par);
   dcc[idx].status |= STAT_GETTING;
   dcc[i].u.xfer->f = f;
   /* don't buffer this */
   dcc[i].sock = sock;
}

static void share_resyncq (int idx, char * par)
{
   if (!allow_resync) {
      dprintf(idx, "s rn Not permitting resync.\n");
      return;
   } else {
      int bfl = bot_flags(dcc[idx].user);
      
      if (!(bfl & BOT_SHARE))
	dprintf(idx, "s rn You are not marked for sharing with me.\n");
      else if (can_resync(dcc[idx].nick)) {
	 dprintf(idx, "s r!\n");
	 dump_resync(idx);
	 dcc[idx].status &= ~STAT_OFFERED;
	 dcc[idx].status |= STAT_SHARE;
	 putlog(LOG_BOTS, "*", "Resync'd user file with %s", dcc[idx].nick);
	 updatebot(-1, dcc[idx].nick,'+',0);
      } else if (bfl & BOT_PASSIVE) {
	 dprintf(idx, "s r!\n");
	 dcc[idx].status &= ~STAT_OFFERED;
	 dcc[idx].status |= STAT_SHARE;
	 updatebot(-1, dcc[idx].nick,'+',0);
	 putlog(LOG_BOTS, "*", "Resyncing user file from %s", dcc[idx].nick);
      } else
	dprintf(idx, "s rn No resync buffer.\n");
   }
}

static void share_resync (int idx, char * par)
{
   if (dcc[idx].status & STAT_OFFERED) {
      if (can_resync(dcc[idx].nick)) {
	 dump_resync(idx);
	 dcc[idx].status &= ~STAT_OFFERED;
	 dcc[idx].status |= STAT_SHARE;
	 updatebot(-1, dcc[idx].nick,'+',0);
	 putlog(LOG_BOTS, "*", "Resync'd user file with %s", dcc[idx].nick);
      }
   }
}

static void share_resync_no (int idx, char * par)
{
   putlog(LOG_BOTS, "*", "Resync refused by %s: %s", dcc[idx].nick, par);
   flush_tbuf(dcc[idx].nick);
   dprintf(idx, "s u?\n");
}

static void share_version (int idx, char * par) {
   /* cleanup any share flags */
   dcc[idx].status &= 
     ~(STAT_SHARE|STAT_GETTING|STAT_SENDING|STAT_OFFERED|STAT_AGGRESSIVE);
   if (dcc[idx].u.bot->numver >= min_share) {
      if (bot_flags(dcc[idx].user) & BOT_AGGRESSIVE) {
	 if (can_resync(dcc[idx].nick))
	   dprintf(idx, "s r?\n");
	 else
	   dprintf(idx, "s u?\n");
	 dcc[idx].status |= STAT_OFFERED;
      }
   }
}

static void hook_read_userfile () {
   int i;
   
   if (!noshare) {
      for (i = 0; i < dcc_total; i++)
	if ((dcc[i].type->flags & DCT_BOT) 
	    && (dcc[i].status & STAT_SHARE)
	  && !(dcc[i].status & STAT_AGGRESSIVE)) {
	   /* cancel any existing transfers */
	   if (dcc[i].status & STAT_SENDING)
	     cancel_user_xfer(-i,0);
	   dprintf(i, "s u?\n");
	   dcc[i].status |= STAT_OFFERED;
	}
   }
}

static void share_endstartup (int idx, char * par) {
   dcc[idx].status &= ~STAT_GETTING;
   /* send to any other sharebots */
   hook_read_userfile();
};

static void share_end (int idx, char * par) {
   putlog(LOG_BOTS,"*","Ending sharing with %s, (%s).",dcc[idx].nick,par);
   cancel_user_xfer(-idx, 0);
   dcc[idx].status &= 
     ~(STAT_SHARE|STAT_GETTING|STAT_SENDING|STAT_OFFERED|STAT_AGGRESSIVE);
}

/* these MUST be sorted */
static botcmd_t C_share[]={
  { "!", (Function) share_endstartup },
  { "+b", (Function) share_pls_ban },
  { "+bc", (Function) share_pls_banchan },
  { "+bh", (Function) share_pls_bothost },
  { "+cr", (Function) share_pls_chrec },
  { "+h", (Function) share_pls_host },
  { "+i", (Function) share_pls_ignore },
  { "-b", (Function) share_mns_ban },
  { "-bc", (Function) share_mns_banchan },
  { "-cr", (Function) share_mns_chrec },
  { "-h", (Function) share_mns_host },
  { "-i", (Function) share_mns_ignore },
  { "a", (Function) share_chattr },
  { "c", (Function) share_change },
  { "chchinfo", (Function) share_chchinfo },
  { "e", (Function) share_end },
  { "h", (Function) share_chhand },
  { "k", (Function) share_killuser },
  { "n", (Function) share_newuser },
  { "r!", (Function) share_resync },
  { "r?", (Function) share_resyncq },
  { "rn", (Function) share_resync_no },
  { "s", (Function) share_stick },
  { "u?", (Function) share_userfileq },
  { "un", (Function) share_ufno },
  { "us", (Function) share_ufsend },
  { "uy", (Function) share_ufyes },
  { "v", (Function) share_version },
  { 0, 0 }
};

static void sharein_mod (int idx, char * code) {
   char * msg;
   int f,i;
   context;
   msg = strchr(code,' ');
   if (msg) {
      *msg = 0;
      msg++;
   } else {
      msg = "";
   }
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
   int i, l;
   va_list va;
   char *format;
   char s[601];
   struct chanset_t * chan;
   va_start(va);
   chan = va_arg(va, struct chanset_t *);
   if (!chan || channel_shared(chan)) {
      format = va_arg(va, char *);
      strcpy(s,"s ");
#ifdef HAVE_VSNPRINTF
      if ((l = vsnprintf(s+2, 509, format, va)) < 0) 
	s[2+(l=509)] = 0;
#else
      l = vsprintf(s+2, format, va);
#endif
      fr.match = FR_CHAN;
      for (i = 0; i < dcc_total; i++)
	if ((dcc[i].type->flags & DCT_BOT) &&
	    (dcc[i].status & STAT_SHARE) &&
	    (!(dcc[i].status & STAT_GETTING)) &&
	    (!(dcc[i].status & STAT_SENDING))) {
	   if (chan) {
	      fr.chan = 0;
	      get_user_flagrec(dcc[i].user,
			       &fr,chan->name);
	   }
	   if (!chan || (fr.chan & BOT_SHARE))
	     tputs(dcc[i].sock, s, l + 2);
	}
      q_resync(s,chan);
   }
   va_end(va);
}

static void shareout_but(va_alist)
va_dcl
{
   int i, x, l;
   va_list va;
   char *format;
   char s[601];
   struct chanset_t * chan;
   va_start(va);
   chan = va_arg(va, struct chanset_t *);
   x = va_arg(va, int);
   format = va_arg(va, char *);
   strcpy(s,"s ");
#ifdef HAVE_VSNPRINTF
   if ((l = vsnprintf(s+2, 509, format, va))<0)
     s[2+(l=509)] = 0;   
#else
   l = vsprintf(s+2, format, va);
#endif
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_BOT) && (i != x) &&
	  (dcc[i].status & STAT_SHARE) &&
	  (!(dcc[i].status & STAT_GETTING)) &&
	  (!(dcc[i].status & STAT_SENDING))) {
	 if (chan) {
	    fr.chan = 0;
	    get_user_flagrec(dcc[i].user,&fr,chan->name);
	 }
	 if (!chan || (fr.chan & BOT_SHARE))
	   tputs(dcc[i].sock, s, l + 2);
      }
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
	 tbuf[i].timer = now;
	 putlog(LOG_MISC, "*", "Creating resync buffer for %s", bot);
	 return;
      }
}

/* flush a certain bot's tbuf */
static int flush_tbuf (char * bot)
{
   int i;
   struct share_msgq *q;
   for (i = 0; i < 5; i++)
      if (!strcasecmp(tbuf[i].bot, bot)) {
	 while (tbuf[i].q != NULL) {
	    q = tbuf[i].q;
	    tbuf[i].q = tbuf[i].q->next;
	    nfree(q->msg);
	    nfree(q);
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
   struct share_msgq *q;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0]) {
	 if (now - tbuf[i].timer > resync_time) {
	    /* EXPIRED */
	    while (tbuf[i].q != NULL) {
	       q = tbuf[i].q;
	       tbuf[i].q = tbuf[i].q->next;
	       nfree(q->msg);
	       nfree(q);
	    }
	    putlog(LOG_MISC, "*", "Flushing resync buffer for clonebot %s.",
		   tbuf[i].bot);
	    tbuf[i].bot[0] = 0;
	 }
      }
   /* resend userfile requests */
   for (i = 0; i < dcc_total; i++)
     if (dcc[i].type->flags & DCT_BOT) {
	if (dcc[i].status & STAT_OFFERED) {
	   if (now - dcc[i].timeval > 120) {
	      if (dcc[i].user && (bot_flags(dcc[i].user) & BOT_AGGRESSIVE))
		dprintf(i, "s u?\n");
	      /* ^ send it again in case they missed it */
	   }
	   /* if it's a share bot that hasnt been sharing, ask again */
	} else if (!(dcc[i].status & STAT_SHARE)) {
	   if (dcc[i].user && (bot_flags(dcc[i].user) & BOT_AGGRESSIVE))
	     dprintf(i, "s u?\n");
	   dcc[i].status |= STAT_OFFERED;
	}
     }
}

static struct share_msgq *q_addmsg (struct share_msgq * qq,
				    struct chanset_t * chan,
				    char * s)
{
   struct share_msgq *q;
   int cnt;
   if (qq == NULL) {
      q = (struct share_msgq *) nmalloc(sizeof(struct share_msgq));
      q->chan = chan;
      q->next = NULL;
      q->msg = (char *) nmalloc(strlen(s) + 1);
      strcpy(q->msg, s);
      return q;
   }
   cnt = 0;
   q = qq;
   while (q->next != NULL) {
      q = q->next;
      cnt++;
   }
   if (cnt > 1000)
      return NULL;		/* return null: did not alter queue */
   q->next = (struct share_msgq *) nmalloc(sizeof(struct share_msgq));
   q = q->next;
   q->chan = chan;
   q->next = NULL;
   q->msg = (char *) nmalloc(strlen(s) + 1);
   strcpy(q->msg, s);
   return qq;
}

/* add stuff to a specific bot's tbuf */
static void q_tbuf (char * bot, char * s, struct chanset_t * chan)
{
   int i;
   struct share_msgq *q;
   for (i = 0; i < 5; i++)
      if (!strcasecmp(tbuf[i].bot, bot)) {
	 if (chan) {
	    fr.match = FR_CHAN;
	    get_user_flagrec(get_user_by_handle(userlist,bot),
			     &fr,chan->name);
	 }
	 if (!chan || (fr.chan & BOT_SHARE)) {
	    q = q_addmsg(tbuf[i].q, chan, s);
	    if (q != NULL)
	      tbuf[i].q = q;
	 }
      }
}

/* add stuff to the resync buffers */
static void q_resync (char * s, struct chanset_t * chan)
{
   int i;
   struct share_msgq *q;
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0]) { 
	 if (chan) {
	    fr.match = FR_CHAN;
	    get_user_flagrec(get_user_by_handle(userlist,tbuf[i].bot),
			     &fr, chan->name);
	 }
	 if (!chan || (fr.chan & BOT_SHARE)) {
	    q = q_addmsg(tbuf[i].q, chan, s);
	    if (q != NULL)
	      tbuf[i].q = q;
	 }
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
static void dump_resync (int idx)
{
   int i;
   struct share_msgq *q;
   context;
   for (i = 0; i < 5; i++)
      if (strcasecmp(dcc[idx].nick, tbuf[i].bot) == 0) {
	 while (tbuf[i].q != NULL) {
	    q = tbuf[i].q;
	    tbuf[i].q = tbuf[i].q->next;
	    dprintf(idx, "%s", q->msg);
	    nfree(q->msg);
	    nfree(q);
	 }
	 tbuf[i].bot[0] = 0;
	 return;
      }
}

/* give status report on tbufs */
static void status_tbufs (int idx)
{
   int i, count;
   struct share_msgq *q;
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

static int write_tmp_userfile (char * fn, struct userrec * bu, int idx)
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
   fprintf(f, "#4v: %s -- %s -- transmit\n", ver, origbotname);
   ok = 1;
   u = bu;
   while ((u != NULL) && (ok)) {
      ok = write_user(u, f, idx);
      u = u->next;
   }
   ok = write_bans(f,idx);
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
   struct chanuserrec *ch;
   struct user_entry * ue;
   char * p;
   
   nu = retu = NULL;
   u = userlist;
   context;
   noshare = 1;
   while (u != NULL) {
      if (((u->flags & (USER_BOT | USER_UNSHARED)) && t) ||
	  (!(u->flags & (USER_BOT | USER_UNSHARED)) && !t)) {

	 
	 p = get_user(&USERENTRY_PASS,u);
	 u1 = adduser(NULL,u->handle,0,p, u->flags);
	 u1->flags_udef = u->flags_udef;
	 if (nu == NULL)
	    nu = retu = u1;
	 else {
	    nu->next = u1;
	    nu = nu->next;
	 }
	 nu->chanrec = NULL;
	 for (ch = u->chanrec;ch;ch = ch->next) {
	    struct chanuserrec *z;
	    z = add_chanrec(nu,ch->channel);
	    z->flags = ch->flags;
	    z->flags_udef = ch->flags_udef;
	    z->laston = ch->laston;
	    set_handle_chaninfo(nu,nu->handle,ch->channel,ch->info);
	 }
	 for (ue = u->entries;ue;ue=ue->next) {
	    context;
	    if (ue->name) {
	       struct list_type * lt;
	       struct user_entry * nue;
	       
	       context;
	       nue = user_malloc(sizeof(struct user_entry));
	       nue->name = user_malloc(strlen(ue->name)+1);
	       nue->type = NULL;
	       nue->u.list = NULL;
	       strcpy(nue->name,ue->name);
	       list_insert((&nu->entries),nue);
	       context;

	       for (lt = ue->u.list;lt;lt=lt->next) {
		  struct list_type * list;
		  
		  context;
		  list = user_malloc(sizeof(struct list_type));
		  list->next = NULL;
		  list->extra = user_malloc(strlen(lt->extra)+1);
		  strcpy(list->extra,lt->extra);
		  list_append((&nue->u.list),list);
		  context;
	       }
	    } else if (ue->type->dup_user) {
	       ue->type->dup_user(nu,u,ue);
	    }
	 }
      } else 
	u1 = NULL;
      u = u->next;
   }
   noshare = 0;
   return retu;
}

static void restore_chandata(int idx)
{
   FILE *f;
   struct userrec *tbu = NULL;
   struct chanset_t *cst;
   char buf[181], hand[HANDLEN+1], *code,*s;
   static struct flag_record fr2 = {FR_CHAN,0,0,0,0,0};
   int i;
   
   for (i = 0;i < dcc_total;i++) 
     if ((i != idx) && !strcasecmp(dcc[i].nick,dcc[idx].nick)
	 && (dcc[i].type == &DCC_BOT)) {
	context;
	f = fopen(userfile, "r");
	if (f == NULL) {
	   putlog(LOG_MISC, "*", "* %s", USERF_BADREREAD);
	   return;
	}
	s = buf;
	fgets(s, 180, f);
	/* Disregard opening statement.  We already know it should be good */
	while (!feof(f)) {
	   s = buf;
	   fgets(s, 180, f);
	   if (!feof(f)) {
	      rmspace(s);
	      if ((s[0] != '#') && (s[0] != ';') && (s[0])) {
		 code = newsplit(&s);
		 if (strcasecmp(code, "!") == 0) {
		    if (hand[0] && tbu) {
		       char *chname, *st, *fl;
		       time_t last;
		       struct chanuserrec *cr = NULL;
		       
		       chname = newsplit(&s);
		       st = newsplit(&s);
		       fl = newsplit(&s);
		       rmspace(s);
		       fr.match = FR_CHAN;
		       break_down_flags(fl,&fr,0);
		       last = (time_t) atoi(st);
		       if ((cst = findchan(chname))) {
			  get_user_flagrec(dcc[i].user,&fr2,chname);
			  if (!channel_shared(cst) || !(fr2.chan & BOT_SHARE)) {
			     cr = get_chanrec(tbu, chname);
			     if (!cr)
			       cr = add_chanrec(tbu, chname);
			     if (s[0])
			       set_handle_chaninfo(tbu, hand, chname, s);
			     cr->flags = fr.chan;
			     cr->flags_udef = fr.udef_chan;
			     cr->laston = last;
			  } else if (!share_greet && s[0])
			       set_handle_chaninfo(tbu, hand, chname, s);
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
		    strncpy(hand, code, HANDLEN);
		    hand[HANDLEN] = 0;
		    tbu = get_user_by_handle(userlist, hand);
		 }
	      }
	   }
	}
	context;
	return;
     }
}

/* erase old user list, switch to new one */
static void finish_share (int idx)
{
   struct userrec *u, *ou;

   int i, j = -1;
   
   for (i = 0; i < dcc_total; i++)
      if ((strcasecmp(dcc[i].nick, dcc[idx].host) == 0) &&
	  (dcc[i].type->flags & DCT_BOT))
	 j = i;
   if (j == -1)
      return;			/* oh well. */
   /* copy the bots over */
   u = dup_userlist(1);
   /* remove global bans & ignores in anticipation of replacement */
   noshare = 1;
   while (global_bans)
     u_delban(NULL,global_bans->banmask,1);
   while (global_ign)
     delignore(global_ign->igmask);
   noshare = 0;
   ou = userlist;
   userlist = NULL; /* do this to prevent .user messups */
   /* read the rest in */
   for (i =0; i < dcc_total;i++) 
     dcc[i].user = get_user_by_handle(u,dcc[i].nick);
   if (!readuserfile(dcc[idx].u.xfer->filename, &u,private_owner)) {
      putlog(LOG_MISC, "*", "%s",USERF_CANTREAD);
      return;
   }
   putlog(LOG_MISC, "*", "%s.", USERF_XFERDONE);
   userlist = u;
   clear_userlist(ou);
   lastuser = NULL;
   clear_chanlist();
   restore_chandata(idx);
   unlink(dcc[idx].u.xfer->filename);	/* done with you! */
   reaffirm_owners();		/* make sure my owners are +n */
   updatebot(-1, dcc[j].nick,'+',0);
}

/* begin the user transfer process */
static void start_sending_users (int idx)
{
   struct userrec *u;
   char s[161], s1[64];
   int i = 1;
   struct chanuserrec *ch;
   struct chanset_t *cst;
   
   context;
   sprintf(s, ".share.%s.%lu", dcc[idx].nick,now);
   u = dup_userlist(0);		/* only non-bots */
   write_tmp_userfile(s, u, idx);
   clear_userlist(u);
   i = raw_dcc_send(s, "*users", "(users)", s);
   if (i > 0) {			/* abort */
      unlink(s);
      dprintf(idx, "s e %s\n", USERF_CANTSEND);
      putlog(LOG_MISC, "*", "%s -- can't send userfile",
	     i == 1 ? "NO MODE DCC CONNECTIONS" : 
	     i == 2 ? "CAN'T OPEN A LISTENING SOCKET" : "BAD FILE" );
      dcc[idx].status &= ~(STAT_SHARE|STAT_SENDING|STAT_AGGRESSIVE);
      return;
   }
   updatebot(-1, dcc[idx].nick,'+',0);
   dcc[idx].status |= STAT_SENDING;
   i = dcc_total - 1;
   strcpy(dcc[i].host, dcc[idx].nick);	/* store bot's nick */
   dprintf(idx, "s us %lu %d %lu\n", 
	   iptolong(natip[0]?(IP) inet_addr(natip):getmyip()), dcc[i].port,
	   dcc[i].u.xfer->length);
   /* start up a tbuf to queue outgoing changes for this bot until the */
   /* userlist is done transferring */
   new_tbuf(dcc[idx].nick);
   /* immediately, queue bot hostmasks & addresses (jump-start) */
   u = userlist;
   while (u != NULL) {
      if ((u->flags & USER_BOT) && !(u->flags & USER_UNSHARED)) {
	 struct bot_addr * bi = get_user(&USERENTRY_BOTADDR,u);
	 struct list_type * t = get_user(&USERENTRY_HOSTS,u);

	 /* send hostmasks */
	 for (;t;t=t->next) {
	    sprintf(s, "s +bh %s %s\n", u->handle, t->extra);
	    q_tbuf(dcc[idx].nick, s,NULL);
	 }
	 /* send address */
	 if (bi) 
	   sprintf(s, "s c BOTADDR %s %s %d %d\n", u->handle, 
		   bi->address, bi->telnet_port, bi->relay_port);
	 q_tbuf(dcc[idx].nick, s, NULL);
	 /* send user-flags */
	 fr.match = FR_GLOBAL;
	 fr.global = u->flags;
	 fr.udef_global = u->flags_udef;
	 build_flags(s1,&fr,NULL);
	 sprintf(s, "s a %s %s\n", u->handle, s1);
	 q_tbuf(dcc[idx].nick, s, NULL);
	 ch = u->chanrec;
	 while (ch) {
	    if (ch->flags & ~BOT_SHARE) {
	       cst = findchan(ch->channel);
	       if (cst && channel_shared(cst)) {
		  fr.match = FR_CHAN;
		  get_user_flagrec(dcc[idx].user,&fr,ch->channel);
		  if (fr.chan & BOT_SHARE) {
		     fr.chan = ch->flags & ~BOT_SHARE;
		     fr.udef_chan = ch->flags_udef;
		     build_flags(s1,&fr,NULL);
		     sprintf(s, "s a %s %s %s\n", u->handle, s1,
			     ch->channel);
		     q_tbuf(dcc[idx].nick, s, cst);
		  }
	       }
	    }
	    ch = ch->next;
	 }
      }
      u = u->next;
   }
   sprintf(s, "s !\n");
   q_tbuf(dcc[idx].nick, s, NULL);
   /* wish could unlink the file here to avoid possibly leaving it lying */
   /* around, but that messes up NFS clients. */
   context;
}

static void (*def_dcc_bot_kill)(int, void *) = 0;

static void cancel_user_xfer (int idx, void * x)
{
   int i, j, k = 0;
   context;
   if (idx < 0) {
      idx = -idx;
      k = 1;
      updatebot(-1, dcc[idx].nick,'-',0);
   }
   flush_tbuf(dcc[idx].nick);
   if (dcc[idx].status & STAT_SHARE) {
      if (dcc[idx].status & STAT_GETTING) {
	 j = 0;
	 for (i = 0; i < dcc_total; i++)
	    if ((strcasecmp(dcc[i].host, dcc[idx].nick) == 0) &&
		((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) ==
		  (DCT_FILETRAN | DCT_FILESEND)))
	       j = i;
	 if (j != 0) {
	    killsock(dcc[j].sock);
	    dcc[j].sock = (long)dcc[j].type;
	    dcc[j].type = &DCC_LOST;
	    unlink(dcc[j].u.xfer->filename);
	 }
	 putlog(LOG_BOTS, "*", "(Userlist download aborted.)");
      }
      if (dcc[idx].status & STAT_SENDING) {
	 j = 0;
	 for (i = 0; i < dcc_total; i++)
	    if ((strcasecmp(dcc[i].host, dcc[idx].nick) == 0) &&
		((dcc[i].type->flags & (DCT_FILETRAN|DCT_FILESEND))
		 == DCT_FILETRAN))
	     j = i;
	 if (j != 0) {
	    killsock(dcc[j].sock);
	    dcc[j].sock = (long)dcc[j].type;
	    dcc[j].type = &DCC_LOST;
	    unlink(dcc[j].u.xfer->filename);
	 }
	 putlog(LOG_BOTS, "*", "(Userlist transmit aborted.)");
      }
      if (allow_resync && (!(dcc[idx].status & STAT_GETTING)) &&
	  (!(dcc[idx].status & STAT_SENDING)))
	 new_tbuf(dcc[idx].nick);
   }
   context;
   if (!k)
     def_dcc_bot_kill(idx,x);
   context;
}


static tcl_ints my_ints [] = {
   {"allow-resync", &allow_resync },
   {"resync-time", &resync_time },
   {"private-owner", &private_owner },
   { 0, 0 }
};

static void cmd_flush (struct userrec * u, int idx, char * par)
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
  { "flush", "n", (Function)cmd_flush, NULL },
};

static char *share_close()
{
   int i;
   
   context;
   module_undepend(MODULE_NAME);
   context;
   putlog(LOG_MISC|LOG_BOTS,"*","Unloaded sharing module, flushing tbuf's...");
   for (i = 0; i < 5; i++)
      if (tbuf[i].bot[0])
	 flush_tbuf(tbuf[i].bot);
   putlog(LOG_MISC|LOG_BOTS,"*","Sending 'share end' to all sharebots...");
   for (i = 0;i < dcc_total;i++)
     if ((dcc[i].type->flags & DCT_BOT) 
	 && (dcc[i].status & STAT_SHARE)) {
	dprintf(i,"s e Unload module\n");
	cancel_user_xfer(-i,0);
	updatebot(-1, dcc[i].nick,'-',0);
	dcc[i].status &= 
	  ~(STAT_SHARE|STAT_GETTING|STAT_SENDING|STAT_OFFERED|STAT_AGGRESSIVE);
     }
   del_hook(HOOK_SHAREOUT,shareout_mod);
   del_hook(HOOK_SHAREIN,sharein_mod);
   del_hook(HOOK_MINUTELY,check_expired_tbufs);
   del_hook(HOOK_READ_USERFILE,hook_read_userfile);
   DCC_BOT.kill = def_dcc_bot_kill;
   rem_tcl_ints(my_ints);
   rem_builtins(H_dcc,my_cmds,1);
   rem_help_reference("share.help");
   return NULL;
}

static int share_expmem()
{
   int i, tot = 0;
   for (i = 0; i < 5; i++) {
      if (tbuf[i].bot[0]) {
	 struct share_msgq * q = tbuf[i].q;
	 while (q) {
	    tot += sizeof(struct share_msgq);
	    tot += strlen(q->msg)+1;
	    q = q->next;
	 }
      }
   }
   return tot;
}

static void share_report (int idx, int details)
{
   int i, j;
   
   if (!details)
     return;
   dprintf(idx,"   Share module, using %d bytes.\n",share_expmem());
   dprintf(idx,"   Private owners: %3s   Allow resync: %3s\n",
	   private_owner ? "yes" : "no", allow_resync ? "yes" : "no");
   for (i = 0; i < dcc_total; i++)
     if (dcc[i].type == &DCC_BOT) {
	if (dcc[i].status & STAT_GETTING) {
	   int ok = 0;
	   for (j = 0; j < dcc_total; j++)
	     if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == (DCT_FILETRAN | DCT_FILESEND)) &&
		 (strcasecmp(dcc[j].host, dcc[i].nick) == 0)) {
		dprintf(idx, "Downloading userlist from %s (%d%% done)\n",
			dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) /
					    ((float) dcc[j].u.xfer->length)));
		ok = 1;
	     }
	   if (!ok)
	     dprintf(idx,"Download userlist from %s (negotiating botentries)\n");
	} else if (dcc[i].status & STAT_SENDING) {
	   for (j = 0; j < dcc_total; j++) {
	      if (((dcc[j].type->flags & (DCT_FILETRAN|DCT_FILESEND)) == DCT_FILETRAN)
		  && (strcasecmp(dcc[j].host, dcc[i].nick) == 0)) {
		 if (dcc[j].type == &DCC_GET) 
		   dprintf(idx, "Sending userlist to %s (%d%% done)\n",
			   dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) /
					       ((float) dcc[j].u.xfer->length)));
		 else
		   dprintf(idx, "Sending userlist to %s (waiting for connect)\n",
			   dcc[i].nick);
	      }
	   }
	} else if (dcc[i].status & STAT_AGGRESSIVE) {
	   dprintf(idx, "   Passively sharing with %s.\n",dcc[i].nick);
	} else if (dcc[i].status & STAT_SHARE) {
	   dprintf(idx, "   Agressively sharing with %s.\n",dcc[i].nick);
	}
     }
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
   (Function) dump_resync
};

char *share_start (Function * global_funcs)
{
   int i;

   global = global_funcs;
   context;
   module_register(MODULE_NAME, share_table, 2, 0);
   if (!(transfer_funcs = module_depend(MODULE_NAME, "transfer", 2, 0)))
      return "You need the transfer module to use userfile sharing.";
   if (!(channels_funcs = module_depend(MODULE_NAME, "channels", 1, 0))) {
      module_undepend(MODULE_NAME);
      return "You need the channels module it use userfile sharing";
   }
   add_hook(HOOK_SHAREOUT,shareout_mod);
   add_hook(HOOK_SHAREIN,sharein_mod);
   add_hook(HOOK_MINUTELY,check_expired_tbufs);
   add_hook(HOOK_READ_USERFILE,hook_read_userfile);
   add_help_reference("share.help");
   for (i = 0; i < 5; i++) {
      tbuf[i].q = NULL;
      tbuf[i].bot[0] = 0;
   }
   def_dcc_bot_kill = DCC_BOT.kill;
   DCC_BOT.kill = cancel_user_xfer;
   add_tcl_ints(my_ints);
   add_builtins(H_dcc,my_cmds,1);
   context;
   return NULL;
}
