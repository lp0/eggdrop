/*
 * mode.c -- handles:
 * queueing and flushing mode changes made by the bot
 * channel mode changes and the bot's reaction to them
 * setting and getting the current wanted channel modes
 * 
 * dprintf'ized, 12dec95
 * multi-channel, 6feb96
 * stopped the bot deopping masters and bots in bitch mode, pteron 23Mar97
 *
 */
/*
 * This file is part of the eggdrop source code
 * copyright (c) 1997 Robey Pointer
 * and is distributed according to the GNU general public license.
 * For full details, read the top of 'main.c' or the file called
 * COPYING that was distributed with this code.
 */

/* reversing this mode? */
static int reversing = 0;

#define PLUS    1
#define MINUS   2
#define CHOP    4
#define BAN     8
#define VOICE   16

static struct flag_record user = {FR_GLOBAL|FR_CHAN,0,0,0,0,0};
static struct flag_record victim = {FR_GLOBAL|FR_CHAN,0,0,0,0,0};

static void flush_mode (struct chanset_t * chan, int pri) {
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
   chan->bytes = 0;
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
   /* do -b before +b to avoid server ban overlap ignores */
   for (i = 0; i < modesperline; i++)
      if ((chan->cmode[i].type & MINUS) && (chan->cmode[i].type & BAN)) {
	 if (!ok) {
	    *p++ = '-';
	    ok = 1;
	 }
	 *p++ = 'b';
	 strcat(post, chan->cmode[i].op);
	 strcat(post, " ");
	 nfree(chan->cmode[i].op);
	 chan->cmode[i].op = NULL;
      }
   ok = 0;
   for (i = 0; i < modesperline; i++)
      if (chan->cmode[i].type & PLUS) {
	 if (!ok) {
	    *p++ = '+';
	    ok = 1;
	 }
	 *p++ = ((chan->cmode[i].type & BAN) ? 'b' : 
		 ((chan->cmode[i].type & CHOP) ? 'o' : 'v'));
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
     if ((chan->cmode[i].type & MINUS) && !(chan->cmode[i].type & BAN)) {
	 if (!ok) {
	    *p++ = '-';
	    ok = 1;
	 }
	 *p++ = (chan->cmode[i].type & CHOP) ? 'o' : 'v';
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
	 dprintf(DP_MODE, "MODE %s %s\n", chan->name, out);
      else
	 dprintf(DP_SERVER, "MODE %s %s\n", chan->name, out);
   }
}

/* queue a channel mode change */
static void real_add_mode (struct chanset_t * chan, 
	       char plus, char mode, char * op) {
   int i, type, ok, l;
   char s[21];

   if (!me_op(chan))
     return; /* no point in queueing the mode */
   if ((mode == 'o') || (mode == 'b') || (mode == 'v')) {
      type = (plus == '+' ? PLUS : MINUS) | 
			(mode == 'o' ? CHOP : (mode == 'b' ? BAN : VOICE));
      /* if -b'n a non-existant ban...nuke it */
      if ((plus == '-') && (mode == 'b'))
	 if (!isbanned(chan, op))
	   return;
      /* op-type mode change */
      for (i = 0; i < modesperline; i++)
	 if ((chan->cmode[i].type == type) && (chan->cmode[i].op != NULL) &&
	     (strcasecmp(chan->cmode[i].op, op) == 0))
	    return;		/* already in there :- duplicate */
      ok = 0;			/* add mode to buffer */
      l = strlen(op) + 1;
      if ((chan->bytes + l) > mode_buf_len)
	flush_mode(chan, NORMAL);
      for (i = 0; i < modesperline; i++)
	 if ((chan->cmode[i].type == 0) && (!ok)) {
	    chan->cmode[i].type = type;
	    chan->cmode[i].op = (char *) channel_malloc(l);
	    chan->bytes += l; /* add 1 for safety */
	    strcpy(chan->cmode[i].op, op);
	    ok = 1;
	 }
      ok = 0;			/* check for full buffer */
      for (i = 0; i < modesperline; i++)
	 if (chan->cmode[i].type == 0)
	    ok = 1;
      if (!ok)
	 flush_mode(chan, NORMAL);	/* full buffer!  flush modes */
      if ((mode == 'b') && (plus == '+') && channel_enforcebans(chan))
        enforce_bans(chan);
/*	recheck_channel(chan,0); */
      return;
   }
   /* +k ? store key */
   if ((plus == '+') && (mode == 'k')) 
     strcpy(chan->key, op);
   /* -k ? store removed key */
   else if ((plus == '-') && (mode == 'k')) 
     strcpy(chan->rmkey, op);
   /* +l ? store limit */
   else if ((plus == '+') && (mode == 'l')) 
     chan->limit = atoi(op);
   else {
      /* typical mode changes */
      if (plus == '+')
	strcpy(s, chan->pls);
      else
	strcpy(s, chan->mns);
      if (!strchr(s, mode)) {
	 if (plus == '+') {
	    chan->pls[strlen(chan->pls) + 1] = 0;
	    chan->pls[strlen(chan->pls)] = mode;
	 } else {
	    chan->mns[strlen(chan->mns) + 1] = 0;
	    chan->mns[strlen(chan->mns)] = mode;
	 }
      }
   }
}


/**********************************************************************/
/* horrible code to parse mode changes */
/* no, it's not horrible, it just looks that way */

static void got_key (struct chanset_t * chan, char * nick, char * from,
		     char * key)
{
   int bogus = 0, i;
   
   set_key(chan, key);
   for (i = 0; i < strlen(key); i++)
     if (((key[i] < 32) || (key[i] == 127)) &&
	 (key[i] != 2) && (key[i] != 31) && (key[i] != 22))
       bogus = 1;
   if (bogus && match_my_nick(nick)) {
      putlog(LOG_MODES, chan->name, "%s on %s!", CHAN_BADCHANKEY, chan->name);
      dprintf(DP_MODE, "KICK %s %s :%s\n", chan->name, nick, CHAN_BADCHANKEY);
   }
   if ((reversing) || (bogus) || ((chan->mode_mns_prot & CHANKEY) &&
				  !(glob_master(user) || glob_bot(user)
				    || chan_master(user))))
     add_mode(chan, '-', 'k', key);
}

static void got_op (struct chanset_t * chan, char * nick, char * from,
	     char * who, struct flag_record * opper)
{
   memberlist *m;
   char s[UHOSTLEN];
   struct userrec * u;
   int check_chan = 0;
   
   m = ismember(chan, who);
   if (!m) {
      putlog(LOG_MISC, chan->name, CHAN_BADCHANMODE, CHAN_BADCHANMODE_ARGS);
      dprintf(DP_MODE, "WHO %s\n", who);
      return;
   }
   if (!m->user) {
      simple_sprintf(s, "%s!%s", m->nick, m->userhost);
      u = get_user_by_host(s);
   } else
     u = m->user;
   get_user_flagrec(u,&victim,chan->name);
	/* Did *I* just get opped? */
   if (!me_op(chan) && match_my_nick(who))
     check_chan = 1;
	/* I'm opped, and the opper isn't me */
   else if (me_op(chan) && !match_my_nick(who) &&
	/* and deop hasn't been sent for this one and it isn't a server op */
	    !chan_sentdeop(m) && nick[0]) {
      /* Channis is +bitch, and the opper isn't a global master or a bot */
      if (channel_bitch(chan) && !(glob_master(*opper) || glob_bot(*opper)) &&
	  /* and the *opper isn't a channel master */
	  !chan_master(*opper) &&
	  /* and the oppee isn't global op/master/bot */
	  !(glob_op(victim) || glob_bot(victim)) &&
	  /* and the oppee isn't a channel op/master */
	  !chan_op(victim)) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= SENTDEOP;
      } else 
	/* opped is channel +d or global +d */
	if ((chan_deop(victim) || (glob_deop(victim) && !chan_op(victim)))
	    && !glob_master(*opper) && !chan_master(*opper)) {
	   add_mode(chan, '-', 'o', who);
	   m->flags |= SENTDEOP;
	} else if (reversing) {
	   add_mode(chan, '-', 'o', who);
	   m->flags |= SENTDEOP;
	}
   } else if (reversing && !chan_sentdeop(m) &&
	      !match_my_nick(who)) {
      add_mode(chan, '-', 'o', who);
      m->flags |= SENTDEOP;
   }
   if (nick[0] == 0) {		/* server op! */
      int ok = 0;
	/* oppee is a global or channel master */
      if (glob_master(victim) || chan_master(victim))
	 ok = 1;
	/* oppee is not channel +d */
      else if (!chan_deop(victim)) {
	 /* oppee is not global +d and is a friend or global op */
	 if (!glob_deop(victim) &&
	     (glob_friend(victim) || glob_op(victim)
	      /* or oppee is a channel friend or a channel op */
	      || chan_friend(victim) || chan_op(victim)))
	   ok = 1;
	 /* oppee is a channel op */
      }
      /* if didn't pass the above AND opper isn't a channel op 
       * AND opper isn't being deopped AND I'm opped
       * AND channel is +stopnethack */
      if (!ok && !chan_hasop(m) && !chan_sentdeop(m) &&
	  me_op(chan) && channel_stopnethack(chan)) {
	 add_mode(chan, '-', 'o', who);
	 m->flags |= (FAKEOP | SENTDEOP);
      }
   } else
     m->flags &= ~FAKEOP;
   m->flags |= CHANOP;
   m->flags &= ~SENTOP;
   if (check_chan)
      recheck_channel(chan,1);
}

static void got_deop (struct chanset_t * chan, char * nick, char * from,
		      char * who)
{
   memberlist *m;
   char s[UHOSTLEN], s1[UHOSTLEN], s2[UHOSTLEN];
   struct userrec * u;
   
   m = ismember(chan, who);
   if (!m) {
      putlog(LOG_MISC, chan->name, CHAN_BADCHANMODE, CHAN_BADCHANMODE_ARGS);
      dprintf(DP_MODE, "WHO %s\n", who);
      return;
   }
   simple_sprintf(s, "%s!%s", m->nick, m->userhost);
   simple_sprintf(s1, "%s!%s", nick, from);
   u = get_user_by_host(s);
   get_user_flagrec(u,&victim,chan->name);
   /* deop'd someone on my oplist? */
   if (me_op(chan)) {
      int ok = 1;
      if (glob_master(victim) || chan_master(victim))
	ok = 0;
      else if ((glob_op(victim) || glob_friend(victim)) && !chan_deop(victim))
	ok = 0;
      else if (chan_op(victim) || chan_friend(victim))
	ok = 0;
      if (!ok && !match_my_nick(nick) &&
	  strcasecmp(who, nick) && chan_hasop(m) &&
	  !match_my_nick(who)) {	/* added 25mar96, robey */
	 /* reop? */
	 /* let's break it down home boy...*/
	 /* is the deopper NOT a master or bot? */
	 if (!glob_master(user) && !glob_bot(user) &&
	     /* is the channel protectops? */
	     channel_protectops(chan) &&
	     /* provided it's not +bitch ... */
	     (!channel_bitch(chan) || 
	      /* or the users a valid op */
	      chan_op(victim) || (glob_op(victim) && !chan_deop(victim))) &&
	     /* and provied the users not a de-op*/
	     !(chan_deop(victim) || (glob_deop(victim) && !chan_op(victim))) &&
	     /* and we havent sent it already */
	     !chan_sentdeop(m)) {
	    /* then we'll bless them */
	    add_mode(chan, '+', 'o', who);
	    m->flags |= SENTOP;
	 } else if (reversing) {
	    add_mode(chan, '+', 'o', who);
	    m->flags |= SENTOP;
	 }
	 /* if the perpetrator is not a friend or bot, then do the
	  * vengeful thing */
	 if (!glob_bot(user) && !glob_friend(user) && !chan_friend(user)
	     && channel_revenge(chan)) {
	    if (nick[0]) {
	       simple_sprintf(s2, "deopped %s", s);
	       take_revenge(chan, s1, s2);	/* punish bad guy */
	    }
	 }
      }
   }
   if (!nick[0])
     putlog(LOG_MODES, chan->name, "TS resync (%s): %s deopped by %s",
	    chan->name, who, from);
   /* check for mass deop */
   if (nick[0]) 
     detect_chan_flood(nick, from, s1, chan, FLOOD_DEOP, who);
   /* having op hides your +v status -- so now that someone's lost ops,
      check to see if they have +v */
   if (!(m->flags & (CHANVOICE|SENTVOICE))) {
      dprintf(DP_MODE, "WHO %s\n", m->nick);
      m->flags |= SENTVOICE;
   }
   /* was the bot deopped? */
   if (match_my_nick(who)) {
      /* cancel any pending kicks.  Ernst 18/3/98 */
      memberlist *m = chan->channel.member;
      while (m->nick[0]) {
	 if (chan_sentkick(m))
	    m->flags &= ~SENTKICK;
	 m = m->next;
      }
      if (chan->need_op[0])
	 do_tcl("need-op", chan->need_op);
      if (!nick[0])
	 putlog(LOG_MODES, chan->name, "TS resync deopped me on %s :(",
	       chan->name);
      /* take revenge */
      if (channel_revenge(chan))
	 if (!glob_friend(user) && !chan_friend(user) && nick[0] &&
	    !match_my_nick(nick)) {
	    simple_sprintf(s2, "deopped %s", botname);
	    take_revenge(chan, s1, s2);
	 }
   }
   m->flags &= ~(FAKEOP | CHANOP | SENTDEOP);
}


static void got_ban (struct chanset_t * chan, char * nick, char * from,
		     char * who)
{
   char me[UHOSTLEN], s[UHOSTLEN], s1[UHOSTLEN];
   int check, i, bogus;
   memberlist *m;
   struct userrec * u;
   
   simple_sprintf(me, "%s!%s", botname, botuserhost);
   simple_sprintf(s, "%s!%s", nick, from);
   newban(chan, who, s);
   bogus = 0;
   check = 1;
   if (!match_my_nick(nick)) {	/* it's not my ban */
      if (channel_nouserbans(chan) && !glob_bot(user) &&
	  !glob_master(user) && !chan_master(user)) {
	 /* no bans made by users */
	 add_mode(chan, '-', 'b', who);
	 return;
      }
      for (i = 0; who[i]; i++)
	 if (((who[i] < 32) || (who[i] == 127)) &&
	     (who[i] != 2) && (who[i] != 22) && (who[i] != 31))
	    bogus = 1;
      if (bogus) {
	 if (glob_bot(user) || glob_friend(user) || chan_friend(user)) {
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
	    m = ismember(chan,nick);
	    if (!m || !chan_sentkick(m)) {
	       if (m)
		 m->flags |= SENTKICK;
	       dprintf(DP_MODE, "KICK %s %s :bogus ban\n", chan->name, nick);
	    }
	 }
	 return;
      }
      /* don't enforce a server ban right away -- give channel users a chance */
      /* to remove it, in case it's fake */
      if (!nick[0])
	 check = 0;
      /* does this remotely match against any of our hostmasks? */
      /* just an off-chance... */
      u = get_user_by_host(who);
      if (u) {
	 get_user_flagrec(u,&victim,chan->name);
	 if (glob_friend(victim) || (glob_op(victim) && !chan_deop(victim)) ||
	     chan_friend(victim) || chan_op(victim)) {
	    if (!glob_master(user) && !glob_bot(user) && !chan_master(user)) {
	       reversing = 1;
	       check = 0;
	    }
	    if (glob_master(victim) || chan_master(victim))
	      check = 0;
	 } else if (wild_match(who, me) && me_op(chan)) {
	    /* ^ don't really feel like being banned today, thank you! */
	    reversing = 1;
	    check = 0;
	 }
      } else {
	 /* banning an oplisted person who's on the channel? */
	 m = chan->channel.member;
	 while (m->nick[0]) {
	    sprintf(s1, "%s!%s", m->nick, m->userhost);
	    if (wild_match(who, s1)) {
	       u = get_user_by_host(s1);
	       if (u) {
		  get_user_flagrec(u,&victim,chan->name);
		  if (glob_friend(victim) || 
		      (glob_op(victim) && !chan_deop(victim))
		      || chan_friend(victim) || chan_op(victim))  {
		     /* remove ban on +o/f/m user, unless placed by another +m/b */
		     if (!glob_master(user) && !glob_bot(user) &&
			 !chan_master(user)) {
			add_mode(chan, '-', 'b', who);
			check = 0;
		     }
		     if (glob_master(victim) || chan_master(victim)) 
		       check = 0;
		  }
	       }
	    }
	    m = m->next;
	 }
      }
   }
   if (check && channel_enforcebans(chan))
      kick_all(chan, who, IRC_BANNED);
   /* is it a server ban from nowhere? */
   if (reversing || 
       (bounce_bans && (!nick[0]) && 
	(!u_equals_ban(global_bans,who) || !u_equals_ban(chan->bans,who))
	&& (check)))
     add_mode(chan, '-', 'b', who);
}

static void got_unban (struct chanset_t * chan, char * nick, char * from,
		       char * who, struct userrec * u)
{
   int i, bogus;
   banlist *b, *old;
   
   b = chan->channel.ban;
   old = NULL;
   while (b->ban[0] && strcasecmp(b->ban, who)) {
      old = b;
      b = b->next;
   }
   if (b->ban[0]) {
      if (old)
	old->next = b->next;
      else
	chan->channel.ban = b->next;
      nfree(b->ban);
      nfree(b->who);
      nfree(b);
   }
   bogus = 0;
   for (i = 0; i < strlen(who); i++)
     if (((who[i] < 32) || (who[i] == 127)) &&
	 (who[i] != 2) && (who[i] != 22) && (who[i] != 31))
       bogus = 1;
   /* it's bogus, not by me, and in fact didn't exist anyway.. */
   if (bogus && !match_my_nick(nick) && !isbanned(chan, who) &&
       /* not by valid +f/+b/+o */
       !(glob_friend(user) || glob_bot(user) || 
	 (glob_op(user) && !chan_deop(user))) &&
       !(chan_friend(user) || chan_op(user))) {
      /* then lets kick the weenie */
      memberlist *m = ismember(chan,nick);
      if (!m || !chan_sentkick(m)) {
	 if (m) 
	   m->flags |= SENTKICK;
	 dprintf(DP_MODE, "KICK %s %s :%s\n", chan->name, nick, CHAN_BADBAN);
      }
      return;
   } 
   if (u_sticky_ban(chan->bans, who) || u_sticky_ban(global_bans,who)) {
      /* that's a sticky ban! No point in being */
      /* sticky unless we enforce it!! */
      add_mode(chan, '+', 'b', who);
   }
   if ((u_equals_ban(global_bans,who) || u_equals_ban(chan->bans, who)) &&
       me_op(chan) && !channel_dynamicbans(chan)) {
      /* that's a permban! */
      if (glob_bot(user) && (bot_flags(u) & BOT_SHARE)) {
	 /* sharebot -- do nothing */
      } else if ((glob_op(user) && !chan_deop(user)) || chan_op(user)) {
	 dprintf(DP_SERVER, "NOTICE %s :%s %s", nick, who, CHAN_PERMBANNED);
      } else
	 add_mode(chan, '+', 'b', who);
   }
}

/* a pain in the ass: mode changes */
static void gotmode (char * from, char * msg)
{
   char * nick, *ch, *op, *chg;
   char s[UHOSTLEN], ms[UHOSTLEN];
   char ms2[3];
   struct userrec * u;
   memberlist *m;
   struct chanset_t *chan;
   
   /* usermode changes? */
   if ((msg[0] == '#') || (msg[0] == '&')) {
      ch = newsplit(&msg);
      chg = newsplit(&msg);
      reversing = 0;
      chan = findchan(ch);
      if (!chan) {
	 putlog(LOG_MISC, "*", CHAN_FORCEJOIN, ch);
	 dprintf(DP_SERVER, "PART %s\n", ch);
      } else if (!channel_pending(chan)) {
	 putlog(LOG_MODES, chan->name, "%s: mode change '%s %s' by %s", 
		ch, chg, msg, from);
	 u = get_user_by_host(from);
	 get_user_flagrec(u,&user,ch);
	 nick = splitnick(&from);
	 m = ismember(chan, nick);
	 if (m && me_op(chan)) {
	    if (chan_fakeop(m)) {
	       putlog(LOG_MODES, ch, CHAN_FAKEMODE, ch);
	       dprintf(DP_MODE, "KICK %s %s :%s\n", ch, nick, CHAN_FAKEMODE_KICK);
	       reversing = 1;
	    } else if (!chan_hasop(m)) {
	       putlog(LOG_MODES, ch, CHAN_DESYNCMODE, ch);
	       dprintf(DP_MODE, "KICK %s %s :%s\n", ch, nick, CHAN_DESYNCMODE_KICK);
	       reversing = 1;
	    }
	 }
	 ms2[0] = '+';
	 ms2[2] = 0;
	 while (*chg) {
	    int todo =0;
	    
	    switch (*chg) {
	     case '+':
	       ms2[0] = '+';
	       break;
	     case '-':
	       ms2[0] = '-';
	       break;
	     case 'i':
	       todo = CHANINV;
	       break;
	     case 'p':
	       todo = CHANPRIV;
	       break;
	     case 's':
	       todo = CHANSEC;
	       break;
	     case 'm':
	       todo = CHANMODER;
	       break;
	     case 't':
	       todo = CHANTOPIC;
	       break;
	     case 'n':
	       todo = CHANNOMSG;
	       break;
	     case 'a':
	       todo = CHANANON;
	       break;
	     case 'q':
	       todo = CHANQUIET;
	       break;
	     case 'l':
	       if (ms2[0] == '-') {
		  check_tcl_mode(nick, from, u, chan->name, "-l");
		  if ((reversing) && (chan->channel.maxmembers != (-1))) {
		     simple_sprintf(s, "%d", chan->channel.maxmembers);
		     add_mode(chan, '+', 'l', s);
		  } else if ((chan->limit_prot != (-1)) && !glob_master(user)
			     && !chan_master(user)) {
		     simple_sprintf(s, "%d", chan->limit_prot);
		     add_mode(chan, '+', 'l', s);
		  }
		  chan->channel.maxmembers = (-1);
	       } else {
		  op = newsplit(&msg);
		  fixcolon(op);
		  chan->channel.maxmembers = atoi(op);
		  simple_sprintf(ms, "+l %d", chan->channel.maxmembers);
		  check_tcl_mode(nick, from, u, chan->name, ms);
		  if ((reversing) ||
		      ((chan->mode_mns_prot & CHANLIMIT) && !glob_master(user)
		       && !chan_master(user))) {
		     if (chan->channel.maxmembers == 0)
		       add_mode(chan, '+', 'l', "23"); /* wtf? 23 ??? */
		     add_mode(chan, '-', 'l', "");
		  }
		  if ((chan->limit_prot != chan->channel.maxmembers) &&
		      (chan->mode_pls_prot & CHANLIMIT) &&
		      !glob_master(user) && !chan_master(user)) {
		     simple_sprintf(s, "%d", chan->limit_prot);
		     add_mode(chan, '+', 'l', s);
		  }
	       }
	       break;
	      case 'k':
	       op = newsplit(&msg);
	       fixcolon(op);
	       simple_sprintf(ms, "%ck %s", ms2[0], op);
	       check_tcl_mode(nick, from, u, chan->name, ms);
	       if (ms2[0] == '+')
		 got_key(chan, nick, from, op);
	       else {
		  if ((reversing) && (chan->channel.key[0]))
		    add_mode(chan, '+', 'k', chan->channel.key);
		  else if ((chan->key_prot[0]) && !glob_master(user)
			   && !chan_master(user))
		    add_mode(chan, '+', 'k', chan->key_prot);
		  set_key(chan, NULL);
	       }
	       break;
	      case 'o':
	       op = newsplit(&msg);
	       fixcolon(op);
	       simple_sprintf(ms, "%co %s", ms2[0], op);
	       check_tcl_mode(nick, from, u, chan->name, ms);
	       if (ms2[0] == '+')
		 got_op(chan, nick, from, op, &user);
	       else
		 got_deop(chan, nick, from, op);
	       break;
	      case 'v':
	       op = newsplit(&msg);
	       fixcolon(op);
	       m = ismember(chan, op);
	       if (!m) {
		  putlog(LOG_MISC, chan->name, 
			 CHAN_BADCHANMODE, CHAN_BADCHANMODE_ARGS2);
		  dprintf(DP_MODE, "WHO %s\n", op);
	       } else {
		  simple_sprintf(ms, "%cv %s", ms2[0], op);
		  check_tcl_mode(nick, from, u, chan->name, ms);
		  get_user_flagrec(m->user,&victim,chan->name);
		  if (ms2[0] == '+') {
		     m->flags &= ~SENTVOICE;
		     m->flags |= CHANVOICE;
		     if (!glob_master(user) && !chan_master(user)) {
			if (channel_autovoice(chan) && 
			    (chan_quiet(victim)
			     || (glob_quiet(victim) && !chan_voice(victim))))
			  add_mode(chan, '-', 'v', op);
			else if (reversing)
			  add_mode(chan, '-', 'v', op);
		     }
		  } else {
		     m->flags &= ~SENTDEVOICE;
		     m->flags &= ~CHANVOICE;
		     if (!glob_master(user) && !chan_master(user)) {
			if (channel_autovoice(chan) && 
			    (chan_voice(victim) || 
			     (glob_voice(victim) && !chan_quiet(victim))))
			  add_mode(chan, '+', 'v', op);
			else if (reversing)
			  add_mode(chan, '+', 'v', op);
		     }
		  }
	       }
	       break;
	      case 'b':
	       op = newsplit(&msg);
	       fixcolon(op);
	       simple_sprintf(ms, "%cb %s", ms2[0], op);
	       check_tcl_mode(nick, from, u, chan->name, ms);
	       if (ms2[0] == '+')
		 got_ban(chan, nick, from, op);
	       else
		 got_unban(chan, nick, from, op,u);
	       break;
	    }
	    if (todo) {
	       ms2[1] = *chg; 
	       check_tcl_mode(nick,from,u,chan->name,ms2);
	       if (ms2[0] == '+')
		 chan->channel.mode |= todo;
	       else
		 chan->channel.mode &= ~todo;
	       if ((((ms2[0] == '+') && (chan->mode_mns_prot & todo)) ||
		    ((ms2[0] == '-') && (chan->mode_pls_prot & todo)))
		   && !glob_master(user) && !chan_master(user))
		 add_mode(chan, ms2[0] == '+' ? '-' : '+', *chg, "");
	       else if (reversing && 
			((ms2[0] == '+') || (chan->mode_pls_prot & todo))
			&& ((ms2[0] == '-') || (chan->mode_mns_prot & todo)))
		 add_mode(chan, ms2[0] == '+' ? '-' : '+', *chg, "");
	    }
	    chg++;
	 }
      }  
   }
}
