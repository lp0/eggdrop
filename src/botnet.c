/* 
   botnet.c -- handles:
   keeping track of which bot's connected where in the chain
   dumping a list of bots or a bot tree to a user
   channel name associations on the party line
   rejecting a bot
   linking, unlinking, and relaying to another bot
   pinging the bots periodically and checking leaf status

   dprintf'ized, 28nov95
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "main.h"
#include "tandem.h"

extern int serv;
extern int dcc_total;
extern struct dcc_t * dcc;
extern char botname[];
extern int connect_timeout;
extern int max_dcc;
extern time_t now;
extern int egg_numver;

/* keep track of tandem bots on the botnet */
tand_t *tandbot;
/* maximum space for tandem bots currently */
static int maxtands = 50;
/* keep track of people on the botnet */
party_t *party;
/* maximum space for party line members currently */
static int maxparty = 50;
/* number of bots on the botnet */
int tands = 0;
/* number of people on the botnet */
int parties = 0;
/* botnet nickname */
char botnetnick[10] = "";

extern void (*kill_all_assoc) ();
extern void (*dump_bot_assoc) (int);

int expmem_botnet()
{
   int size = 0;
   context;
   size += (maxtands * sizeof(tand_t));
   size += (maxparty * sizeof(party_t));
   return size;
}

void init_bots()
{
   /* grab space for 50 bots for now -- expand later as needed */
   maxtands = 50;
   tandbot = (tand_t *) nmalloc(maxtands * sizeof(tand_t));
   maxparty = 50;
   party = (party_t *) nmalloc(maxparty * sizeof(party_t));
}

/* add a tandem bot to our chain list */
void addbot (char * who, char * from, char * next,char * extra)
{
   if (tands == maxtands) {
      /* expand tandem bot space */
      maxtands += 50;
      tandbot = (tand_t *) nrealloc((void *) tandbot, maxtands * sizeof(tand_t));
   }
   strcpy(tandbot[tands].bot, who);
   strcpy(tandbot[tands].via, from);
   strcpy(tandbot[tands].next, next);
   tandbot[tands].share = extra[0] == '+' ? 2 : extra[0] == '-' ? 1 : 0;
   if (extra[0])
     tandbot[tands].ver = atoi(extra+1);
   else
     tandbot[tands].ver = 0;
   tands++;
}

void updatebot (char * who, char share, char * extra) {
   int i;
   context;
   for (i = 0; i < tands; i++)
      if (strcasecmp(tandbot[i].bot, who) == 0) {
	 if (share) 
	   tandbot[i].share = share == '+' ? 2 : share == '-' ? 1 : 0;
	 if (extra)
	   tandbot[i].ver = atoi(extra);
	 tandout_but(nextbot(who),"update %s %c%d\n",
		     who, tandbot[i].share == 2 ? '+' :
		     tandbot[i].share == 1 ? '-' : '?',
		     tandbot[i].ver);
      }
}

/* for backward 1.0 compatibility: */
/* grab the (first) sock# for a user on another bot */
int partysock (char * bot, char * nick)
{
   int i;
   for (i = 0; i < parties; i++) {
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (strcasecmp(party[i].nick, nick) == 0))
	 return party[i].sock;
   }
   return 0;
}

/* new botnet member */
void addparty (char * bot, char * nick, int chan, char flag, int sock,
		     char * from)
{
   int i;
   context;
   for (i = 0; i < parties; i++) {
      /* just changing the channel of someone already on? */
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (party[i].sock == sock)) {
	 party[i].chan = chan;
	 party[i].timer = time(NULL);
	 if (from[0]) {
	    if (flag == ' ')
	       flag = '-';
	    party[i].flag = flag;
	    strcpy(party[i].from, from);
	 }
	 return;
      }
   }
   /* new member */
   if (parties == maxparty) {
      maxparty += 50;
      party = (party_t *) nrealloc((void *) party, maxparty * sizeof(party_t));
   }
   strcpy(party[parties].nick, nick);
   strcpy(party[parties].bot, bot);
   party[parties].chan = chan;
   party[parties].sock = sock;
   party[parties].status = 0;
   party[parties].away[0] = 0;
   party[parties].timer = time(NULL);	/* cope. */
   if (from[0]) {
      if (flag == ' ')
	 flag = '-';
      party[parties].flag = flag;
      strcpy(party[parties].from, from);
   } else {
      party[parties].flag = ' ';
      strcpy(party[parties].from, "(unknown)");
   }
   parties++;
}

/* alter status flags for remote party-line user */
void partystat (char * bot, int sock, int add, int rem)
{
   int i;
   for (i = 0; i < parties; i++) {
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (party[i].sock == sock)) {
	 party[i].status |= add;
	 party[i].status &= ~rem;
      }
   }
}

/* other bot is sharing idle info */
void partysetidle (char * bot, int sock, int secs)
{
   int i;
   for (i = 0; i < parties; i++) {
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (party[i].sock == sock)) {
	 party[i].timer = (time(NULL) - (time_t) secs);
      }
   }
}

/* un-idle someone */
void partyidle (char * bot, char * nick)
{
   int i;
   for (i = 0; i < parties; i++) {
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (strcasecmp(party[i].nick, nick) == 0)) {
	 party[i].timer = time(NULL);
      }
   }
}

/* set away message */
void partyaway (char * bot, int sock, char * msg)
{
   int i;
   for (i = 0; i < parties; i++) {
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (party[i].sock == sock)) {
	 strncpy(party[i].away, msg, 60);
	 party[i].away[60] = 0;
	 if (!msg[0])
	    strcpy(party[i].away, "(unknown)");
      }
   }
}

/* remove a tandem bot from the chain list */
void rembot (char * who, char * from)
{
   int i,j;
   context;
   for (i = 0; i < tands; i++)
      if ((strcasecmp(tandbot[i].bot, who) == 0) &&
	  (strcasecmp(tandbot[i].via, from) == 0)) {
	 tands--;
	 for (j=i;j < tands;j++) {
	    strcpy(tandbot[j].bot, tandbot[j+1].bot);
	    strcpy(tandbot[j].via, tandbot[j+1].via);
	    strcpy(tandbot[j].next, tandbot[j+1].next);
	    tandbot[j].ver = tandbot[j+1].ver;
	    tandbot[j].share = tandbot[j+1].share;
	 }
	 check_tcl_disc(who);
      }
}

void remparty (char * bot, int sock)
{
   int i;
   context;
   for (i = 0; i < parties; i++)
      if ((strcasecmp(party[i].bot, bot) == 0) &&
	  (party[i].sock == sock)) {
	 parties--;
	 if (i < parties) {
	    strcpy(party[i].bot, party[parties].bot);
	    strcpy(party[i].nick, party[parties].nick);
	    party[i].chan = party[parties].chan;
	    party[i].sock = party[parties].sock;
	    party[i].flag = party[parties].flag;
	    party[i].status = party[parties].status;
	    party[i].timer = party[parties].timer;
	    strcpy(party[i].from, party[parties].from);
	    strcpy(party[i].away, party[parties].away);
	 }
      }
}

/* cancel every user that was on a certain bot */
void rempartybot (char * bot)
{
   int i;
   for (i = 0; i < parties; i++)
      if (strcasecmp(party[i].bot, bot) == 0) {
	 remparty(bot, party[i].sock);
	 i--;
      }
}

/* remove every bot linked 'via' bot <x> */
void unvia (int idx, char * who)
{
   int i;
   rembot(who, who);
   rempartybot(who);
   for (i = 0; i < tands; i++)
      if (strcasecmp(tandbot[i].via, who) == 0) {
	 tandout_but(idx, "unlinked %s\n", tandbot[i].bot);
	 rempartybot(tandbot[i].bot);
	 rembot(tandbot[i].bot, who);
	 i--;
      }
}

/* return index into dcc list of the bot that connects us to bot <x> */
int nextbot (char * who)
{
   int i, j;
   for (i = 0; i < tands; i++)
      if (strcasecmp(who, tandbot[i].bot) == 0) {
	 for (j = 0; j < dcc_total; j++)
	    if ((strcasecmp(tandbot[i].via, dcc[j].nick) == 0) 
		&& (dcc[j].type == & DCC_BOT))
	       return j;
	 return -1;		/* we're not connected to 'via' */
      }
   return -1;			/* no such bot in the chain */
}

/* return name of the bot that is directly connected to bot X */
char *lastbot (char * who)
{
   int i;
   for (i = 0; i < tands; i++)
      if (strcasecmp(who, tandbot[i].bot) == 0)
	 return tandbot[i].next;
   return "*";
}

/* modern version of 'whom' (use local data) */
void answer_local_whom (int idx, int chan)
{
   char *s, c, idle[20];
   int i;
   context;
   if (chan == (-1))
      dprintf(idx, "Users across the botnet (+: party line, *: local channel)\n");
   else if (chan > 0) {
      s = get_assoc_name(chan);
      if (s == NULL)
	 dprintf(idx, "Users on channel %s%d:\n",
		 (chan < 100000) ? "" : "*", chan % 100000);
      else
	 dprintf(idx, "Users on channel '%s%s' (%s%d):\n",
		 (chan < 100000) ? "" : "*", s, (chan < 100000) ? "" : "*", chan % 100000);
   }
   dprintf(idx, "%-10s   %-9s  Host\n", "Nick", "Bot");
   dprintf(idx, "----------   ---------  ------------------------------\n");
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_CHAT) {
	 if ((chan == (-1)) || ((chan >= 0) && (dcc[i].u.chat->channel == chan))) {
	    c = geticon(i);
	    if (c == '-')
	       c = ' ';
	    if (now - dcc[i].timeval > 300) {
	       unsigned long days, hrs, mins;
	       days = (now - dcc[i].timeval) / 86400;
	       hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	       mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	       if (days > 0)
		  sprintf(idle, " [idle %lud%luh]", days, hrs);
	       else if (hrs > 0)
		  sprintf(idle, " [idle %luh%lum]", hrs, mins);
	       else
		  sprintf(idle, " [idle %lum]", mins);
	    } else
	       idle[0] = 0;
	    dprintf(idx, "%c%-9s %c %-9s  %s%s\n", c, dcc[i].nick,
		  (dcc[i].u.chat->channel == 0) && (chan == (-1)) ? '+' :
		    (dcc[i].u.chat->channel > 100000) && (chan == (-1)) ? '*' : ' ',
		    botnetnick, dcc[i].host, idle);
	    if (dcc[i].u.chat->away != NULL)
	       dprintf(idx, "   AWAY: %s\n", dcc[i].u.chat->away);
	 }
      }
   for (i = 0; i < parties; i++) {
      if ((chan == (-1)) || ((chan >= 0) && (party[i].chan == chan))) {
	 c = party[i].flag;
	 if (c == '-')
	    c = ' ';
	 if (party[i].timer == 0L)
	    strcpy(idle, " [idle?]");
	 else if (now - party[i].timer > 300) {
	    unsigned long days, hrs, mins;
	    days = (now - party[i].timer) / 86400;
	    hrs = ((now - party[i].timer) - (days * 86400)) / 3600;
	    mins = ((now - party[i].timer) - (hrs * 3600)) / 60;
	    if (days > 0)
	       sprintf(idle, " [idle %lud%luh]", days, hrs);
	    else if (hrs > 0)
	       sprintf(idle, " [idle %luh%lum]", hrs, mins);
	    else
	       sprintf(idle, " [idle %lum]", mins);
	 } else
	    idle[0] = 0;
	 dprintf(idx, "%c%-9s %c %-9s  %s%s\n", c, party[i].nick,
	 (party[i].chan == 0) && (chan == (-1)) ? '+' : ' ', party[i].bot,
		 party[i].from, idle);
	 if (party[i].status & PLSTAT_AWAY)
	    dprintf(idx, "   AWAY: %s\n", party[i].away);
      }
   }
}

/* show z a list of all bots connected */
void tell_bots (int idx)
{
   char s[512];
   int i;
   if (!tands) {
      dprintf(idx, "No bots linked.\n");
      return;
   }
   strcpy(s, botnetnick);
   strcat(s, ", ");
   for (i = 0; i < tands; i++) {
      strcat(s, tandbot[i].bot);
      strcat(s, ", ");
      if (strlen(s) > 480) {
	 s[strlen(s) - 2] = 0;
	 dprintf(idx, "Bots: %s\n", s);
	 s[0] = 0;
      }
   }
   if (s[0]) {
      s[strlen(s) - 2] = 0;
      dprintf(idx, "Bots: %s\n", s);
   }
   dprintf(idx, "(total: %d)\n", tands + 1);
}

/* show a simpleton bot tree */
void tell_bottree (int idx, int showver)
{
   int i;
   char s[161], last[20][10], this[161], *x;
   int lev = 0, more, mark[20], ok, cnt;
   char work[1024];
   int tothops = 0;
   strcpy(this, botnetnick);
   more = 1;
   if (tands == 0) {
      dprintf(idx, "No bots linked.\n");
      return;
   }
   s[0] = 0;
   for (i = 0; i < tands; i++)
      if (!tandbot[i].next[0])
	 sprintf(&s[strlen(s)], "%s, ", tandbot[i].bot);
   if (s[0]) {
      s[strlen(s) - 2] = 0;
      dprintf(idx, "(No trace info for: %s)\n", s);
   }
   if (showver)
     dprintf(idx, "%s (%d.%d.%d)\n", this,
	     egg_numver/1000000,
	     egg_numver%1000000/10000,
	     egg_numver%10000/100);
   else 
     dprintf(idx, "%s\n", this);
   work[0] = 0;
   while (more) {
      if (lev == 20) {
	 dprintf(idx, "\nTree too complex!\n");
	 return;
      }
      cnt = 0;
      tothops += lev;
      for (i = 0; i < tands; i++)
	 if (strcasecmp(tandbot[i].next, this) == 0)
	    cnt++;
      if (cnt) {
	 for (i = 0; i < lev; i++) {
	    if (mark[i])
	       strcat(work, "  |  ");
	    else
	       strcat(work, "     ");
	 }
	 if (cnt > 1)
	    strcat(work, "  |-");
	 else
	    strcat(work, "  `-");
	 s[0] = 0;
	 i = 0;
	 while (!s[0]) {
	    if (strcasecmp(tandbot[i].next, this) == 0) {
	      if (tandbot[i].ver) {
		 sprintf(s, "%c%s", 
			 tandbot[i].share == 2 ? '+' :
			 tandbot[i].share == 1 ? '-' : '?',
			 tandbot[i].bot);
		 if (showver)
		   sprintf(s+strlen(s)," (%d.%d.%d)",
			 tandbot[i].ver/1000000,
			 tandbot[i].ver%1000000/10000,
			 tandbot[i].ver%10000/100);
	      } else {
		 sprintf(s, "-%s",tandbot[i].bot);
	      }
	    } else
	       i++;
	 }
	 dprintf(idx, "%s%s\n", work, s);
	 if (cnt > 1)
	    mark[lev] = 1;
	 else
	    mark[lev] = 0;
	 work[0] = 0;
	 strcpy(last[lev], this);
	 if ((x = strchr(s+1,' ')))
	   *x = 0;
	 strcpy(this, s+1);
	 lev++;
	 more = 1;
      } else {
	 while (cnt == 0) {
	    /* no subtrees from here */
	    if (lev == 0) {
	       dprintf(idx, "(( tree error ))\n");
	       return;
	    }
	    ok = 0;
	    for (i = 0; i < tands; i++) {
	       if (strcasecmp(tandbot[i].next, last[lev - 1]) == 0) {
		  if (strcasecmp(tandbot[i].bot, this) == 0)
		     ok = 1;
		  else if (ok) {
		     cnt++;
		     if (cnt == 1) {
		       if (tandbot[i].ver) {
			  sprintf(s, "%c%s",
				  tandbot[i].share == 2 ? '+' :
				  tandbot[i].share == 1 ? '-' : '?',
				  tandbot[i].bot);
			  if (showver)
			    sprintf(s+strlen(s)," (%d.%d.%d)",
				    tandbot[i].ver/1000000,
				    tandbot[i].ver%1000000/10000,
				    tandbot[i].ver%10000/100);
		       } else {
			  sprintf(s,"-%s",tandbot[i].bot);
		       }
		     }
		  }
	       }
	    }
	    if (cnt) {
	       for (i = 1; i < lev; i++) {
		  if (mark[i - 1])
		     strcat(work, "  |  ");
		  else
		     strcat(work, "     ");
	       }
	       more = 1;
	       if (cnt > 1)
		  dprintf(idx, "%s  |-%s\n", work, s);
	       else
		  dprintf(idx, "%s  `-%s\n", work, s);
	       if ((x = strchr(s+1,' ')))
		 *x = 0;
	       strcpy(this, s+1);
	       work[0] = 0;
	       if (cnt > 1)
		  mark[lev - 1] = 1;
	       else
		  mark[lev - 1] = 0;
	    } else {
	       /* this was the last child */
	       lev--;
	       if (lev == 0) {
		  more = 0;
		  cnt = 999;
	       } else {
		  more = 1;
		  strcpy(this, last[lev]);
	       }
	    }
	 }
      }
   }
   /* hop information: (9d) */
   dprintf(idx, "Average hops: %3.1f, total bots: %d\n",
	   ((float) tothops) / ((float) tands), tands + 1);
}

/* dump list of links to a new bot */
void dump_links (int z)
{
   int i;
   for (i = 0; i < tands; i++) {
      dprintf(z, "nlinked %s %s %c%d\n", tandbot[i].bot, tandbot[i].next,
	      tandbot[i].share == 2 ? '+' : tandbot[i].share == 1 ? '-' : '?',
	      tandbot[i].ver);
   }
   /* dump party line members */
   for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type == &DCC_CHAT) {
	 if (dcc[i].u.chat->channel < 100000) {
	    dprintf(z, "join %s %s %d %c%d %s\n", botnetnick, dcc[i].nick,
	    dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	    if (dcc[i].u.chat->away != NULL)
	       dprintf(z, "away %s %d %s\n", botnetnick, dcc[i].sock,
		       dcc[i].u.chat->away);
	    dprintf(z, "idle %s %d %lu\n", botnetnick, dcc[i].sock,
		    time(NULL) - dcc[i].timeval);
	 }
      }
   }
   for (i = 0; i < parties; i++) {
      dprintf(z, "join %s %s %d %c%d %s\n", party[i].bot, party[i].nick,
	      party[i].chan, party[i].flag, party[i].sock, party[i].from);
      if (party[i].status & PLSTAT_AWAY)
	 dprintf(z, "away %s %d %s\n", party[i].bot, party[i].sock, party[i].away);
      if (party[i].timer != 0L)
	 dprintf(z, "idle %s %d %lu\n", party[i].bot, party[i].sock,
		 time(NULL) - party[i].timer);
   }
   context;
   dump_bot_assoc(z);
}

int in_chain (char * who)
{
   int i;
   for (i = 0; i < tands; i++)
      if (strcasecmp(tandbot[i].bot, who) == 0)
	 return 1;
   if (strcasecmp(who, botnetnick) == 0)
      return 1;
   return 0;
}

void reject_bot (char * who)
{
   int i;
   i = nextbot(who);
   if (i < 0)
      return;
   if (strcasecmp(dcc[i].nick, who) == 0) {
      /* we're directly connected to the offending bot?! (shudder!) */
      putlog(LOG_BOTS, "*", "Rejecting bot %s", dcc[i].nick);
      chatout("*** Rejected bot %s\n", dcc[i].nick);
      tandout_but(i, "chat %s Rejected bot %s\n", botnetnick, dcc[i].nick);
      tandout_but(i, "unlinked %s\n", dcc[i].nick);
      tprintf(dcc[i].sock, "bye\n");
      killsock(dcc[i].sock);
      lostdcc(i);
   } else {
      tprintf(dcc[i].sock, "reject %s %s\n", botnetnick, who);
   }
}

/* break link with a tandembot */
int botunlink (int idx, char * nick, char * reason)
{
   int i;
   context;
   if (nick[0] == '*')
      dprintf(idx, "Unlinking all bots ...\n");
   for (i = 0; i < dcc_total; i++) {
      if ((nick[0] == '*') || (strcasecmp(dcc[i].nick, nick) == 0)) {
	 if (dcc[i].type == &DCC_FORK_BOT) {
	    if (idx >= 0)
	       dprintf(idx, "Killed link attempt to %s.\n", dcc[i].nick);
	    putlog(LOG_BOTS, "*", "Killed attempt to link %s at %s:%d", dcc[i].nick,
		   dcc[i].host, dcc[i].port);
	    killsock(dcc[i].sock);
	    dcc[i].sock = (long)dcc[i].type;
	    dcc[i].type = &DCC_LOST;
	    if (nick[0] != '*')
	       return 1;
	 }
	 if (dcc[i].type == &DCC_BOT_NEW) {
	    if (idx >= 0)
	       dprintf(idx, "No longer trying to link to %s.\n",
		       dcc[i].nick);
	    putlog(LOG_BOTS, "*", "Stopped trying to link %s at %s:%d", dcc[i].nick,
		   dcc[i].host, dcc[i].port);
	    killsock(dcc[i].sock);
	    dcc[i].sock = (long)dcc[i].type;
	    dcc[i].type = &DCC_LOST;
	    if (nick[0] != '*')
	       return 1;
	    else
	       i--;
	 }
	 if (dcc[i].type == &DCC_BOT) {
	    if (idx >= 0)
	       dprintf(idx, "Breaking link with %s.\n", dcc[i].nick);
	    tprintf(dcc[i].sock, "bye\n");
	    if (reason && reason[0]) {
	       chatout("*** Unlinked from: %s (%s)\n", dcc[i].nick, reason);
	       tandout_but(i, "chat %s Unlinked from: %s (%s)\n",
			   botnetnick, dcc[i].nick, reason);
	    } else {
	       chatout("*** Unlinked from: %s\n", dcc[i].nick);
	       tandout_but(i, "chat %s Unlinked from: %s\n", botnetnick, dcc[i].nick);
	    }
	    tandout_but(i, "unlinked %s\n", dcc[i].nick);
	    killsock(dcc[i].sock);
	    dcc[i].sock = (long)dcc[i].type;
	    dcc[i].type = &DCC_LOST;
	    if (nick[0] != '*')
	       return 1;
	    else
	       i--;
	 }
      }
   }
   if ((idx >= 0) && (nick[0] != '*'))
      dprintf(idx, "Not connected to that bot.\n");
   if (nick[0] == '*') {
      dprintf(idx, "Smooshing bot tables and assocs...\n");
      tands = 0;
      parties = 0;
      kill_all_assoc();
   }
   return 0;
}

/* link to another bot */
int botlink (char * linker, int idx, char * nick)
{
   char s[121], *p;
   int port, i;
   context;
   if (!(get_attr_handle(nick) & USER_BOT)) {
      if (idx >= 0)
	 dprintf(idx, "%s is not a known bot.\n", nick);
      return 0;
   }
   if (strcasecmp(nick, botnetnick) == 0) {
      if (idx >= 0)
	 dprintf(idx, "Link to myself?  Oh boy, Freud would have a field day.\n");
      return 0;
   }
   if ((in_chain(nick)) && (idx >= 0)) {
      dprintf(idx, "That bot is already connected up.\n");
      return 0;
   }
   /* address to connect to is in 'info' */
   get_handle_info(nick, s);
   if (!s[0]) {
      if (idx >= 0) {
	 dprintf(idx, "No telnet port stored for '%s'.\n", nick);
	 dprintf(idx, "Use: .chaddr %s <address>:<port#>[/<relay-port#>]\n", nick);
      }
      return 0;
   }
   p = strchr(s, ':');
   if (p == NULL)
      port = 2222;
   else {
      *p = 0;
      p++;
      port = atoi(p);
   }
   if (dcc_total == max_dcc) {
      if (idx >= 0)
	 dprintf(idx, "No more dcc entries left: dcc table is full.\n");
      return 0;
   }
   context;
   correct_handle(nick);
   i = new_dcc (&DCC_FORK_BOT,sizeof(struct bot_info));
   dcc[i].port = port;
   dcc[i].addr = 0L;
   strcpy(dcc[i].nick, nick);
   strcpy(dcc[i].host, s);
   dcc[i].u.bot->status = 0;
   dcc[i].timeval = time(NULL);
   strcpy(dcc[i].u.bot->linker, linker);
   strcpy(dcc[i].u.bot->version, "(primitive bot)");
   if (idx != (-1))
      putlog(LOG_BOTS, "*", "Linking to %s at %s:%d ...", nick, s, port);
   dcc[i].u.bot->x = idx;
   dcc[i].timeval = time(NULL);
/*  tandout("trying %s %s\n",botnetnick,dcc[i].nick);   */
   dcc[i].u.bot->port = dcc[i].port;	/* remember where i started */
   context;
   dcc[i].sock = getsock(SOCK_STRONGCONN);
   if (open_telnet_raw(dcc[i].sock, s, dcc[i].port) < 0) {
      failed_link(i);
      return 0;
   }
   return 1;
}
     
static void failed_tandem_relay (int idx)
{
   int uidx = (-1), i;
   context;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_PRE_RELAY) &&
	  (dcc[i].u.relay->sock == dcc[idx].sock))
	 uidx = i;
   if (uidx < 0) {
      putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d",
	     dcc[idx].sock, dcc[idx].u.relay->sock);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   if (dcc[idx].port >= dcc[idx].port + 3) {
      struct chat_info *ci = dcc[uidx].u.relay->chat;
      dprintf(uidx, "Could not link to %s.\n", dcc[idx].nick);
      nfree(dcc[uidx].u.relay);
      dcc[uidx].u.chat = ci;
      dcc[uidx].type = &DCC_CHAT;
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   killsock(dcc[idx].sock);
   dcc[idx].sock = getsock(SOCK_STRONGCONN);
   dcc[idx].port++;
   dcc[idx].timeval = time(NULL);
   if (open_telnet_raw(dcc[idx].sock, dcc[idx].host, dcc[idx].port) < 0)
      failed_tandem_relay(idx);
}

/* relay to another tandembot */
void tandem_relay (int idx, char * nick,int i)
{
   char s[121], *p, *p1;
   int port;
   struct chat_info *ci;
   context;
   if (!(get_attr_handle(nick) & USER_BOT)) {
      dprintf(idx, "%s is not a listed bot.\n", nick);
      return;
   }
   if (strcasecmp(nick, botnetnick) == 0) {
      dprintf(idx, "Relay to myself?  What on EARTH would be the point?!\n");
      return;
   }
   /* address to connect to is in 'info' */
   get_handle_info(nick, s);
   if (!s[0]) {
      dprintf(idx, "No telnet port stored for '%s'.\n", nick);
      dprintf(idx, "Use: .chaddr %s <address>:<port#>[/<relay-port#>]\n", nick);
      return;
   }
   p = strchr(s, ':');
   if (p == NULL)
      port = 2222;
   else {
      /* check for possible relay-port */
      *p++ = 0;
      p1 = strchr(p, '/');
      if (p1 == NULL)
	 port = atoi(p);	/* use link port# */
      else {
	 p1++;
	 port = atoi(p1);
      }
   }
   if (dcc_total == max_dcc) {
      dprintf(idx, "No more dcc table entries available.\n");
      return;
   }
   i = new_dcc(&DCC_FORK_RELAY,sizeof(struct relay_info));
   dcc[i].u.relay->chat = get_data_ptr(sizeof(struct chat_info));
   dcc[i].port = port;
   dcc[i].addr = 0L;
   strcpy(dcc[i].nick, nick);
   strcpy(dcc[i].host, s);
   dcc[i].u.relay->chat->away = NULL;
   dcc[i].u.relay->chat->status = 0;
   dcc[i].timeval = time(NULL);
   dcc[i].u.relay->chat->msgs_per_sec = 0;
   dcc[i].u.relay->chat->con_flags = 0;
   dcc[i].u.relay->chat->buffer = NULL;
   dcc[i].u.relay->chat->max_line = 0;
   dcc[i].u.relay->chat->line_count = 0;
   dcc[i].u.relay->chat->current_lines = 0;
   dprintf(idx, "Connecting to %s at %s:%d ...\n", nick, s, port);
   dprintf(idx, "(Type *BYE* on a line by itself to abort.)\n");
   context;
   ci = dcc[idx].u.chat;
   context;
   dcc[idx].u.relay = get_data_ptr(sizeof(struct relay_info));
   context;
   dcc[idx].u.relay->chat = ci;
   context;
   dcc[idx].type = &DCC_PRE_RELAY;
   context;
   dcc[i].port = dcc[i].port;
   dcc[i].sock = getsock(SOCK_STRONGCONN);
   dcc[idx].u.relay->sock = dcc[i].sock;
   dcc[i].u.relay->sock = dcc[idx].sock;
   dcc[i].timeval = time(NULL);
   if (open_telnet_raw(dcc[i].sock, s, dcc[i].port) < 0)
      failed_tandem_relay(i);
}

/* input from user before connect is ready */
static void pre_relay (int idx, char * buf,int i)
{
   int tidx = (-1);
   context;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_FORK_RELAY) &&
	  (dcc[i].u.relay->sock == dcc[idx].sock))
	 tidx = i;
   if (tidx < 0) {
      putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d",
	     dcc[i].sock, dcc[i].u.relay->sock);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
   }
   if (strcasecmp(buf, "*bye*") == 0) {
      /* disconnect */
      struct chat_info *ci = dcc[idx].u.relay->chat;
      dprintf(idx, "Aborting relay attempt to %s.\n", dcc[tidx].nick);
      dprintf(idx, "You are now back on %s.\n\n", botnetnick);
      putlog(LOG_MISC, "*", "Relay aborted: %s -> %s", dcc[idx].nick,
	     dcc[tidx].nick);
      nfree(dcc[idx].u.relay);
      dcc[idx].u.chat = ci;
      dcc[idx].type = &DCC_CHAT;
      if (dcc[idx].u.chat->channel >= 0) {
	 chanout2(dcc[idx].u.chat->channel, "%s joined the party line.\n",
		  dcc[idx].nick);
	 context;
	 if (dcc[idx].u.chat->channel < 100000)
	    tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
		    dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock,
		    dcc[idx].host);
      }
      notes_read(dcc[idx].nick, "", -1, idx);
      killsock(dcc[tidx].sock);
      lostdcc(tidx);
      return;
   }
   context;
}

/* user disconnected before her relay had finished connecting */
static void failed_pre_relay (int idx)
{
   int tidx = (-1), i;
   context;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_FORK_RELAY) &&
	  (dcc[i].u.relay->sock == dcc[idx].sock))
	 tidx = i;
   if (tidx < 0) {
      putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d",
	     dcc[i].sock, dcc[i].u.relay->sock);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
   }
   killsock(dcc[idx].sock);
   killsock(dcc[tidx].sock);
   putlog(LOG_MISC, "*", "Lost dcc connection to [%s]%s/%d", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   putlog(LOG_MISC, "*", "(Dropping relay attempt to %s)", dcc[tidx].nick);
   check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
   dcc[idx].sock = (long)dcc[idx].type;
   dcc[idx].type = &DCC_LOST;
   dcc[tidx].sock = (long)dcc[tidx].type;
   dcc[tidx].type = &DCC_LOST;
}

static void cont_tandem_relay (int idx,char * buf,int i)
{
   int uidx = (-1);
   context;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_PRE_RELAY) &&
	  (dcc[i].u.relay->sock == dcc[idx].sock))
	 uidx = i;
   if (uidx < 0) {
      putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d",
	     dcc[i].sock, dcc[i].u.relay->sock);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
   }
   dcc[uidx].type = &DCC_RELAYING;
   dcc[idx].type = &DCC_RELAY;
   dcc[idx].u.relay->sock = dcc[uidx].sock;
   dcc[uidx].u.relay->sock = dcc[idx].sock;
   dprintf(uidx, "Success!\n\n");
   dprintf(uidx, "NOW CONNECTED TO RELAY BOT %s ...\n", dcc[idx].nick);
   dprintf(uidx, "(You can type *BYE* to prematurely close the connection.)\n\n");
   putlog(LOG_MISC, "*", "Relay link: %s -> %s", dcc[uidx].nick, dcc[idx].nick);
   if (dcc[uidx].u.relay->chat->channel >= 0) {
      chanout2(dcc[uidx].u.relay->chat->channel, "%s left the party line.\n",
	       dcc[uidx].nick);
      context;
      if (dcc[uidx].u.relay->chat->channel < 100000)
	 tandout("part %s %s %d\n", botnetnick, dcc[uidx].nick, dcc[uidx].sock);
   }
}

static void eof_dcc_relay (int idx) {
   int j;
   struct chat_info *ci;
   
   for (j = 0; dcc[j].sock != dcc[idx].u.relay->sock; j++);
   /* in case echo was off, turn it back on: */
   if (dcc[j].u.relay->chat->status & STAT_TELNET)
     tprintf(dcc[j].sock, "\377\374\001\r\n");
   putlog(LOG_MISC, "*", "Ended relay link: %s -> %s", dcc[j].nick,
	  dcc[idx].nick);
   dprintf(j, "\n\n*** RELAY CONNECTION DROPPED.\n");
   dprintf(j, "You are now back on %s.\n", botnetnick);
   if (dcc[j].u.chat->channel >= 0) {
      chanout2(dcc[j].u.relay->chat->channel, "%s rejoined the party line.\n",
	       dcc[j].nick);
      context;
      if (dcc[j].u.relay->chat->channel < 100000)
	tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[j].nick,
		dcc[j].u.relay->chat->channel, geticon(j), dcc[j].sock,
		dcc[j].host);
   }
   ci = dcc[j].u.relay->chat;
   nfree(dcc[j].u.relay);
   dcc[j].u.chat = ci;
   dcc[j].type = &DCC_CHAT;
   check_tcl_chjn(botnetnick, dcc[j].nick, dcc[j].u.chat->channel,
		  geticon(j), dcc[j].sock, dcc[j].host);
   notes_read(dcc[j].nick, "", -1, j);
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void eof_dcc_relaying (int idx) {
   int j, x = dcc[idx].u.relay->sock;
   putlog(LOG_MISC, "*", "Lost dcc connection to [%s]%s/%d", dcc[idx].nick,
	  dcc[idx].host, dcc[idx].port);
   killsock(dcc[idx].sock);
   lostdcc(idx);
   for (j = 0; (dcc[j].sock != x) || (dcc[j].type == &DCC_FORK_RELAY) ||
	(dcc[j].type == &DCC_LOST); j++);
   putlog(LOG_MISC, "*", "(Dropping relay link to %s)", dcc[j].nick);
   killsock(dcc[j].sock);
   lostdcc(j);		/* drop connection to the bot */
}

/* for relays: swallow all codes as if they don't exist */
static void swallow_telnet_codes (char * buf)
{
   unsigned char *p = (unsigned char *) buf;
   int mark;
   while (*p != 0) {
      while ((*p != 255) && (*p != 0))
	 p++;			/* search for IAC */
      if (*p == 255) {
	 mark = 2;
	 if (!*(p + 1))
	    mark = 1;		/* bogus */
	 if ((*(p + 1) >= 251) && (*(p + 1) <= 254)) {
	    mark = 3;
	    if (!*(p + 2))
	       mark = 2;	/* bogus */
	 }
	 strcpy((char *) p, (char *) (p + mark));
      }
   }
}

static void dcc_relay (int idx, char * buf,int j)
{
   for (j = 0; (dcc[j].sock != dcc[idx].u.relay->sock) ||
	(dcc[j].type != &DCC_RELAYING); j++);
   /* if redirecting to a non-telnet user, swallow telnet codes */
   if (!(dcc[j].u.relay->chat->status & STAT_TELNET)) {
      swallow_telnet_codes(buf);
      if (!buf[0])
	 tprintf(dcc[idx].u.relay->sock, " \n");
      else
	 tprintf(dcc[idx].u.relay->sock, "%s\n", buf);
      return;
   }
   /* telnet user */
   if (!buf[0])
      tprintf(dcc[idx].u.relay->sock, " \r\n");
   else
      tprintf(dcc[idx].u.relay->sock, "%s\r\n", buf);
}

static void dcc_relaying (int idx, char * buf,int j)
{
   struct chat_info *ci;
   if (strcasecmp(buf, "*BYE*") != 0) {
      tprintf(dcc[idx].u.relay->sock, "%s\n", buf);
      return;
   }
   for (j = 0; (dcc[j].sock != dcc[idx].u.relay->sock) ||
	(dcc[j].type != &DCC_RELAY); j++);
   /* in case echo was off, turn it back on: */
   if (dcc[idx].u.relay->chat->status & STAT_TELNET)
      tprintf(dcc[idx].sock, "\377\374\001\r\n");
   dprintf(idx, "\n(Breaking connection to %s.)\n", dcc[j].nick);
   dprintf(idx, "You are now back on %s.\n\n", botnetnick);
   putlog(LOG_MISC, "*", "Relay broken: %s -> %s", dcc[idx].nick, dcc[j].nick);
   if (dcc[idx].u.relay->chat->channel >= 0) {
      chanout2(dcc[idx].u.relay->chat->channel,
	       "%s joined the party line.\n", dcc[idx].nick);
      context;
      if (dcc[idx].u.relay->chat->channel < 100000)
	 tandout("join %s %s %d %c%d %s\n", botnetnick, dcc[idx].nick,
	    dcc[idx].u.relay->chat->channel, geticon(idx), dcc[idx].sock,
		 dcc[idx].host);
   }
   ci = dcc[idx].u.relay->chat;
   nfree(dcc[idx].u.relay);
   dcc[idx].u.chat = ci;
   dcc[idx].type = &DCC_CHAT;
   if (dcc[idx].u.chat->channel >= 0)
      check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel,
		     geticon(idx), dcc[idx].sock, dcc[idx].host);
   notes_read(dcc[idx].nick, "", -1, idx);
   killsock(dcc[j].sock);
   lostdcc(j);
}

static void display_relay (int i,char * other) {
   sprintf(other,"rela  -> sock %d", dcc[i].u.relay->sock);
}

static void display_relaying (int i,char * other) {
   sprintf(other,">rly  -> sock %d", dcc[i].u.relay->sock);
}

static void display_tandem_relay (int i,char * other) {
   strcpy(other,"other  rela");
}

static void display_pre_relay (int i,char * other) {
   strcpy(other,"other  >rly");
}

static int expmem_relay (int idx) {
   int tot = sizeof(struct relay_info) + sizeof(struct chat_info);
   if (dcc[idx].u.relay->chat->away != NULL)
     tot += strlen(dcc[idx].u.relay->chat->away) + 1;
   return tot;
}

static void kill_relay (int idx) {
   nfree(dcc[idx].u.relay->chat);
   nfree(dcc[idx].u.relay);
}

struct dcc_table DCC_RELAY = {
   eof_dcc_relay,
   dcc_relay,
   0,
   0,
   display_relay,
   expmem_relay,
   kill_relay,
   0
};

static void out_relay (int idx,char * buf) {
   char * p = buf;
   if (dcc[idx].u.relay->chat->status & STAT_TELNET)
     p = add_cr(buf);
   tputs(dcc[idx].sock,buf,strlen(buf));
}

struct dcc_table DCC_RELAYING = {
   eof_dcc_relaying,
   dcc_relaying,
   0,
   0,
   display_relaying,
   expmem_relay,
   kill_relay,
   out_relay
};

struct dcc_table DCC_FORK_RELAY = {
   failed_tandem_relay,
   cont_tandem_relay,
   & connect_timeout,
   failed_tandem_relay,
   display_tandem_relay,
   expmem_relay,
   kill_relay,
   0
};

struct dcc_table DCC_PRE_RELAY = {
   failed_pre_relay,
   pre_relay,
   0,
   0,
   display_pre_relay,
   expmem_relay,
   kill_relay,
   0
};

/* once a monute, send 'ping' to each bot -- no exceptions */
void check_botnet_pings()
{
   int i, j;
   context;
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT)
	 if (dcc[i].u.bot->status & STAT_PINGED) {
	    putlog(LOG_BOTS, "*", "Ping timeout: %s", dcc[i].nick);
	    chatout("*** Ping timeout: %s\n", dcc[i].nick);
	    tandout_but(i, "chat %s Ping timeout: %s\n", botnetnick, dcc[i].nick);
	    tandout_but(i, "unlinked %s\n", dcc[i].nick);
	    killsock(dcc[i].sock);
	    lostdcc(i);
	 }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	 tprintf(dcc[i].sock, "ping\n");
	 dcc[i].u.bot->status |= STAT_PINGED;
      }
   for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT)
	 if (dcc[i].u.bot->status & STAT_OFFERED)
	    if (now - dcc[i].timeval > 120) {
	       tprintf(dcc[i].sock, "userfile?\n");
	       /* ^ send it again in case they missed it */
	    }
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_BOT) && (dcc[i].u.bot->status & STAT_LEAF)) {
	 for (j = 0; j < tands; j++) {
	    if ((strcasecmp(tandbot[j].via, dcc[i].nick) == 0) &&
		(strcasecmp(tandbot[j].via, tandbot[j].bot) != 0)) {
	       /* not leaflike behavior */
	       if (dcc[i].u.bot->status & STAT_WARNED) {
		  putlog(LOG_BOTS, "*", "No longer tolerating %s acting like a hub.",
			 dcc[i].nick);
		  tprintf(dcc[i].sock, "bye\n");
		  chatout("*** Disconnected %s (unleaflike behavior)\n",
			  dcc[i].nick);
		  tandout_but(i, "chat %s Disconnected %s (unleaflike behavior)\n",
			      botnetnick, dcc[i].nick);
		  tandout_but(i, "unlinked %s\n", dcc[i].nick);
		  killsock(dcc[i].sock);
		  lostdcc(i);
	       } else {
		  tprintf(dcc[i].sock, "reject %s %s\n", botnetnick, tandbot[j].bot);
		  dcc[i].u.bot->status |= STAT_WARNED;
	       }
	    } else
	       dcc[i].u.bot->status &= ~STAT_WARNED;
	 }
      }
   context;
}

void zapfbot (int idx)
{
   chatout("*** Dropped bot: %s\n", dcc[idx].nick);
   tandout_but(idx, "unlinked %s\n", dcc[idx].nick);
   tandout_but(idx, "chat %s Dropped bot: %s\n", botnetnick, dcc[idx].nick);
   killsock(dcc[idx].sock);
   dcc[idx].sock = (long)dcc[idx].type;
   dcc[idx].type = &DCC_LOST;
}

void restart_chons()
{
   int i;
   /* dump party line members */
   context;
   for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type == &DCC_CHAT) {
	 check_tcl_chon(dcc[i].nick, dcc[i].sock);
	 check_tcl_chjn(botnetnick, dcc[i].nick, dcc[i].u.chat->channel,
			geticon(i), dcc[i].sock, dcc[i].host);
      }
   }
   for (i = 0; i < parties; i++) {
      check_tcl_chjn(party[i].bot, party[i].nick, party[i].chan,
		     party[i].flag, party[i].sock, party[i].from);
   }
   context;
}
