/*
 * channels.c - part of channels.mod
 * support for channels withing the bot 
 */

#define MODULE_NAME "channels"
#define MAKING_CHANNELS
#include "../module.h"
#include "channels.h"
#include <sys/stat.h>

static int setstatic = 1;
static int use_info = 1;
static int ban_time = 60;
static char chanfile [121];
static Function * global = NULL;
static int chan_hack = 0;

#include "cmdschan.c"
#include "tclchan.c"
#include "userchan.c"

void * channel_malloc (int size, char * file, int line) {
   char * p;
#ifdef EBUG_MEM
   p =  ((void *)(global[0](size,MODULE_NAME,file,line)));
#else
   p = nmalloc(size);
#endif
   bzero(p,size);
   return p;
}

static void set_mode_protect (struct chanset_t * chan, char * set)
{
   int i, pos = 1;
   char *s, *s1;
   
   /* clear old modes */
   chan->mode_mns_prot = chan->mode_pls_prot = 0;
   chan->limit_prot = (-1);
   chan->key_prot[0] = 0;
   for (s = newsplit(&set);*s;s++) {
      i = 0;
      switch(*s) {
       case '+':
	 pos = 1;
	 break;
       case '-':
	 pos = 0;
	 break;
       case 'i':
	 i = CHANINV;
	 break;
       case 'p':
	 i = CHANPRIV;
	 break;
       case 's':
	 i = CHANSEC;
	 break;
       case 'm':
	 i = CHANMODER;
	 break;
       case 't':
	 i = CHANTOPIC;
	 break;
       case 'n':
	 i = CHANNOMSG;
	 break;
       case 'a':
	 i = CHANANON;
	 break;
       case 'q':
	 i = CHANQUIET;
	 break;
       case 'l':
	 i = CHANLIMIT;
	 chan->limit_prot = (-1);
	 if (pos) {
	    s1 = newsplit(&set);
	    if (s1[0])
	      chan->limit_prot = atoi(s1);
	 }
	 break;
       case 'k':
	 i = CHANKEY;
	 chan->key_prot[0] = 0;
	 if (!pos) {
	    s1 = newsplit(&set);
	    if (s1[0])
	      strcpy(chan->key_prot, s1);
	 }
	 break;
      }
      if (i) {
	 if (pos) {
	    chan->mode_pls_prot |= i;
	    chan->mode_mns_prot &= ~i;
	 } else {
	    chan->mode_pls_prot &= ~i;
	    chan->mode_mns_prot |= i;
	 }
      }
   }
}

static void get_mode_protect (struct chanset_t * chan, char * s)
{
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
}

/* returns true if this is one of the channel bans */
static int isbanned (struct chanset_t * chan, char * user)
{
   banlist *b;
   
   b = chan->channel.ban;
   while (b->ban[0] && strcasecmp(b->ban, user))
     b = b->next;
   if (!b->ban[0])
      return 0;
   return 1;
}

/* destroy a chanset in the list */
/* does NOT free up memory associated with channel data inside the chanset! */
static int killchanset (struct chanset_t * chan)
{
   struct chanset_t *c = chanset, *old = NULL;
   while (c) {
      if (c == chan) {
	 if (old)
	    old->next = c->next;
	 else
	    chanset = c->next;
	 nfree(c);
	 return 1;
      }
      old = c;
      c = c->next;
   }
   return 0;
}

/* bind this to chon and *if* the users console channel == ***
 * then set it to a specific channel */
static void channels_chon (char * handle, int idx) {
   struct flag_record fr = {FR_CHAN|FR_ANYWH|FR_GLOBAL,0,0,0,0,0};
   int find, found = 0;
   struct chanset_t * chan = chanset;
   
   if (!findchan(dcc[idx].u.chat->con_chan)
       && ((dcc[idx].u.chat->con_chan[0] != '*')
       || (dcc[idx].u.chat->con_chan[1] != 0))) {
      
      get_user_flagrec(dcc[idx].user,&fr,NULL);
      if (glob_op(fr))
	found = 1;
      if (chan_owner(fr))
	find = USER_OWNER;
      else if (chan_master(fr))
	find = USER_MASTER;
      else
	find = USER_OP;
      fr.match = FR_CHAN;
      while (chan && !found) {
	 get_user_flagrec(dcc[idx].user,&fr,NULL);
	 if (fr.chan & find)
	   found = 1;
	 else
	   chan = chan->next;
      }
      if (!chan)
	chan = chanset;
      if (chan)
	strcpy(dcc[idx].u.chat->con_chan, chan->name);
      else
	strcpy(dcc[idx].u.chat->con_chan, "*");
   }
}

static void write_channels()
{
   FILE *f;
   char s[121], w[1024];
   struct chanset_t *chan;

   context;
   sprintf(s, "%s~new", chanfile);
   f = fopen(s, "w");
   chmod(s, 0600);
   if (f == NULL) {
      putlog(LOG_MISC, "*", "ERROR writing channel file.");
      return;
   }
   putlog(LOG_MISC, "*", "Writing channel file ...");
   fprintf(f, "#Dynamic Channel File for %s (%s) -- written %s\n",
	   origbotname, ver, ctime(&now));
   for (chan = chanset; chan; chan = chan->next) {
      if (channel_static(chan)) {
	 fprintf(f, "channel set %s ", chan->name);
      } else {
	 fprintf(f, "channel add %s {", chan->name);
      }
      get_mode_protect(chan, w);
      if (w[0])
	fprintf(f, "chanmode \"%s\" ", w);
      if (chan->idle_kick)
	fprintf(f, "idle-kick %d ", chan->idle_kick);
      else
	fprintf(f, "dont-idle-kick ");
      if (chan->need_op[0])
	fprintf(f, "need-op {%s} ", chan->need_op);
      if (chan->need_invite[0])
	fprintf(f, "need-invite {%s} ", 
		chan->need_invite);
      if (chan->need_key[0])
	fprintf(f, "need-key {%s} ", 
		chan->need_key);
      if (chan->need_unban[0])
	fprintf(f, "need-unban {%s} ", 
		chan->need_unban);
      if (chan->need_limit[0])
	fprintf(f, "need-limit {%s} ", 
		chan->need_limit);
      if (chan->flood_pub_thr) 
	fprintf(f, "flood-chan %d:%d ", 
		chan->flood_pub_thr, chan->flood_pub_time);
      if (chan->flood_ctcp_thr) 
	fprintf(f, "flood-ctcp %d:%d ", 
		chan->flood_ctcp_thr, chan->flood_ctcp_time);
      if (chan->flood_join_thr) 
	fprintf(f, "flood-join %d:%d ", 
		chan->flood_join_thr, chan->flood_join_time);
      if (chan->flood_kick_thr) 
	fprintf(f, "flood-kick %d:%d ", 
		chan->flood_kick_thr, chan->flood_kick_time);
      if (chan->flood_deop_thr) 
	fprintf(f, "flood-deop %d:%d ", 
		chan->flood_deop_thr, chan->flood_deop_time);
      fprintf(f, "%cclearbans ", 
	      channel_clearbans(chan) ? '+' : '-');
      fprintf(f, "%cenforcebans ", 
	      channel_enforcebans(chan) ? '+' : '-');
      fprintf(f, "%cdynamicbans ", 
	      channel_dynamicbans(chan) ? '+' : '-');
      fprintf(f, "%cuserbans ", 
	      channel_nouserbans(chan) ? '-' : '+');
      fprintf(f, "%cautoop ", 
	      channel_autoop(chan) ? '+' : '-');
      fprintf(f, "%cbitch ", 
	      channel_bitch(chan) ? '+' : '-');
      fprintf(f, "%cgreet ", 
	      channel_greet(chan) ? '+' : '-');
      fprintf(f, "%cprotectops ", 
	      channel_protectops(chan) ? '+' : '-');
      fprintf(f, "%cstatuslog ", 
	      channel_logstatus(chan) ? '+' : '-');
      fprintf(f, "%cstopnethack ", 
	      channel_stopnethack(chan) ? '+' : '-');
      fprintf(f, "%crevenge ", 
	      channel_revenge(chan) ? '+' : '-');
      fprintf(f, "%cautovoice ", 
	      channel_autovoice(chan) ? '+' : '-');
      fprintf(f, "%csecret ", 
	      channel_secret(chan) ? '+' : '-');
      if (fprintf(f, "%c\n", channel_static(chan) ? ' ' : '}') == EOF) {
	 putlog(LOG_MISC, "*", "ERROR writing channel file.");
	 fclose(f);
	 return;
      }
   }
   fclose(f);
   unlink(chanfile);
#ifdef RENAME
   rename(s, chanfile);
#else
   movefile(s, chanfile);
#endif
}

static void read_channels(int create)
{
   struct chanset_t *chan, *chan2;
   if (!chanfile[0])
      return;
   for (chan = chanset; chan; chan = chan->next) 
     if (!channel_static(chan))
       chan->status |= CHAN_FLAGGED;
   chan_hack = 1; 
   if (!readtclprog(chanfile) && create) {
      FILE * f;
      /* assume file isnt there & therfore make it */
      putlog(LOG_MISC,"*","Creating channel file");
      f = fopen(chanfile,"w");
      if (!f) 
         putlog(LOG_MISC, "*", "Couldn't create channel file: %s.  Dropping",
				chanfile);
      fclose(f);
   }
   chan_hack = 0;
   chan = chanset; 
   while (chan != NULL) {
      if (chan->status & CHAN_FLAGGED) {
	 putlog(LOG_MISC, "*", "No longer supporting channel %s", chan->name);
	 dprintf(DP_SERVER, "PART %s\n", chan->name);
	 clear_channel(chan, 0);
	 while (chan->bans)
	   u_delban(chan,chan->bans->banmask,1);
	 chan2 = chan->next;
	 killchanset(chan);
	 chan = chan2;
      } else
	 chan = chan->next;
   }
}

static void channels_prerehash () {
   struct chanset_t *chan;
   
   for (chan = chanset;chan; chan = chan->next) {
      chan->status |= CHAN_FLAGGED;
      /* flag will be cleared as the channels are re-added by the config file */
      /* any still flagged afterwards will be removed */
      if (chan->status & CHAN_STATIC)
	chan->status &= ~CHAN_STATIC;
      /* flag is added to channels read from config file */
   }
   setstatic = 1;
}

static void channels_rehash () {
   struct chanset_t *chan;

   setstatic = 0;
   read_channels(1);
   /* remove any extra channels */
   
   chan = chanset;
   while (chan) {
      if (chan->status & CHAN_FLAGGED) {
	 putlog(LOG_MISC, "*", "No longer supporting channel %s", chan->name);
	 dprintf(DP_SERVER, "PART %s\n", chan->name);
	 clear_channel(chan, 0);
	 while (chan->bans)
	   u_delban(chan,chan->bans->banmask,1);
	 killchanset(chan);
	 chan = chanset;
      } else
	chan = chan->next;
   }

}

static cmd_t my_chon[] = {
     { "*", "", (Function) channels_chon, "channels:chon" },
};
   
static void channels_report (int idx, int details)
{
   struct chanset_t * chan;
   int i;
   char s[1024], s2[100];
   struct flag_record fr = { FR_CHAN|FR_GLOBAL,0,0,0,0,0};
   
   chan = chanset;
   while (chan != NULL) {
      if (idx != DP_STDOUT)
	get_user_flagrec(dcc[idx].user,&fr,chan->name);
      if ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr)) {
	    s[0] = 0;
	 if (channel_greet(chan))
	   strcat(s, "greet, ");
	 if (channel_autoop(chan))
	   strcat(s, "auto-op, ");
	 if (channel_bitch(chan))
	      strcat(s, "bitch, ");
	 if (s[0])
	   s[strlen(s) - 2] = 0;
	 if (!s[0])
	   strcpy(s, MISC_LURKING);
	 get_mode_protect(chan, s2);
	 if (channel_active(chan))
	   dprintf(idx, "   %-10s: %2d member%c, enforcing \"%s\"  (%s)\n", chan->name,
		   chan->channel.members, chan->channel.members == 1 ? ' ' : 's', s2, s);
	 else
	   dprintf(idx, "   %-10s: (inactive), enforcing \"%s\"  (%s)\n",
		   chan->name, s2, s);
	 if (details) {
	    s[0] = 0;
	    i = 0;
	    if (channel_clearbans(chan))
	      i += my_strcpy(s + i, "clear-bans ");
	    if (channel_enforcebans(chan))
	      i += my_strcpy(s + i, "enforce-bans ");
	    if (channel_dynamicbans(chan)) 
	      i += my_strcpy(s + i, "dynamic-bans ");
	    if (channel_nouserbans(chan))
	      i += my_strcpy(s + i, "forbid-user-bans ");
	    if (channel_autoop(chan))
	      i += my_strcpy(s + i, "op-on-join ");
	    if (channel_bitch(chan))
	      i += my_strcpy(s + i, "bitch ");
	    if (channel_greet(chan))
	      i += my_strcpy(s + i, "greet ");
	    if (channel_protectops(chan))
	      i += my_strcpy(s + i, "protect-ops ");
	    if (channel_logstatus(chan))
	      i += my_strcpy(s + i, "log-status ");
	    if (channel_revenge(chan))
	      i += my_strcpy(s + i, "revenge ");
	    if (channel_stopnethack(chan))
	      i += my_strcpy(s + i, "stopnethack ");
	    if (channel_secret(chan))
	      i += my_strcpy(s + i, "secret ");
	    if (channel_shared(chan))
	      i += my_strcpy(s + i, "shared ");
	    if (!channel_static(chan))
	      i += my_strcpy(s + i, "dynamic ");
	    if (channel_autovoice(chan))
	      i += my_strcpy(s + i, "autovoice ");
	    dprintf(idx, "      Options: %s\n", s);
	    if (chan->need_op[0])
	      dprintf(idx, "      To get ops I do: %s\n", chan->need_op);
	    if (chan->need_invite[0])
	      dprintf(idx, "      To get invited I do: %s\n", chan->need_invite);
	    if (chan->need_limit[0])
	      dprintf(idx, "      To get the channel limit up'd I do: %s\n", chan->need_limit);
	    if (chan->need_unban[0])
	      dprintf(idx, "      To get unbanned I do: %s\n", chan->need_unban);
	    if (chan->need_key[0])
	      dprintf(idx, "      To get the channel key I do: %s\n", chan->need_key);
	    if (chan->idle_kick)
	      dprintf(idx, "      Kicking idle users after %d min\n", chan->idle_kick);
	 }
      }
      chan = chan->next;
   }
   if (details)
     dprintf(idx, "   Bans last %d mins.\n", ban_time);
}

static int channels_expmem ()
{
   int tot = 0;
   banlist *b;
   struct chanset_t *chan = chanset;
   
   context;
   while (chan != NULL) {
      tot += sizeof(struct chanset_t);
      tot += strlen(chan->channel.key) + 1;
      if (chan->channel.topic)
	tot += strlen(chan->channel.topic) + 1;
      tot += (sizeof(struct memstruct) * (chan->channel.members + 1));
      b = chan->channel.ban;
      while (b != NULL) {
	 tot += strlen(b->ban) + 1;
	 if (b->ban[0])
	    tot += strlen(b->who) + 1;
	 tot += sizeof(struct banstruct);
	 b = b->next;
      }
      chan = chan->next;
   }
   return tot;
}

static tcl_ints my_tcl_ints [] = {
     {"share-greet", 0, 0},
     {"use-info", &use_info, 0},
     {"ban-time", &ban_time, 0},
     { 0, 0, 0 }
};

static tcl_strings my_tcl_strings [] = {
     {"chanfile", chanfile, 120, STR_PROTECT},
     {0,0,0,0}
};

static char *channels_close()
{
   context;
   write_channels();
   rem_builtins(H_chon,my_chon,1);
   rem_builtins(H_dcc,C_dcc_irc,15);
   rem_tcl_commands(channels_cmds);
   rem_tcl_strings(my_tcl_strings);
   rem_tcl_ints(my_tcl_ints);
   del_hook(HOOK_USERFILE,write_channels);
   del_hook(HOOK_REHASH,channels_rehash);
   del_hook(HOOK_PRE_REHASH,channels_prerehash);
   del_hook(HOOK_USERFILE,channels_writeuserfile);
   del_hook(HOOK_MINUTELY,check_expired_bans);
   rem_help_reference("channels.help");
   rem_help_reference("chaninfo"); 
   module_undepend(MODULE_NAME);
   return NULL;
}

char *channels_start ();

static Function channels_table[] =
{
   /* 0 - 3 */
     (Function) channels_start,
     (Function) channels_close,
     (Function) channels_expmem,
     (Function) channels_report,
     /* 4 - 7 */
     (Function) u_setsticky_ban,
     (Function) u_delban,
     (Function) u_addban,
     (Function) write_bans,
     /* 8 - 11 */
     (Function) get_chanrec,
     (Function) add_chanrec,
     (Function) del_chanrec,
     (Function) set_handle_chaninfo,
     /* 12 - 15 */
     (Function) channel_malloc,
     (Function) u_match_ban,
     (Function) u_equals_ban,
     (Function) clear_channel,
     /* 16 - 19 */
     (Function) set_handle_laston,
     (Function) &ban_time,
     (Function) &use_info,
     (Function) get_handle_chaninfo,
     /* 20 - 23 */
     (Function) u_sticky_ban,
     (Function) isbanned,
     (Function) add_chanrec_by_handle,
};

char *channels_start (Function * global_funcs)
{
   global = global_funcs;
   context;
   module_register(MODULE_NAME, channels_table, 1, 0);
   if (!module_depend(MODULE_NAME, "eggdrop", 103, 0))
     return "This module needs eggdrop1.3.0 or later";
   strcpy(chanfile,"chanfile");
   add_hook(HOOK_MINUTELY,check_expired_bans);
   add_hook(HOOK_USERFILE,write_channels);
   add_hook(HOOK_REHASH,channels_rehash);
   add_hook(HOOK_PRE_REHASH,channels_prerehash);
   add_hook(HOOK_USERFILE,channels_writeuserfile);
   add_builtins(H_chon,my_chon,1);
   add_builtins(H_dcc,C_dcc_irc,15);
   add_tcl_commands(channels_cmds);
   add_tcl_strings(my_tcl_strings);
   add_help_reference("channels.help");
   add_help_reference("chaninfo"); 
   my_tcl_ints[0].val = &share_greet;
   add_tcl_ints(my_tcl_ints);
   read_channels(0);
   return NULL;
}
