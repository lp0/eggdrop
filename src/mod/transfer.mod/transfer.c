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
#include "../../files.h"
#include "../../users.h"
#include "../../cmdt.h"
#include <sys/stat.h>
#ifdef HAVE_NAT
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifndef NO_FILE_SYSTEM
/* copy files to /tmp before transmitting? */
#ifdef MODULES
static
#endif
int copy_to_tmp = 1;
/* timeout time on DCC xfers */
#ifdef MODULES
static
#endif
int wait_dcc_xfer = 300;
/* maximum number of simultaneous file downloads allowed */
#ifdef MODULES
static
#endif
int dcc_limit = 3;
#ifdef MODULES
static
#endif
int dcc_block = 1024;

typedef struct zarrf {
   char *dir;			/* starts with '*' -> absolute dir */
   char *file;			/*    (otherwise -> dccdir) */
   char nick[NICKLEN];		/* who queued this file */
   char to[NICKLEN];		/* who will it be sent to */
   struct zarrf *next;
} fileq_t;


static fileq_t *fileq = NULL;

#ifdef MODULES
#define raw_dcc_send transfer_dcc_send
static int transfer_dcc_send PROTO((char *, char *, char *, char *));

static
#endif
void wipe_tmp_filename PROTO2(char *, fn, int, idx)
{
   int i, ok = 1;
   if (!copy_to_tmp)
      return;
   for (i = 0; i < dcc_total; i++)
      if (i != idx)
	 if ((dcc[i].type == DCC_GET) || (dcc[i].type == DCC_GET_PENDING))
	    if (strcmp(dcc[i].u.xfer->filename, fn) == 0)
	       ok = 0;
   if (ok)
      unlink(fn);
}

/* given idx of a completed file operation, check to make sure no other
   file transfers are happening currently on that file -- if there aren't
   any, erase the file (it's just a copy anyway) */
#ifdef MODULES
static
#endif
void wipe_tmp_file PROTO1(int, idx)
{
   wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
}

/* return true if this user has >= the maximum number of file xfers going */
#ifdef MODULES
static
#endif
int at_limit PROTO1(char *, nick)
{
   int i, x = 0;
   for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type == DCC_GET) || (dcc[i].type == DCC_GET_PENDING))
	 if (strcasecmp(dcc[i].nick, nick) == 0)
	    x++;
   return (x >= dcc_limit);
}

#ifdef MODULES
static
#endif
int expmem_fileq()
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

#ifdef MODULES
static
#endif
void queue_file PROTO4(char *, dir, char *, file, char *, from, char *, to)
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

#ifdef MODULES
static
#endif
void deq_this PROTO1(fileq_t *, this)
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
#ifdef MODULES
static
#endif
void flush_fileq PROTO1(char *, to)
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

#ifdef MODULES
static
#endif
void send_next_file PROTO1(char *, to)
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

#ifdef MODULES
static
#endif
void check_tcl_sent PROTO3(char *, hand, char *, nick, char *, path)
{
   int atr;
   modcontext;
   atr = get_allattr_handle(hand);
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", nick, 0);
   Tcl_SetVar(interp, "_aa", path, 0);
   modcontext;
   check_tcl_bind(&H_sent, hand, atr, " $_n $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   modcontext;
}

#ifdef MODULES
static
#endif
void check_tcl_rcvd PROTO3(char *, hand, char *, nick, char *, path)
{
   int atr;
   modcontext;
   atr = get_allattr_handle(hand);
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", nick, 0);
   Tcl_SetVar(interp, "_aa", path, 0);
   modcontext;
   check_tcl_bind(&H_rcvd, hand, atr, " $_n $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   modcontext;
}

#ifdef MODULES
static
#endif
void eof_dcc_send PROTO1(int, idx)
{
   int ok, j;
   char ofn[121], nfn[121], hand[41], s[161];

   modcontext;
   if (dcc[idx].u.xfer->length == dcc[idx].u.xfer->sent) {
      /* success */
      ok = 0;
      fclose(dcc[idx].u.xfer->f);
      if (strcmp(dcc[idx].nick, "*users") == 0) {
	 finish_share(idx);
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
#ifdef MODULES
	 module_entry *fs = module_find("filesys", 1, 1);
	 if (fs != NULL) {
	    Function f = fs->funcs[FILESYS_ADDFILE];
	    f(dcc[idx].u.xfer->dir, dcc[idx].u.xfer->filename, hand);
	 }
#else
	 add_file(dcc[idx].u.xfer->dir, dcc[idx].u.xfer->filename, hand);
#endif
	 stats_add_upload(hand, dcc[idx].u.xfer->length);
	 check_tcl_rcvd(hand, dcc[idx].nick, nfn);
      }
      modcontext;
      for (j = 0; j < dcc_total; j++)
	 if ((!ok) && ((dcc[j].type == DCC_CHAT) || (dcc[j].type == DCC_FILES)) &&
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
	     (dcc[x].type == DCC_BOT))
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
	 cancel_user_xfer(y);
	 killsock(dcc[y].sock);
	 dcc[y].sock = dcc[y].type;
	 dcc[y].type = DCC_LOST;
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

#ifdef MODULES
static
#endif
void dcc_get PROTO3(int, idx, char *, buf, int, len)
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
	 int x, y = 0;
	 for (x = 0; x < dcc_total; x++)
	    if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
		(dcc[x].type == DCC_BOT))
	       y = x;
	 if (y != 0)
	    dcc[y].u.bot->status &= ~STAT_SENDING;
	 putlog(LOG_FILES, "*", "Completed userfile transfer to %s.",
		dcc[y].nick);
	 unlink(dcc[idx].u.xfer->filename);
	 /* any sharebot things that were queued: */
	 dump_resync(dcc[y].sock, dcc[y].nick);
	 xnick[0] = 0;
      } else {
	 check_tcl_sent(dcc[idx].u.xfer->from, dcc[idx].nick,
			dcc[idx].u.xfer->dir);
#ifdef MODULES
	 {
	    module_entry *fs = module_find("filesys", 1, 1);
	    if (fs != NULL) {
	       Function f = fs->funcs[FILESYS_INCRGOTS];
	       f(dcc[idx].u.xfer->dir);
	    }
	 }
#else
	 incr_file_gots(dcc[idx].u.xfer->dir);
#endif
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
   dcc[idx].u.xfer->pending = time(NULL);
}

#ifdef MODULES
static
#endif
void eof_dcc_get PROTO1(int, idx)
{
   char xnick[NICKLEN];
   modcontext;
   fclose(dcc[idx].u.xfer->f);
   if (strcmp(dcc[idx].nick, "*users") == 0) {
      int x, y = 0;
      for (x = 0; x < dcc_total; x++)
	 if ((strcasecmp(dcc[x].nick, dcc[idx].host) == 0) &&
	     (dcc[x].type == DCC_BOT))
	    y = x;
      putlog(LOG_MISC, "*", "Lost userfile transfer; aborting.");
      /* unlink(dcc[idx].u.xfer->filename); *//* <- already unlinked */
      flush_tbuf(dcc[y].nick);
      xnick[0] = 0;
      /* drop that bot */
      modprintf(-dcc[y].sock, "bye\n");
      tandout_but(y, "unlinked %s\n", dcc[y].nick);
      tandout_but(y, "chat %s Disconnected %s (aborted userfile transfer)\n",
		  botnetnick, dcc[y].nick);
      chatout("*** Disconnected %s (aborted userfile transfer)\n", dcc[y].nick);
      cancel_user_xfer(y);
      killsock(dcc[y].sock);
      dcc[y].sock = dcc[y].type;
      dcc[y].type = DCC_LOST;
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

#ifdef MODULES
static
#endif
void dcc_get_pending PROTO2(int, idx, char *, buf)
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
   dcc[idx].type = DCC_GET;
   bf = (char *) modmalloc(dcc[idx].u.xfer->sent + 1);
   fread(bf, dcc[idx].u.xfer->sent, 1, dcc[idx].u.xfer->f);
   tputs(dcc[idx].sock, bf, dcc[idx].u.xfer->sent);
   modfree(bf);
   dcc[idx].u.xfer->pending = time(NULL);
   /* leave f open until file transfer is complete */
}

#ifdef MODULES
static
#endif
void dcc_send PROTO3(int, idx, char *, buf, int, len)
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
   dcc[idx].u.xfer->pending = time(NULL);
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

#ifdef MODULES
static int transfer_activity_hook PROTO3(int, idx, char *, buf, int, len)
{
   modcontext;
   switch (dcc[idx].type) {
   case DCC_GET_PENDING:
      modcontext;
      dcc_get_pending(idx, buf);
      return 1;
   case DCC_SEND:
      modcontext;
      dcc_send(idx, buf, len);
      return 1;
   case DCC_GET:
      modcontext;
      dcc_get(idx, buf, len);
      return 1;
   }
   return 0;
}

static int transfer_eof_hook PROTO1(int, idx)
{
   modcontext;
   switch (dcc[idx].type) {
   case DCC_SEND:
      eof_dcc_send(idx);
      return 1;
   case DCC_GET_PENDING:
   case DCC_GET:
      eof_dcc_get(idx);
      return 1;
   }
   return 0;
}
#endif

#ifdef MODULES
static
#endif
int tcl_getfileq STDVAR
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

#ifdef MODULES
static
#endif
void show_queued_files PROTO1(int, idx)
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
      if (((dcc[i].type == DCC_GET_PENDING) || (dcc[i].type == DCC_GET)) &&
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
	 if (dcc[i].type == DCC_GET_PENDING)
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

#ifdef MODULES
static
#endif
void fileq_cancel PROTO2(int, idx, char *, par)
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
      if (((dcc[i].type == DCC_GET_PENDING) || (dcc[i].type == DCC_GET)) &&
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
	    if (dcc[i].type == DCC_GET)
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

#ifdef MODULES
static
#endif
int raw_dcc_send PROTO4(char *, filename, char *, nick, char *, from, char *, dir)
{
   int zz, port, i;
   char *nfn;
   IP host;
   struct stat ss;
   modcontext;
   if ((i = new_dcc(DCC_GET_PENDING)) == -1)
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
   dcc[i].u.xfer->pending = time(NULL);
   dcc[i].u.xfer->f = fopen(filename, "r");
   if (dcc[i].u.xfer->f == NULL) {
      lostdcc(i);
      return DCCSEND_BADFN;
   }
   if (nick[0] != '*') {
#ifndef NO_IRC
#ifdef HAVE_NAT
      modprintf(DP_HELP, "PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n", nick, nfn,
		iptolong((IP) inet_addr(natip)), port, ss.st_size);
#else
      modprintf(DP_HELP, "PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n", nick, nfn,
		iptolong(host), port, ss.st_size);
#endif
#endif
      putlog(LOG_FILES, "*", "Begin DCC send %s to %s", nfn, nick);
   }
   return DCCSEND_OK;
}

#ifdef MODULES
static
#endif
int tcl_dccsend STDVAR
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

#ifdef MODULES
static tcl_cmds mytcls[] =
{
   {"dccsend", tcl_dccsend},
   {"getfileq", tcl_getfileq},
   {0, 0}
};
#endif

#ifdef MODULES
static int transfer_timeout PROTO1(int, i)
{
   time_t now = time(NULL);
   char xx[1024];

   modcontext;
   switch (dcc[i].type) {
   case DCC_GET_PENDING:
   case DCC_GET:
      if (now - dcc[i].u.xfer->pending > wait_dcc_xfer) {
	 if (strcmp(dcc[i].nick, "*users") == 0) {
	    int x, y = 0;
	    for (x = 0; x < dcc_total; x++)
	       if ((strcasecmp(dcc[x].nick, dcc[i].host) == 0) &&
		   (dcc[x].type == DCC_BOT))
		  y = x;
	    if (y != 0) {
	       dcc[y].u.bot->status &= ~STAT_SENDING;
	       dcc[y].u.bot->status &= ~STAT_SHARE;
	    }
	    unlink(dcc[i].u.xfer->filename);
	    flush_tbuf(dcc[y].nick);
	    putlog(LOG_MISC, "*", "Timeout on userfile transfer.");
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
	 i--;
	 if (!at_limit(xx))
	    send_next_file(xx);
      }
      return 1;
   case DCC_SEND:
      if (now - dcc[i].u.xfer->pending > wait_dcc_xfer) {
	 if (strcmp(dcc[i].nick, "*users") == 0) {
	    int x, y = 0;
	    for (x = 0; x < dcc_total; x++)
	       if ((strcasecmp(dcc[x].nick, dcc[i].host) == 0) &&
		   (dcc[x].type == DCC_BOT))
		  y = x;
	    if (y != 0) {
	       dcc[y].u.bot->status &= ~STAT_GETTING;
	       dcc[y].u.bot->status &= ~STAT_SHARE;
	    }
	    unlink(dcc[i].u.xfer->filename);
	    putlog(LOG_MISC, "*", "Timeout on userfile transfer.");
	 } else {
	    modprintf(DP_HELP, "NOTICE %s :Timeout during transfer, aborting %s.\n",
		      dcc[i].nick, dcc[i].u.xfer->filename);
	    putlog(LOG_FILES, "*", "DCC timeout: SEND %s (%s) at %lu/%lu",
	       dcc[i].u.xfer->filename, dcc[i].nick, dcc[i].u.xfer->sent,
		   dcc[i].u.xfer->length);
	    sprintf(xx, "%s%s", tempdir, dcc[i].u.xfer->filename);
	    unlink(xx);
	 }
	 killsock(dcc[i].sock);
	 lostdcc(i);
	 i--;
      }
      return 1;
   }
   return 0;
}

static int transfer_cont_got_dcc PROTO1(int, idx)
{
   char s1[121];

   if (dcc[idx].type != DCC_SEND)
      return 0;
   sprintf(s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
   if (strcmp(dcc[idx].nick, "*users") != 0) {
      putlog(LOG_MISC, "*", "DCC connection: SEND %s (%s)",
	     dcc[idx].type == DCC_SEND ? dcc[idx].u.xfer->filename : "",
	     s1);
   }
   return 1;
}

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
   for (i = dcc_total - 1; i >= 0; i--)
      transfer_eof_hook(i);
   modcontext;
   while (fileq != NULL) {
      deq_this(fileq);
   }
   modcontext;
   del_hook(HOOK_ACTIVITY, transfer_activity_hook);
   del_hook(HOOK_EOF, transfer_eof_hook);
   del_hook(HOOK_TIMEOUT, transfer_timeout);
   del_hook(HOOK_CONNECT, transfer_cont_got_dcc);
   rem_tcl_commands(mytcls);
   rem_tcl_ints(myints);
   module_undepend(MODULE_NAME);
   modcontext;
   return NULL;
}

static int transfer_expmem()
{
   return expmem_fileq();
}

static void transfer_report PROTO1(int, idx)
{
   modprintf(idx, "   DCC block is %d%s, max concurrent d/ls is %d\n", dcc_block,
	     (dcc_block == 0) ? " (turbo dcc)" : "", dcc_limit);
   modprintf(idx, "   Using %d bytes of memory\n", transfer_expmem());
}

Function *global;
char *transfer_start PROTO((Function *));

static Function transfer_table[] =
{
   (Function) transfer_start,
   (Function) transfer_close,
   (Function) transfer_expmem,
   (Function) transfer_report,

   (Function) raw_dcc_send,
   (Function) fileq_cancel,
   (Function) at_limit,
   (Function) queue_file,

   (Function) show_queued_files,
   (Function) & copy_to_tmp,
   (Function) wipe_tmp_filename,
};

char *transfer_start PROTO1(Function *, egg_func_table)
{
   global = egg_func_table;
   module_register(MODULE_NAME, transfer_table, 1, 1);
   module_depend(MODULE_NAME, "eggdrop", 101, 4);
   add_hook(HOOK_ACTIVITY, transfer_activity_hook);
   add_hook(HOOK_EOF, transfer_eof_hook);
   add_hook(HOOK_TIMEOUT, transfer_timeout);
   add_hook(HOOK_CONNECT, transfer_cont_got_dcc);
   add_tcl_commands(mytcls);
   add_tcl_ints(myints);
   return NULL;
}
#endif

#endif
