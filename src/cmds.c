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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "eggdrop.h"
#include "users.h"
#include "chan.h"
#include "proto.h"
#include "tclegg.h"

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
extern struct dcc_t dcc[];
extern char botname[];
extern char botuser[];
extern char dccdir[];
extern struct userrec *userlist;
extern char owner[];
extern struct chanset_t *chanset;
extern int make_userfile;
extern int conmask;
extern Tcl_Interp *interp;
extern tcl_timer_t *timer,*utimer;
extern char chanfile[];
#ifndef NO_FILE_SYSTEM
extern int dcc_users;
#endif
extern int gban_total;

/* Tcl Prototypes */
/* void Tcl_DeleteInterp(Tcl_Interp *); */


/* Do we have any flags that will allow us ops on a channel? */
int has_op(idx,par)
int idx; char *par;
{
  struct chanset_t *chan; int atr,chatr;
  context;
  if (par[0]=='#') chan=findchan(par);
  else chan=findchan(dcc[idx].u.chat->con_chan);
  if (chan==NULL) {
    dprintf(idx,"Invalid console channel.\n");
    return 0;
  }
  atr=get_attr_handle(dcc[idx].nick);
  chatr=get_chanattr_handle(dcc[idx].nick,chan->name);
  if ((atr & (USER_MASTER|USER_OWNER|USER_GLOBAL)) ||
      (chatr & (CHANUSER_MASTER|CHANUSER_OWNER|CHANUSER_OP))) return 1;
  else {
    dprintf(idx,"You are not a channel op on %s.\n",chan->name);
    return 0;
  }
  return 1;
}

void cmd_who(idx,par)
int idx; char *par;
{
  int i;
  if (par[0]) {
    if (dcc[idx].u.chat->channel<0) {
      dprintf(idx,"You have chat turned off.\n");
      return;
    }
    putlog(LOG_CMDS,"*","#%s# who %s",dcc[idx].nick,par);
    if (strcasecmp(par,botnetnick)==0) tell_who(idx,dcc[idx].u.chat->channel);
    else {
      i=nextbot(par); if (i<0) {
	dprintf(idx,"That bot isn't connected.\n");
/*	tandout("who %d:%s@%s %s %d\n",dcc[idx].sock,dcc[idx].nick,
		botnetnick,par,dcc[idx].u.chat->channel);  */
      }
      else if (dcc[idx].u.chat->channel >99999)
	dprintf(idx,"You are on a local channel\n");
      else tprintf(dcc[i].sock,"who %d:%s@%s %s %d\n",dcc[idx].sock,
		   dcc[idx].nick,botnetnick,par,dcc[idx].u.chat->channel);
    }
  }
  else {
    putlog(LOG_CMDS,"*","#%s# who",dcc[idx].nick);
    if (dcc[idx].u.chat->channel<0) tell_who(idx,0);
    else tell_who(idx,dcc[idx].u.chat->channel);
  }
}

void cmd_botinfo(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# botinfo",dcc[idx].nick);
  tandout("info? %d:%s@%s\n",dcc[idx].sock,dcc[idx].nick,botnetnick);
}

void cmd_whom(idx,par)
int idx; char *par;
{
  if (dcc[idx].u.chat->channel<0) {
    dprintf(idx,"You have chat turned off.\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# whom %s",dcc[idx].nick,par);
  if (!par[0]) answer_local_whom(idx,dcc[idx].u.chat->channel);
  else if (par[0]=='*') answer_local_whom(idx,-1);
  else {
    int chan=atoi(par);
    if (par[0]=='*') {
      if ((par[1]<'0' || par[1]>'9')) {
	chan=get_assoc(par+1);
	if (chan<0) {
	  dprintf(idx, "No such channel.\n");
	  return;
        }
      }
      else chan=100000+atoi(par+1);
      if (chan<100000||chan>199999) {
        dprintf(idx,"Channel # out of range : Local channels are *0 - *99999\n");
        return;
      }
    }
    else {
      if ((par[0]<'0') || (par[0]>'9')) {
        chan=get_assoc(par);
        if (chan<0) {
          dprintf(idx,"No such channel.\n");
          return;
        }
      }
      if ((chan<0) || (chan>99999)) {
        dprintf(idx,"Channel # out of range: must be 0-99999\n");
        return;
      }
    }
    answer_local_whom(idx,chan);
  }
}

void cmd_me(idx,par)
int idx; char *par;
{
  int i;
  if (dcc[idx].u.chat->channel<0) {
    dprintf(idx,"You have chat turned off.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx,"Usage: me <action>\n");
    return;
  }
  if (dcc[idx].u.chat->away!=NULL) not_away(idx);
  for (i=0; i<dcc_total; i++) {
    if ((dcc[i].type==DCC_CHAT) && 
	(dcc[i].u.chat->channel==dcc[idx].u.chat->channel) &&
	((i!=idx) || (dcc[i].u.chat->status&STAT_ECHO)))
      dprintf(i,"* %s %s\n",dcc[idx].nick,par);
    if (dcc[i].type==DCC_BOT)
      if (dcc[idx].u.chat->channel<100000)
        tprintf(dcc[i].sock,"actchan %s@%s %d %s\n",dcc[idx].nick,botnetnick,
	        dcc[idx].u.chat->channel,par);
  }
  check_tcl_act(dcc[idx].nick,dcc[idx].u.chat->channel,par);
}

void cmd_motd(idx,par)
int idx; char *par;
{
  int i;
  if (par[0]) {
    putlog(LOG_CMDS,"*","#%s# motd %s",dcc[idx].nick,par);
    if (strcasecmp(par,botnetnick)==0) show_motd(idx);
    else {
      i=nextbot(par); if (i<0) 
	dprintf(idx,"That bot isn't connected.\n");
      else tprintf(dcc[i].sock,"motd %d:%s@%s %s\n",dcc[idx].sock,
		   dcc[idx].nick,botnetnick,par);
    }
  }
  else {
    putlog(LOG_CMDS,"*","#%s# motd",dcc[idx].nick);
    show_motd(idx);
  }
}

#ifndef NO_FILE_SYSTEM
void cmd_files(idx,par)
int idx; char *par;
{
  int atr=get_attr_handle(dcc[idx].nick);
  if (dccdir[0]==0) dprintf(idx,"There is no file transfer area.\n");
  else if (too_many_filers()) {
    dprintf(idx,"The maximum of %d people are in the file area right now.\n",
	    dcc_users);
    dprintf(idx,"Please try again later.\n");
  }
  else {
    if (!(atr & (USER_MASTER|USER_XFER)))
      dprintf(idx,"You don't have access to the file area.\n");
    else {
      putlog(LOG_CMDS,"*","#%s# files",dcc[idx].nick);
      dprintf(idx,"Entering file system...\n");
      if (dcc[idx].u.chat->channel>=0) {
	chanout2(dcc[idx].u.chat->channel,"%s is away: file system\n",
		 dcc[idx].nick);
	context;
	if (dcc[idx].u.chat->channel<100000)
          tandout("away %s %d file system\n",botnetnick,dcc[idx].sock);
      }
      set_files(idx); dcc[idx].type=DCC_FILES;
      dcc[idx].u.file->chat->status|=STAT_CHAT;
      if (!welcome_to_files(idx)) {
	struct chat_info *ci=dcc[idx].u.file->chat;
	nfree(dcc[idx].u.file); dcc[idx].u.chat=ci;
	dcc[idx].type=DCC_CHAT;
	putlog(LOG_FILES,"*","File system broken.");
	if (dcc[idx].u.chat->channel>=0) {
	  chanout2(dcc[idx].u.chat->channel,"%s has returned.\n",
		   dcc[idx].nick);
          context;
	  if (dcc[idx].u.chat->channel<100000)
	    tandout("unaway %s %d\n",botnetnick,dcc[idx].sock);
	}
      }
    }
  }
}
#endif

void cmd_note(idx,par)
int idx; char *par;
{
  char handle[512],*p; int echo;
  split(handle,par); if (!handle[0]) {
    dprintf(idx,"Usage: note <to-whom> <message>\n");
    return;
  }
  /* could be file system user */
  echo=(dcc[idx].type==DCC_CHAT)?(dcc[idx].u.chat->status&STAT_ECHO):
        (dcc[idx].u.file->chat->status&STAT_ECHO);
  p=strchr(handle,','); while (p!=NULL) {
    *p=0; p++;
    add_note(handle,dcc[idx].nick,par,idx,echo);
    strcpy(handle,p); p=strchr(handle,',');
  }
  add_note(handle,dcc[idx].nick,par,idx,echo);
}

void cmd_away(idx,par)
int idx; char *par;
{
  if (strlen(par)>60) par[60]=0;
  set_away(idx,par);
}

void cmd_newpass(idx,par)
int idx; char *par;
{
  char new[512];
  nsplit(new,par);
  if (!par[0]) {
    dprintf(idx,"Usage: newpass <newpassword>\n");
    return;
  }
  if (strlen(new)>9) new[9]=0;
  if (strlen(new)<4) {
    dprintf(idx,"Please use at least 4 characters.\n");
    return;
  }
  change_pass_by_handle(dcc[idx].nick,new);
  putlog(LOG_CMDS,"*","#%s# newpass...",dcc[idx].nick);
  dprintf(idx,"Changed password to '%s'\n",new);
}

void cmd_bots(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# bots",dcc[idx].nick);
  tell_bots(idx);
}

void cmd_bottree(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# bottree",dcc[idx].nick);
  tell_bottree(idx);
}

void cmd_help(idx,par)
int idx; char *par;
{
  int atr,chatr;
  atr=get_attr_handle(dcc[idx].nick);
  chatr=get_chanattr_handle(dcc[idx].nick,dcc[idx].u.chat->con_chan);
  if (chatr & CHANUSER_OP) atr |= USER_PSUEDOOP;
  if (chatr & CHANUSER_MASTER) atr |= USER_PSUMST;
  if (chatr & CHANUSER_OWNER) atr |= USER_PSUOWN;
  if (par[0]) {
    putlog(LOG_CMDS,"*","#%s# help %s",dcc[idx].nick,par);
    tellhelp(idx,par,atr);
  }
  else {
    putlog(LOG_CMDS,"*","#%s# help",dcc[idx].nick);
    if (atr & (USER_GLOBAL|USER_PSUEDOOP|USER_MASTER|USER_PSUMST|USER_BOTMAST))
      tellhelp(idx,"help",atr);
    else tellhelp(idx,"helpparty",atr);
  }
}

#ifndef NO_IRC
void cmd_act(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: act <action>\n");
    return;
  }
  if (!has_op(idx," ")) return;
  putlog(LOG_CMDS,"*","#%s# (%s) .act %s",dcc[idx].nick,
         dcc[idx].u.chat->con_chan,par);
  mprintf(serv,"PRIVMSG %s :\001ACTION %s\001\n",dcc[idx].u.chat->con_chan,
	  par);
}
#endif

#ifndef NO_IRC
void cmd_msg(idx,par)
int idx; char *par;
{
  char nick[512];
  split(nick,par); if (!nick[0]) {
    dprintf(idx,"Usage: msg <nick> <message>\n");
  }
  else {
    putlog(LOG_CMDS,"*","#%s# msg %s %s",dcc[idx].nick,nick,par);
    mprintf(serv,"PRIVMSG %s :%s\n",nick,par);
  }
}
#endif

#ifndef NO_IRC
void cmd_say(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: say <message>\n");
    return;
  }
  if (!has_op(idx," ")) return;
  putlog(LOG_CMDS,"*","#%s# (%s) .say %s",dcc[idx].nick,
         dcc[idx].u.chat->con_chan,par);
  mprintf(serv,"PRIVMSG %s :%s\n",dcc[idx].u.chat->con_chan,par);
}
#endif

#ifndef NO_IRC
void cmd_kickban(idx,par)
int idx; char *par;
{
  struct chanset_t *chan;
  if (!par[0]) {
    dprintf(idx,"Usage: kickban <nick> [reason]\n");
    return;
  }
  if (!has_op(idx," ")) return;
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (!me_op(chan)) {
    dprintf(idx,"I can't help you now because I'm not a channel op on %s.\n",
            chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# (%s) .kickban %s",dcc[idx].nick,chan->name,par);
  user_kickban(idx,par);
}
#endif

#ifndef NO_IRC
void cmd_op(idx,par)
int idx; char *par;
{
  struct chanset_t *chan; char nick[512];
  nsplit(nick,par);
  if (!nick[0]) {
    dprintf(idx,"Usage: op <nick> [channel]\n");
    return;
  }
  if (par[0]) {
    chan=findchan(par);
    if (chan==NULL) {
      dprintf(idx,"I'm not on channel %s.\n",par);
      return;
    }
    if (!has_op(idx,chan->name)) return;
  }
  else {
    if (!has_op(idx," ")) return;
    chan=findchan(dcc[idx].u.chat->con_chan);
  }
  if (!me_op(chan)) {
    dprintf(idx,"I can't help you now because I'm not a chan op on %s.\n",
            chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# (%s) .op %s %s",dcc[idx].nick,chan->name,nick,par);
  give_op(nick,chan,idx);
}
#endif

#ifndef NO_IRC
void cmd_deop(idx,par)
int idx; char *par;
{
  struct chanset_t *chan; char nick[512];
  nsplit(nick,par);
  if (!nick[0]) {
    dprintf(idx,"Usage: deop <nick> [channel]\n");
    return;
  }
  if (par[0]) {
    chan=findchan(par);
    if (chan==NULL) {
      dprintf(idx,"I'm not on channel %s.\n",par);
      return;
    }
    if (!has_op(idx,chan->name)) return;
  }
  else {
    if (!has_op(idx," ")) return;
    chan=findchan(dcc[idx].u.chat->con_chan);
  }
  if (!me_op(chan)) {
    dprintf(idx,"I can't help you now because I'm not a channel op on %s.\n",
            chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# (%s) .deop %s %s",dcc[idx].nick,chan->name,nick,par);
  give_deop(nick,chan,idx);
}
#endif

#ifndef NO_IRC
void cmd_kick(idx,par)
int idx; char *par;
{
  struct chanset_t *chan;
  if (!par[0]) {
    dprintf(idx,"Usage: kick <nick> [reason]\n");
    return;
  }
  if (!has_op(idx," ")) return;
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (!me_op(chan)) {
    dprintf(idx,"I can't help you now because I'm not a channel op on %s.\n",
            chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# (%s) .kick %s",dcc[idx].nick,chan->name,par);
  user_kick(idx,par);
}
#endif

#ifndef NO_IRC
void cmd_invite(idx,par)
int idx; char *par;
{
  struct chanset_t *chan;
  if (!has_op(idx," ")) return;
  chan=findchan(dcc[idx].u.chat->con_chan);
  putlog(LOG_CMDS,"*","#%s# (%s) .invite %s",dcc[idx].nick,chan->name,par);
  if (!me_op(chan) && (chan->channel.mode&CHANINV)) {
    dprintf(idx,"I'm not chop on %s, so I can't invite anyone.\n",chan->name);
    return;
  }
  if (ischanmember(chan->name,par) && !is_split(chan->name,par)) {
    dprintf(idx,"%s is already on %s!\n",par,chan->name);
    return;
  }
  mprintf(serv,"INVITE %s %s\n",par,chan->name);
  dprintf(idx,"Inviting %s to %s.\n",par,chan->name);
}
#endif

void cmd_resetbans(idx,par)
int idx; char *par;
{
  struct chanset_t *chan;
  if (!has_op(idx," ")) return;
  chan=findchan(dcc[idx].u.chat->con_chan);
  putlog(LOG_CMDS,"*","#%s# (%s) .resetbans",dcc[idx].nick,chan->name);
  dprintf(idx,"Resetting bans on %s...\n",chan->name); resetbans(chan);
}

void cmd_pls_ban(idx,par)
int idx; char *par;
{
  char who[512],note[512],s[UHOSTLEN]; struct chanset_t *chan;
  char chname[512];
  chname[0]=0;
  if (!par[0]) {
    dprintf(idx,"Usage: +ban <hostmask> [channel] [reason]\n");
    return;
  }
  nsplit(who,par); rmspace(who);
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (!(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
    if (!has_op(idx," ")) return;
    strcpy(chname,dcc[idx].u.chat->con_chan);
  }
  else if (par[0]=='#') {
    nsplit(chname,par); rmspace(chname);
    if (!has_op(idx,chname)) return;
    chan=findchan(chname);
  }
  rmspace(par); 
  if (!par[0]) strcpy(note,"request");
  else { strncpy(note,par,65); note[65]=0; }
  /* fix missing ! or @ BEFORE checking against myself */
  if (strchr(who,'!')==NULL) strcat(s,"!*@*");   /* lame nick ban */
  if (strchr(who,'@')==NULL) strcat(s,"@*");     /* brain-dead? */
  sprintf(s,"%s!%s",botname,botuserhost);
  if (wild_match(who,s)) {
    dprintf(idx,"Duh...  I think I'll ban myself today, Marge!\n");
    putlog(LOG_CMDS,"*","#%s# attempted +ban %s",dcc[idx].nick,who);
    return;
  }
  if (strlen(who)>70) who[70]=0;
  /* irc can't understand bans longer than that */
  if (chname[0]!=0) {    
    u_addban(chan->bans,who,dcc[idx].nick,note,0L);
    putlog(LOG_CMDS,"*","#%s# +b %s %s (%s)",dcc[idx].nick,who,chname,note);
    dprintf(idx,"New %s ban: %s (%s)\n",chname,who,note);
    if (me_op(chan)) add_mode(chan,'+','b',who);
    recheck_channel(chan);
    return;
  }
  addban(who,dcc[idx].nick,note,0L);
  putlog(LOG_CMDS,"*","#%s# +ban %s (%s)",dcc[idx].nick,who,note);
  dprintf(idx,"New ban: %s (%s)\n",who,note);
  chan=chanset; while (chan!=NULL) {
    if (me_op(chan)) add_mode(chan,'+','b',who);
    recheck_channel(chan);
    chan=chan->next;
  }
}

void cmd_mns_ban(idx,par)
int idx; char *par;
{
  int i,j,k; struct chanset_t *chan; char s[UHOSTLEN],ban[512],chname[512];
  chname[0]=0;
  if (!par[0]) {
    dprintf(idx,"Usage: -ban <hostmask|ban #> [channel]\n");
    return;
  }
  nsplit(ban,par); rmspace(ban); rmspace(par);
  if (par[0]=='#') {
    strcpy(chname,par);
    if (!has_op(idx,chname)) return;
  }
  else if (!has_op(idx," ")) return;
  if (is_global_ban(ban) && !(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
    dprintf(idx,"You do no have Bot Master privileges to remove Global Bans.\n");
    return;
  }
  i=delban(ban);
  if (i>0) {
    putlog(LOG_CMDS,"*","#%s# -ban %s",dcc[idx].nick,ban);
    dprintf(idx,"Removed ban: %s\n",ban);
    chan=chanset; while (chan!=NULL) {
      if (me_op(chan)) add_mode(chan,'-','b',ban);
      chan=chan->next;
    }
    return;
  }
  /* channel-specific ban? */
  if (chname[0]!=0) {
    if ((chan=findchan(chname))==NULL) {
      dprintf (idx,"No such channel.\n");
      return;
   }
  } 
  else chan=findchan(dcc[idx].u.chat->con_chan);
  sprintf(s,"%d",i+atoi(ban));
  j=u_delban(chan->bans,s);
  if (j>0) {
    putlog(LOG_CMDS,"*","#%s# -ban %s",dcc[idx].nick,s);
    dprintf(idx,"Removed channel ban: %s\n",s);
    add_mode(chan,'-','b',s);
    return;
  } 
  else {
    k=u_delban(chan->bans,ban);
    if (k>0) {
      putlog(LOG_CMDS,"*","#%s# -ban %s",dcc[idx].nick,ban);
      dprintf(idx,"Removed channel ban: %s\n",ban);
      add_mode(chan,'-','b',ban);
      return;
    }
  }
  /* okay, not in any ban list -- might be ban on channel */
  if (atoi(ban)>0) {
    if (kill_chanban(dcc[idx].u.chat->con_chan,idx,1-i-j,atoi(ban)))
      putlog(LOG_CMDS,"*","#%s# -ban %s",dcc[idx].nick,ban);
  }
  else if (kill_chanban_name(dcc[idx].u.chat->con_chan,idx,ban))
    putlog(LOG_CMDS,"*","#%s# -ban %s",dcc[idx].nick,ban);
}

void cmd_bans(idx,par)
int idx; char *par;
{
  if (strcasecmp(par,"all")==0) {
    putlog(LOG_CMDS,"*","#%s# bans all",dcc[idx].nick);
    tell_bans(idx,1,"");
  }
  else {
    putlog(LOG_CMDS,"*","#%s# bans %s",dcc[idx].nick,par);
    tell_bans(idx,0,par);
  }
}

#ifndef NO_IRC
void cmd_channel(idx,par)
int idx; char *par;
{
  if (par[0]) {
    if (!has_op(idx,par)) return;
  }
  else if (!has_op(idx," ")) return;
  putlog(LOG_CMDS,"*","#%s# (%s) .channel %s",dcc[idx].nick,
         dcc[idx].u.chat->con_chan,par);
  tell_verbose_chan_info(idx,par);
}
#endif

void cmd_addlog(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: addlog <message>\n");
    return;
  }
  dprintf(idx,"Placed entry in the log file.\n");
  putlog(LOG_MISC,"*","%s: %s",dcc[idx].nick,par);
}

#ifndef NO_IRC
void cmd_servers(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# servers",dcc[idx].nick);
  tell_servers(idx);
}
#endif

void cmd_whois(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: whois <handle>\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# whois %s",dcc[idx].nick,par);
  tell_user_ident(idx,par,(get_attr_handle(dcc[idx].nick) & USER_MASTER));
}

void cmd_match(idx,par)
int idx; char *par;
{
  int start=1,limit=20; char s[512],s1[512],chname[512];
  putlog(LOG_CMDS,"*","#%s# match %s",dcc[idx].nick,par);
  nsplit(s,par);
  if ((par[0]=='#') || (par[0]=='+') || (par[0]=='&')) nsplit(chname,par);
  else chname[0]=0;
  if (atoi(par)>0) {
    split(s1,par);
    if (atoi(s1)>0) start=atoi(s1);
    limit=atoi(par);
  }
  tell_users_match(idx,s,start,limit,(get_attr_handle(dcc[idx].nick) &
		   USER_MASTER),chname);
}

void cmd_status(idx,par)
int idx; char *par;
{
  int atr=0;
  if (strcasecmp(par,"all")==0) {
    atr=get_attr_handle(dcc[idx].nick);
    if (!(atr&USER_MASTER)) {
      dprintf(idx,"YOu do not have Bot Master priveleges.\n");
      return;
    }
    putlog(LOG_CMDS,"*","#%s# status all",dcc[idx].nick);
    tell_verbose_status(idx,1);
    tell_mem_status_dcc(idx);
    dprintf(idx,"\n");
    tell_settings(idx);
  }
  else {
    putlog(LOG_CMDS,"*","#%s# status",dcc[idx].nick);
    tell_verbose_status(idx,1);
    tell_mem_status_dcc(idx);
  }
}

void cmd_dccstat(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# dccstat",dcc[idx].nick);
  tell_dcc(idx);
}

void cmd_pls_ignore(idx,par)
int idx; char *par;
{
  char who[512],note[66];
  if (!par[0]) {
    dprintf(idx,"Usage: +ignore <hostmask> [comment]\n");
    return;
  }
  nsplit(who,par); who[UHOSTLEN-1]=0;
  strncpy(note,par,65); note[65]=0;
  if (match_ignore(who)) {
    dprintf(idx,"That already matches an existing ignore.\n");
    return;
  }
  dprintf(idx,"Now ignoring: %s (%s)\n",who,note);
  addignore(who,dcc[idx].nick,note,0L);
  putlog(LOG_CMDS,"*","#%s# +ignore %s %s",dcc[idx].nick,who,note);
}

void cmd_mns_ignore(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: -ignore <hostmask>\n");
    return;
  }
  if (delignore(par)) {
    putlog(LOG_CMDS,"*","#%s# -ignore %s",dcc[idx].nick,par);
    dprintf(idx,"No longer ignoring: %s\n",par);
  }
  else dprintf(idx,"Can't find that ignore.\n");
}

void cmd_ignores(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# ignores %s",dcc[idx].nick,par);
  tell_ignores(idx,par);
}

void cmd_boot(idx,par)
int idx; char *par;
{
  int i,files=0,ok=0; char who[512];
  if (!par[0]) {
    dprintf(idx,"Usage: boot nick[@bot]\n");
    return;
  }
  nsplit(who,par);
  if (strchr(who,'@')!=NULL) {
    char whonick[512];
    splitc(whonick,who,'@'); whonick[20]=0;
    if (strcasecmp(who,botnetnick)==0) {
      cmd_boot(idx,whonick);
      return;
    }
#ifdef REMOTE_BOOTS
    i=nextbot(who); if (i<0) {
      dprintf(idx,"No such bot connected.\n");
      return;
    }
    tprintf(dcc[i].sock,"reject %s@%s %s@%s %s\n",dcc[idx].nick,botnetnick,
	    whonick,who,par[0]?par:dcc[idx].nick);
    putlog(LOG_MISC,"*","#%s# boot %s@%s (%s)",dcc[idx].nick,whonick,who,
	   par[0]?par:dcc[idx].nick);
#else
    dprintf(idx,"Remote boots are disabled here.\n");
#endif
    return;
  }
  for (i=0; i<dcc_total; i++)
    if ((strcasecmp(dcc[i].nick,who)==0) && (!ok) &&
	((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES))) {
#ifdef OWNER
      if ((get_attr_handle(who) & USER_OWNER) &&
	  (strcasecmp(dcc[idx].nick,who)!=0)) {
	dprintf(idx,"Can't boot the bot owner.\n");
	return;
      }
#endif
      if ((get_attr_handle(who) & USER_MASTER) && 
	  (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
	dprintf(idx,"Can't boot a bot master.\n");
	return;
      }
      files=(dcc[i].type==DCC_FILES);
      if (files) dprintf(idx,"Booted %s from the file section.\n",dcc[i].nick);
      else dprintf(idx,"Booted %s from the bot.\n",dcc[i].nick);
      putlog(LOG_CMDS,"*","#%s# boot %s %s",dcc[idx].nick,who,par);
      do_boot(i,dcc[idx].nick,par); ok=1;
    }
  if (!ok) dprintf(idx,"Who?  No such person on the party line.\n");
}

void cmd_console(idx,par)
int idx; char *par;
{
  char nick[512],s[2],s1[512]; int dest=0,i,ok=0,pls,md,atr;
  if (!par[0]) {
    dprintf(idx,"Your console is %s: %s (%s)\n",dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
    return;
  }
  atr=get_attr_handle(dcc[idx].nick);
  strcpy(s1,par);
  split(nick,par);
  if ((nick[0]) && (nick[0]!='#') && (nick[0]!='&') && 
      (atr & (USER_MASTER|USER_OWNER))) {
    for (i=0; i<dcc_total; i++)
      if ((strcasecmp(nick,dcc[i].nick)==0) && (dcc[i].type==DCC_CHAT) && (!ok)) {
	ok=1; dest=i;
      }
    if (!ok) {
      dprintf(idx,"No such user on the party line!\n");
      return;
    }
    nick[0]=0;
  }
  else dest=idx;
  if (!nick[0]) nsplit(nick,par);
  while (nick[0]) {
    if ((nick[0]=='#') || (nick[0]=='&') || (nick[0]=='*')) {
      if ((nick[0]!='*') && (findchan(nick)==NULL)) {
	dprintf(idx,"Invalid console channel: %s\n",nick);
	return;
      }
      if ((!(get_chanattr_handle(dcc[idx].nick,nick) & 
	  (CHANUSER_OP|CHANUSER_MASTER))) && (!(atr & 
	  (USER_GLOBAL|USER_MASTER)))) {
 	dprintf(idx,"You don't have op or master access to channel %s\n",nick);
	return;
      }
      strncpy(dcc[dest].u.chat->con_chan,nick,80);
      dcc[dest].u.chat->con_chan[80]=0;
    }
    else {
      pls=1;
      if ((nick[0]!='+') && (nick[0]!='-')) dcc[dest].u.chat->con_flags=0;
      for (i=0; i<strlen(nick); i++) {
	if (nick[i]=='+') pls=1;
	else if (nick[i]=='-') pls=(-1);
	else {
	  s[0]=nick[i]; s[1]=0;
	  md=logmodes(s);
	  if ((dest==idx) && !(atr & USER_MASTER) && pls) {
	    if(get_chanattr_handle(dcc[idx].nick,dcc[dest].u.chat->con_chan) 
               & CHANUSER_MASTER)
	      md&=~(LOG_RAW|LOG_FILES|LOG_LEV1|LOG_LEV2|
		    LOG_LEV3|LOG_LEV4|LOG_LEV5|LOG_LEV6|
		    LOG_LEV7|LOG_LEV8|LOG_DEBUG);
            else
	      md&=~(LOG_MISC|LOG_CMDS|LOG_RAW|LOG_FILES|LOG_LEV1|LOG_LEV2|
		    LOG_LEV3|LOG_LEV4|LOG_LEV5|LOG_LEV6|LOG_LEV7|LOG_LEV8|
		    LOG_WALL|LOG_DEBUG);
	  }
	  if (pls==1) dcc[dest].u.chat->con_flags|=md;
	  else dcc[dest].u.chat->con_flags&=~md;
	}
      }
    }
    nsplit(nick,par);
  }
  putlog(LOG_CMDS,"*","#%s# console %s",dcc[idx].nick,s1);
  if (dest==idx) {
    dprintf(idx,"Set your console to %s: %s (%s)\n",
	    dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
  }
  else {
    dprintf(idx,"Set console of %s to %s: %s (%s)\n",dcc[dest].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
    dprintf(dest,"%s set your console to %s: %s (%s)\n",dcc[idx].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
  }
}

#ifndef NO_IRC
void cmd_adduser(idx,par)
int idx; char *par;
{
  char nick[512];struct chanset_t *chan;
  if ((par==NULL) || (!par[0])) {
    dprintf(idx,"Usage: adduser <nick> [handle]\n");
    return;
  }
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (chan==NULL) {
    dprintf(idx,"Your console channel is invalid.\n");
    return;
  }
  nsplit(nick,par);
  if (!par[0]) {
    if (add_chan_user(nick,idx,nick))
      putlog(LOG_CMDS,"*","#%s# adduser %s",dcc[idx].nick,nick);
  }
  else {
    if (add_chan_user(nick,idx,par))
      putlog(LOG_CMDS,"*","#%s# adduser %s %s",dcc[idx].nick,nick,par);
  }
}
#endif

void cmd_pls_user(idx,par)
int idx; char *par;
{
  char handle[512],host[512];
  nsplit(handle,par); nsplit(host,par);
  if (!host[0]) {
    dprintf(idx,"Usage: +user <handle> <hostmask>\n");
    return;
  }
  if (strlen(handle)>9) handle[9]=0;  /* max len = 9 */
  if (is_user(handle)) {
    dprintf(idx,"Someone already exists by that name.\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# +user %s %s",dcc[idx].nick,handle,host);
  userlist=adduser(userlist,handle,host,"-",0);
  dprintf(idx,"Added %s (%s) with no password or flags.\n",handle,host);
}

void cmd_pls_bot(idx,par)
int idx; char *par;
{
  char handle[512],addr[512];
  nsplit(handle,par); if (!par[0]) {
    dprintf(idx,"Usage: +bot <handle> <address:port#>\n");
    return;
  }
  nsplit(addr,par);
  if (strlen(handle)>9) handle[9]=0;  /* max len = 9 */
  if (is_user(handle)) {
    dprintf(idx,"Someone already exists by that name.\n");
    return;
  }
  if (strlen(addr)>60) addr[60]=0;
  if (strchr(addr,':')==NULL) strcat(addr,":3333");
  putlog(LOG_CMDS,"*","#%s# +bot %s %s",dcc[idx].nick,handle,addr);
  userlist=adduser(userlist,handle,"none","-",USER_BOT);
  set_handle_info(userlist,handle,addr);
  dprintf(idx,"Added bot '%s' with address '%s' and no password.\n",
	  handle,addr);
  if (!add_bot_hostmask(idx,handle))
    dprintf(idx,"You'll want to add a hostmask if this bot will ever %s",
	    "be on any channels that I'm on.\n");
}

#ifndef NO_IRC
void cmd_deluser(idx,par)
int idx; char *par;
{
  char nick[512];struct chanset_t *chan;
  if ((par==NULL) || (!par[0])) {
    dprintf(idx,"Usage: deluser <nick>\n");
    return;
  }
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (chan==NULL) {
    dprintf(idx,"Your console channel is invalid.\n");
    return;
  }
  nsplit(nick,par);
#ifdef OWNER
  if (!(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
      (get_attr_handle(par) & USER_OWNER)) {
    dprintf(idx,"Can't remove the bot owner!\n");
    return;
  }
  if (!(get_chanattr_handle(dcc[idx].nick,chan->name) & CHANUSER_OWNER) &&
      (get_chanattr_handle(par,chan->name) & CHANUSER_OWNER) &&
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"Can't remove the channel owner!\n");
    return;
  }
#endif
  if (del_chan_user(nick,idx))
    putlog(LOG_CMDS,"*","#%s# deluser %s",dcc[idx].nick,nick);
}
#endif

void cmd_mns_user(idx,par)
int idx; char *par;
{
  int atr=get_attr_handle(dcc[idx].nick), atr1=get_attr_handle(par);
  if (!par[0]) {
    dprintf(idx,"Usage: -user <nick>\n");
    return;
  }
#ifdef OWNER
  if (!(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
      (get_attr_handle(par) & USER_OWNER)) {
    dprintf(idx,"Can't remove the bot owner!\n");
    return;
  }
  if ((get_attr_handle(par) & BOT_SHARE) &&               
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"You can't remove shared bots.\n");
    return;                               
  }                                               
#endif
  if ((atr & USER_BOTMAST) && (!(atr & (USER_MASTER|USER_OWNER))) 
      && (!(atr1 & USER_BOT))) {
    dprintf(idx,"Can't remove users who aren't bots!\n");
    return;
  }
  if (deluser(par)) {
    putlog(LOG_CMDS,"*","#%s# -user %s",dcc[idx].nick,par);
    dprintf(idx,"Deleted %s.\n",par);
  }
  else dprintf(idx,"Failed.\n");
}

void cmd_chnick(idx,par)
int idx; char *par;
{
  char hand[512]; int i;
  split(hand,par); if ((!hand[0]) || (!par[0])) {
    dprintf(idx,"Usage: chnick <oldnick> <newnick>\n");
    return;
  }
  if (strlen(par)>9) par[9]=0;
  for (i=0; i<strlen(par); i++)
    if ((par[i]<=32) || (par[i]>=127) || (par[i]=='@')) par[i]='?';
  if (par[0]=='*') {
    dprintf(idx,"Bizarre quantum forces prevent nicknames from starting with *\n");
    return;
  }
  if ((is_user(par)) && (strcasecmp(hand,par)!=0)) {
    dprintf(idx,"Already a user %s.\n",par);
    return;
  }
  if ((strcasecmp(par,origbotname)==0) || (strcasecmp(par,botnetnick)==0)) {
    dprintf(idx,"Hey!  That MY name!\n",par);
    return;
  }
  if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_OWNER))) &&
      (!(get_attr_handle(hand) & USER_BOT))) {
    dprintf(idx,"You can't change nick for non-bots.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(hand) & BOT_SHARE) &&               
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"You can't change shared bot's nick.\n");
    return;                               
  }
  if ((get_attr_handle(hand) & USER_OWNER) &&
      (strcasecmp(dcc[idx].nick,hand)!=0)) {
    dprintf(idx,"Can't change the bot owner's handle.\n");
    return;
  }
#endif
  if (change_handle(hand,par)){
    notes_change(idx,hand,par);
    putlog(LOG_CMDS,"*","#%s# chnick %s %s",dcc[idx].nick,hand,par);
    dprintf(idx,"Changed.\n");
    for (i=0; i<dcc_total; i++) {
      if ((strcasecmp(dcc[i].nick,hand)==0) && (dcc[i].type!=DCC_BOT)) {
	char s[10];
	strcpy(s,dcc[i].nick); strcpy(dcc[i].nick,par);
	if ((dcc[i].type==DCC_CHAT) && (dcc[i].u.chat->channel>=0)) {
	  chanout2(dcc[i].u.chat->channel,"Nick change: %s -> %s\n",s,par);
	  if (dcc[i].u.chat->channel<100000) {
	    context;
	    tandout("part %s %s %d\n",botnetnick,s,dcc[i].sock);
	    tandout("join %s %s %d %c%d %s\n",botnetnick,par,
		    dcc[i].u.chat->channel,geticon(i),dcc[i].sock,dcc[i].host);
	  }
	}
      }
    }
  }
  else dprintf(idx,"Failed.\n");
}

void cmd_nick(idx,par)
int idx; char *par;
{
  int i; char icon;
  if (!par[0]) {
    dprintf(idx,"Usage: nick <new-handle>\n");
    return;
  }
  if (strlen(par)>9) par[9]=0;
  for (i=0; i<strlen(par); i++)
    if ((par[i]<=32) || (par[i]>=127) || (par[i]=='@')) par[i]='?';
  if (par[0]=='*') {
    dprintf(idx,"Bizarre quantum forces prevent nicknames from starting with *\n");
    return;
  }
  if ((is_user(par)) && (strcasecmp(dcc[idx].nick,par)!=0)) {
    dprintf(idx,"Somebody is already using %s.\n",par);
    return;
  }
  if ((strcasecmp(par,origbotname)==0) || (strcasecmp(par,botnetnick)==0)) {
    dprintf(idx,"Hey!  That MY name!\n",par);
    return;
  }
  icon=geticon(idx);
  if (change_handle(dcc[idx].nick,par)) {
    notes_change(idx,dcc[idx].nick,par);
    putlog(LOG_CMDS,"*","#%s# nick %s",dcc[idx].nick,par);
    dprintf(idx,"Okay, changed.\n");
    if (dcc[idx].u.chat->channel>=0) {
      chanout2(dcc[idx].u.chat->channel,"Nick change: %s -> %s\n",
	       dcc[idx].nick,par);
      context;
      if (dcc[idx].u.chat->channel<100000) {
        tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
        tandout("join %s %s %d %c%d %s\n",botnetnick,par,
		dcc[idx].u.chat->channel,icon,dcc[idx].sock,dcc[idx].host);
      }
    }
    for (i=0; i<dcc_total; i++)
      if ((idx!=i) && (strcasecmp(dcc[idx].nick,dcc[i].nick)==0))
	strcpy(dcc[i].nick,par);
    strcpy(dcc[idx].nick,par);
  }
  else dprintf(idx,"Failed.\n");
}

void cmd_pls_host(idx,par)
int idx; char *par;
{
  char handle[512],host[512]; int chatr;
  nsplit(handle,par); nsplit(host,par);
  chatr=get_chanattr_handle(handle,dcc[idx].u.chat->con_chan);
  if (!host[0]) {
    dprintf(idx,"Usage: +host <handle> <newhostmask>\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if (ishost_for_handle(handle,host)) {
    dprintf(idx,"That hostmask is already there.\n");
    return;
  }
  if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) && 
      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_OWNER))) &&
      (!(get_attr_handle(handle) & USER_BOT))) {
    dprintf(idx,"You can't add hostmasks to non-bots.\n");
    return;
  }
  if (chatr==0 && (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
    dprintf(idx,"You can't add hostmasks to non-channel users.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & USER_OWNER) &&
      !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't add hostmasks to the bot owner.\n");
    return;
  }
#endif
  putlog(LOG_CMDS,"*","#%s# +host %s %s",dcc[idx].nick,handle,host);
  addhost_by_handle(handle,host);
  dprintf(idx,"Added '%s' to %s\n",host,handle);
}

void cmd_mns_host(idx,par)
int idx; char *par;
{
  char handle[512],host[512]; int chatr;
  nsplit(handle,par); nsplit(host,par);
  chatr=get_chanattr_handle(handle,dcc[idx].u.chat->con_chan);
  if (!host[0]) {
    dprintf(idx,"Usage: -host <handle> <hostmask>\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_OWNER))) &&
      (!(get_attr_handle(handle) & USER_BOT))) {
    dprintf(idx,"You can't remove hostmasks from non-bots.\n");
    return;
  }
  if (chatr==0 && (!(get_attr_handle(dcc[idx].nick) & USER_MASTER))) {
    dprintf(idx,"You can't remove hostmasks from non-channel users.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & BOT_SHARE) &&               
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"You can't remove hostmask from a shared bot.\n");
    return;                               
  }
  if ((get_attr_handle(handle) & USER_OWNER) &&
      !(get_attr_handle(dcc[idx].nick) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't remove hostmasks from the bot owner.\n");
    return;
  }
#endif
  if (delhost_by_handle(handle,host)) {
    putlog(LOG_CMDS,"*","#%s# -host %s %s",dcc[idx].nick,handle,host);
    dprintf(idx,"Removed '%s' from %s\n",host,handle);
  }
  else dprintf(idx,"Failed.\n");
}

void cmd_chpass(idx,par)
int idx; char *par;
{
  char handle[512],new[512];
  if (!par[0]) {
    dprintf(idx,"Usage: chpass <handle> [password]\n");
    return;
  }
  split(handle,par);
  if (!handle[0]) {
    if (!is_user(par)) {
      dprintf(idx,"No such user.\n");
      return;
    }
    if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
        (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_OWNER))) &&
        (!(get_attr_handle(par) & USER_BOT))) {
      dprintf(idx,"You can't change password for non-bots.\n");
      return;
    }
#ifdef OWNER
    if ((get_attr_handle(par) & BOT_SHARE) && 
        (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
      dprintf(idx,"You can't change shared bot's password.\n");
      return;
    }
    if ((get_attr_handle(par) & USER_OWNER) &&
        (strcasecmp(par,dcc[idx].nick)!=0)) {
      dprintf(idx,"Can't change the bot owner's password.\n");
      return;
    }
#endif
    putlog(LOG_CMDS,"*","#%s# chpass %s [nothing]",dcc[idx].nick,par);
    change_pass_by_handle(par,"-");
    dprintf(idx,"Removed password.\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if ((get_attr_handle(dcc[idx].nick) & USER_BOTMAST) &&
      (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_OWNER))) &&
      (!(get_attr_handle(handle) & USER_BOT))) {
    dprintf(idx,"You can't change password for non-bots.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & BOT_SHARE) &&
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"You can't change shared bot's password.\n");
    return;
  } 
  if ((get_attr_handle(handle) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't change the bot owner's password.\n");
    return;
  }
#endif
  nsplit(new,par);
  if (strlen(new)>9) new[9]=0;
  if (strlen(new)<4) {
    dprintf(idx,"Please use at least 4 characters.\n");
    return;
  }
  change_pass_by_handle(handle,new);
  putlog(LOG_CMDS,"*","#%s# chpass %s [something]",dcc[idx].nick,handle);
  dprintf(idx,"Changed password.\n");
}

void cmd_chaddr(idx,par)
int idx; char *par;
{
  char handle[512],addr[512];
  nsplit(handle,par); nsplit(addr,par);
  if (!handle[0]) {
    dprintf(idx,"Usage: chaddr <botname> <address:botport#/userport#>\n");
    return;
  }
  if (!(get_attr_handle(handle) & USER_BOT)) {
    dprintf(idx,"Useful only for tandem bots.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & BOT_SHARE) &&               
      (!(get_attr_handle(dcc[idx].nick) & USER_OWNER))) {
    dprintf(idx,"You can't change shared bot's address.\n");
    return;                               
  }                                               
#endif
  putlog(LOG_CMDS,"*","#%s# chaddr %s %s",dcc[idx].nick,handle,addr);
  dprintf(idx,"Changed bot's address.\n");
  set_handle_info(userlist,handle,addr);
}

void cmd_comment(idx,par)
int idx; char *par;
{
  char handle[512];
  split(handle,par); if (!handle[0]) {
    dprintf(idx,"Usage: comment <handle> <newcomment>\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user!\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't change comment on the bot owner.\n");
    return;
  }
#endif
  putlog(LOG_CMDS,"*","#%s# comment %s %s",dcc[idx].nick,handle,par);
  if (strcasecmp(par,"none")==0) {
    dprintf(idx,"Okay, comment blanked.\n");
    set_handle_comment(userlist,handle,"");
    return;
  }
  dprintf(idx,"Changed comment.\n");
  set_handle_comment(userlist,handle,par);
}

void cmd_email(idx,par)
int idx; char *par;
{
  char s[161];
  if (!par[0]) {
    putlog(LOG_CMDS,"*","#%s# email",dcc[idx].nick);
    get_handle_email(dcc[idx].nick,s);
    if (s[0]) {
      dprintf(idx,"Your email address is: %s\n",s);
      dprintf(idx,"(You can remove it with '.email none')\n");
    }
    else dprintf(idx,"You have no email address set.\n");
    return;
  }
  if (strcasecmp(par,"none")==0) {
    dprintf(idx,"Removed your email address.\n");
    set_handle_email(userlist,dcc[idx].nick,"");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# email %s",dcc[idx].nick,par);
  dprintf(idx,"Your email address: %s\n",par);
  set_handle_email(userlist,dcc[idx].nick,par);
}

void cmd_chemail(idx,par)
int idx; char *par;
{
  char handle[512];
  nsplit(handle,par);
  if (!handle[0]) {
    dprintf(idx,"Usage: chemail <handle> [address]\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't change email address of the bot owner.\n");
    return;
  }
#endif
  if (!par[0]) {
    putlog(LOG_CMDS,"*","#%s# chemail %s",dcc[idx].nick,handle);
    dprintf(idx,"Wiped email for %s\n",handle);
    set_handle_email(userlist,handle,"");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# chemail %s %s",dcc[idx].nick,handle,par);
  dprintf(idx,"Changed email for %s to: %s\n",handle,par);
  set_handle_email(userlist,handle,par);
}

#ifndef NO_IRC
void cmd_dump(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: dump <server stuff>\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# dump %s",dcc[idx].nick,par);
  mprintf(serv,"%s\n",par);
}
#endif

void cmd_reset(idx,par)
int idx; char *par;
{
  struct chanset_t *chan; int atr,chatr=0;
  atr=get_attr_handle(dcc[idx].nick);
  if (par[0]) {
    chan=findchan(par);
    if (chan==NULL) {
      dprintf(idx,"I don't monitor channel %s\n",par);
      return;
    }
    chatr=get_chanattr_handle(dcc[idx].nick,chan->name);
    if ((!(atr & USER_MASTER)) && (!(chatr & CHANUSER_MASTER))) {
      dprintf(idx,"You are not a master on %s.\n",chan->name);
      return;
    }
    putlog(LOG_CMDS,"*","#%s# reset %s",dcc[idx].nick,par);
    dprintf(idx,"Resetting channel info for %s...\n",par);
    reset_chan_info(chan); return;
  }
  if (!(atr & USER_MASTER)) {
    dprintf(idx,"You are not a Bot Master.\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# reset all",dcc[idx].nick);
  dprintf(idx,"Resetting channel info for all channels...\n");
  chan=chanset; while (chan!=NULL) {
    reset_chan_info(chan); chan=chan->next;
  }
}

void cmd_restart(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# restart",dcc[idx].nick);
  dprintf(idx,"Restarting.\n");
  if (make_userfile) {
    putlog(LOG_MISC,"*","Uh, guess you don't need to create a new userfile.");
    make_userfile=0;
  }
  write_userfile();
  putlog(LOG_MISC,"*","Restarting ...");
  wipe_timers(interp,&utimer);
  wipe_timers(interp,&timer);
/*  Tcl_DeleteInterp(interp);
  init_tcl(); */
  rehash();
}

void cmd_rehash(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# rehash",dcc[idx].nick);
  dprintf(idx,"Rehashing.\n");
  if (make_userfile) {
    putlog(LOG_MISC,"*","Uh, guess you don't need to create a new userfile.");
    make_userfile=0;
  }
  write_userfile();
  putlog(LOG_MISC,"*","Rehashing ...");
  rehash();
}

void cmd_reload(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# reload",dcc[idx].nick);
  dprintf(idx,"Reloading user file...\n");
  reload();
}

void cmd_die(idx,par)
int idx; char *par;
{
  char s[512];
  putlog(LOG_CMDS,"*","#%s# die %s",dcc[idx].nick,par);
  if (par[0]) {
    chatout("*** BOT SHUTDOWN (%s: %s)\n",dcc[idx].nick,par);
    tandout("chat %s BOT SHUTDOWN (%s: %s)\n",botnetnick,dcc[idx].nick,par);
    tprintf(serv,"QUIT :%s\n",par);
  }
  else {
    chatout("*** BOT SHUTDOWN (authorized by %s)\n",dcc[idx].nick);
    tandout("chat %s BOT SHUTDOWN (authorized by %s)\n",botnetnick,
	    dcc[idx].nick);
    tprintf(serv,"QUIT :%s\n",dcc[idx].nick);
  }
  tandout("bye\n");
  write_userfile();
  sleep(3);   /* give the server time to understand */
  sprintf(s,"DIE BY %s!%s (%s)",dcc[idx].nick,dcc[idx].host,par[0]?par:
	  "request");
  fatal(s,0);
}

#ifndef NO_IRC
void cmd_jump(idx,par)
int idx; char *par;
{
  char other[512],port[512];
  if (par[0]) {
    nsplit(other,par); nsplit(port,par);
    if (!port[0]) sprintf(port,"%d",DEFAULT_PORT);
    putlog(LOG_CMDS,"*","#%s# jump %s %s %s",dcc[idx].nick,other,port,par);
    strcpy(newserver,other); newserverport=atoi(port);
    strcpy(newserverpass,par);
  }
  else putlog(LOG_CMDS,"*","#%s# jump",dcc[idx].nick);
  dprintf(idx,"Jumping servers...\n");
  tprintf(serv,"QUIT :changing servers\n");
  if (serv>=0) killsock(serv); serv=(-1);
}
#endif

void cmd_debug(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# debug",dcc[idx].nick);
  debug_mem_to_dcc(idx);
}

void cmd_info(idx,par)
int idx; char *par;
{
  char s[512],chname[512]; int locked=0;
  if (!use_info) {
    dprintf(idx,"Info storage is turned off.\n");
    return;
  }
  /* yes, if the default info line is locked, then all the channel ones */
  /* are too. */
  get_handle_info(dcc[idx].nick,s);
  if (s[0]=='@') locked=1;
  if ((par[0]=='#') || (par[0]=='+') || (par[0]=='&')) {
    nsplit(chname,par);
    if (!defined_channel(chname)) {
      dprintf(idx,"No such channel.\n");
      return;
    }
    get_handle_chaninfo(dcc[idx].nick,chname,s);
    if (s[0]=='@') locked=1;
  }
  else chname[0]=0;
  if (!par[0]) {
    if (s[0]=='@') strcpy(s,&s[1]);
    if (s[0]) {
      if (chname[0]) {
	dprintf(idx,"Info on %s: %s\n",chname,s);
	dprintf(idx,"Use '.info %s none' to remove it.\n",chname);
      }
      else {
	dprintf(idx,"Default info: %s\n",s);
	dprintf(idx,"Use '.info none' to remove it.\n");
      }
    }
    else dprintf(idx,"No info has been set for you.\n");
    putlog(LOG_CMDS,"*","#%s# info %s",dcc[idx].nick,chname);
    return;
  }
  if ((locked) && !(get_attr_handle(dcc[idx].nick) & USER_MASTER)) {
    dprintf(idx,"Your info line is locked.  Sorry.\n");
    return;
  }
  if (strcasecmp(par,"none")==0) {
    par[0]=0;
    if (chname[0]) {
      set_handle_chaninfo(userlist,dcc[idx].nick,chname,par);
      dprintf(idx,"Removed your info line on %s.\n",chname);
      putlog(LOG_CMDS,"*","#%s# info %s none",dcc[idx].nick,chname);
    }
    else {
      set_handle_info(userlist,dcc[idx].nick,par);
      dprintf(idx,"Removed your default info line.\n");
      putlog(LOG_CMDS,"*","#%s# info none",dcc[idx].nick);
    }
    return;
  }
  if (par[0]=='@') strcpy(par,&par[1]);
  if (chname[0]) {
    set_handle_chaninfo(userlist,dcc[idx].nick,chname,par);
    dprintf(idx,"Your info on %s is now: %s\n",chname,par);
    putlog(LOG_CMDS,"*","#%s# info %s ...",dcc[idx].nick,chname);
  }
  else {
    set_handle_info(userlist,dcc[idx].nick,par);
    dprintf(idx,"Your default info is now: %s\n",par);
    putlog(LOG_CMDS,"*","#%s# info ...",dcc[idx].nick);
  }
}

void cmd_chinfo(idx,par)
int idx; char *par;
{
  char handle[512],chname[512];
  if (!use_info) {
    dprintf(idx,"Info storage is turned off.\n");
    return;
  }
  split(handle,par);
  if (!handle[0]) {
    dprintf(idx,"Usage: chinfo <handle> [channel] <new-info>\n");
    return;
  }
  if (!is_user(handle)) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if ((par[0]=='#') || (par[0]=='+') || (par[0]=='&')) nsplit(chname,par);
  else chname[0]=0;
  if (get_attr_handle(handle) & USER_BOT) {
    dprintf(idx,"Useful only for users.\n");
    return;
  }
#ifdef OWNER
  if ((get_attr_handle(handle) & USER_OWNER) &&
      (strcasecmp(handle,dcc[idx].nick)!=0)) {
    dprintf(idx,"Can't change info for the bot owner.\n");
    return;
  }
#endif
  if (chname[0])
    putlog(LOG_CMDS,"*","#%s# chinfo %s %s %s",dcc[idx].nick,handle,chname,par);
  else putlog(LOG_CMDS,"*","#%s# chinfo %s %s",dcc[idx].nick,handle,par);
  if (strcasecmp(par,"none")==0) par[0]=0;
  if (chname[0]) {
    set_handle_chaninfo(userlist,handle,chname,par);
    if (par[0]=='@')
      dprintf(idx,"New info (LOCKED) for %s on %s: %s\n",handle,chname,&par[1]);
    else if (par[0])
      dprintf(idx,"New info for %s on %s: %s\n",handle,chname,par);
    else dprintf(idx,"Wiped info for %s on %s\n",handle,chname);
  }
  else {
    set_handle_info(userlist,handle,par);
    if (par[0]=='@')
      dprintf(idx,"New default info (LOCKED) for %s: %s\n",handle,&par[1]);
    else if (par[0])
      dprintf(idx,"New default info for %s: %s\n",handle,par);
    else dprintf(idx,"Wiped default info for %s\n",handle);
  }
}

#ifdef ENABLE_SIMUL
void cmd_simul(idx,par)
int idx; char *par;
{
  char nick[512]; int i,ok=0;
  nsplit(nick,par);
  for (i=0; i<dcc_total; i++)
    if ((strcasecmp(nick,dcc[i].nick)==0) && (!ok) &&
	((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES))) {
      putlog(LOG_CMDS,"*","#%s# simul %s %s",dcc[idx].nick,nick,par);
      dcc_activity(dcc[i].sock,par,strlen(par)); ok=1;
    }
  if (!ok) dprintf(idx,"No such user on the party line.\n");
}
#endif

void cmd_link(idx,par)
int idx; char *par;
{
  char s[512]; int i;
  putlog(LOG_CMDS,"*","#%s# link %s",dcc[idx].nick,par);
  if (strchr(par,' ')==NULL) botlink(dcc[idx].nick,idx,par);
  else {
    split(s,par); i=nextbot(s); if (i<0) {
      dprintf(idx,"No such bot online.\n");
      return;
    }
    tprintf(dcc[i].sock,"link %d:%s@%s %s %s\n",dcc[idx].sock,dcc[idx].nick,
	    botnetnick,s,par);
  }
}

void cmd_unlink(idx,par)
int idx; char *par;
{
  int i;
  char bot[200];
  if (!par[0]) {
    dprintf(idx,"Usage: unlink <bot> [reason]\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# unlink %s",dcc[idx].nick,par);
  nsplit(bot,par);
  i=nextbot(bot); if (i<0) {
    botunlink(idx,bot,par);
    return;
  }
  /* if we're directly connected to that bot, just do it */
  if (strcasecmp(dcc[i].nick,bot)==0) botunlink(idx,bot,par);
  else {
    tprintf(dcc[i].sock,"unlink %d:%s@%s %s %s %s\n",dcc[idx].sock,
	    dcc[idx].nick,botnetnick,lastbot(bot),bot,par);
  }
}

void cmd_relay(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: relay <bot>\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# relay %s",dcc[idx].nick,par);
  tandem_relay(idx,par);
}

void cmd_save(idx,par)
int idx; char *par;
{
  putlog(LOG_CMDS,"*","#%s# save",dcc[idx].nick);
  write_userfile();
}

void cmd_trace(idx,par)
int idx; char *par;
{
  int i;
  if (!par[0]) {
    dprintf(idx,"Usage: trace <botname>\n");
    return;
  }
  if (strcasecmp(par,botnetnick)==0) {
    dprintf(idx,"That's me!  Hiya! :)\n");
    return;
  }
  i=nextbot(par); if (i<0) {
    dprintf(idx,"Unreachable bot.\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# trace %s",dcc[idx].nick,par);
  tprintf(dcc[i].sock,"trace %d:%s@%s %s %s\n",dcc[idx].sock,dcc[idx].nick,
	  botnetnick,par,botnetnick);
}

#ifndef NO_IRC
void cmd_topic(idx,par)
int idx; char *par;
{
  struct chanset_t *chan;
  if (!has_op(idx," ")) return;
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (!par[0]) {
    if (chan->channel.topic[0]==0) {
      dprintf(idx,"No topic is set for %s\n",chan->name);
      return;
    } 
    else {
      dprintf(idx,"The topic for %s is: %s\n",chan->name,chan->channel.topic);
      return;
    }
  }
  if ((channel_optopic(chan)) && (!me_op(chan))) {
    dprintf(idx,"I'm not a channel op on %s and the channel is +t.\n",
	    chan->name);
    return;
  }
  mprintf(serv,"TOPIC %s :%s\n",chan->name,par);
  dprintf(idx,"Changing topic...\n");
  strcpy(chan->channel.topic,par);
  putlog(LOG_CMDS,"*","#%s# (%s) .topic %s",dcc[idx].nick,chan->name,par);
}
#endif

void cmd_binds(idx,par)
int idx; char *par;
{
  tell_binds(idx,par);
  putlog(LOG_CMDS,"*","#%s# binds %s",dcc[idx].nick,par);
}

void cmd_banner(idx,par)
int idx; char *par;
{
  char s[540]; int i;
  if (!par[0]) {
    dprintf(idx,"Usage: banner <message>\n");
    return;
  }
  sprintf(s,"### \007\007\007BOTWIDE MESSAGE FROM %s:\n",dcc[idx].nick);
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES))
      dprintf(i,s);
  sprintf(s,"###   %s\n",par);
  for (i=0; i<dcc_total; i++)
    if ((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES))
      dprintf(i,s);
}

/* after messing with someone's user flags, make sure the dcc-chat flags
   are set correctly */
int check_dcc_attrs(char *hand,int flags,int oatr)
{
  int i,stat;
#ifdef OWNER
/* if it matches someone in the owner list, make sure he/she has +n */
  if (owner[0]) {
    char *p,s[512];
    strcpy(s,owner); p=strchr(s,','); while (p!=NULL) {
      *p=0; rmspace(s);
      if (strcasecmp(hand,s)==0) flags|=USER_OWNER;
      strcpy(s,p+1); p=strchr(s,',');
    }
    rmspace(s); if (strcasecmp(hand,s)==0) flags|=USER_OWNER;
    set_attr_handle(hand,flags);
  }
#endif
  for (i=0; i<dcc_total; i++) {
    if (((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES)) &&
	(strcasecmp(hand,dcc[i].nick)==0)) {
      if (dcc[i].type==DCC_CHAT) stat=dcc[i].u.chat->status;
      else stat=dcc[i].u.file->chat->status;
      if ((dcc[i].type==DCC_CHAT) && ((flags & 
          (USER_GLOBAL|USER_MASTER|USER_OWNER|USER_BOTMAST))!=(oatr & 
          (USER_GLOBAL|USER_MASTER|USER_OWNER|USER_BOTMAST)))) {
        tandout("part %s %s\n",botnetnick,dcc[i].nick);
        tandout("join %s %s %d %c %s\n",botnetnick,dcc[i].nick,
		dcc[i].u.chat->channel,geticon(i),dcc[i].host);
      }
      if ((oatr&USER_MASTER) && !(flags&USER_MASTER)) {
        dcc[i].u.chat->con_flags &= ~(LOG_MISC|LOG_CMDS|LOG_RAW|LOG_FILES|
                                      LOG_LEV1|LOG_LEV2|LOG_LEV3|LOG_LEV4|
                                      LOG_LEV5|LOG_LEV6|LOG_LEV7|LOG_LEV8|
				      LOG_WALL|LOG_DEBUG);
	if (master_anywhere(hand)) dcc[i].u.chat->con_flags |= (LOG_MISC|LOG_CMDS);
	dprintf(i,"*** POOF! ***\n");
	dprintf(i,"You are no longer a master on this bot.\n");
      }
      if (!(oatr&USER_MASTER) && (flags&USER_MASTER)) {
	dcc[i].u.chat->con_flags |= conmask;
	dprintf(i,"*** POOF! ***\n");
	dprintf(i,"You are now a master on this bot.\n");
      }
      if (!(oatr&USER_BOTMAST) && (flags&USER_BOTMAST)) {
	dprintf(i,"### POOF! ###\n");
	dprintf(i,"You are now a botnet master on this bot.\n");
      }
      if ((oatr & USER_BOTMAST) && !(flags&USER_BOTMAST)) {
	dprintf(i,"### POOF! ###\n");
	dprintf(i,"You are no longer a botnet master on this bot.\n");
      }
#ifdef OWNER
      if (!(oatr & USER_OWNER) && (flags&USER_OWNER)) {
	dprintf(i,"@@@ POOF! @@@\n");
	dprintf(i,"You are now an OWNER of this bot.\n");
      }
      if ((oatr & USER_OWNER) && !(flags&USER_OWNER)) {
	dprintf(i,"@@@ POOF! @@@\n");
	dprintf(i,"You are no longer an owner of this bot.\n");
      }
#endif
      if ((stat&STAT_PARTY) && (flags&USER_GLOBAL)) stat&=~STAT_PARTY;
      if (!(stat&STAT_PARTY) && !(flags&USER_GLOBAL) && !(flags&USER_MASTER))
	stat|=STAT_PARTY;
      if ((stat&STAT_CHAT) && !(flags&USER_PARTY) && !(flags&USER_MASTER) &&
	  (!(flags&USER_GLOBAL) || require_p))
	stat&=~STAT_CHAT;
      if ((dcc[i].type==DCC_FILES) && !(stat&STAT_CHAT) && 
	  ((flags&USER_MASTER) || (flags&USER_PARTY) || 
	  ((flags&USER_GLOBAL) && !require_p)))
	stat|=STAT_CHAT;
      if (dcc[i].type==DCC_CHAT) dcc[i].u.chat->status=stat;
      else dcc[i].u.file->chat->status=stat;
      /* check if they no longer have access to wherever they are */
      /* DON'T kick someone off the party line just cos they lost +p */
      /* (pinvite script removes +p after 5 mins automatically) */
      if ((dcc[i].type==DCC_FILES) && !(flags&USER_XFER) &&
	  !(flags&USER_MASTER)) {
	dprintf(i,"-+- POOF! -+-\n");
	dprintf(i,"You no longer have file area access.\n\n");
	putlog(LOG_MISC,"*","DCC user [%s]%s removed from file system",
	       dcc[i].nick,dcc[i].host);
	if (dcc[i].u.file->chat->status&STAT_CHAT) {
	  struct chat_info *ci;
	  ci=dcc[i].u.file->chat; nfree(dcc[i].u.file);
	  dcc[i].u.chat=ci;
	  dcc[i].u.chat->status&=(~STAT_CHAT); dcc[i].type=DCC_CHAT;
	  if (dcc[i].u.chat->channel>=0) {
	    chanout2(dcc[i].u.chat->channel,"%s has returned.\n",dcc[i].nick);
	    context;
	    if (dcc[i].u.chat->channel<100000)
	      tandout("unaway %s %d\n",botnetnick,dcc[i].sock);
	  }
	}
	else {
	  killsock(dcc[i].sock); 
	  dcc[i].sock=dcc[i].type; dcc[i].type=DCC_LOST;
	  tandout("part %s %s %d\n",botnetnick,dcc[i].nick,dcc[i].sock);
	}
      }
    }
    if ((dcc[i].type==DCC_BOT) && (strcasecmp(hand,dcc[i].nick)==0)) {
      if ((dcc[i].u.bot->status&STAT_LEAF) && !(flags&BOT_LEAF))
	dcc[i].u.bot->status&=~(STAT_LEAF|STAT_WARNED);
      if (!(dcc[i].u.bot->status&STAT_LEAF) && (flags&BOT_LEAF))
	dcc[i].u.bot->status|=STAT_LEAF;
    }
  }
  return flags;
}

int check_dcc_chanattrs(char *hand,char *chname,int chflags,int ochatr)
{
  int i,chatr,found=0; struct chanset_t *chan; chan=chanset;
  for (i=0; i<dcc_total; i++) {
    if (((dcc[i].type==DCC_CHAT) || (dcc[i].type==DCC_FILES)) &&
        (strcasecmp(hand,dcc[i].nick)==0)) {
      if ((dcc[i].type==DCC_CHAT) && ((chflags &
          (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER))!=(ochatr &
          (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER)))) {
        tandout("part %s %s\n",botnetnick,dcc[i].nick);
        tandout("join %s %s %d %c %s\n",botnetnick,dcc[i].nick,
		dcc[i].u.chat->channel,geticon(i),dcc[i].host);
      }
      if ((ochatr&CHANUSER_MASTER) && !(chflags&CHANUSER_MASTER)) {
        if (!(get_attr_handle(hand) & USER_MASTER)) 
	  dcc[i].u.chat->con_flags &= ~(LOG_MISC|LOG_CMDS);
        dprintf(i,"*** POOF! ***\n");
        dprintf(i,"You are no longer a master on %s.\n",chname);
      }
      if (!(ochatr&CHANUSER_MASTER) && (chflags&CHANUSER_MASTER)) {
        dcc[i].u.chat->con_flags |= conmask;
	if (!(get_attr_handle(hand) & USER_MASTER))
	  dcc[i].u.chat->con_flags &= ~(LOG_LEV1|LOG_LEV2|LOG_LEV3|LOG_LEV4|
				        LOG_LEV5|LOG_LEV6|LOG_LEV7|LOG_LEV8|
				        LOG_RAW|LOG_DEBUG|LOG_WALL|LOG_FILES);
        dprintf(i,"*** POOF! ***\n");
        dprintf(i,"You are now a master on %s.\n",chname);
      }
#ifdef OWNER
      if (!(ochatr & CHANUSER_OWNER) && (chflags&CHANUSER_OWNER)) {
        dprintf(i,"@@@ POOF! @@@\n");
        dprintf(i,"You are now an OWNER of %s.\n",chname);
      }
      if ((ochatr & CHANUSER_OWNER) && !(chflags&CHANUSER_OWNER)) {
        dprintf(i,"@@@ POOF! @@@\n");
        dprintf(i,"You are no longer an owner of %s.\n",chname);
      }
#endif
      if (((ochatr & (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER)) &&
	  (!(chflags & (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER)))) ||
	  ((chflags & (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER)) &&
	  (!(ochatr & (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER))))) {
        while (chan!=NULL && found==0) {
	  chatr=get_chanattr_handle(dcc[i].nick,chan->name);
	  if (chatr & (CHANUSER_OP|CHANUSER_MASTER|CHANUSER_OWNER)) found=1;
	  else chan=chan->next;
        }
        if (chan==NULL) chan=chanset;
        strcpy(dcc[i].u.chat->con_chan,chan->name);
      }
    }
  }
  return chflags;
}

/* some flags are mutually exclusive -- this roots them out */
int sanity_check(int atr)
{
  if ((atr&BOT_HUB) && (atr&BOT_ALT)) atr&=~BOT_ALT;
  if (atr&BOT_REJECT) {
    if (atr&BOT_SHARE) atr&=~(BOT_SHARE|BOT_REJECT);
    if (atr&BOT_HUB) atr&=~(BOT_HUB|BOT_REJECT);
    if (atr&BOT_ALT) atr&=~(BOT_ALT|BOT_REJECT);
  }
  if (atr&USER_BOT) {
    if (atr & (USER_PARTY|USER_MASTER|USER_COMMON|USER_OWNER))
      atr &= ~(USER_PARTY|USER_MASTER|USER_COMMON|USER_OWNER);
  }
  else {
    if (atr & (BOT_LEAF|BOT_REJECT|BOT_ALT|BOT_SHARE|BOT_HUB))
      atr &= ~(BOT_LEAF|BOT_REJECT|BOT_ALT|BOT_SHARE|BOT_HUB);
  }
#ifdef OWNER
  /* can't be owner without also being master */
  if (atr&USER_OWNER) atr|=USER_MASTER;
#endif
  /* can't be botnet master without party-line access */
  if (atr&USER_BOTMAST) atr|=USER_PARTY;
  return atr;
}

/* sanity check on channel attributes */
int chan_sanity_check(chatr)
int chatr;
{
  if ((chatr&CHANUSER_OP) && (chatr&CHANUSER_DEOP))
    chatr&=~(CHANUSER_OP|CHANUSER_DEOP);
#ifdef OWNER
  /* can't be channel owner without also being channel master */
  if (chatr&CHANUSER_OWNER) chatr|=CHANUSER_MASTER;
#endif
  return chatr;
}

void cmd_chattr(idx,par)
int idx; char *par;
{
  char hand[512],s[21],chg[512]; struct chanset_t *chan;
  int i,pos,f,atr,oatr,chatr=0,ochatr=0,recheck=0,own,chown=0;
  if (!par[0]) {
    dprintf(idx,"Usage: chattr <handle> [changes [channel]]\n");
    return;
  }
  nsplit(hand,par); 
  if ((hand[0]=='*') || (!is_user(hand))) {
    dprintf(idx,"No such user!\n");
    return;
  }
  own=(get_attr_handle(dcc[idx].nick) & USER_OWNER);
  oatr=atr=get_attr_handle(hand); pos=1;
  if (par[0]) {
    /* make changes */
    nsplit(chg,par);
    if (par[0]) {
      if ((!defined_channel(par)) && (par[0]!='*')) {
	dprintf(idx,"No channel record for %s\n",par);
	return;
      }
    }
    if (!par[0] && (!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_BOTMAST))) 
        && (!(get_chanattr_handle(dcc[idx].nick,par) & CHANUSER_MASTER))) {
      dprintf (idx,"You do not have Bot Master privileges.\n");
      return;
    }
    if (!(get_chanattr_handle(dcc[idx].nick,par) & CHANUSER_MASTER) &&
	!(get_attr_handle(dcc[idx].nick) & (USER_MASTER|USER_BOTMAST))) {
      dprintf(idx,"You do not have channel master privileges for channel %s\n",par);
      return;
    }
    if (par[0]) {
      if (par[0]=='*') {
	chown=0;
	ochatr=chatr=get_chanattr_handle(hand,chanset->name);
      } 
      else {
	chown=(get_chanattr_handle(dcc[idx].nick,par)&CHANUSER_OWNER);
	ochatr=chatr=get_chanattr_handle(hand,par);
      }
    }
    for (i=0; i<strlen(chg); i++) {
      if (chg[i]=='+') pos=1;
      else if (chg[i]=='-') pos=0;
      else {
	s[1]=0; s[0]=chg[i];
	if (par[0]) {
	  /* channel-specific */
	  f=str2chflags(s);
#ifdef OWNER
	  if (!own && !chown) f&=~(CHANUSER_MASTER|CHANUSER_OWNER);
	/* only owners and channel owners can +/- channel masters and owners */
#endif
	  change_chanflags(userlist,hand,par,(pos?f:0),(pos?0:f));
	  recheck=1; /* any change to channel flags means recheck chans */
	  /* FIXME: other stuff here? */
	}
	else {
	  f=str2flags(s);
	  /* nobody can modify the +b bot flag */
	  f&=~USER_BOT;
	  /* nobody can modify the +M, +N, and +O psuedo flags */
	  f&=~(USER_PSUEDOOP|USER_PSUMST|USER_PSUOWN);
#ifdef OWNER
	  if (!own) f&=(~(USER_OWNER|USER_MASTER|USER_BOTMAST|BOT_SHARE));
	  /* only owner can do +/- owner,master,share,botnet master */
#endif
	  atr=pos?(atr|f):(atr&~f);
	}
      }
    }
    if (!par[0]) {
      atr=sanity_check(atr);
      atr=check_dcc_attrs(hand,atr,oatr);
      set_attr_handle(hand,atr);
    }
  }
  if (par[0])
    putlog(LOG_CMDS,"*","#%s# chattr %s %s %s",dcc[idx].nick,hand,chg,par);
  else if (chg[0]) putlog(LOG_CMDS,"*","#%s# chattr %s %s",dcc[idx].nick,hand,chg);
  else putlog(LOG_CMDS,"*","#%s# chattr %s",dcc[idx].nick,hand);
  /* get current flags and display them */
  if (!par[0]) {
    flags2str(atr,s);
    dprintf(idx,"Flags for %s are now +%s\n",hand,s);
  }
  else if (par[0]=='*') {
    /* every channel */
    chan=chanset; while (chan!=NULL) {
      chatr=get_chanattr_handle(hand,chan->name);
      chatr=chan_sanity_check(chatr);
      if (chan==chanset) chatr=check_dcc_chanattrs(hand,chan->name,chatr,ochatr);
      chflags2str(chatr,s);
      dprintf(idx,"Flags for %s are now +%s %s\n",hand,s,chan->name);
      set_chanattr_handle(hand,chan->name,chatr);
      chan=chan->next;
    }
  }
  else {
    chatr=get_chanattr_handle(hand,par);
    chatr=chan_sanity_check(chatr);
    chatr=check_dcc_chanattrs(hand,par,chatr,ochatr);
    chflags2str(chatr,s);
    dprintf(idx,"Flags for %s are now +%s %s\n",hand,s,par);
    set_chanattr_handle(hand,par,chatr);
  }
  if (recheck) {
    chan=chanset; while (chan!=NULL) {
      recheck_channel(chan);
      chan=chan->next;
    }
  }
}

void cmd_notes(idx,par)
int idx; char *par;
{
  char fcn[512];
  if (!par[0]) {
    dprintf(idx,"Usage: notes index\n");
    dprintf(idx,"       notes read <# or ALL>\n");
    dprintf(idx,"       notes erase <# or ALL>\n");
    return;
  }
  nsplit(fcn,par);
  if (strcasecmp(fcn,"index")==0) notes_read(dcc[idx].nick,"",-1,idx);
  else if (strcasecmp(fcn,"read")==0) {
    if (strcasecmp(par,"all")==0) notes_read(dcc[idx].nick,"",0,idx);
    else notes_read(dcc[idx].nick,"",atoi(par),idx);
  }
  else if (strcasecmp(fcn,"erase")==0) {
    if (strcasecmp(par,"all")==0) notes_del(dcc[idx].nick,"",0,idx);
    else notes_del(dcc[idx].nick,"",atoi(par),idx);
  }
  else dprintf(idx,"Function must be one of INDEX, READ, or ERASE.\n");
  putlog(LOG_CMDS,"*","#%s# notes %s %s",dcc[idx].nick,fcn,par);
}

void cmd_chat(idx,par)
int idx; char *par;
{
  int newchan,oldchan;
  if (strcasecmp(par,"off")==0) {
    /* turn chat off */
    if (dcc[idx].u.chat->channel<0)
      dprintf(idx,"You weren't in chat anyway!\n");
    else {
      dprintf(idx,"Leaving chat mode...\n");
      check_tcl_chpt(botnetnick,dcc[idx].nick,dcc[idx].sock);
      chanout2(dcc[idx].u.chat->channel,"%s left the party line.\n",
	       dcc[idx].nick);
      context;
      if (dcc[idx].u.chat->channel<100000)
        tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
    }
    dcc[idx].u.chat->channel=(-1);
  }
  else {
    if (par[0]=='*') {     
      if (((par[1]<'0') || (par[1]>'9'))) {
	if (par[1]==0) newchan=0;
	else newchan=get_assoc(par+1);
	if (newchan<0) {
	  dprintf(idx,"No channel by the name.\n");
	  return;
	}
      }
      else newchan=100000+atoi(par+1);
      if (newchan<100000 || newchan>199999) {
	dprintf(idx,"Channel # out of range: local channels must be *0-*99999\n");
	return;
      }
    }
    else {
      if (((par[0]<'0') || (par[0]>'9')) && (par[0])) {
        if (strcasecmp(par,"on")==0) newchan=0;
        else newchan=get_assoc(par);
        if (newchan<0) {
	  dprintf(idx,"No channel by that name.\n");
	  return;
        }
      }
      else newchan=atoi(par);
      if ((newchan<0) || (newchan>99999)) {
        dprintf(idx,"Channel # out of range: must be between 0 and 99999.\n");
        return;
      }
    }
    /* if coming back from being off the party line, make sure they're
       not away */
    if ((dcc[idx].u.chat->channel<0) && (dcc[idx].u.chat->away!=NULL))
      not_away(idx);
    if (dcc[idx].u.chat->channel==newchan) {
      if (newchan==0) dprintf(idx,"You're already on the party line!\n");
      else dprintf(idx,"You're already on channel %s%d!\n",
		   (newchan<100000)?"":"*",newchan%100000);
    }
    else {
      oldchan=dcc[idx].u.chat->channel;
      if (oldchan>=0)
	check_tcl_chpt(botnetnick,dcc[idx].nick,dcc[idx].sock);
      if (oldchan==0) {
	chanout2(0,"%s left the party line.\n",dcc[idx].nick);
        context;
      }
      else if (oldchan>0) {
	chanout2(oldchan,"%s left the channel.\n",dcc[idx].nick);
        context;
      }
      dcc[idx].u.chat->channel=newchan;
      if (newchan==0) {
	dprintf(idx,"Entering the party line...\n");
	chanout2(0,"%s joined the party line.\n",dcc[idx].nick);
        context;
      }
      else {
	dprintf(idx,"Joining channel '%s'...\n",par);
	chanout2(newchan,"%s joined the channel.\n",dcc[idx].nick);
        context;
      }
      check_tcl_chjn(botnetnick,dcc[idx].nick,newchan,geticon(idx),
		     dcc[idx].sock,dcc[idx].host);
      if (newchan<100000)
        tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[idx].nick,newchan,
	        geticon(idx),dcc[idx].sock,dcc[idx].host);
      else if (oldchan<100000)
        tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
    }
  }
}

void cmd_echo(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Echo is currently %s.\n",dcc[idx].u.chat->status&STAT_ECHO?
	    "on":"off");
    return;
  }
  if (strcasecmp(par,"on")==0) {
    dprintf(idx,"Echo turned on.\n");
    dcc[idx].u.chat->status|=STAT_ECHO;
    return;
  }
  if (strcasecmp(par,"off")==0) {
    dprintf(idx,"Echo turned off.\n");
    dcc[idx].u.chat->status&=~STAT_ECHO;
    return;
  }
  dprintf(idx,"Usage: echo <on/off>\n");
}

void cmd_assoc(idx,par)
int idx; char *par;
{
  char num[512]; int chan;
  if (!par[0]) {
    putlog(LOG_CMDS,"*","#%s# assoc",dcc[idx].nick);
    dump_assoc(idx);
    return;
  }
  nsplit(num,par);
  if (num[0]=='*') {
    chan=100000+atoi(num+1);
    if (chan<100000 || chan>19999) {
      dprintf(idx,"Channel # out of range: must be *0-*99999\n");
      return;
    }
  }
  else {
    chan=atoi(num);
    if (chan==0) {
      dprintf(idx,"You can't name the main party line; it's just a party line.\n");
      return;
    }
    if ((chan<1) || (chan>99999)) {
      dprintf(idx,"Channel # out of range: must be 1-99999\n");
      return;
    }
  }
  if (!par[0]) {
    /* remove an association */
    if (get_assoc_name(chan)==NULL) {
      dprintf(idx,"Channel %s%d has no name.\n",
	      (chan<100000) ? "":"*",chan%100000);
      return;
    }
    kill_assoc(chan);
    putlog(LOG_CMDS,"*","#%s# assoc %d",dcc[idx].nick,chan);
    dprintf(idx,"Okay, removed name for channel %s%d.\n",
	    (chan<100000)?"":"*",chan%100000);
    chanout2(chan,"%s removed this channel's name.\n",dcc[idx].nick);
    context;
    if (chan<100000) tandout("assoc %d 0\n",chan);
    return;
  }
  if (strlen(par)>20) {
    dprintf(idx,"Channel's name can't be that long (20 chars max).\n");
    return;
  }
  if ((par[0]>='0') && (par[0]<='9')) {
    dprintf(idx,"First character of the channel name can't be a digit.\n");
    return;
  }
  add_assoc(par,chan);
  putlog(LOG_CMDS,"*","#%s# assoc %d %s",dcc[idx].nick,chan,par);
  dprintf(idx,"Okay, channel %s%d is '%s' now.\n",
	  (chan<100000)?"":"*",chan%100000,par);
  chanout2(chan,"%s named this channel '%s'\n",dcc[idx].nick,par);
  context;
  if (chan<100000) tandout("assoc %d %s\n",chan,par);
}

void cmd_fries(idx,par)
int idx; char *par;
{
  dprintf(idx,"* %s juliennes some fries for you.\n",botnetnick);
  dprintf(idx,"Enjoy!\n");
}

void cmd_beer(idx,par)
int idx; char *par;
{
  dprintf(idx,"* %s throws you a cold brew.\n",botnetnick);
  dprintf(idx,"It's Miller time. :)\n");
}

void cmd_flush(idx,par)
int idx; char *par;
{
  if (!par[0]) {
    dprintf(idx,"Usage: flush <botname>\n");
    return;
  }
  if (flush_tbuf(par))
    dprintf(idx,"Flushed resync buffer for %s\n",par);
  else dprintf(idx,"There is no resync buffer for that bot.\n");
}

int stripmodes(char *s)
{
  int i; int res=0;
  for (i=0; i<strlen(s); i++)
    switch(tolower(s[i])) {
      case 'b': res|=STRIP_BOLD; break;
      case 'c': res|=STRIP_COLOR; break;
      case 'r': res|=STRIP_REV; break;
      case 'u': res|=STRIP_UNDER; break;
      case '*': res|=STRIP_ALL; break;
    }
  return res;
}

char *stripmasktype(int x)
{
  static char s[20]; char *p=s;
  if (x&STRIP_BOLD) *p++='b';
  if (x&STRIP_COLOR) *p++='c';
  if (x&STRIP_REV) *p++='r';
  if (x&STRIP_UNDER) *p++='u';
  *p=0;
  return s;
}

char *stripmaskname(int x)
{
  static char s[161];
  s[0]=0;
  if (x&STRIP_BOLD) strcat(s,"bold, ");
  if (x&STRIP_COLOR) strcat(s,"color, ");
  if (x&STRIP_REV) strcat(s,"reverse, ");
  if (x&STRIP_UNDER) strcat(s,"underline, ");
  if (!s[0]) strcpy(s,"none, ");
  s[strlen(s)-2]=0;
  return s;
}

void cmd_strip(idx,par)
int idx; char *par;
{
  char nick[512],s[2],s1[512]; int dest=0,i,pls,md,ok=0,atr;
  if (!par[0]) {
    dprintf(idx,"Your current strip settings are: %s (%s)\n",
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
    return;
  }
  strcpy(s1,par);
  split(nick,par);
  atr=get_attr_handle(dcc[idx].nick);
  if ((nick[0]) && (nick[0]!='+') && (nick[0]!='-') &&
      (atr&USER_MASTER)) {
    for (i=0; i<dcc_total; i++)
      if ((strcasecmp(nick,dcc[i].nick)==0) && (dcc[i].type==DCC_CHAT) &&
         (!ok)) {
        ok=1; dest=i;
      }
    if (!ok) {
      dprintf(idx,"No such user on the party line!\n");
      return;
    }
    nick[0]=0;
  }
  else dest=idx;
  if (!nick[0]) nsplit(nick,par);
  while (nick[0]) {
    pls=1;
    if ((nick[0]!='+') && (nick[0]!='-')) dcc[dest].u.chat->strip_flags=0;
    for (i=0; i<strlen(nick); i++) {
      if (nick[i]=='+') pls=1;
      else if (nick[i]=='-') pls=(-1);
      else {
	s[0]=nick[i]; s[1]=0;
	md=stripmodes(s);
	if (pls==1) dcc[dest].u.chat->strip_flags|=md;
	else dcc[dest].u.chat->strip_flags&=~md;
      }
    }
    nsplit(nick,par);
  }
  putlog(LOG_CMDS,"*","#%s# strip %s",dcc[idx].nick,s1);
  if (dest==idx) {
    dprintf(idx,"Your strip settings are: %s (%s)\n",
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
  }
  else {
    dprintf(idx,"Strip setting for %s: %s (%s)\n",dcc[dest].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
    dprintf(dest,"%s set your strip settings to: %s (%s)\n",dcc[idx].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
  }
}

void cmd_stick_yn(idx,par,yn)
int idx; char *par; int yn;
{
  int i,j; struct chanset_t *chan; char s[UHOSTLEN];
  if (!par[0]) {
    dprintf(idx,"Usage: %sstick <ban>\n",yn?"":"un");
    return;
  }
  i=setsticky_ban(par,yn);
  if (i>0) {
    putlog(LOG_CMDS,"*","#%s# %sstick %s",dcc[idx].nick,yn?"":"un",par);
    dprintf(idx,"%stuck: %s\n",yn?"S":"Uns",par);
    return;
  }
  /* channel-specific ban? */
  chan=findchan(dcc[idx].u.chat->con_chan);
  if (chan==NULL) {
    dprintf(idx,"Invalid console channel.\n"); return;
  }
  if (atoi(par)) sprintf(s,"%d",i+atoi(par));
  else strcpy(s,par);
  j=u_setsticky_ban(chan->bans,s,yn);
  if (j>0) {
    putlog(LOG_CMDS,"*","#%s# %sstick %s",dcc[idx].nick,yn?"":"un",s);
    dprintf(idx,"%stuck: %s\n",yn?"S":"Uns",s);
    return;
  }
  /* well fuxor then. */
  dprintf(idx,"No such ban.\n");
}

void cmd_stick(idx,par)
int idx; char *par;
{
  cmd_stick_yn(idx,par,1);
}

void cmd_unstick(idx,par)
int idx; char *par;
{
  cmd_stick_yn(idx,par,0);
}

void cmd_filestats (idx,par)
int idx; char *par;
{
  char nick[512];
  context;
  nsplit(nick,par);
  if (nick[0]==0) tell_file_stats(idx,dcc[idx].nick);
  else if (!is_user(nick)) dprintf(idx,"No such user.\n");
  else if ((!strcmp(par,"clear")) &&
	   !(get_attr_handle(dcc[idx].nick)&USER_MASTER)) {
    set_handle_uploads(userlist,nick,0,0);
    set_handle_dnloads(userlist,nick,0,0);
  }
  else tell_file_stats(idx,nick);
}
   
void cmd_pls_chrec (idx,par)
int idx; char *par;
{
  char nick[512], chn[512]; struct chanset_t *chan; struct userrec *u;
  struct chanuserrec *chanrec;
  context;
  if (!par[0]) {
    dprintf(idx,"Usage: +chrec <User> [channel]\n");
    return;
  }
  nsplit(nick,par);
  u=get_user_by_handle(userlist,nick);
  if (u==NULL) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if (par[0]==0) chan=findchan(dcc[idx].u.chat->con_chan);
  else {
   nsplit(chn,par);
   chan=findchan(chn);
  }
  if (chan==NULL) {
    dprintf(idx,"No such channel.\n");
    return;
  }  
  if (!(get_attr_handle(dcc[idx].nick) & USER_MASTER) &&
      !(get_chanattr_handle(dcc[idx].nick,chan->name) & CHANUSER_MASTER)) {
    dprintf(idx,"You have no permission to do that on %s.\n",
	    chan->name);
    return;
  }
  chanrec=get_chanrec(u,chan->name);
  if (chanrec!=NULL) {
    dprintf(idx,"User %s already has a channel record for %s.\n",nick,chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# +chrec %s %s",dcc[idx].nick,nick,chan->name);
  add_chanrec(u,chan->name,0,0);
  dprintf(idx,"Added %s channel record for %s.\n",chan->name,nick);
}

void cmd_mns_chrec (idx,par)
int idx; char *par;
{
  char nick[512],chn[512]; struct chanset_t *chan; struct userrec *u;
  struct chanuserrec *chanrec;
  context;
  if (!par[0]) {
    dprintf(idx,"Usage: -chrec <User> [channel]\n");
    return;
  }
  nsplit(nick,par);
  u=get_user_by_handle(userlist,nick);
  if(u==NULL) {
    dprintf(idx,"No such user.\n");
    return;
  }
  if (par[0]==0) chan=findchan(dcc[idx].u.chat->con_chan);
  else {
   nsplit(chn,par);
   chan=findchan(chn);
  }
  if (chan==NULL) {
    dprintf(idx,"No such channel.\n");
    return;
  }  
  if (!(get_attr_handle(dcc[idx].nick)&USER_MASTER) &&
      !(get_chanattr_handle(dcc[idx].nick,chan->name)&CHANUSER_MASTER)) {
    dprintf(idx,"You have no permission to do that on %s.\n",
	    chan->name);
    return;
  }
  chanrec=get_chanrec(u,chan->name);
  if (chanrec==NULL) {
    dprintf(idx,"User %s doesn't have a channel record for %s.\n",nick,chan->name);
    return;
  }
  putlog(LOG_CMDS,"*","#%s# -chrec %s %s",dcc[idx].nick,nick,chan->name);
  del_chanrec(u,chan->name);
  dprintf(idx,"Removed %s channel record for %s.\n",chan->name,nick);
}

void cmd_su(idx,par)
int idx; char *par;
{
  int atr=get_attr_handle(dcc[idx].nick);
  if (strlen(par)==0) {
    dprintf(idx,"Usage: su <user>\n");
    return;
  }
  if (!is_user(par)) {
    dprintf(idx,"No such user.\n");
    return;
  }
  putlog(LOG_CMDS,"*","#%s# su %s",dcc[idx].nick,par);
  if (!(atr & USER_OWNER)) {
    tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
    chanout2(dcc[idx].u.chat->channel,"%s left the party line.\n",
	     dcc[idx].nick);
    context;
    strcpy(dcc[idx].nick,par);
    dprintf(idx,"Enter password for %s\n",par);
    dcc[idx].type=DCC_CHAT_PASS;
    return;
  }
  if (atr & USER_OWNER) {
    tandout("part %s %s %d\n",botnetnick,dcc[idx].nick,dcc[idx].sock);
    chanout2(dcc[idx].u.chat->channel,"%s left the party line.\n",dcc[idx].nick);
    context;
    dprintf(idx,"Setting your username to %s.\n",par);
    strcpy(dcc[idx].nick,par);
    tandout("join %s %s %d %c%d %s\n",botnetnick,dcc[idx].nick,
            dcc[idx].u.chat->channel,geticon(idx),dcc[idx].sock,
            dcc[idx].host);
    chanout2(dcc[idx].u.chat->channel,"%s has joined the party line.\n",dcc[idx].nick);
    context;
    return;
  }
}

#ifndef NO_IRC

void cmd_chanadd (idx,par)
int idx; char *par;
{
  char chname[512],null[]="";
  nsplit(chname,par);
  if (!chname[0]) {
    dprintf(idx,"Usage: chanadd <#channel>\n");
    return;
  }
  if (findchan(chname)!=NULL) {
    dprintf(idx,"That channel already exists!\n");
    return;
  }
  tcl_channel_add(0,chname,null);
  putlog(LOG_CMDS,"*","#%s# chanadd %s",dcc[idx].nick,chname);
}

void cmd_chandel (idx,par)
int idx; char *par;
{
  char chname[512];
  struct chanset_t *chan;
  nsplit(chname,par);
  if (!chname[0]) {
    dprintf(idx,"Usage: chandel <#channel>\n");
    return;
  }
  chan=findchan(chname);
  if (chan==NULL) {
    dprintf(idx,"That channel doesnt exist!\n");
    return;
  }
  if (chan->stat & CHANSTATIC) {
    dprintf(idx,"Cannot remove %s, it is not a dynamic channel!.\n",
	    chan->name);
    return;
  }
  if (serv>=0) mprintf(serv,"PART %s\n",chan->name);
  clear_channel(chan,0);
  freeuser(chan->bans);
  killchanset(chname);
  dprintf(idx,"Channel %s removed from the bot.\n");
  dprintf(idx,"This includes any channel specific bans you set.\n");
  putlog(LOG_CMDS,"*","#%s# chandel %s",dcc[idx].nick,chname);
}

void cmd_chaninfo (idx,par)
int idx; char *par;
{
  char chname[256],work[512];
  struct chanset_t *chan;
  nsplit(chname,par);
  if (!chname[0]) {
    dprintf(idx,"Usage: chaninfo <channel>\n");
    return;
  }
  chan=findchan(chname);
  if (chan==NULL) {
    dprintf(idx,"No such channel defined.\n");
    return;
  }
  if (chan->stat & CHANSTATIC) 
    dprintf(idx,"Settings for static channel %s\n",chname);
  else dprintf(idx,"Settings for dynamic channel %s\n",chname);
  get_mode_protect(chan,work);
  dprintf(idx,  "Protect modes (chanmode): %s\n",work[0]?work:"None");
  if (chan->idle_kick) 
    dprintf(idx,"Idle Kick after (idle-kick): %d\n",chan->idle_kick);
  else dprintf(idx,"Idle Kick after (idle-kick): DONT!\n");
  /* only bot owners can see/change these (they're TCL commands) */
  if (get_attr_handle(dcc[idx].nick) & USER_OWNER) {
    if (chan->need_op[0])
      dprintf(idx,"To regain op's (need-op):\n%s\n",chan->need_op);
    if (chan->need_invite[0])
      dprintf(idx,"To get invite (need-invite):\n%s\n",chan->need_invite);
    if (chan->need_key[0])
      dprintf(idx,"To get key (need-key):\n%s\n",chan->need_key);
    if (chan->need_unban[0]) 
      dprintf(idx,"If Im banned (need-unban):\n%s\n",chan->need_unban);
    if (chan->need_limit[0])
      dprintf(idx,"When channel full (need-limit):\n%s\n",chan->need_limit);
  }
  dprintf(idx,"Other modes:\n");
  dprintf(idx,"     %cclearbans  %cenforcebans  %cdynamicbans  %cuserbans\n",
	  (chan->stat & CHAN_CLEARBANS)? '+' : '-',
	  (chan->stat & CHAN_ENFORCEBANS) ? '+' : '-',
	  (chan->stat & CHAN_DYNAMICBANS) ? '+' : '-',
	  (chan->stat & CHAN_NOUSERBANS) ? '-' : '+');
  dprintf(idx,"     %cautoop     %cbitch        %cgreet        %cprotectops\n",
	  (chan->stat & CHAN_OPONJOIN) ? '+' : '-',
	  (chan->stat & CHAN_BITCH) ? '+' : '-',
	  (chan->stat & CHAN_GREET) ? '+' : '-',
	  (chan->stat & CHAN_PROTECTOPS) ? '+' : '-');
  dprintf(idx,"     %cstatuslog  %cstopnethack  %crevenge     %csecret\n",
	  (chan->stat & CHAN_LOGSTATUS) ? '+' : '-',
	  (chan->stat & CHAN_STOPNETHACK) ? '+' : '-',
	  (chan->stat & CHAN_REVENGE) ? '+' : '-',
	  (chan->stat & CHAN_SECRET) ? '+' : '-');
  putlog(LOG_CMDS,"*","#%s# chaninfo %s",dcc[idx].nick,chname);
}

void cmd_chanset (idx,par)
int idx; char *par;
{
  char chname[512],work[512],args[512],answer[512];
  char *list[2];
  struct chanset_t *chan;
  nsplit(chname,par);
  if (!chname[0] || !par[0]) {
    dprintf(idx,"Usage: chanset <#channel> <settings>\n");
    return;
  }
  chan=findchan(chname);
  if (chan==NULL) {
    dprintf(idx,"That channel doesnt exist!\n");
    return;
  }
  nsplit(work,par);
  list[0]=work;
  list[1]=args;
  answer[0]=0;
  while (work[0]) { 
    if (work[0]=='+' || work[0]=='-' ||
	(strcmp(work,"dont-idle-kick")==0)) {
      if (tcl_channel_modify(0,chan,1,list)==TCL_OK) {
	strcat(answer,work); strcat(answer," ");
      } 
      else dprintf(idx,"Error trying to set %s for %s, invalid mode\n",
		   work,chname);
      nsplit(work,par);
      continue;
    } 
    /* the rest have an unknow amount of args, so assume the rest of the *
     * line is args. Woops nearly made a nasty little hole here :) we'll *
     * just ignore any non global +n's trying to set the need-commands   */
    if ((strncmp(work,"need-",5)!=0) ||
	(get_attr_handle(dcc[idx].nick)&USER_OWNER)) {
      strcpy(args,par);
      if (tcl_channel_modify(0,chan,2,list)==TCL_OK) {
	strcat(answer,work);
	strcat(answer," { ");
	strcat(answer,par);
	strcat(answer," }");
      } 
      else dprintf(idx,"Error trying to set %s for %s, invalid option\n",
		   work,chname);
    }
    break;
  }
  if (answer[0]) {
    dprintf(idx,"Successfully set modes { %s } on %s.\n",
	    answer,chname);
    if (chan->stat & CHANSTATIC)
      dprintf(idx,"Changes to %s are not permanent.\n",chname);
    putlog(LOG_CMDS,"*","#%s# chanset %s %s",dcc[idx].nick,chname,answer);
  }
}

void cmd_chansave (idx,par)
int idx; char *par;
{
  if (!chanfile[0])
    dprintf(idx,"No channel saving file defined.\n");
  else dprintf(idx,"Saving all dynamic channel settings.\n");
  putlog(LOG_MISC,"*","#%s# chansave",dcc[idx].nick);
  write_channels();
}

void cmd_chanload (idx,par)
int idx; char *par;
{
  if (!chanfile[0])
    dprintf(idx,"No channel saving file defined.\n");
  else dprintf(idx,"Reloading all dynamic channel settings.\n");
  putlog(LOG_MISC,"*","#%s# chanload",dcc[idx].nick);
  read_channels();
}

#endif /* !NO_IRC */

void cmd_fixcodes(idx,par)
int idx; char *par;
{
  if (dcc[idx].u.chat->status&STAT_ECHO) {
    dcc[idx].u.chat->status=STAT_TELNET;
    dprintf(idx,"Turned on telnet codes\n");
    putlog(LOG_CMDS,"*","#%s# fixcodes (telnet on)",dcc[idx].nick);
    return;
  }
  if (dcc[idx].u.chat->status&STAT_TELNET) {
    dcc[idx].u.chat->status=STAT_ECHO;
    dprintf(idx,"Turned off telnet codes\n");
    putlog(LOG_CMDS,"*","#%s# fixcodes (telnet off)",dcc[idx].nick);
    return;
  }
}

void cmd_page(idx,par)
int idx; char *par;
{
  int a;
  if (!par[0]) {
    if (dcc[idx].u.chat->status & STAT_PAGE) {
      dprintf(idx,"Currently paging outputs to %d lines.\n",
	      dcc[idx].u.chat->max_line);
    } 
    else dprintf(idx,"You dont have paging on.\n");
    return;
  }
  a=atoi(par);
  if (strcasecmp(par,"off")==0 || (a==0 && par[0]==0)) {
    dcc[idx].u.chat->status&=~STAT_PAGE;
    dcc[idx].u.chat->max_line=1; /* flush_lines needs this */
    while (dcc[idx].u.chat->buffer!=NULL)
      flush_lines(idx);
    dprintf(idx,"Paging turned off.\n");
    putlog(LOG_CMDS,"*","#%s# page off",dcc[idx].nick);
    return;
  }
  if (a > 0) {
    dprintf(idx,"Paging turned on, stopping every %d lines.\n",a);
    dcc[idx].u.chat->status |= STAT_PAGE;
    dcc[idx].u.chat->max_line=a;
    dcc[idx].u.chat->line_count=0;
    dcc[idx].u.chat->current_lines=0;
    putlog(LOG_CMDS,"*","#%s# page %d",dcc[idx].nick,a);
    return;
  } 
  dprintf(idx,"Usage: page <off or #>\n");
}
