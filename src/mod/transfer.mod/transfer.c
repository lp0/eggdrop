/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#define MOD_FILESYS
#define MODULE_NAME "transfer"

#include "../module.h"
#include "../../tandem.h"

#include "../../users.h"
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern char natip[];

/* copy files to /tmp before transmitting? */
int copy_to_tmp = 1;
/* timeout time on DCC xfers */
static int wait_dcc_xfer = 300;
/* maximum number of simultaneous file downloads allowed */
static int dcc_limit = 3;
static int dcc_block = 1024;

typedef struct zarrf {
   char *dir;			/* starts with '*' -> absolute dir */
   char *file;			/*    (otherwise -> dccdir) */
   char nick[NICKLEN];		/* who queued this file */
   char to[NICKLEN];		/* who will it be sent to */
   struct zarrf *next;
} fileq_t;


static fileq_t *fileq = NULL;

#undef MATCH
#define MATCH (match+sofar)

/* this function SHAMELESSLY :) pinched from match.c in the original 
 * sourc, see that file for info about the authour etc */

#define QUOTE '\\'
#define WILDS '*'
#define WILDQ '?'
#define NOMATCH 0
/*========================================================================*
 * EGGDROP:   wild_match_file(char *ma, char *na)                         *
 * IrcII:     NOT USED                                                    *
 *                                                                        *
 * Features:  Forward, case-sensitive, ?, *                               *
 * Best use:  File mask matching, as it is case-sensitive                 *
 *========================================================================*/
int wild_match_file(register unsigned char * m, register unsigned char * n)
{
   unsigned char *ma = m, *lsm = 0, *lsn = 0;
   int match = 1;
   register unsigned int sofar = 0;

   /* take care of null strings (should never match) */
   if ((m == 0) || (n == 0) || (!*n))
      return NOMATCH;
   /* (!*m) test used to be here, too, but I got rid of it.  After all,
      If (!*n) was false, there must be a character in the name (the
      second string), so if the mask is empty it is a non-match.  Since
      the algorithm handles this correctly without testing for it here
      and this shouldn't be called with null masks anyway, it should be
      a bit faster this way */

   while (*n) {
      /* Used to test for (!*m) here, but this scheme seems to work better */
      switch (*m) {
      case 0:
	 do
	    m--;		/* Search backwards      */
	 while ((m > ma) && (*m == '?'));	/* For first non-? char  */
	 if ((m > ma) ? ((*m == '*') && (m[-1] != QUOTE)) : (*m == '*'))
	    return MATCH;	/* nonquoted * = match   */
	 break;
      case WILDS:
	 do
	    m++;
	 while (*m == WILDS);	/* Zap redundant wilds   */
	 lsm = m;
	 lsn = n;		/* Save * fallback spot  */
	 match += sofar;
	 sofar = 0;
	 continue;		/* Save tally count      */
      case WILDQ:
	 m++;
	 n++;
	 continue;		/* Match one char        */
      case QUOTE:
	 m++;			/* Handle quoting        */
      }
      if (*m == *n) {		/* If matching           */
	 m++;
	 n++;
	 sofar++;
	 continue;		/* Tally the match       */
      }
      if (lsm) {		/* Try to fallback on *  */
	 n = ++lsn;
	 m = lsm;		/* Restore position      */
	 /* Used to test for (!*n) here but it wasn't necessary so it's gone */
	 sofar = 0;
	 continue;		/* Next char, please     */
      }
      return NOMATCH;		/* No fallbacks=No match */
   }
   while (*m == WILDS)
      m++;			/* Zap leftover *s       */
   return (*m) ? NOMATCH : MATCH;	/* End of both = match   */
}

static p_tcl_hash_list H_rcvd, H_sent;

static int builtin_sentrcvd STDVAR {
   Function F = (Function) cd;

   BADARGS(4, 4, " hand nick path");
   if (!check_validity(argv[0],builtin_sentrcvd)) {
      Tcl_AppendResult(irp, "bad builtin command call!", NULL);
      return TCL_ERROR; 
   }
   F(argv[1], argv[2],argv[3]);
   return TCL_OK;
}

void wipe_tmp_filename (char * fn, int idx)
{
   int i, ok = 1;
   if (!copy_to_tmp)
      return;
   for (i = 0; i < dcc_total; i++)
      if (i != idx)
	 if ((dcc[i].type == &DCC_GET) || (dcc[i].type == &DCC_GET_PENDING))
	    if (strcmp(dcc[i].u.xfer->filename, fn) == 0)
	       ok = 0;
   if (ok)
      unlink(fn);
}

/* given idx of a completed file operation, check to make sure no other
   file transfers are happening currently on that file -- if there aren't
   any, erase the file (it's just a copy anyway) */
static void wipe_tmp_file (int idx)
{
   wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
}

/* return true if this user has >= the maximum number of file xfers going */
int at_limit (char * nick)
{
   int i, x = 0;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == &DCC_GET) || (dcc[i].type == &DCC_GET_PENDING))
	 if (strcasecmp(dcc[i].nick, nick) == 0)
	    x++;
   return (x >= dcc_limit);
}

static int expmem_fileq()
{
   fileq_t *q = fileq;
   int tot = 0;
   modcontext;
   while (q != NULL) {
      tot += strlen(q->dir) + strlen(q->file) + 2 + sizeof(fileq_t);
      q = q->next;
   }
   return tot;
}

void queue_file (char * dir, char * file, char * from, char * to)
{
   fileq_t *q = fileq;
   fileq = (fileq_t *) modmalloc(sizeof(fileq_t));
   fileq->next = q;
   fileq->dir = (char *) modmalloc(strlen(dir) + 1);
   fileq->file = (char *) modmalloc(strlen(file) + 1);
   strcpy(fileq->dir, dir);
   strcpy(fileq->file, file);
   strcpy(fileq->nick, from);
   strcpy(fileq->to, to);
}

static void deq_this (fileq_t * this)
{
   fileq_t *q = fileq, *last = NULL;
   while ((q != this) && (q != NULL)) {
      last = q;
      q = q->next;
   }
   if (q == NULL)
      return;			/* bogus ptr */
   if (last != NULL)
      last->next = q->next;
   else
      fileq = q->next;
   modfree(q->dir);
   modfree(q->file);
   modfree(q);
}

/* remove all files queued to a certain user */
static void flush_fileq (char * to)
{
   fileq_t *q = fileq;
   int fnd = 1;
   while (fnd) {
      q = fileq;
      fnd = 0;
      while (q != NULL) {
	 if (strcasecmp(q->to, to) == 0) {
	    deq_this(q);
	    q = NULL;
	    fnd = 1;
	 }
	 if (q != NULL)
	    q = q->next;
      }
   }
}

static void send_next_file (char * to)
{
   fileq_t *q = fileq, *this = NULL;
   char s[256], s1[256];
   int x;
   while (q != NULL) {
      if (strcasecmp(q->to, to) == 0)
	 this = q;
      q = q->next;
   }
   if (this == NULL)
      return;			/* none */
   /* copy this file to /tmp */
   if (this->dir[0] == '*')	/* absolute path */
      sprintf(s, "%s/%s", &this->dir[1], this->file);
   else {
      char *p = strchr(this->dir, '*');
      if (p == NULL) {		/* if it's messed up */
	 send_next_file(to);
	 return;
      }
      p++;
      sprintf(s, "%s%s%s", p, p[0] ? "/" : "", this->file);
      strcpy(this->dir, &(p[atoi(this->dir)]));
   }
   if (copy_to_tmp) {
      sprintf(s1, "%s%s", tempdir, this->file);
      if (copyfile(s, s1) != 0) {
	 putlog(LOG_FILES | LOG_MISC, "*", "Refused dcc get %s: copy to %s FAILED!",
		this->file, tempdir);
	 modprintf(DP_HELP, "NOTICE %s :File system is broken; aborting queued files.\n",
		   this->to);
	 strcpy(s, this->to);
	 flush_fileq(s);
	 return;
      }
   } else
      strcpy(s1, s);
   if (this->dir[0] == '*')
      sprintf(s, "%s/%s", &this->dir[1], this->file);
   else
      sprintf(s, "%s%s%s", this->dir, this->dir[0] ? "/" : "", this->file);
   x = raw_dcc_send(s1, this->to, this->nick, s);
   if (x == 1) {
      wipe_tmp_filename(s1, -1);
      putlog(LOG_FILES, "*", "DCC connections full: GET %s [%s]", s1, this->nick);
      modprintf(DP_HELP, "NOTICE %s :DCC connections full; aborting queued files.\n",
		this->to);
      strcpy(s, this->to);
      flush_fileq(s);
      return;
   }
   if (x == 2) {
      wipe_tmp_filename(s1, -1);
      putlog(LOG_FILES, "*", "DCC socket error: GET %s [%s]", s1, this->nick);
      modprintf(DP_HELP, "NOTICE %s :DCC socket error; aborting queued files.\n",
		this->to);
      strcpy(s, this->to);
      flush_fileq(s);
      return;
   }
   if (strcasecmp(this->to, this->nick) != 0)
      modprintf(DP_HELP, "NOTICE %s :Here is a file from %s ...\n", this->to,
		this->nick);
   deq_this(this);
}

static void check_tcl_sent (char * hand, char * nick, char * path)
{
   struct flag_record fr = {0,0,0};
   
   modcontext;
   get_allattr_handle(hand,&fr);
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", nick, 0);
   Tcl_SetVar(interp, "_aa", path, 0);
   modcontext;
   check_tcl_bind(H_sent, hand, &fr, " $_n $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   modcontext;
}

static void check_tcl_rcvd (char * hand, char * nick, char * path)
{
   struct flag_record fr = {0,0,0};
   
   modcontext;
   get_allattr_handle(hand,&fr);
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", nick, 0);
   Tcl_SetVar(interp, "_aa", path, 0);
   modcontext;
   check_tcl_bind(H_rcvd, hand, &fr, " $_n $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   modcontext;
}

static void eof_dcc_send (int idx)
{
   int ok, j;
   char ofn[121], nfn[121], hand[41], s[161];

   modcontext;
   if (dcc[idx].u.xfer->length == dcc[idx].u.xfer->sent) {
      /* success */
      ok = 0;
      fclose(dcc[idx].u.xfer->f);
      if (strcmp(dcc[idx].nick, "*users") == 0) {
	 module_entry * me = module_find("share",0,0);
	 if (me && me->funcs[SHARE_FINISH]) 
	   ((me->funcs)[SHARE_FINISH])(idx);
	 killsock(dcc[idx].sock);
	 lostdcc(idx);
	 return;
      }
      putlog(LOG_FILES, "*", "Completed dcc send %s from %s!%s",
	     dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
      sprintf(s, "%s!%s", dcc[idx].nick, dcc[idx].host);
      get_handle_by_host(hand, s);
      /* move the file from /tmp */
      sprintf(ofn, "%s%s", tempdir, dcc[idx].u.xfer->filename);
      sprintf(nfn, "%s%s", dcc[idx].u.xfer->dir, dcc[idx].u.xfer->filename);
      if (movefile(ofn, nfn))
	 putlog(LOG_MISC | LOG_FILES, "*", "FAILED move %s from %s ! File lost!",
		dcc[idx].u.xfer->filename, tempdir);
      else {
	 /* add to file database */
	 module_entry *fs = module_find("filesys", 1, 1);
	 if (fs != NULL) {
	    Function f = fs->funcs[FILESYS_ADDFILE];
	    f(dcc[idx].u.xfer->dir, dcc[idx].u.xfer->filename, hand);
	 }
	 stats_add_upload(hand, dcc[idx].u.xfer->length);
	 check_tcl_rcvd(hand, dcc[idx].nick, nfn);
      }
      modcontext;
      for (j = 0; j < dcc_total; j++)
	 if ((!ok) && ((dcc[j].type == &DCC_CHAT) || 
		       (dcc[j].type == &DCC_FILES)) &&
	     (strcasecmp(dcc[j].nick, hand) == 0)) {
	    ok = 1;
	    modprintf(j, "Thanks for the file!\n");
	 }
      modcontext;
      if (!ok)
	 modprintf(DP_HELP, "NOTICE %s :Thanks for the file!\n",
		   dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
   }
   /* failure :( */
   fclose(dcc[idx].u.xfer->f);
   if (strcmp(dcc[idx].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	 if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
	     (dcc[x].type == &DCC_BOT))
	    y = x;
      if (y) {
	 putlog(LOG_MISC, "*", "Lost userfile transfer to %s; aborting.",
		dcc[y].nick);
	 unlink(dcc[idx].u.xfer->filename);
	 /* drop that bot */
	 modprintf(y, "bye\n");
	 tandout_but(y, "unlinked %s\n", dcc[y].nick);
	 tandout_but(y, "chat %s Disconnected %s (aborted userfile transfer)\n",
		     botnetnick, dcc[y].nick);
	 chatout("*** Disconnected %s (aborted userfile transfer)\n",
		 dcc[y].nick);
	 killsock(dcc[y].sock);
	 dcc[y].sock = (long)dcc[y].type;
	 dcc[y].type = &DCC_LOST;
      } else {
	 putlog(LOG_FILES, "*", "Lost dcc send %s from %s!%s (%lu/%lu)",
		dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host,
		dcc[idx].u.xfer->sent, dcc[idx].u.xfer->length);
	 sprintf(s, "%s%s", tempdir, dcc[idx].u.xfer->filename);
	 unlink(s);
      }
      killsock(dcc[idx].sock);
      lostdcc(idx);
   }
}

static void dcc_get (int idx, char * buf, int len)
{
   unsigned char bbuf[4],xnick[NICKLEN], *bf;
   unsigned long cmp, l;
   int w = len + dcc[idx].u.xfer->sofar, p = 0;

   modcontext;
   if (w < 4) {
      my_memcpy(&(dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar]),buf,len);
      dcc[idx].u.xfer->sofar += len;
      return;
   } else if (w == 4) {
      my_memcpy(bbuf,dcc[idx].u.xfer->buf,dcc[idx].u.xfer->sofar);
      my_memcpy(&(bbuf[dcc[idx].u.xfer->sofar]),buf,len);
   } else {      
      p = ((w-1) & ~3) - dcc[idx].u.xfer->sofar;
      w = w - ((w - 1) & ~3);
      if (w < 4) {
	 my_memcpy(dcc[idx].u.xfer->buf,&(buf[p]),w);
	 return;
      }
      my_memcpy(bbuf,&(buf[p]),w);
   } /* go back and read it again, it *does* make sense ;) */
   dcc[idx].u.xfer->sofar = 0;
   modcontext;
   /* this is more compatable than ntohl for machines where an int */
   /* is more than 4 bytes: */
   cmp = ((unsigned int) (bbuf[0]) << 24) + ((unsigned int) (bbuf[1]) << 16) +
       ((unsigned int) (bbuf[2]) << 8) + bbuf[3];
   dcc[idx].u.xfer->acked = cmp;
   if ((cmp > dcc[idx].u.xfer->sent) && (cmp <= dcc[idx].u.xfer->length)) {
      /* attempt to resume I guess */
      if (strcmp(dcc[idx].nick, "*users") == 0) {
	 putlog(LOG_MISC, "*", "!!! Trying to skip ahead on userfile transfer");
      } else {
	 fseek(dcc[idx].u.xfer->f, cmp, SEEK_SET);
	 dcc[idx].u.xfer->sent = cmp;
	 putlog(LOG_FILES, "*", "Resuming file transfer at %dk for %s to %s",
	   (int) (cmp / 1024), dcc[idx].u.xfer->filename, dcc[idx].nick);
      }
   }
   if (cmp != dcc[idx].u.xfer->sent)
      return;
   if (dcc[idx].u.xfer->sent == dcc[idx].u.xfer->length) {
      /* successful send, we're done */
      killsock(dcc[idx].sock);
      fclose(dcc[idx].u.xfer->f);
      if (strcmp(dcc[idx].nick, "*users") == 0) {
	 module_entry *me = module_find("share", 0, 0);
	 int x, y = 0;
	 for (x = 0; x < dcc_total; x++)
	    if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
		(dcc[x].type == &DCC_BOT))
	       y = x;
	 if (y != 0)
	    dcc[y].u.bot->status &= ~STAT_SENDING;
	 putlog(LOG_FILES, "*", "Completed userfile transfer to %s.",
		dcc[y].nick);
	 unlink(dcc[idx].u.xfer->filename);
	 /* any sharebot things that were queued: */
	 if (me && me->funcs[SHARE_DUMP_RESYNC]) 
	   ((me->funcs)[SHARE_DUMP_RESYNC])(dcc[y].sock,dcc[y].nick);
	 xnick[0] = 0;
      } else {
	 module_entry *fs = module_find("filesys", 1, 1);
	 check_tcl_sent(dcc[idx].u.xfer->from, dcc[idx].nick,
			dcc[idx].u.xfer->dir);
	 if (fs != NULL) {
	    Function f = fs->funcs[FILESYS_INCRGOTS];
	    f(dcc[idx].u.xfer->dir);
	 }
	 /* download is credited to the user who requested it */
	 /* (not the user who actually received it) */
	 stats_add_dnload(dcc[idx].u.xfer->from, dcc[idx].u.xfer->length);
	 putlog(LOG_FILES, "*", "Finished dcc send %s to %s",
		dcc[idx].u.xfer->filename, dcc[idx].nick);
	 wipe_tmp_file(idx);
	 strcpy((char *) xnick, dcc[idx].nick);
      }
      lostdcc(idx);
      /* any to dequeue? */
      if (!at_limit(xnick))
	 send_next_file(xnick);
      return;
   }
   modcontext;
   /* note: is this fseek necessary any more? */
/*    fseek(dcc[idx].u.xfer->f,dcc[idx].u.xfer->sent,0);   */
   l = dcc_block;
   if ((l == 0) || (dcc[idx].u.xfer->sent + l > dcc[idx].u.xfer->length))
      l = dcc[idx].u.xfer->length - dcc[idx].u.xfer->sent;
   bf = (unsigned char *) modmalloc(l + 1);
   fread(bf, l, 1, dcc[idx].u.xfer->f);
   tputs(dcc[idx].sock, bf, l);
   modfree(bf);
   dcc[idx].u.xfer->sent += l;
   dcc[idx].timeval = now;
}

static void eof_dcc_get (int idx)
{
   char xnick[NICKLEN];
   modcontext;
   fclose(dcc[idx].u.xfer->f);
   if (strcmp(dcc[idx].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	 if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
	     (dcc[x].type == &DCC_BOT))
	    y = x;
      putlog(LOG_MISC, "*", "Lost userfile transfer; aborting.");
      /* unlink(dcc[idx].u.xfer->filename); *//* <- already unlinked */
      xnick[0] = 0;
      /* drop that bot */
      modprintf(-dcc[y].sock, "bye\n");
      tandout_but(y, "unlinked %s\n", dcc[y].nick);
      tandout_but(y, "chat %s Disconnected %s (aborted userfile transfer)\n",
		  botnetnick, dcc[y].nick);
      chatout("*** Disconnected %s (aborted userfile transfer)\n", dcc[y].nick);
      killsock(dcc[y].sock);
      dcc[y].sock = (long)dcc[y].type;
      dcc[y].type = &DCC_LOST;
      return;
   } else {
      putlog(LOG_FILES, "*", "Lost dcc get %s from %s!%s",
	     dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
      wipe_tmp_file(idx);
      strcpy(xnick, dcc[idx].nick);
   }
   killsock(dcc[idx].sock);
   lostdcc(idx);
   /* send next queued file if there is one */
   if (!at_limit(xnick))
      send_next_file(xnick);
   modcontext;
}

static void dcc_get_pending (int idx, char * buf,int len)
{
   unsigned long ip;
   unsigned short port;
   int i;
   char *bf, s[UHOSTLEN];
   modcontext;
   i = answer(dcc[idx].sock, s, &ip, &port, 1);
   killsock(dcc[idx].sock);
   dcc[idx].sock = i;
   dcc[idx].addr = ip;
   dcc[idx].port = (int) port;
   if (dcc[idx].sock == -1) {
      neterror(s);
      modprintf(DP_HELP, "NOTICE %s :Bad connection (%s)\n", dcc[idx].nick, s);
      putlog(LOG_FILES, "*", "DCC bad connection: GET %s (%s!%s)",
	     dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
      lostdcc(idx);
      return;
   }
   /* file was already opened */
   if ((dcc_block == 0) || (dcc[idx].u.xfer->length < dcc_block))
      dcc[idx].u.xfer->sent = dcc[idx].u.xfer->length;
   else
      dcc[idx].u.xfer->sent = dcc_block;
   dcc[idx].type = &DCC_GET;
   bf = (char *) modmalloc(dcc[idx].u.xfer->sent + 1);
   fread(bf, dcc[idx].u.xfer->sent, 1, dcc[idx].u.xfer->f);
   tputs(dcc[idx].sock, bf, dcc[idx].u.xfer->sent);
   modfree(bf);
   dcc[idx].timeval = now;
   /* leave f open until file transfer is complete */
}

static void dcc_send (int idx, char * buf, int len)
{
   char s[512];
   unsigned long sent;
   modcontext;
   fwrite(buf, len, 1, dcc[idx].u.xfer->f);
   dcc[idx].u.xfer->sent += len;
   /* put in network byte order */
   sent = dcc[idx].u.xfer->sent;
   s[0] = (sent / (1 << 24));
   s[1] = (sent % (1 << 24)) / (1 << 16);
   s[2] = (sent % (1 << 16)) / (1 << 8);
   s[3] = (sent % (1 << 8));
   tputs(dcc[idx].sock, s, 4);
   dcc[idx].timeval = now;
   if ((dcc[idx].u.xfer->sent > dcc[idx].u.xfer->length) &&
       (dcc[idx].u.xfer->length > 0)) {
      modprintf(DP_HELP, "NOTICE %s :Bogus file length.\n", dcc[idx].nick);
      putlog(LOG_FILES, "*", "File too long: dropping dcc send %s from %s!%s",
	     dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
      fclose(dcc[idx].u.xfer->f);
      sprintf(s, "%s%s", tempdir, dcc[idx].u.xfer->filename);
      unlink(s);
      killsock(dcc[idx].sock);
      lostdcc(idx);
   }
}

static int tcl_getfileq STDVAR
{
   char s[512];
   fileq_t *q = fileq;
    BADARGS(2, 2, " handle");
   while (q != NULL) {
      if (strcasecmp(q->nick, argv[1]) == 0) {
	 if (q->dir[0] == '*')
	    sprintf(s, "%s %s/%s", q->to, &q->dir[1], q->file);
	 else
	    sprintf(s, "%s /%s%s%s", q->to, q->dir, q->dir[0] ? "/" : "", q->file);
	 Tcl_AppendElement(irp, s);
      }
      q = q->next;
   }
   return TCL_OK;
}

void show_queued_files (int idx)
{
   int i, cnt = 0;
   fileq_t *q = fileq;
   while (q != NULL) {
      if (strcasecmp(q->nick, dcc[idx].nick) == 0) {
	 if (!cnt) {
	    modprintf(idx, "  Send to    Filename\n");
	    modprintf(idx, "  ---------  --------------------\n");
	 }
	 cnt++;
	 if (q->dir[0] == '*')
	    modprintf(idx, "  %-9s  %s/%s\n", q->to, &q->dir[1], q->file);
	 else
	    modprintf(idx, "  %-9s  /%s%s%s\n", q->to, q->dir, q->dir[0] ? "/" : "",
		      q->file);
      }
      q = q->next;
   }
   for (i = 0; i < dcc_total; i++) {
      if (((dcc[i].type == &DCC_GET_PENDING) || (dcc[i].type == &DCC_GET)) &&
	  ((strcasecmp(dcc[i].nick, dcc[idx].nick) == 0) ||
	   (strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick) == 0))) {
	 char *nfn;
	 if (!cnt) {
	    modprintf(idx, "  Send to    Filename\n");
	    modprintf(idx, "  ---------  --------------------\n");
	 }
	 nfn = strrchr(dcc[i].u.xfer->filename, '/');
	 if (nfn == NULL)
	    nfn = dcc[i].u.xfer->filename;
	 else
	   nfn++;
	 cnt++;
	 if (dcc[i].type == &DCC_GET_PENDING)
	    modprintf(idx, "  %-9s  %s  [WAITING]\n", dcc[i].nick, nfn);
	 else
	    modprintf(idx, "  %-9s  %s  (%.1f%% done)\n", dcc[i].nick, nfn,
		      (100.0 * ((float) dcc[i].u.xfer->sent /
				(float) dcc[i].u.xfer->length)));
      }
   }
   if (!cnt)
      modprintf(idx, "No files queued up.\n");
   else
      modprintf(idx, "Total: %d\n", cnt);
}

void fileq_cancel (int idx, char * par)
{
   int fnd = 1, matches = 0, atot = 0, i;
   fileq_t *q;
   char s[256];
   while (fnd) {
      q = fileq;
      fnd = 0;
      while (q != NULL) {
	 if (strcasecmp(dcc[idx].nick, q->nick) == 0) {
	    if (q->dir[0] == '*')
	       sprintf(s, "%s/%s", &q->dir[1], q->file);
	    else
	       sprintf(s, "/%s%s%s", q->dir, q->dir[0] ? "/" : "", q->file);
	    if (wild_match_file(par, s)) {
	       modprintf(idx, "Cancelled: %s to %s\n", s, q->to);
	       fnd = 1;
	       deq_this(q);
	       q = NULL;
	       matches++;
	    }
	    if ((!fnd) && (wild_match_file(par, q->file))) {
	       modprintf(idx, "Cancelled: %s to %s\n", s, q->to);
	       fnd = 1;
	       deq_this(q);
	       q = NULL;
	       matches++;
	    }
	 }
	 if (q != NULL)
	    q = q->next;
      }
   }
   for (i = 0; i < dcc_total; i++) {
      if (((dcc[i].type == &DCC_GET_PENDING) || (dcc[i].type == &DCC_GET)) &&
	  ((strcasecmp(dcc[i].nick, dcc[idx].nick) == 0) ||
	   (strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick) == 0))) {
	 char *nfn = strrchr(dcc[i].u.xfer->filename, '/');
	 if (nfn == NULL)
	    nfn = dcc[i].u.xfer->filename;
	 else
	    nfn++;
	 if (wild_match_file(par, nfn)) {
	    modprintf(idx, "Cancelled: %s  (aborted dcc send)\n", nfn);
	    if (strcasecmp(dcc[i].nick, dcc[idx].nick) != 0)
	       modprintf(DP_HELP, "NOTICE %s :Transfer of %s aborted by %s\n", dcc[i].nick,
			 nfn, dcc[idx].nick);
	    if (dcc[i].type == &DCC_GET)
	       putlog(LOG_FILES, "*", "DCC cancel: GET %s (%s) at %lu/%lu", nfn,
		dcc[i].nick, dcc[i].u.xfer->sent, dcc[i].u.xfer->length);
	    wipe_tmp_file(i);
	    atot++;
	    matches++;
	    killsock(dcc[i].sock);
	    lostdcc(i);
	    i--;
	 }
      }
   }
   if (!matches)
      modprintf(idx, "No matches.\n");
   else
      modprintf(idx, "Cancelled %d file%s.\n", matches, matches > 1 ? "s" : "");
   for (i = 0; i < atot; i++)
      if (!at_limit(dcc[idx].nick))
	 send_next_file(dcc[idx].nick);
}

#define DCCSEND_OK     0
#define DCCSEND_FULL   1	/* dcc table is full */
#define DCCSEND_NOSOCK 2	/* can't open a listening socket */
#define DCCSEND_BADFN  3	/* no such file */

int raw_dcc_send (char * filename, char * nick, char * from, char * dir)
{
   int zz, port, i;
   char *nfn;
   IP host;
   struct stat ss;
   modcontext;
   if ((i = new_dcc(&DCC_GET_PENDING,sizeof(struct xfer_info))) == -1)
      return DCCSEND_FULL;
   port = reserved_port;
   zz = open_listen(&port);
   if (zz == (-1)) {
      lostdcc(i);
      return DCCSEND_NOSOCK;
   }
   nfn = strrchr(filename, '/');
   if (nfn == NULL)
      nfn = filename;
   else
      nfn++;
   host = getmyip();
   stat(filename, &ss);
   dcc[i].sock = zz;
   dcc[i].addr = (IP) (-559026163);
   dcc[i].port = port;
   strcpy(dcc[i].nick, nick);
   strcpy(dcc[i].host, "irc");
   strcpy(dcc[i].u.xfer->filename, filename);
   strcpy(dcc[i].u.xfer->from, from);
   strcpy(dcc[i].u.xfer->dir, dir);
   dcc[i].u.xfer->length = ss.st_size;
   dcc[i].u.xfer->sent = 0;
   dcc[i].u.xfer->sofar = 0;
   dcc[i].u.xfer->acked = 0;
   dcc[i].timeval = now;
   dcc[i].u.xfer->f = fopen(filename, "r");
   if (dcc[i].u.xfer->f == NULL) {
      lostdcc(i);
      return DCCSEND_BADFN;
   }
   if (nick[0] != '*') {
#ifndef NO_IRC
      modprintf(DP_HELP, "PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n", nick, nfn,
		iptolong(natip[0]?(IP) inet_addr(natip):iptolong(host)),
		port, ss.st_size);
#endif
      putlog(LOG_FILES, "*", "Begin DCC send %s to %s", nfn, nick);
   }
   return DCCSEND_OK;
}

static int tcl_dccsend STDVAR
{
   char s[5], sys[512], *nfn;
   int i;
   FILE *f;
    BADARGS(3, 3, " filename ircnick");
    f = fopen(argv[1], "r");
   if (f == NULL) {
      /* file not found */
      Tcl_AppendResult(irp, "3", NULL);
      return TCL_OK;
   }
   fclose(f);
   nfn = strrchr(argv[1], '/');
   if (nfn == NULL)
      nfn = argv[1];
   else
      nfn++;
   if (at_limit(argv[2])) {
      /* queue that mother */
      if (nfn == argv[1])
	 queue_file("*", nfn, "(script)", argv[2]);
      else {
	 nfn--;
	 *nfn = 0;
	 nfn++;
	 sprintf(sys, "*%s", argv[1]);
	 queue_file(sys, nfn, "(script)", argv[2]);
      }
      Tcl_AppendResult(irp, "4", NULL);
      return TCL_OK;
   }
   if (copy_to_tmp) {
      sprintf(sys, "%s%s", tempdir, nfn);	/* new filename, in /tmp */
      copyfile(argv[1], sys);
   } else
      strcpy(sys, argv[1]);
   i = raw_dcc_send(sys, argv[2], "*", argv[1]);
   if (i > 0)
      wipe_tmp_filename(sys, -1);
   sprintf(s, "%d", i);
   Tcl_AppendResult(irp, s, NULL);
   return TCL_OK;
}

static tcl_cmds mytcls[] =
{
   {"dccsend", tcl_dccsend},
   {"getfileq", tcl_getfileq},
   {0, 0}
};

static void transfer_get_timeout (int i)
{
   char xx[1024];

   modcontext;
   if (strcmp(dcc[i].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	if ((strcasecmp(dcc[x].nick, dcc[i].host) == 0) &&
	    (dcc[x].type == &DCC_BOT))
	  y = x;
      if (y != 0) {
	 dcc[y].u.bot->status &= ~STAT_SENDING;
	 dcc[y].u.bot->status &= ~STAT_SHARE;
      }
      unlink(dcc[i].u.xfer->filename);
      putlog(LOG_MISC, "*", "Timeout on userfile transfer.");
      modprintf(y, "bye\n");
      tandout_but(y, "unlinked %s\n", dcc[y].nick);
      tandout_but(y, "chat %s Disconnected %s (timed-out userfile transfer)\n",
		  botnetnick, dcc[y].nick);
      chatout("*** Disconnected %s (timed-out userfile transfer)\n", dcc[y].nick);
      killsock(dcc[y].sock);
      dcc[y].sock = (long)dcc[y].type;
      dcc[y].type = &DCC_LOST;
      xx[0] = 0;
   } else {
      char *p;
      strcpy(xx, dcc[i].u.xfer->filename);
      p = strrchr(xx, '/');
      modprintf(DP_HELP, "NOTICE %s :Timeout during transfer, aborting %s.\n",
		dcc[i].nick, p ? p + 1 : xx);
      putlog(LOG_FILES, "*", "DCC timeout: GET %s (%s) at %lu/%lu", p ? p + 1 : xx,
	     dcc[i].nick, dcc[i].u.xfer->sent, dcc[i].u.xfer->length);
      wipe_tmp_file(i);
      strcpy(xx, dcc[i].nick);
   }
   killsock(dcc[i].sock);
   lostdcc(i);
   if (!at_limit(xx))
     send_next_file(xx);
}

static void transfer_cont_got_dcc (int idx,char * x,int y)
{
   char s1[121];

   if (dcc[idx].type != &DCC_FORK_SEND)
      return;
   dcc[idx].type = &DCC_SEND;
   sprintf(s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
   if (strcmp(dcc[idx].nick, "*users") != 0) {
      putlog(LOG_MISC, "*", "DCC connection: SEND %s (%s)",
	     dcc[idx].type == &DCC_SEND ? dcc[idx].u.xfer->filename : "",
	     s1);
   }
}

static void tout_dcc_send (int idx) {
   if (strcmp(dcc[idx].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
	    (dcc[x].type == &DCC_BOT))
	  y = x;
      if (y != 0) {
	 dcc[y].u.bot->status &= ~STAT_GETTING;
	 dcc[y].u.bot->status &= ~STAT_SHARE;
      }
      unlink(dcc[idx].u.xfer->filename);
      putlog(LOG_MISC, "*", "Timeout on userfile transfer.");
   } else {
      char xx[1024];
      modprintf(DP_HELP, "NOTICE %s :Timeout during transfer, aborting %s.\n",
		dcc[idx].nick, dcc[idx].u.xfer->filename);
      putlog(LOG_FILES, "*", "DCC timeout: SEND %s (%s) at %lu/%lu",
	     dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].u.xfer->sent,
	     dcc[idx].u.xfer->length);
      sprintf(xx, "%s%s", tempdir, dcc[idx].u.xfer->filename);
      unlink(xx);
   }
   killsock(dcc[idx].sock);
   lostdcc(idx);
}

static void display_dcc_get (int idx,char * buf) {
   if (dcc[idx].u.xfer->sent == dcc[idx].u.xfer->length)
     sprintf(buf,"send  (%lu)/%lu\n    Filename: %s\n",dcc[idx].u.xfer->acked,
	     dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
   else
     sprintf(buf,"send  %lu/%lu\n    Filename: %s\n",dcc[idx].u.xfer->sent,
	     dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
}

static void display_dcc_get_p (int idx,char * buf) {
   sprintf(buf,"send  waited %lus    Filename: %s\n",now - dcc[idx].timeval,
	   dcc[idx].u.xfer->filename);
}

static void display_dcc_send (int idx,char * buf) {
   sprintf(buf,"send  %lu/%lu\n    Filename: %s\n",dcc[idx].u.xfer->sent,
	   dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
}

static void display_dcc_fork_send (int idx,char * buf) {
   sprintf(buf,"conn  send");
}

static int expmem_dcc_xfer (int idx) {
   return sizeof(struct xfer_info);
}

static void kill_dcc_xfer (int idx) {
   modfree(dcc[idx].u.xfer);
}
   
static void out_dcc_xfer (int idx, char * buf) {
}
   
static struct dcc_table transfer_send_table = {
   eof_dcc_send,
   dcc_send,
   &wait_dcc_xfer,
   tout_dcc_send,
   display_dcc_send,
   expmem_dcc_xfer,
   kill_dcc_xfer,
   out_dcc_xfer
};

static struct dcc_table transfer_fork_send_table = {
   0,
   transfer_cont_got_dcc,
   &wait_dcc_xfer,
   0,
   display_dcc_fork_send,
   expmem_dcc_xfer,
   kill_dcc_xfer,
   out_dcc_xfer
};

static struct dcc_table transfer_get_table = {
   eof_dcc_get,
   dcc_get,
   &wait_dcc_xfer,
   transfer_get_timeout,
   display_dcc_get,
   expmem_dcc_xfer,
   kill_dcc_xfer,
   out_dcc_xfer
};

static struct dcc_table transfer_getp_table = {
   eof_dcc_get,
   dcc_get_pending,
   &wait_dcc_xfer,
   transfer_get_timeout,
   display_dcc_get_p,
   expmem_dcc_xfer,
   kill_dcc_xfer,
   out_dcc_xfer
};

   
static tcl_ints myints[] =
{
   {"max-dloads", &dcc_limit},
   {"dcc-block", &dcc_block},
   {"copy-to-tmp", &copy_to_tmp},
   {"xfer-timeout", &wait_dcc_xfer},
   {0, 0}
};

static char *transfer_close()
{
   int i;

   modcontext;
   putlog(LOG_MISC, "*", "Unloading transfer module, killing all transfer connections..");
   modcontext;
   for (i = dcc_total - 1; i >= 0; i--) {
     if ((dcc[i].type == &DCC_GET) || (dcc[i].type == &DCC_GET_PENDING))
	eof_dcc_get(i);
      else if (dcc[i].type == &DCC_SEND)
	eof_dcc_send(i);
      else if (dcc[i].type == &DCC_FORK_SEND) 
	DCC_FORK_CHAT.eof(i);
   }
   modcontext;
   while (fileq != NULL) {
      deq_this(fileq);
   }
   modcontext;
   del_hash_table(H_rcvd);
   del_hash_table(H_sent);
   rem_tcl_commands(mytcls);
   rem_tcl_ints(myints);
   memset(&DCC_GET,0,sizeof(struct dcc_table));
   memset(&DCC_GET_PENDING,0,sizeof(struct dcc_table));
   memset(&DCC_SEND,0,sizeof(struct dcc_table));
   memset(&DCC_FORK_SEND,0,sizeof(struct dcc_table));
   module_undepend(MODULE_NAME);
   modcontext;
   return NULL;
}

static int transfer_expmem()
{
   return expmem_fileq();
}

static void transfer_report (int idx)
{
   modprintf(idx, "    DCC block is %d%s, max concurrent d/ls is %d\n", dcc_block,
	     (dcc_block == 0) ? " (turbo dcc)" : "", dcc_limit);
   modprintf(idx, "    Using %d bytes of memory\n", transfer_expmem());
}

char *transfer_start ();

static Function transfer_table[] =
{
   (Function) transfer_start,
   (Function) transfer_close,
   (Function) transfer_expmem,
   (Function) transfer_report,
};

char *transfer_start ()
{
   module_register(MODULE_NAME, transfer_table, 1, 1);
   module_depend(MODULE_NAME, "eggdrop", 102, 0);
   add_tcl_commands(mytcls);
   add_tcl_ints(myints);
   H_rcvd = add_hash_table("rcvd",HT_STACKABLE,builtin_sentrcvd);
   H_sent = add_hash_table("sent",HT_STACKABLE,builtin_sentrcvd);
   /* more bodges :/ */
   memcpy(&DCC_GET,&transfer_get_table,sizeof(struct dcc_table));
   memcpy(&DCC_GET_PENDING,&transfer_getp_table,sizeof(struct dcc_table));
   memcpy(&DCC_SEND,&transfer_send_table,sizeof(struct dcc_table));
   memcpy(&DCC_FORK_SEND,&transfer_fork_send_table,sizeof(struct dcc_table));
   DCC_FORK_SEND.eof = DCC_FORK_CHAT.eof;
   DCC_FORK_SEND.timeout = DCC_FORK_CHAT.timeout;
   return NULL;
}
