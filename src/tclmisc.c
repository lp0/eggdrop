
/*
   tclmisc.c -- handles:
   Tcl stubs for file system commands
   Tcl stubs for everything else

   dprintf'ized, 1aug96
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
#include "eggdrop.h"
#include "proto.h"
#include "cmdt.h"
#include "tclegg.h"
#include "files.h"

/* eggdrop always uses the same interpreter */
extern Tcl_Interp *interp;
extern int serv;
extern tcl_timer_t *timer, *utimer;
extern struct dcc_t dcc[];
extern int dcc_total;
extern char dccdir[];

/***********************************************************************/

int tcl_putserv STDVAR
{
   char s[512], *p;
    BADARGS(2, 2, " text");
    strncpy(s, argv[1], 511);
    s[511] = 0;
    p = strchr(s, '\n');
   if (p != NULL)
      *p = 0;
    p = strchr(s, '\r');
   if (p != NULL)
      *p = 0;
    mprintf(serv, "%s\n", s);
    return TCL_OK;
}

int tcl_puthelp STDVAR
{
   char s[512], *p;
    BADARGS(2, 2, " text");
    strncpy(s, argv[1], 511);
    s[511] = 0;
    p = strchr(s, '\n');
   if (p != NULL)
      *p = 0;
    p = strchr(s, '\r');
   if (p != NULL)
      *p = 0;
    hprintf(serv, "%s\n", s);
    return TCL_OK;
}

int tcl_putlog STDVAR
{
   char logtext[501];
    BADARGS(2, 2, " text");
    strncpy(logtext, argv[1], 500);
    logtext[500] = 0;
    putlog(LOG_MISC, "*", "%s", logtext);
    return TCL_OK;
}

int tcl_putcmdlog STDVAR
{
   char logtext[501];
    BADARGS(2, 2, " text");
    strncpy(logtext, argv[1], 500);
    logtext[500] = 0;
    putlog(LOG_CMDS, "*", "%s", logtext);
    return TCL_OK;
}

int tcl_putxferlog STDVAR
{
   char logtext[501];
    BADARGS(2, 2, " text");
    strncpy(logtext, argv[1], 500);
    logtext[500] = 0;
    putlog(LOG_FILES, "*", "%s", logtext);
    return TCL_OK;
}

int tcl_putloglev STDVAR
{
   int lev = 0;
   char logtext[501];
    BADARGS(4, 4, " level channel text");
    lev = logmodes(argv[1]);
   if (lev == 0) {
      Tcl_AppendResult(irp, "no valid log-level given", NULL);
      return TCL_ERROR;
   }
   strncpy(logtext, argv[3], 500);
   logtext[500] = 0;
   putlog(lev, argv[2], "%s", logtext);
   return TCL_OK;
}

int tcl_bind STDVAR
{
   int fl, tp;
   if ((long int) cd == 1) {
      BADARGS(5, 5, " type flags cmd/mask procname")
   } else {
      BADARGS(4, 5, " type flags cmd/mask ?procname?")
   }
   fl = str2flags(argv[2]);
   tp = get_bind_type(argv[1]);
   if (tp < 0) {
      Tcl_AppendResult(irp, "bad type, should be one of: dcc, msg, fil, pub, ",
		"msgm, pubm, join, part, sign, kick, topc, mode, ctcp, ",
		   "ctcr, nick, raw, bot, chon, chof, sent, rcvd, chat, "
		  "link, disc, splt, rejn, filt, flud, note, act, notc, "
		       "wall, chjn, chpt, bcst, time",
		       NULL);
      return TCL_ERROR;
   }
   if ((long int) cd == 1) {
      if (!cmd_unbind(tp, fl, argv[3], argv[4])) {
	 /* don't error if trying to re-unbind a builtin */
	 if ((strcmp(argv[3], &argv[4][5]) != 0) || (argv[4][0] != '*') ||
	 (strncmp(argv[1], &argv[4][1], 3) != 0) || (argv[4][4] != ':')) {
	    Tcl_AppendResult(irp, "no such binding", NULL);
	    return TCL_ERROR;
	 }
      }
   } else {
      if (argc == 4)
	 return tcl_getbinds(tp, argv[3]);
      cmd_bind(tp, fl, argv[3], argv[4]);
   }
   Tcl_AppendResult(irp, argv[3], NULL);
   return TCL_OK;
}

int tcl_timer STDVAR
{
   unsigned long x;
   char s[41];
    BADARGS(3, 3, " minutes command");
   if (atoi(argv[1]) < 0) {
      Tcl_AppendResult(irp, "time value must be positive", NULL);
      return TCL_ERROR;
   }
   if (argv[2][0] != '#') {
      x = add_timer(&timer, atoi(argv[1]), argv[2], 0L);
      sprintf(s, "timer%lu", x);
      Tcl_AppendResult(irp, s, NULL);
   }
   return TCL_OK;
}

int tcl_utimer STDVAR
{
   unsigned long x;
   char s[41];
    BADARGS(3, 3, " seconds command");
   if (atoi(argv[1]) < 0) {
      Tcl_AppendResult(irp, "time value must be positive", NULL);
      return TCL_ERROR;
   }
   if (argv[2][0] != '#') {
      x = add_timer(&utimer, atoi(argv[1]), argv[2], 0L);
      sprintf(s, "timer%lu", x);
      Tcl_AppendResult(irp, s, NULL);
   }
   return TCL_OK;
}

int tcl_killtimer STDVAR
{
   BADARGS(2, 2, " timerID");
   if (strncmp(argv[1], "timer", 5) != 0) {
      Tcl_AppendResult(irp, "argument is not a timerID", NULL);
      return TCL_ERROR;
   }
   if (remove_timer(&timer, atol(&argv[1][5])))
       return TCL_OK;
   Tcl_AppendResult(irp, "invalid timerID", NULL);
   return TCL_ERROR;
}

int tcl_killutimer STDVAR
{
   BADARGS(2, 2, " timerID");
   if (strncmp(argv[1], "timer", 5) != 0) {
      Tcl_AppendResult(irp, "argument is not a timerID", NULL);
      return TCL_ERROR;
   }
   if (remove_timer(&utimer, atol(&argv[1][5])))
       return TCL_OK;
   Tcl_AppendResult(irp, "invalid timerID", NULL);
   return TCL_ERROR;
}

int tcl_unixtime STDVAR
{
   char s[20];
   time_t t;
    BADARGS(1, 1, "");
    t = time(NULL);
    sprintf(s, "%lu", (unsigned long) t);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_time STDVAR
{
   char s[81];
   time_t t;
    BADARGS(1, 1, "");
    t = time(NULL);
    strcpy(s, ctime(&t));
    strcpy(s, &s[11]);
    s[5] = 0;
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_date STDVAR
{
   char s[81];
   time_t t;
    BADARGS(1, 1, "");
    t = time(NULL);
    strcpy(s, ctime(&t));
    s[10] = s[24] = 0;
    strcpy(s, &s[8]);
    strcpy(&s[8], &s[20]);
    strcpy(&s[2], &s[3]);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_timers STDVAR
{
   BADARGS(1, 1, "");
   list_timers(irp, timer);
   return TCL_OK;
}

int tcl_utimers STDVAR
{
   BADARGS(1, 1, "");
   list_timers(irp, utimer);
   return TCL_OK;
}

int tcl_ctime STDVAR
{
   time_t tt;
   char s[81];
    BADARGS(2, 2, " unixtime");
    tt = (time_t) atol(argv[1]);
    strcpy(s, ctime(&tt));
    s[strlen(s) - 1] = 0;
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_myip STDVAR
{
   char s[21];
    BADARGS(1, 1, "");
    sprintf(s, "%lu", iptolong(getmyip()));
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_rand STDVAR
{
   unsigned long x;
   char s[41];
    BADARGS(2, 2, " limit");
   if (atol(argv[1]) <= 0) {
      Tcl_AppendResult(irp, "random limit must be greater than zero", NULL);
      return TCL_ERROR;
   }
   x = random() % (atol(argv[1]));
   sprintf(s, "%lu", x);
   Tcl_AppendResult(irp, s, NULL);
   return TCL_OK;
}

int tcl_sendnote STDVAR
{
   char s[5], from[21], to[21], msg[451];
    BADARGS(4, 4, " from to message");
    strncpy(from, argv[1], 20);
    from[20] = 0;
    strncpy(to, argv[2], 20);
    to[20] = 0;
    strncpy(msg, argv[3], 450);
    msg[450] = 0;
    sprintf(s, "%d", add_note(to, from, msg, -1, 0));
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

int tcl_dumpfile STDVAR
{
   char nick[NICKLEN], fn[81];
    BADARGS(3, 3, " nickname filename");
    strncpy(nick, argv[1], NICKLEN - 1);
    nick[NICKLEN - 1] = 0;
    strncpy(fn, argv[2], 80);
    fn[80] = 0;
    showtext(argv[1], argv[2], 0);
    return TCL_OK;
}

int tcl_dccdumpfile STDVAR
{
   char fn[81];
   int idx, i, atr;
    BADARGS(3, 3, " idx filename");
    strncpy(fn, argv[2], 80);
    fn[80] = 0;
    i = atoi(argv[1]);
    idx = findidx(i);
   if (idx < 0) {
      Tcl_AppendResult(irp, "illegal idx", NULL);
      return TCL_ERROR;
   }
   atr = get_attr_handle(dcc[idx].nick);
   telltext(idx, fn, atr);
   return TCL_OK;
}

int tcl_backup STDVAR
{
   BADARGS(1, 1, "");
   backup_userfile();
   return TCL_OK;
}

int tcl_die STDVAR
{
   BADARGS(1, 2, " ?reason?");
   if (argc == 2)
      fatal(argv[1], 0);
   else
      fatal("EXIT", 0);
   /* should never return, but, to keep gcc happy: */
   return TCL_OK;
}

int tcl_strftime STDVAR
{
   char buf[512];
   struct tm *tm1;
   time_t t;
    BADARGS(2, 3, " format ?time?");
   if (argc == 3)
       t = atol(argv[2]);
   else
       t = time(NULL);
    tm1 = localtime(&t);
   if (strftime(buf, sizeof(buf) - 1, argv[1], tm1)) {
      Tcl_AppendResult(irp, buf, NULL);
      return TCL_OK;
   }
   Tcl_AppendResult(irp, " error with strftime", NULL);
   return TCL_ERROR;
}

#ifndef NO_FILE_SYSTEM
#ifndef MODULES
#include "mod/filesys.mod/tclfiles.c"
#endif
#endif
