/*
 * seen.c   - Implement the seen.tcl script functionality via module.
 *
 *            by ButchBub - Scott G. Taylor (staylor@mrynet.com) 
 *
 *      REQUIRED: Eggdrop Module version 1.2.0
 *
 *      0.1     1997-07-29      Initial.
 *      1.0     1997-07-31      Release.
 *      1.1     1997-08-05      Add nick->handle lookup for NICK's.
 *
 */

/*
 *  Currently, PUB, DCC and MSG commands are supported.  No party-line
 *      filtering is performed.
 *
 *  For boyfriend/girlfriend support, this module relies on the XTRA
 *      fields in the userfile to use BF and GF, respectively, for
 *      these fields.  
 *
 *  userinfo1.0.tcl nicely compliments this script by providing 
 *      the necessary commands to facilitate modification of these
 *      fields via DCC and IRC MSG commands.
 *
 *  A basic definition of the parsing syntax follows:
 *
 *      trigger :: seen [ <key> [ [ and | or ] <key> [...]]]
 *
 *        <key> :: <keyword> [ <keyarg> ]
 *
 *    <keyword> :: god | jesus | shit | me | yourself | my | <nick>'s |
 *                 your
 *       <nick> :: (any current on-channel IRC nick or userlist nick or handle)
 *
 *     <keyarg> :: (see below)
 *
 *              KEYWORD KEYARG
 *
 *              my      boyfriend
 *                      bf
 *                      girlfriend
 *                      gf
 *              your    owner
 *                      admin
 *                      (other)
 *              NICK's  boyfriend   
 *                      bf
 *                      girlfriend
 *                      gf
 *
 */

#define MAKING_SEEN
#define MODULE_NAME "seen"

#include <time.h>
#include <varargs.h>

#include "../../users.h"
#include "../module.h"
#include "../../chan.h"

extern char admin[];

Function *global;
void wordshift();
void process_seen();
void do_seen();
char *match_trigger();
char *getxtra();
char *fixnick();
extern struct chanset_t *chanset;

typedef struct {
        char    *key;
        char    *text;
} trig_data;

static trig_data trigdata[]  = {
   {"god", "Let's not get into a religious discussion, %s"},
   {"jesus", "Let's not get into a religious discussion, %s"},
   {"shit", "Here's looking at you, %s"},
   {"yourself", "Yeah, whenever I look in a mirror..."},
   {(char *) botnetnick, "You found me, %s!"},
   {0, 0}
};

static int seen_expmem()
{
int size = 0;
   return size;
}

/*
 * PUB `seen' trigger.
 */

static int pub_seen (char *nick, char *host, char *hand, 
                                                char *channel, char *text)  
{
char prefix[50];
modcontext;
   sprintf(prefix, "PRIVMSG %s :", channel);
   do_seen(DP_SERVER, prefix, nick, hand, channel, text);
   return 0;
}

static int msg_seen (char *nick, char *host, char *hand, char *text)  
{
char prefix[50];
modcontext;
   if(hand[0] == '*') {
      putlog(LOG_MISC, "*", "[%s!%s] seen %s", nick, host, text);
      return 0;
   }
   putlog(LOG_MISC, "*", "(%s!%s) !%s! SEEN %s", nick, host, hand, text);
   sprintf(prefix, "PRIVMSG %s :", nick);
   do_seen(DP_SERVER, prefix, nick, hand, "", text);
   return 0;
}

static int dcc_seen (int idx, char *par)
{
modcontext;
   putlog(LOG_CMDS, "*", "#%s# seen %s", dcc[idx].nick, par);
   do_seen(idx, "", dcc[idx].nick, dcc[idx].nick, "", par);
   return 0;
}

void do_seen(int idx, char *prefix, char *nick, char *hand, char *channel, 
                                                          char *text)
{
char    stuff[512];
char    word1[512], word2[512];
char    whotarget[512];
char    object[512];
char    *oix;
char    whoredirect[512];
struct  userrec *urec;
struct  chanset_t *chan;
memberlist *m;
int     onchan = 0;
int     i;

   whotarget[0] = 0;
   whoredirect[0] = 0;
   object[0] = 0;

/* Was ANYONE specified */

   if(!text[0]) {
      sprintf(stuff, 
         "%sUm, %s, it might help if you ask me about _someone_...\n", 
                          prefix, nick);
      dprintf (idx, stuff);
      return;
   }

   wordshift(word1, text);
   oix = index(word1, '\'');

   /* Have we got a NICK's target? */

   if (oix == word1) return;       /* Skip anything starting with ' */

modcontext;
   if( oix && *oix  &&
         ((oix[1] && ( oix[1] == 's' || oix[1]  == 'S') && !oix[2]) ||
        (!oix[1] && (oix[-1] == 's' || oix[-1] == 'z' || oix[-1] == 'x'
                  || oix[-1] == 'S' || oix[-1] == 'Z' || oix[-1] == 'X')))) {

      strncpy(object, word1, oix - word1);
      object[oix - word1] = 0;

      wordshift(word1, text);

      if(!word1[0]) {
         dprintf(idx, "%s%s's what, %s?\n", prefix, object, nick);
         return;
      }

      urec = get_user_by_handle (userlist, object);
      if (!urec) {
         chan = chanset;
         while(chan) {
            onchan = 0;
            if (ismember(chan, object)) {
               onchan = 1;
               getchanhost (chan->name, object, word2);
               sprintf(stuff, "%s!%s", object, word2);
               get_handle_by_host (word2, stuff);
               if(!strcasecmp(object, word2)) break;
               strcat(whoredirect, object);
               strcat(whoredirect, " is ");
               strcat(whoredirect, word2);
               strcat(whoredirect, ", and ");
	       strcpy(object,word2);
               break;
            }
            chan = chan->next;
         }

         if (!onchan) {
            dprintf(idx, "%sI don't think I know who %s is, %s.\n",
                           prefix, object, nick);
            return;
         }
      }

      if(!strcasecmp(word1, "bf") || !strcasecmp(word1, "boyfriend")) {
         strcpy(whotarget, getxtra(object, "BF"));
         if (whotarget[0]) {
            sprintf(whoredirect, "%s boyfriend is %s, and ",
                           fixnick(object), whotarget);
            goto TARGETCONT;
         }
      
         dprintf(idx, 
            "%sI don't know who %s boyfriend is, %s.\n",
                           prefix, fixnick(object), nick);
         return;
      } 
      if(!strcasecmp(word1, "gf") || !strcasecmp(word1, "girlfriend")) {
         strcpy(whotarget, getxtra(object, "GF"));
         if (whotarget[0]) {
            sprintf(whoredirect, "%s girlfriend is %s, and ",
                           fixnick(object), whotarget);
            goto TARGETCONT;
         }
      
         dprintf(idx, 
            "%sI don't know who %s girlfriend is, %s.\n",
                           prefix, fixnick(object), nick);
         return;
      }

      dprintf(idx, 
            "%sWhy are you bothering me with questions about %s %s, %s?\n",
                           prefix, fixnick(object), word1, nick);
      return;
   }

   /* Keyword "my" */

   if (!strcasecmp(word1, "my")) {

      wordshift(word1, text);

      if (!word1[0]) {
         dprintf(idx, "%sYour what, %s?\n", prefix, nick);
         return;
      } 

      /* Do I even KNOW the requestor? */

      if(hand[0] == '*' || !hand[0]) {
         dprintf(idx, 
          "%sI don't know you, %s, so I don't know about your %s.\n",
                                            prefix, nick, word1);
         return;
      } 

      /* "my boyfriend" */

      if (!strcasecmp(word1, "boyfriend") || !strcasecmp(word1, "bf")) {
         strcpy(whotarget, getxtra(hand, "BF"));
         if (whotarget[0]) {
            sprintf(whoredirect, "%s, your boyfriend is %s, and ",
                                 nick, whotarget);
         } else {
            dprintf(idx, 
                  "%sI didn't know you had a boyfriend, %s\n",
                               prefix, nick);
            return;
         }
      } 

      /* "my girlfriend" */

        else if (!strcasecmp(word1, "girlfriend") || !strcasecmp(word1, "gf")) {
         strcpy(whotarget, getxtra(hand, "BF"));
         if (whotarget[0]) {
            sprintf(whoredirect, "%s, your girlfriend is %s, and ",
                                 nick, whotarget);
         } else {
            dprintf(idx, 
                  "%sI don't know you had a girlfriend, %s\n",
                               prefix, nick);
            return;
         }
      } else {
         dprintf(idx, 
                  "%sI don't know anything about your %s, %s.\n",
                               prefix, word1, nick);
         return;
      }
   } 

      /* "your" keyword */

     else if (!strcasecmp(word1, "your")) {
      wordshift(word1, text);

      /* "your admin" */

      if (!strcasecmp(word1, "owner") || !strcasecmp(word1, "admin")) {
         if(admin[0]) {
            strcpy(word2, admin);
            wordshift(whotarget, word2);
            strcat(whoredirect, "My owner is ");
            strcat(whoredirect, whotarget);
            strcat(whoredirect, ", and ");
            if (!strcasecmp(whotarget, hand)) {
               strcat(whoredirect, "that's YOU");
               if (!strcasecmp(hand, nick)) {
                  strcat(whoredirect, "!!!");
               } else {
                  strcat(whoredirect, ", ");
                  strcat(whoredirect, nick);
                  strcat(whoredirect, "!");
              }
               dprintf(idx, "%s%s\n", prefix, whoredirect);
               return;
            }
         } else {          /* owner variable munged or not set */
         dprintf(idx, 
            "%sI don't seem to recall who my owner is right now...\n",
                           prefix);
            return;
         }
      } else {             /* no "your" target specified */
         dprintf(idx, "%sLet's not get personal, %s.\n",
                           prefix, nick);
         return;
      }
   } 

      /* Check for keyword match in the internal table */

    else if (match_trigger(word1)) {
      sprintf(word2, "%s%s\n", prefix, match_trigger(word1));
      dprintf(idx, word2, nick);
      return;
   } 
 
      /* Otherwise, make the target to the first word and continue */

    else {
      strcpy(whotarget, word1);
   }

TARGETCONT:

modcontext;
/* Looking for ones own nick? */

   if(!strcasecmp(nick, whotarget)) {
      dprintf(idx, "%s%sLooking for yourself, eh %s?\n", 
                           prefix, whoredirect, nick);
      return;
   }

/* Check if nick is on a channel */

   chan = chanset;
   while(chan) {
      if (ismember(chan, whotarget)) {
         onchan = 1;
         getchanhost (chan->name, whotarget, word2);
         sprintf(word1, "%s!%s", whotarget, word2);
         get_handle_by_host (word2, word1);
         if(!strcasecmp(whotarget, word2)) break;
         strcat(whoredirect, whotarget);
         strcat(whoredirect, " is ");
         strcat(whoredirect, word2);
         strcat(whoredirect, ", and ");
         break;
      }
      chan = chan->next;
   }

/* Check if nick is on a channel by xref'ing to handle */

   if (!onchan) {
      chan = chanset;
      while(chan) {
         m = chan->channel.member;
         while (m->nick[0]) {
            sprintf(word2, "%s!%s", m->nick, m->userhost);
            get_handle_by_host(word1, word2);
            if(!strcasecmp(word1, whotarget)) {
               onchan = 1;
               strcat(whoredirect, whotarget);
               strcat(whoredirect, " is ");
               strcat(whoredirect, m->nick);
               strcat(whoredirect, ", and ");
               strcpy(whotarget, m->nick);
               break;
            } else {
            }
            m = m->next;
         }
         chan = chan->next;
      }
   }

/* Check if the target was on the channel, but is netsplit */

   chan = findchan(channel);

   if (chan && is_split(chan->name, whotarget)) {
      dprintf(idx, 
            "%s%s%s was just here, but got netsplit.\n",
                        prefix, whoredirect, whotarget);
      return;
   } 

/* Check if the target IS on the channel */

   if (chan && ismember(chan, whotarget)) {
      dprintf(idx, "%s%s%s is on the channel right now!\n",
                        prefix, whoredirect, whotarget);
      return;
   }

/* Target not on this channel.   Check other channels */

   chan = chanset;
   while (chan) {

      if (is_split(chan->name, whotarget)) {
         dprintf(idx, 
               "%s%s%s was just on %s, but got netsplit.\n",
                        prefix, whoredirect, whotarget, chan->name);
         return;
      }
      
      if (ismember(chan, whotarget)) {
         dprintf(idx, 
                        "%s%s%s is on %s right now!\n",
                        prefix, whoredirect, whotarget, chan->name);
         return;
      }
      chan = chan->next;
   }
            
/*
 * Target isn't on any of my channels.  See if target matches a handle
 * in my userlist 
 */

   urec = get_user_by_handle (userlist, whotarget);

/* No match, then bail out */

   if (!urec) {
      dprintf(idx, "%s%sI don't know who %s is.\n",
                        prefix, whoredirect, whotarget);
      return;
   }

/* We had a userlist match to a handle */

/* Is the target currently DCC CHAT to me on the botnet? */

   for (i = 0; i < dcc_total; i++) {
      if(dcc[i].type == &DCC_CHAT) {
         if(!strcasecmp(whotarget, dcc[i].nick)) {
            if (!strcasecmp(channel, dcc[i].u.chat->con_chan) &&
                        dcc[i].u.chat->con_flags & LOG_PUBLIC) {
               strcat(whoredirect, whotarget);
               strcat(whoredirect, 
                 " is 'observing' this channel right now from my party line!");
               dprintf(idx, "%s%s\n", 
                        prefix, whoredirect);
            } else {
               dprintf(idx, 
                  "%s%s%s is linked to me via DCC CHAT right now!\n",
                        prefix, whoredirect, whotarget);
            }
            return;
         }
      }
   }

/* Target known, but nowhere to be seen.   Give last IRC and botnet time */

   strftime(word2, 50, "%A, %B %3, %Y at %l:%M%p %Z", 
                             localtime((time_t *) &urec->laston));
   word1[0] = 0;
   if(!urec->lastonchan || !urec->lastonchan[0]){
      dprintf(idx, "%s%sI've never seen %s around.\n", 
                           prefix, whoredirect, whotarget);
   } else {
      if(urec->lastonchan[0] == '#')
         sprintf(word1, "on IRC channel %s", urec->lastonchan);
      else if (urec->lastonchan[0] == '@')
         sprintf(word1, "on %s", urec->lastonchan + 1);
      else if(urec->lastonchan[0] != 0)
         sprintf(word1, "on my %s", urec->lastonchan);
      else 
         strcpy(word1, "seen");
      dprintf(idx, "%s%s%s was last %s on %s\n", 
               prefix, whoredirect, whotarget, word1, word2);
   }
}

char  fixednick[512];
char *fixnick (char *nick)
{
   strcpy(fixednick, nick);
   strcat(fixednick, "'");
   switch (nick[strlen(nick) - 1]) {
      case 's':
      case 'S':
      case 'x':
      case 'X':
      case 'z':
      case 'Z':
         break;
      default:
         strcat(fixednick, "s");
         break;
   }
   return fixednick;
}

char *match_trigger( char *word)
{
trig_data *t = trigdata;

   while(t->key) {
      if (!strcasecmp(word, t->key))
         return t->text;
      t++;
   }
   return (char *)0;
}

char   outfield[512];

char *getxtra(char *hand, char *field)
{

struct userrec *urec;
char *p1, *p2;

   outfield[0]=0;

   urec = get_user_by_handle(userlist, hand);

   for (p1 = urec->xtra; *p1; p1++) {
      if (*p1 == '{') {                            /* Find next { */
         for (p2 = p1++; p2 && *p2 != ' '; p2++);  /* Find next space */
         if (strlen(p1) > strlen(field)) {
            if (!strncasecmp(p1, field, strlen(field))) {
               p1 = p2;
               for (p2++; p2 && *p2; p2++) {
                  if (*p2 == '{') p1 = p2 + 1;
                  if (*p2 == '}') {
                     p2--; break;
                  }
               }
               strncpy(outfield, p1 + 1, p2 - p1);
               outfield[p2 - p1] = 0;
               return outfield;
            }
         }
      }
   }
   return outfield;
}

void wordshift (char *first, char *rest)
{
LOOPIT:
   split(first, rest);
   if(!first[0]) {
      strcpy(first, rest);
      rest[0] = 0;
   }
   if (!strcasecmp(first, "and") || !strcasecmp(first, "or"))
      goto LOOPIT;
}

/* 
 * Report on current seen info for .modulestat.
 */

static void seen_report (int idx)
{
   dprintf(idx, "     seen.so - PUB, DCC and MSG \"seen\" commands.\n");
}

/*
 * PUB channel builtin commands.
 */

static cmd_t seen_pub[] =
{
   {"seen",   "", pub_seen, 0},
   {0, 0, 0, 0}
};
static cmd_t seen_dcc[] =
{
   {"seen",   "", dcc_seen, 0},
   {0, 0, 0, 0}
};
static cmd_t seen_msg[] =
{
   {"seen",   "", msg_seen, 0},
   {0, 0, 0, 0}
};

static char *seen_close()
{
p_tcl_hash_list H_pub;
    
   H_pub = find_hash_table("pub");
   rem_builtins(H_pub, seen_pub);
   H_pub = find_hash_table("dcc");
   rem_builtins(H_pub, seen_dcc);
   H_pub = find_hash_table("msg");
   rem_builtins(H_pub, seen_msg);
   module_undepend(MODULE_NAME);
   return NULL;
}

char *seen_start ();

static Function seen_table[] =
{
   (Function) seen_start,
   (Function) seen_close,
   (Function) seen_expmem,
   (Function) seen_report,
};

char *seen_start (Function *egg_func_table)
{
p_tcl_hash_list H_pub;

   global = egg_func_table;
   modcontext;
   module_register(MODULE_NAME, seen_table, 1, 1);
   if(!module_depend(MODULE_NAME, "eggdrop", 102, 0)) 
      return 
          "MODULE `seen' cannot be loaded on Eggdrops prior to version 1.2.0";
   H_pub = find_hash_table("pub");
   add_builtins(H_pub, seen_pub);
   H_pub = find_hash_table("dcc");
   add_builtins(H_pub, seen_dcc);
   H_pub = find_hash_table("msg");
   add_builtins(H_pub, seen_msg);
   return NULL;
}
