/*
   tcl.c -- handles:
   the code for every command eggdrop adds to Tcl
   Tcl initialization
   getting and setting Tcl/eggdrop variables

   dprintf'ized, 4feb96
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "main.h"

/* used for read/write to internal strings */
typedef struct {
   char *str;			/* pointer to actual string in eggdrop */
   int max;			/* max length (negative: read-only var when protect is on) */
   /*   (0: read-only ALWAYS) */
   int flags;			/* 1 = directory */
} strinfo;

/* used for read/write to integer couplets */
typedef struct {
   int *left;			/* left side of couplet */
   int *right;			/* right side */
} coupletinfo;

/* used for read/write to flags */
typedef struct {
   char *flag;			/* pointer to actual flag in eggdrop */
   char def;			/* normal value for the name of the flag */
} flaginfo;

/* turn on/off readonly protection */
int protect_readonly = 0;
/* fields to display in a .whois */
char whois_fields[121] = "";

/* timezone bot is in */
char time_zone[41] = "EST";

/* eggdrop always uses the same interpreter */
Tcl_Interp *interp;

extern int curserv, serv, backgrd;
extern int shtime, learn_users, share_users, share_greet, use_info,
 passive, strict_host, require_p, keep_all_logs, copy_to_tmp, use_stderr,
 upload_to_cd, never_give_up, allow_new_telnets, keepnick,
 strict_servernames, check_stoned, quiet_reject, serverror_quit;
extern int botserverport, min_servs, default_flags, conmask, newserverport,
 save_users_at, switch_logfiles_at, server_timeout, connect_timeout,
 firewallport, reserved_port, notify_users_at;
extern int flood_thr, flood_pub_thr, flood_join_thr, ban_time, ignore_time,
 flood_ctcp_thr, flood_time, flood_pub_time, flood_join_time, flood_ctcp_time;
extern char botname[], origbotname[], botuser[], botrealname[], botserver[],
 motdfile[], admin[], userfile[], altnick[], firewall[], helpdir[],
 initserver[], notify_new[], notefile[], hostname[], myip[], botuserhost[],
 tempdir[], newserver[], textdir[], ctcp_version[], ctcp_finger[], ctcp_userinfo[],
 owner[], newserverpass[], newbotname[], network[], botnetnick[], chanfile[];
extern char flag[], chanflag[];
extern int online, maxnotes, modesperline, maxqmsg, wait_split, wait_info,
 wait_dcc_xfer, note_life, default_port,die_on_sighup,die_on_sigterm,
 trigger_on_ignore,answer_ctcp, lowercase_ctcp, max_logs, enable_simul;
extern struct eggqueue *serverlist;
extern struct dcc_t * dcc;
extern int dcc_total;
extern char egg_version[];
extern int egg_numver;
extern tcl_timer_t *timer, *utimer;
extern time_t online_since;
extern log_t * logs;
extern int tands;
extern char natip[];
int dcc_flood_thr = 3;
int debug_tcl = 0;
int raw_binds = 0;
int use_silence = 0;
int remote_boots = 1;
int bounce_bans = 0;
int use_console_r = 0;
/* needs at least 4 or 5 just to get started */
int max_dcc = 5;

/* prototypes for tcl */
Tcl_Interp *Tcl_CreateInterp();
int strtot = 0;

int expmem_tcl()
{
   int i, tot = 0;
   context;
   for (i = 0; i < max_logs; i++)
      if (logs[i].filename != NULL) {
	 tot += strlen(logs[i].filename) + 1;
	 tot += strlen(logs[i].chname) + 1;
      }
   return tot + strtot;
}

/***********************************************************************/

/* logfile [<modes> <channel> <filename>] */
static int tcl_logfile STDVAR
{
   int i;
   char s[151];
    BADARGS(1, 4, " ?logModes channel logFile?");
   if (argc == 1) {
      /* they just want a list of the logfiles and modes */
      for (i = 0; i < max_logs; i++)
	 if (logs[i].filename != NULL) {
	    strcpy(s, masktype(logs[i].mask));
	    strcat(s, " ");
	    strcat(s, logs[i].chname);
	    strcat(s, " ");
	    strcat(s, logs[i].filename);
	    Tcl_AppendElement(interp, s);
	 }
      return TCL_OK;
   }
   BADARGS(4, 4, " ?logModes channel logFile?");
   for (i = 0; i < max_logs; i++)
      if ((logs[i].filename != NULL) && (strcmp(logs[i].filename, argv[3]) == 0)) {
	 logs[i].mask = logmodes(argv[1]);
	 nfree(logs[i].chname);
	 logs[i].chname = NULL;
	 if (!logs[i].mask) {
	    /* ending logfile */
	    nfree(logs[i].filename);
	    logs[i].filename = NULL;
	    if (logs[i].f != NULL) {
	       fclose(logs[i].f);
	       logs[i].f = NULL;
	    }
	 } else {
	    logs[i].chname = (char *) nmalloc(strlen(argv[2]) + 1);
	    strcpy(logs[i].chname, argv[2]);
	 }
	 Tcl_AppendResult(interp, argv[3], NULL);
	 return TCL_OK;
      }
   for (i = 0; i < max_logs; i++)
      if (logs[i].filename == NULL) {
	 logs[i].mask = logmodes(argv[1]);
	 logs[i].filename = (char *) nmalloc(strlen(argv[3]) + 1);
	 strcpy(logs[i].filename, argv[3]);
	 logs[i].chname = (char *) nmalloc(strlen(argv[2]) + 1);
	 strcpy(logs[i].chname, argv[2]);
	 Tcl_AppendResult(interp, argv[3], NULL);
	 return TCL_OK;
      }
   Tcl_AppendResult(interp, "reached max # of logfiles", NULL);
   return TCL_ERROR;
}

int findidx (int z)
{
   int j;
   for (j = 0; j < dcc_total; j++)
      if (dcc[j].sock == z)
	 if ((dcc[j].type == &DCC_CHAT) || (dcc[j].type == &DCC_FILES) ||
	     (dcc[j].type == &DCC_SCRIPT) || (dcc[j].type == &DCC_SOCKET))
	    return j;
   return -1;
}

static void nick_change (char * new)
{
#ifndef NO_IRC
   if (strcasecmp(origbotname, new) != 0) {
      if (origbotname[0])
	 putlog(LOG_MISC, "*", "* IRC NICK CHANGE: %s -> %s", origbotname, new);
      strcpy(origbotname, new);
      /* start all over with nick chasing: */
      strcpy(newbotname, botname);	/* store old nick in case something goes wrong */
      strcpy(botname, origbotname);	/* blah, this is kinda silly */
      if (serv >= 0)
	 tprintf(serv, "NICK %s\n", botname);
   }
#endif
}

static void botnet_change (char * new)
{
   if (strcasecmp(botnetnick, new) != 0) {
      /* trying to change bot's nickname */
      if (tands > 0) {
	 putlog(LOG_MISC, "*", "* Tried to change my botnet nick, but I'm still linked to a botnet.");
	 putlog(LOG_MISC, "*", "* (Unlink and try again.)");
	 return;
      } else {
	 if (botnetnick[0])
	    putlog(LOG_MISC, "*", "* IDENTITY CHANGE: %s -> %s", botnetnick, new);
	 strcpy(botnetnick, new);
      }
   }
}

/**********************************************************************/

/* called when some script tries to change flag1..flag0 */
/* (possible that the new value will be invalid, so we ignore the change) */
static char *tcl_eggflag (ClientData cdata, Tcl_Interp * irp, char * name1,
		   char * name2, int flags)
{
   char s1[2], *s;
   flaginfo *fi = (flaginfo *) cdata;
   if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
      sprintf(s1, "%c", *(fi->flag));
      Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
      return NULL;
   } else {			/* writes */
      s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
      if (s != NULL) {
	 s1[0] = *s;
	 s1[1] = 0;
	 if (s1[0] == *(fi->flag))
	    return NULL;	/* nothing changed */
	 if ((str2flags(s1)) && (s1[0] != fi->def)) {	/* already in use! */
	    s1[0] = *(fi->flag);
	    Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
	    return NULL;
	 }
	 *(fi->flag) = s1[0];
      }
      return NULL;
   }
}

/* read/write normal integer */
static char *tcl_eggint (ClientData cdata, Tcl_Interp * irp, char * name1,
			 char * name2, int flags)
{
   char *s, s1[40];
   long l;
   if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
      /* special cases */
      if ((int *) cdata == &conmask)
	 strcpy(s1, masktype(conmask));
      else if ((int *) cdata == &default_flags)
	 flags2str(default_flags, s1);
      else if ((time_t *) cdata == &online_since)
	 sprintf(s1, "%lu", *(unsigned long *) cdata);
      else
	 sprintf(s1, "%d", *(int *) cdata);
      Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
      return NULL;
   } else {			/* writes */
      s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
      if (s != NULL) {
	 if ((int *) cdata == &conmask) {
	    if (s[0])
	       conmask = logmodes(s);
	    else
	       conmask = LOG_MODES | LOG_MISC | LOG_CMDS;
	 } else if ((int *) cdata == &default_flags)
	    default_flags = str2flags(s);
	 else if ((time_t *) cdata == &online_since)
	    return "read-only variable";
	 else if (protect_readonly && 
		  ((int *)cdata == &enable_simul))
	   return "read-only variable";
	 else {
	    if (Tcl_ExprLong(interp, s, &l) == TCL_ERROR)
	       return interp->result;
	    if ((int *) cdata == &modesperline) {
	       if (l < 3)
		  l = 3;
	       if (l > 6)
		  l = 6;
	    }
	    if ((int *) cdata == &max_dcc) {
	       if (l < max_dcc)
		 return "you can't DECREASE max-dcc";
	       max_dcc = l;
	       init_dcc_max();
	    } else if ((int *) cdata == &max_logs) {
	       if (l < max_logs)
		 return "you can't DECREASE max-logs";
	       max_logs = l;
	       init_misc();
	  
	    } else 
	      *(int *) cdata = (int) l;
	 }
      }
      return NULL;
   }
}

/* read/write normal string variable */
static char *tcl_eggstr (ClientData cdata, Tcl_Interp * irp, char * name1,
			 char * name2, int flags)
{
   char *s;
   strinfo *st = (strinfo *) cdata;
   if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
      if ((st->str == firewall) && (firewall[0])) {
	 char s1[161];
	 sprintf(s1, "%s:%d", firewall, firewallport);
	 Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
      } else
	 Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
      if (st->max <= 0) {
	 if ((flags & TCL_TRACE_UNSETS) && (protect_readonly || (st->max == 0))) {
	    /* no matter what we do, it won't return the error */
	    Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
			 tcl_eggstr, (ClientData) st);
	    return "read-only variable";
	 }
      }
      return NULL;
   } else {			/* writes */
      if ((st->max < 0) && (protect_readonly || (st->max == 0))) {
	 Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
	 return "read-only variable";
      }
      s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
      if (s != NULL) {
	 if (strlen(s) > st->max)
	    s[st->max] = 0;
	 if (st->str == origbotname)
	    nick_change(s);
	 if (st->str == botnetnick)
	    botnet_change(s);
	 else if (st->str == firewall) {
	    splitc(firewall, s, ':');
	    if (!firewall[0])
	       strcpy(firewall, s);
	    else
	       firewallport = atoi(s);
	 } else
	    strcpy(st->str, s);
	 if ((st->flags) && (s[0])) {
	    if (st->str[strlen(st->str) - 1] != '/')
	       strcat(st->str, "/");
	 }
	 /* special cases */
	 if (st->str == textdir) {
	    if (!(st->str[0]))
	       strcpy(st->str, helpdir);
	 }
      }
      return NULL;
   }
}

/* trace a flag */
static void tcl_traceflag (char * name, char * ptr, char def)
{
   flaginfo *fi;
   fi = (flaginfo *) nmalloc(sizeof(flaginfo));
   strtot += sizeof(flaginfo);
   fi->def = def;
   fi->flag = ptr;
   Tcl_TraceVar(interp, name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		tcl_eggflag, (ClientData) fi);
}

static void tcl_untraceflag (char * name)
{
   flaginfo *fi;
   fi = (flaginfo *) Tcl_VarTraceInfo(interp, name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
				      tcl_eggflag, NULL);
   strtot -= sizeof(flaginfo);
   Tcl_UntraceVar(interp, name,
		  TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		  tcl_eggflag, (ClientData) fi);
   nfree(fi);
}

/* trace an int */
#define tcl_traceint(name,ptr) \
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggint,(ClientData)ptr)

/* set up a string variable to be traced (takes a little more effort than */
/* the others, cos the max length has to be stored too) */
static void tcl_tracestr2 (char * name, char * ptr, int len, int dir)
{
   strinfo *st;
   st = (strinfo *) nmalloc(sizeof(strinfo));
   strtot += sizeof(strinfo);
   st->max = len - dir;
   st->str = ptr;
   st->flags = dir;
   Tcl_TraceVar(interp, name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		tcl_eggstr, (ClientData) st);
}
#define tcl_tracestr(a,b,c) tcl_tracestr2(a,b,c,0)
#define tcl_tracedir(a,b,c) tcl_tracestr2(a,b,c,1)

/**********************************************************************/

/* oddballs */

/* read/write the server list */
static char *tcl_eggserver (ClientData cdata, Tcl_Interp * irp, char * name1,
			    char * name2, int flags)
{
   Tcl_DString ds;
   char *slist, **list;
   struct eggqueue *q;
   int lc, code, i;
   if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
      /* create server list */
      Tcl_DStringInit(&ds);
      q = serverlist;
      while (q != NULL) {
	 Tcl_DStringAppendElement(&ds, q->item);
	 q = q->next;
      }
      slist = Tcl_DStringValue(&ds);
      Tcl_SetVar2(interp, name1, name2, slist, TCL_GLOBAL_ONLY);
      Tcl_DStringFree(&ds);
      return NULL;
   } else {			/* writes */
      if (serverlist) {
	 clearq(serverlist);
	 serverlist = NULL;
      }
      slist = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
      if (slist != NULL) {
	 code = Tcl_SplitList(interp, slist, &lc, &list);
	 if (code == TCL_ERROR)
	    return interp->result;
	 for (i = 0; i < lc; i++)
	    add_server(list[i]);
	 /* tricky way to make the bot reset its server pointers */
	 /* perform part of a '.jump <current-server>': */
	 curserv = (-1);
	 n_free(list, "", 0);
	 if (botserver[0])
	    next_server(&curserv, botserver, &botserverport, "");
      }
      return NULL;
   }
}

/* read/write integer couplets (int1:int2) */
static char *tcl_eggcouplet (ClientData cdata, Tcl_Interp * irp, char * name1,
			    char * name2, int flags)
{
   char *s, s1[41];
   coupletinfo *cp = (coupletinfo *) cdata;
   if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
      sprintf(s1, "%d:%d", *(cp->left), *(cp->right));
      Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
      return NULL;
   } else {			/* writes */
      s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
      if (s != NULL) {
	 if (strlen(s) > 40)
	    s[40] = 0;
	 splitc(s1, s, ':');
	 if (s1[0]) {
	    *(cp->left) = atoi(s1);
	    *(cp->right) = atoi(s);
	 } else
	    *(cp->left) = atoi(s);
      }
      return NULL;
   }
}

/* trace the servers */
#define tcl_traceserver(name,ptr) \
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggserver,(ClientData)ptr)

#define tcl_untraceserver(name,ptr) \
  Tcl_UntraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggserver,(ClientData)ptr)


/* allocate couplet space for tracing couplets */
static void tcl_tracecouplet (char * name, int * lptr, int * rptr)
{
   coupletinfo *cp;
   cp = (coupletinfo *) nmalloc(sizeof(coupletinfo));
   strtot += sizeof(coupletinfo);
   cp->left = lptr;
   cp->right = rptr;
   Tcl_TraceVar(interp, name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		tcl_eggcouplet, (ClientData) cp);
}

static void tcl_untracecouplet (char * name)
{
   coupletinfo *cp;
   cp = (coupletinfo *) Tcl_VarTraceInfo(interp, name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
					 tcl_eggcouplet, NULL);
   strtot -= sizeof(coupletinfo);
   Tcl_UntraceVar(interp, name,
		  TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		  tcl_eggcouplet, (ClientData) cp);
   nfree(cp);
}
/**********************************************************************/

/* add/remove tcl commands */
void add_tcl_commands (tcl_cmds * tab)
{
   int i;
   for (i = 0; tab[i].name; i++)
      Tcl_CreateCommand(interp, tab[i].name, tab[i].func, NULL, NULL);
}

void rem_tcl_commands (tcl_cmds * tab)
{
   int i;
   for (i = 0; tab[i].name; i++)
      Tcl_DeleteCommand(interp, tab[i].name);
}

static tcl_strings def_tcl_strings[] =
{
   {"nick", origbotname, NICKLEN, 0},
   {"botnet-nick", botnetnick, NICKLEN, 0},
   {"altnick", altnick, NICKLEN, 0},
   {"realname", botrealname, 80, 0},
   {"username", botuser, 10, 0},
   {"userfile", userfile, 120, STR_PROTECT},
   {"channel-file", chanfile, 120, STR_PROTECT},
   {"motd", motdfile, 120, 0},
   {"admin", admin, 120, 0},
   {"init-server", initserver, 120, 0},
   {"notefile", notefile, 120, 0},
   {"help-path", helpdir, 120, STR_DIR},
   {"temp-path", tempdir, 120, STR_DIR},
   {"text-path", textdir, 120, STR_DIR},
   {"notify-newusers", notify_new, 120, 0},
   {"ctcp-version", ctcp_version, 120, 0},
   {"ctcp-finger", ctcp_finger, 120, 0},
   {"ctcp-userinfo", ctcp_userinfo, 120, 0},
   {"owner", owner, 120, STR_PROTECT},
   {"my-hostname", hostname, 120, 0},
   {"my-ip", myip, 120, 0},
   {"network", network, 40, 0},
   {"whois-fields", whois_fields, 120, 0},
   {"timezone", time_zone, 40, 0},
   {"nat-ip", natip, 120, 0},
  /* always very read-only */
   {"version", egg_version, 0, 0},
   {"botnick", botname, 0, 0},
   {"firewall", firewall, 120, 0},
   {0, 0, 0, 0}
};

  /* ints */

static tcl_ints def_tcl_ints[] =
{
   {"servlimit", &min_servs},
   {"ban-time", &ban_time},
   {"ignore-time", &ignore_time},
   {"dcc-flood-thr", &dcc_flood_thr},
   {"save-users-at", &save_users_at},
   {"notify-users-at", &notify_users_at},
   {"switch-logfiles-at", &switch_logfiles_at},
   {"server-timeout", &server_timeout},
   {"connect-timeout", &connect_timeout},
   {"reserved-port", &reserved_port},
  /* booleans (really just ints) */
   {"log-time", &shtime},
   {"learn-users", &learn_users},
   {"require-p", &require_p},
   {"use-info", &use_info},
   {"strict-host", &strict_host},
   {"keep-all-logs", &keep_all_logs},
   {"never-give-up", &never_give_up},
   {"open-telnets", &allow_new_telnets},
   {"keep-nick", &keepnick},
   {"uptime", (int *) &online_since},
   {"console", &conmask},
   {"default-flags", &default_flags},
   {"strict-servernames", &strict_servernames},
   {"check-stoned", &check_stoned},
   {"serverror-quit", &serverror_quit},
   {"quiet-reject", &quiet_reject},
  /* moved from eggdrop.h */
   {"modes-per-line", &modesperline},
   {"max-queue-msg", &maxqmsg},
   {"wait-split", &wait_split},
   {"wait-info", &wait_info},
   {"default-port", &default_port},
   {"note-life", &note_life},
   {"max-notes", &maxnotes},
   {"numversion", &egg_numver},
   {"debug-tcl", &debug_tcl },
   {"die-on-sighup", &die_on_sighup },
   {"die-on-sigterm", &die_on_sigterm },
   {"trigger-on-ignore", &trigger_on_ignore },
   {"answer-ctcp", &answer_ctcp },
   {"lowercase-ctcp", &lowercase_ctcp },
   {"raw-binds", &raw_binds },
   {"share-greet", &share_greet},
   {"use-silence", &use_silence},
   {"remote-boots", &remote_boots},
   {"bounce-bans", &bounce_bans},
   {"use-console-r", &use_console_r},
   {"max-dcc", &max_dcc},
   {"max-logs", &max_logs},
   {"enable-simul", &enable_simul},
   {0, 0}
};

/* set up Tcl variables that will hook into eggdrop internal vars via */
/* trace callbacks */
static void init_traces()
{
   int i;
   char x[15];

   add_tcl_strings(def_tcl_strings);
   add_tcl_ints(def_tcl_ints);
   /* put traces on the 10 flag variables */
   for (i = 0; i < 10; i++) {
      sprintf(x, "flag%d", i);
      tcl_traceflag(x, &(flag[i]), i + '0');
      sprintf(x, "chanflag%d", i);
      tcl_traceflag(x, &(chanflag[i]), i + '0');
   }
   /* weird ones */
   tcl_traceserver("servers", NULL);
   tcl_tracecouplet("flood-msg", &flood_thr, &flood_time);
   tcl_tracecouplet("flood-chan", &flood_pub_thr, &flood_pub_time);
   tcl_tracecouplet("flood-join", &flood_join_thr, &flood_join_time);
   tcl_tracecouplet("flood-ctcp", &flood_ctcp_thr, &flood_ctcp_time);
}

void kill_tcl()
{
   int i;
   char x[15];

   rem_tcl_strings(def_tcl_strings);
   rem_tcl_ints(def_tcl_ints);

   for (i = 0; i < 10; i++) {
      sprintf(x, "flag%d", i);
      tcl_untraceflag(x);
      sprintf(x, "chanflag%d", i);
      tcl_untraceflag(x);
   }
   tcl_untraceserver("servers", NULL);

   tcl_untracecouplet("flood-msg");
   tcl_untracecouplet("flood-chan");
   tcl_untracecouplet("flood-join");
   tcl_untracecouplet("flood-ctcp");
   kill_hash();
   Tcl_DeleteInterp(interp);
}

extern tcl_cmds tcluser_cmds [],tcldcc_cmds[],tclmisc_cmds[],tclchan_cmds[];

/* not going through Tcl's crazy main() system (what on earth was he
   smoking?!) so we gotta initialize the Tcl interpreter */
void init_tcl()
{
   /* initialize the interpreter */
   interp = Tcl_CreateInterp();
   Tcl_Init(interp);
   init_hash();
   init_traces();
   /* add new commands */
   /* isnt this much neater :) */
   add_tcl_commands(tcluser_cmds);
   add_tcl_commands(tcldcc_cmds);
   add_tcl_commands(tclmisc_cmds);
   add_tcl_commands(tclchan_cmds);
   
#define Q(A,B) Tcl_CreateCommand(interp,A,B,NULL,NULL)
   Q("logfile", tcl_logfile);
}

/* set Tcl variables to match eggdrop internal variables */
void set_tcl_vars()
{
   char s[121];
   /* variables that we won't re-read... only for convenience of scripts */
   sprintf(s, "%s:%d", botserver, botserverport);
   Tcl_SetVar(interp, "server", s, TCL_GLOBAL_ONLY);
   sprintf(s, "%s!%s", botname, botuserhost);
   Tcl_SetVar(interp, "botname", s, TCL_GLOBAL_ONLY);
   /* cos we have to: */
   Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
}

/**********************************************************************/


void do_tcl (char * whatzit, char * script)
{
   int code;
   FILE * f = 0;
   
   if (debug_tcl) {
      f = fopen("DEBUG.TCL", "a");
      if (f != NULL)
	fprintf(f, "eval: %s\n", script);
   }
   set_tcl_vars();
   context;
   code = Tcl_Eval(interp, script);
   if (debug_tcl &&(f != NULL)) {
      fprintf(f, "done eval, result=%d\n", code);
      fclose(f);
   }
   if (code != TCL_OK) {
      putlog(LOG_MISC, "*", "Tcl error in script for '%s':", whatzit);
      putlog(LOG_MISC, "*", "%s", interp->result);
   }
}

/* read and interpret the configfile given */
/* return 1 if everything was okay */
int readtclprog (char * fname)
{
   int code;
   FILE *f;
   set_tcl_vars();
   f = fopen(fname, "r");
   if (f == NULL)
      return 0;
   fclose(f);
   if (debug_tcl) {
      f = fopen("DEBUG.TCL", "a");
      if (f != NULL) {
	 fprintf(f, "Sourcing file %s ...\n", fname);
	 fclose(f);
      }
   }
   code = Tcl_EvalFile(interp, fname);
   if (code != TCL_OK) {
      if (use_stderr) {
	 tprintf(STDERR, "Tcl error in file '%s':\n", fname);
	 tprintf(STDERR, "%s\n", Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
      } else {
	 putlog(LOG_MISC, "*", "Tcl error in file '%s':", fname);
	 putlog(LOG_MISC, "*", "%s\n", Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
      }
      /* try to go on anyway (shrug) */
   }
   /* refresh internal variables */
   return 1;
}

void add_tcl_strings (tcl_strings * list)
{
   int i;
   for (i = 0; list[i].name; i++) {
      if (list[i].length > 0) {
	 char *p = Tcl_GetVar(interp, list[i].name, TCL_GLOBAL_ONLY);
	 if (p != NULL) {
	    strncpy(list[i].buf, p, list[i].length);
	    list[i].buf[list[i].length] = 0;
	    if (list[i].flags & STR_DIR) {
	       int x = strlen(list[i].buf);
	       if ((x > 0) && (x < (list[i].length - 1))
		   && (list[i].buf[x - 1] != '/')) {
		  list[i].buf[x++] = '/';
		  list[i].buf[x] = 0;
	       }
	    }
	 }
      }
      tcl_tracestr2(list[i].name, list[i].buf,
	(list[i].flags & STR_PROTECT) ? -list[i].length : list[i].length,
		    (list[i].flags & STR_DIR));

   }
}

void rem_tcl_strings (tcl_strings * list)
{
   int i;
   strinfo *st;

   for (i = 0; list[i].name; i++) {
      st = (strinfo *) Tcl_VarTraceInfo(interp, list[i].name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
					tcl_eggstr, NULL);
      Tcl_UntraceVar(interp, list[i].name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		     tcl_eggstr, st);
      if (st != NULL) {
	 strtot -= sizeof(strinfo);
	 nfree(st);
      }
   }
}

void add_tcl_ints (tcl_ints * list)
{
   int i;
   for (i = 0; list[i].name; i++) {
      char *p = Tcl_GetVar(interp, list[i].name, TCL_GLOBAL_ONLY);
      if (p != NULL)
	 *(list[i].val) = atoi(p);
      Tcl_TraceVar(interp, list[i].name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		   tcl_eggint, (ClientData) list[i].val);
   }
}

void rem_tcl_ints (tcl_ints * list)
{
   int i;
   for (i = 0; list[i].name; i++) {
      Tcl_UntraceVar(interp, list[i].name,
		   TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		     tcl_eggint, (ClientData) list[i].val);
   }
}
