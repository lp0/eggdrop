/* 
   misc.c -- handles:
   reading and sending notes
   killing old notes and changing the destinations

   dprintf'ized, 5aug96
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "eggdrop.h"
#include "proto.h"

extern char notefile[];
extern int dcc_total;
extern struct dcc_t dcc[];
extern char botnetnick[];
extern int serv;

/* Maximum number of notes to allow stored for each user */
int maxnotes = 50;
/* number of DAYS a note lives */
int note_life = 60;

/* determine how many notes are waiting for a user */
int num_notes PROTO1(char *, user)
{
   int tot = 0;
   FILE *f;
   char s[513], to[30];
   if (!notefile[0])
      return 0;
   f = fopen(notefile, "r");
   if (f == NULL)
      return 0;
   while (!feof(f)) {
      fgets(s, 512, f);
      if (!feof(f)) {
	 if (s[strlen(s) - 1] == '\n')
	    s[strlen(s) - 1] = 0;
	 rmspace(s);
	 if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* not comment */
	    nsplit(to, s);
	    if (strcasecmp(to, user) == 0)
	       tot++;
	 }
      }
   }
   fclose(f);
   return tot;
}

/* change someone's handle */
void notes_change PROTO3(int, idx, char *, oldnick, char *, newnick)
{
   FILE *f, *g;
   char s[513], to[30];
   int tot = 0;
   if (!notefile[0])
      return;
   f = fopen(notefile, "r");
   if (f == NULL)
      return;
   sprintf(s, "%s~new", notefile);
   g = fopen(s, "w");
   if (g == NULL) {
      fclose(f);
      return;
   }
   while (!feof(f)) {
      fgets(s, 512, f);
      if (!feof(f)) {
	 if (s[strlen(s) - 1] == '\n')
	    s[strlen(s) - 1] = 0;
	 rmspace(s);
	 if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* not comment */
	    nsplit(to, s);
	    if (strcasecmp(to, oldnick) == 0) {
	       tot++;
	       fprintf(g, "%s %s\n", newnick, s);
	    } else
	       fprintf(g, "%s %s\n", to, s);
	 } else
	    fprintf(g, "%s\n", s);
      }
   }
   fclose(f);
   fclose(g);
   unlink(notefile);
   sprintf(s, "%s~new", notefile);
   movefile(s, notefile);
   if ((tot > 0) && (idx >= 0))
      dprintf(idx, "Switched %d note%s from %s to %s.\n", tot, tot == 1 ? "" : "s",
	      oldnick, newnick);
}

/* get rid of old useless notes */
void expire_notes()
{
   FILE *f, *g;
   char s[513], to[30], from[30], ts[30];
   int tot = 0, lapse;
   time_t now = time(NULL);
   if (!notefile[0])
      return;
   f = fopen(notefile, "r");
   if (f == NULL)
      return;
   sprintf(s, "%s~new", notefile);
   g = fopen(s, "w");
   if (g == NULL) {
      fclose(f);
      return;
   }
   while (!feof(f)) {
      fgets(s, 512, f);
      if (!feof(f)) {
	 if (s[strlen(s) - 1] == '\n')
	    s[strlen(s) - 1] = 0;
	 rmspace(s);
	 if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* not comment */
	    nsplit(to, s);
	    nsplit(from, s);
	    nsplit(ts, s);
	    lapse = (now - (time_t) atol(ts)) / 86400;
	    if (lapse > note_life)
	       tot++;
	    else if (!is_user(to))
	       tot++;
	    else
	       fprintf(g, "%s %s %s %s\n", to, from, ts, s);
	 } else
	    fprintf(g, "%s\n", s);
      }
   }
   fclose(f);
   fclose(g);
   unlink(notefile);
   sprintf(s, "%s~new", notefile);
   movefile(s, notefile);
   if (tot > 0)
      putlog(LOG_MISC, "*", "Expired %d note%s", tot, tot == 1 ? "" : "s");
}

/* add note to notefile */
int add_note PROTO5(char *, to, char *, from, char *, msg, int, idx, int, echo)
{
   FILE *f;
   int status, i, iaway, sock;
   char *p, botf[81], ss[81], ssf[81];
   if (strlen(msg) > 450)
      msg[450] = 0;		/* notes have a limit */
   /* note length + PRIVMSG header + nickname + date  must be <512  */
   p = strchr(to, '@');
   if (p != NULL) {		/* cross-bot note */
      *p = 0;
      p++;
      if (strcasecmp(p, botnetnick) == 0)	/* to me?? */
	 return add_note(to, from, msg, idx, echo);	/* start over, dimwit. */
      if (strcasecmp(from, botnetnick) != 0)
	 sprintf(botf, "%s@%s", from, botnetnick);
      else
	 strcpy(botf, botnetnick);
      i = nextbot(p);
      if (i < 0) {
	 if (idx >= 0)
	    dprintf(idx, "That bot isn't here.\n");
	 return NOTE_ERROR;
      }
      if ((idx >= 0) && (echo))
	 dprintf(idx, "-> %s@%s: %s\n", to, p, msg);
      if (idx >= 0)
	 tprintf(dcc[i].sock, "priv %d:%s %s@%s %s\n", dcc[idx].sock,
		 botf, to, p, msg);
      else
	 tprintf(dcc[i].sock, "priv %s %s@%s %s\n", botf, to, p, msg);
      return NOTE_OK;		/* forwarded to the right bot */
   }
   /* might be form "sock:nick" */
   splitc(ssf, from, ':');
   rmspace(ssf);
   splitc(ss, to, ':');
   rmspace(ss);
   if (!ss[0])
      sock = (-1);
   else
      sock = atoi(ss);
   /* don't process if there's a note binding for it */
   if (idx != (-2)) {		/* notes from bots don't trigger it */
      if (check_tcl_note(from, to, msg)) {
	 if ((idx >= 0) && (echo))
	    dprintf(idx, "-> %s: %s\n", to, msg);
	 return NOTE_TCL;
      }
   }
   if (!is_user(to)) {
      if (idx >= 0)
	 dprintf(idx, "I don't know anyone by that name.\n");
      return NOTE_ERROR;
   }
   if (get_attr_handle(to) & USER_BOT) {
      if (idx >= 0)
	 dprintf(idx, "That's a bot.  You can't leave notes for a bot.\n");
      return NOTE_ERROR;
   }
   status = NOTE_STORED;
   iaway = 0;
   /* online right now? */
   for (i = 0; i < dcc_total; i++) {
      if (((dcc[i].type == DCC_CHAT) || (dcc[i].type == DCC_FILES)) &&
	  ((sock == (-1)) || (sock == dcc[i].sock)) &&
	  (strcasecmp(dcc[i].nick, to) == 0)) {
	 int aok = 1;
	 if (dcc[i].type == DCC_CHAT)
	    if ((dcc[i].u.chat->away != NULL) &&
		(idx != (-2))) {
	       /* only check away if it's not from a bot */
	       aok = 0;
	       if (idx >= 0)
		  dprintf(idx, "%s is away: %s\n", dcc[i].nick, dcc[i].u.chat->away);
	       if (!iaway)
		  iaway = i;
	       status = NOTE_AWAY;
	    }
	 if (dcc[i].type == DCC_FILES)
	    if ((dcc[i].u.file->chat->away != NULL) &&
		(idx != (-2))) {
	       aok = 0;
	       if (idx >= 0)
		  dprintf(idx, "%s is away: %s\n", dcc[i].nick,
			  dcc[i].u.file->chat->away);
	       if (!iaway)
		  iaway = i;
	       status = NOTE_AWAY;
	    }
	 if (aok) {
	    if ((idx == (-2)) || (strcasecmp(from, botnetnick) == 0))
	       if (!ssf[0])
		  dprintf(i, "*** [%s] %s\n", from, msg);
	       else
		  dprintf(i, "*** [%s (%s)] %s\n", from, ssf, msg);
	    else {
	       if (!ssf[0])
		  dprintf(i, "%cNote [%s]: %s\n", 7, from, msg);
	       else
		  dprintf(i, "%cNote [%s (%s)]: %s\n", 7, from, ssf, msg);
	    }
	    if ((idx >= 0) && (echo))
	       dprintf(idx, "-> %s: %s\n", to, msg);
	    return NOTE_OK;
	 }
      }
   }
   if (notefile[0] == 0) {
      if (idx >= 0)
	 dprintf(idx, "Notes are not supported by this bot.\n");
      return NOTE_ERROR;
   }
   if (idx == (-2))
      return NOTE_OK;		/* error msg from a tandembot: don't store */
   if (num_notes(to) >= maxnotes) {
      if (idx >= 0)
	 dprintf(idx, "Sorry, that user has too many notes already.\n");
      return NOTE_FULL;
   }
   f = fopen(notefile, "a");
   if (f == NULL)
      f = fopen(notefile, "w");
   if (f == NULL) {
      if (idx >= 0)
	 dprintf(idx, "Can't create notefile.  Sorry.\n");
      putlog(LOG_MISC, "*", "Notefile unreachable!");
      return NOTE_ERROR;
   }
   fprintf(f, "%s %s %lu %s\n", to, from, time(NULL), msg);
   fclose(f);
   if (idx >= 0)
      dprintf(idx, "Stored message.\n");
   if (status == NOTE_AWAY) {
      /* user is away in all sessions -- just notify the user that a */
      /* message arrived and was stored.  (only oldest session is notified.) */
      dprintf(iaway, "*** Note arrived for you.\n");
   }
   return status;
}

/* rd=-1 : index
   rd=0 : read all msgs
   rd>0 : read msg #n

   idx=-1 : /msg
 */
void notes_read PROTO4(char *, hand, char *, nick, int, rd, int, idx)
{
   FILE *f;
   char s[601], to[15], dt[81], from[81];
   time_t tt;
   int ix = 1;
   if (!notefile[0]) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
      return;
   }
   f = fopen(notefile, "r");
   if (f == NULL) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
      return;
   }
   while (!feof(f)) {
      fgets(s, 600, f);
      if (s[strlen(s) - 1] == '\n')
	 s[strlen(s) - 1] = 0;
      if (!feof(f)) {
	 rmspace(s);
	 if ((s[0]) && (s[0] != '#') & (s[0] != ';')) {		/* not comment */
	    split(to, s);
	    if (strcasecmp(to, hand) == 0) {
	       int lapse;
	       split(from, s);
	       split(dt, s);
	       tt = atol(dt);
	       strcpy(dt, ctime(&tt));
	       dt[16] = 0;
	       strcpy(dt, &dt[4]);
	       lapse = (int) ((time(NULL) - tt) / 86400);
	       if (lapse > note_life - 7) {
		  if (lapse >= note_life)
		     strcat(dt, " -- EXPIRES TODAY");
		  else
		     sprintf(&dt[strlen(dt)], " -- EXPIRES IN %d DAY%s",
			     note_life - lapse, (note_life - lapse) == 1 ? "" : "S");
	       }
	       if ((ix == rd) || (rd == 0)) {
		  if (idx >= 0)
		     dprintf(idx, "%2d. %s (%s): %s\n", ix, from, dt, s);
		  else
		     hprintf(serv, "NOTICE %s :%2d. %s (%s): %s\n", nick, ix, from,
			     dt, s);
	       }
	       if (rd < 0) {
		  if (idx >= 0) {
		     if (ix == 1)
			dprintf(idx, "### You have the following notes waiting:\n");
		     dprintf(idx, "  %2d. %s (%s)\n", ix, from, dt);
		  } else
		     hprintf(serv, "NOTICE %s :%2d. %s (%s)\n", nick, ix, from, dt);
	       }
	       ix++;
	    }
	 }
      }
   }
   fclose(f);
   if ((rd >= ix) && (rd > 0)) {
      if (idx >= 0)
	 dprintf(idx, "You don't have that many messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You don't have that many messages.\n", nick);
   }
   if (rd < 0) {
      if (ix == 1) {
	 if (idx >= 0)
	    dprintf(idx, "You have no messages waiting.\n");
	 else
	    hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
      } else {
	 if (idx >= 0)
	    dprintf(idx, "### Use '.notes read' to read them.\n");
	 else
	    hprintf(serv, "NOTICE %s :(%d total)\n", nick, ix - 1);
      }
   }
   if ((rd == 0) && (ix == 1)) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages waiting.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
   }
}

void notes_del PROTO4(char *, hand, char *, nick, int, dl, int, idx)
{
   FILE *f, *g;
   char s[513], to[81];
   int in = 1;
   if (!notefile[0]) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
      return;
   }
   f = fopen(notefile, "r");
   if (f == NULL) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
      return;
   }
   sprintf(s, "%s~new", notefile);
   g = fopen(s, "w");
   if (g == NULL) {
      if (idx >= 0)
	 dprintf(idx, "Can't modify the note file. :(\n");
      else
	 hprintf(serv, "NOTICE %s :Can't modify the note file. :(\n", nick);
      fclose(f);
      return;
   }
   while (!feof(f)) {
      fgets(s, 512, f);
      if (s[strlen(s) - 1] == '\n')
	 s[strlen(s) - 1] = 0;
      if (!feof(f)) {
	 rmspace(s);
	 if ((s[0]) && (s[0] != '#') && (s[0] != ';')) {	/* not comment */
	    split(to, s);
	    if (strcasecmp(to, hand) == 0) {
	       if ((dl > 0) && (in != dl))
		  fprintf(g, "%s %s\n", to, s);
	       in++;
	    } else
	       fprintf(g, "%s %s\n", to, s);
	 } else
	    fprintf(g, "%s\n", s);
      }
   }
   fclose(f);
   fclose(g);
   unlink(notefile);
   sprintf(s, "%s~new", notefile);
#ifdef RENAME
   rename(s, notefile);
#else
   movefile(s, notefile);
#endif
   if ((dl >= in) && (dl > 0)) {
      if (idx >= 0)
	 dprintf(idx, "You don't have that many messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You don't have that many messages.\n",
		 nick);
   } else if (in == 1) {
      if (idx >= 0)
	 dprintf(idx, "You have no messages.\n");
      else
	 hprintf(serv, "NOTICE %s :You have no messages.\n", nick);
   } else {
      if (dl == 0) {
	 if (idx >= 0)
	    dprintf(idx, "Erased all notes.\n");
	 else
	    hprintf(serv, "NOTICE %s :Erased all notes.\n", nick);
      } else {
	 if (idx >= 0)
	    dprintf(idx, "Erased #%d; %d left.\n", dl, in - 2);
	 else
	    hprintf(serv, "NOTICE %s :Erased #%d; %d left.\n", nick, dl, in - 2);
      }
   }
}
