/*
   tclhash.c -- handles:
   bind and unbind
   checking and triggering the various bindings
   listing current bindings

   dprintf'ized, 4feb96
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
#include "eggdrop.h"
#include "proto.h"
#include "cmdt.h"
#include "tclegg.h"

extern Tcl_Interp *interp;
extern int dcc_total;
extern struct dcc_t dcc[];
extern int require_p;
extern cmd_t C_dcc[];
#ifndef NO_IRC
extern cmd_t C_msg[];
#ifndef NO_FILE_SYSTEM
extern cmd_t C_file[];
#endif
#endif

Tcl_HashTable H_msg, H_dcc, H_fil, H_pub, H_msgm, H_pubm, H_join, H_part,
 H_sign, H_kick, H_topc, H_mode, H_ctcp, H_ctcr, H_nick, H_raw, H_bot,
 H_chon, H_chof, H_sent, H_rcvd, H_chat, H_link, H_disc, H_splt, H_rejn,
 H_filt, H_flud, H_note, H_act, H_notc, H_wall, H_bcst, H_chjn, H_chpt,
 H_time;

int hashtot = 0;

int expmem_tclhash()
{
   return hashtot;
}

/* initialize hash tables */
void init_hash()
{
   Tcl_InitHashTable(&H_msg, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_dcc, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_fil, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_pub, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_msgm, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_pubm, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_notc, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_join, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_part, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_sign, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_kick, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_topc, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_mode, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_ctcp, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_ctcr, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_nick, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_raw, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_bot, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_chon, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_chof, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_sent, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_rcvd, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_chat, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_link, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_disc, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_splt, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_rejn, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_filt, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_flud, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_note, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_act, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_wall, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_bcst, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_chjn, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_chpt, TCL_STRING_KEYS);
   Tcl_InitHashTable(&H_time, TCL_STRING_KEYS);
}

void *tclcmd_alloc PROTO1(int, size)
{
   tcl_cmd_t *x = (tcl_cmd_t *) nmalloc(sizeof(tcl_cmd_t));
   hashtot += sizeof(tcl_cmd_t);
   x->func_name = (char *) nmalloc(size);
   hashtot += size;
   return (void *) x;
}

void tclcmd_free PROTO1(void *, ptr)
{
   tcl_cmd_t *x = ptr;
   hashtot -= sizeof(tcl_cmd_t);
   hashtot -= strlen(x->func_name) + 1;
   nfree(x->func_name);
   nfree(x);
}

void clean_up_hash(Tcl_HashTable * hash)
{
   Tcl_HashSearch srch;
   Tcl_HashEntry *he;
   tcl_cmd_t *tt, *tt1;

   context;
   for (he = Tcl_FirstHashEntry(hash, &srch); (he != NULL);
	he = Tcl_NextHashEntry(&srch)) {
      tt = Tcl_GetHashValue(he);
      while (tt != NULL) {
	 tt1 = tt->next;
	 tclcmd_free(tt);
	 tt = tt1;
      }
   }
   Tcl_DeleteHashTable(hash);
}

void kill_hash()
{
   clean_up_hash(&H_msg);
   clean_up_hash(&H_dcc);
   clean_up_hash(&H_fil);
   clean_up_hash(&H_pub);
   clean_up_hash(&H_msgm);
   clean_up_hash(&H_pubm);
   clean_up_hash(&H_notc);
   clean_up_hash(&H_join);
   clean_up_hash(&H_part);
   clean_up_hash(&H_sign);
   clean_up_hash(&H_kick);
   clean_up_hash(&H_topc);
   clean_up_hash(&H_mode);
   clean_up_hash(&H_ctcp);
   clean_up_hash(&H_ctcr);
   clean_up_hash(&H_nick);
   clean_up_hash(&H_raw);
   clean_up_hash(&H_bot);
   clean_up_hash(&H_chon);
   clean_up_hash(&H_chof);
   clean_up_hash(&H_sent);
   clean_up_hash(&H_rcvd);
   clean_up_hash(&H_chat);
   clean_up_hash(&H_link);
   clean_up_hash(&H_disc);
   clean_up_hash(&H_splt);
   clean_up_hash(&H_rejn);
   clean_up_hash(&H_filt);
   clean_up_hash(&H_flud);
   clean_up_hash(&H_note);
   clean_up_hash(&H_act);
   clean_up_hash(&H_wall);
   clean_up_hash(&H_bcst);
   clean_up_hash(&H_chjn);
   clean_up_hash(&H_chpt);
   clean_up_hash(&H_time);
}

/* returns hashtable for that type */
/* also sets 'stk' if stackable, and sets 'name' the name, if non-NULL */
Tcl_HashTable *gethashtable PROTO3(int, typ, int *, stk, char *, name)
{
   char *nam = NULL;
   int st = 0;
   Tcl_HashTable *ht = NULL;
   switch (typ) {
   case CMD_MSG:
      ht = &H_msg;
      nam = "msg";
      break;
   case CMD_DCC:
      ht = &H_dcc;
      nam = "dcc";
      break;
   case CMD_FIL:
      ht = &H_fil;
      nam = "fil";
      break;
   case CMD_PUB:
      ht = &H_pub;
      nam = "pub";
      break;
   case CMD_MSGM:
      ht = &H_msgm;
      nam = "msgm";
      st = 1;
      break;
   case CMD_PUBM:
      ht = &H_pubm;
      nam = "pubm";
      st = 1;
      break;
   case CMD_NOTC:
      ht = &H_notc;
      nam = "notc";
      st = 1;
      break;
   case CMD_JOIN:
      ht = &H_join;
      nam = "join";
      st = 1;
      break;
   case CMD_PART:
      ht = &H_part;
      nam = "part";
      st = 1;
      break;
   case CMD_SIGN:
      ht = &H_sign;
      nam = "sign";
      st = 1;
      break;
   case CMD_KICK:
      ht = &H_kick;
      nam = "kick";
      st = 1;
      break;
   case CMD_TOPC:
      ht = &H_topc;
      nam = "topc";
      st = 1;
      break;
   case CMD_MODE:
      ht = &H_mode;
      nam = "mode";
      st = 1;
      break;
   case CMD_CTCP:
      ht = &H_ctcp;
      nam = "ctcp";
      break;
   case CMD_CTCR:
      ht = &H_ctcr;
      nam = "ctcr";
      break;
   case CMD_NICK:
      ht = &H_nick;
      nam = "nick";
      st = 1;
      break;
   case CMD_RAW:
      ht = &H_raw;
      nam = "raw";
      st = 1;
      break;
   case CMD_BOT:
      ht = &H_bot;
      nam = "bot";
      break;
   case CMD_CHON:
      ht = &H_chon;
      nam = "chon";
      st = 1;
      break;
   case CMD_CHOF:
      ht = &H_chof;
      nam = "chof";
      st = 1;
      break;
   case CMD_SENT:
      ht = &H_sent;
      nam = "sent";
      st = 1;
      break;
   case CMD_RCVD:
      ht = &H_rcvd;
      nam = "rcvd";
      st = 1;
      break;
   case CMD_CHAT:
      ht = &H_chat;
      nam = "chat";
      st = 1;
      break;
   case CMD_LINK:
      ht = &H_link;
      nam = "link";
      st = 1;
      break;
   case CMD_DISC:
      ht = &H_disc;
      nam = "disc";
      st = 1;
      break;
   case CMD_SPLT:
      ht = &H_splt;
      nam = "splt";
      st = 1;
      break;
   case CMD_REJN:
      ht = &H_rejn;
      nam = "rejn";
      st = 1;
      break;
   case CMD_FILT:
      ht = &H_filt;
      nam = "filt";
      st = 1;
      break;
   case CMD_FLUD:
      ht = &H_flud;
      nam = "flud";
      st = 1;
      break;
   case CMD_NOTE:
      ht = &H_note;
      nam = "note";
      break;
   case CMD_ACT:
      ht = &H_act;
      nam = "act";
      st = 1;
      break;
   case CMD_WALL:
      ht = &H_wall;
      nam = "wall";
      st = 1;
      break;
   case CMD_BCST:
      ht = &H_bcst;
      nam = "bcst";
      st = 1;
      break;
   case CMD_CHJN:
      ht = &H_chjn;
      nam = "chjn";
      st = 1;
      break;
   case CMD_CHPT:
      ht = &H_chpt;
      nam = "chpt";
      st = 1;
      break;
   case CMD_TIME:
      ht = &H_time;
      nam = "time";
      st = 1;
      break;
   }
   if (name != NULL)
      strcpy(name, nam);
   if (stk != NULL)
      *stk = st;
   return ht;
}

int get_bind_type PROTO1(char *, name)
{
   int tp = (-1);
   if (strcasecmp(name, "dcc") == 0)
      tp = CMD_DCC;
   if (strcasecmp(name, "msg") == 0)
      tp = CMD_MSG;
   if (strcasecmp(name, "fil") == 0)
      tp = CMD_FIL;
   if (strcasecmp(name, "pub") == 0)
      tp = CMD_PUB;
   if (strcasecmp(name, "msgm") == 0)
      tp = CMD_MSGM;
   if (strcasecmp(name, "pubm") == 0)
      tp = CMD_PUBM;
   if (strcasecmp(name, "notc") == 0)
      tp = CMD_NOTC;
   if (strcasecmp(name, "join") == 0)
      tp = CMD_JOIN;
   if (strcasecmp(name, "part") == 0)
      tp = CMD_PART;
   if (strcasecmp(name, "sign") == 0)
      tp = CMD_SIGN;
   if (strcasecmp(name, "kick") == 0)
      tp = CMD_KICK;
   if (strcasecmp(name, "topc") == 0)
      tp = CMD_TOPC;
   if (strcasecmp(name, "mode") == 0)
      tp = CMD_MODE;
   if (strcasecmp(name, "ctcp") == 0)
      tp = CMD_CTCP;
   if (strcasecmp(name, "ctcr") == 0)
      tp = CMD_CTCR;
   if (strcasecmp(name, "nick") == 0)
      tp = CMD_NICK;
   if (strcasecmp(name, "bot") == 0)
      tp = CMD_BOT;
   if (strcasecmp(name, "chon") == 0)
      tp = CMD_CHON;
   if (strcasecmp(name, "chof") == 0)
      tp = CMD_CHOF;
   if (strcasecmp(name, "sent") == 0)
      tp = CMD_SENT;
   if (strcasecmp(name, "rcvd") == 0)
      tp = CMD_RCVD;
   if (strcasecmp(name, "chat") == 0)
      tp = CMD_CHAT;
   if (strcasecmp(name, "link") == 0)
      tp = CMD_LINK;
   if (strcasecmp(name, "disc") == 0)
      tp = CMD_DISC;
   if (strcasecmp(name, "rejn") == 0)
      tp = CMD_REJN;
   if (strcasecmp(name, "splt") == 0)
      tp = CMD_SPLT;
   if (strcasecmp(name, "filt") == 0)
      tp = CMD_FILT;
   if (strcasecmp(name, "flud") == 0)
      tp = CMD_FLUD;
   if (strcasecmp(name, "note") == 0)
      tp = CMD_NOTE;
   if (strcasecmp(name, "act") == 0)
      tp = CMD_ACT;
   if (strcasecmp(name, "raw") == 0)
      tp = CMD_RAW;
   if (strcasecmp(name, "wall") == 0)
      tp = CMD_WALL;
   if (strcasecmp(name, "bcst") == 0)
      tp = CMD_BCST;
   if (strcasecmp(name, "chjn") == 0)
      tp = CMD_CHJN;
   if (strcasecmp(name, "chpt") == 0)
      tp = CMD_CHPT;
   if (strcasecmp(name, "time") == 0)
      tp = CMD_TIME;
   return tp;
}

/* remove command */
int cmd_unbind PROTO4(int, typ, int, flags, char *, cmd, char *, proc)
{
   tcl_cmd_t *tt, *last;
   Tcl_HashEntry *he;
   Tcl_HashTable *ht;
   ht = gethashtable(typ, NULL, NULL);
   he = Tcl_FindHashEntry(ht, cmd);
   if (he == NULL)
      return 0;			/* no such binding */
   tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
   last = NULL;
   while (tt != NULL) {
      /* if procs are same, erase regardless of flags */
      if (strcasecmp(tt->func_name, proc) == 0) {
	 /* erase it */
	 if (last != NULL)
	    last->next = tt->next;
	 else {
	    if (tt->next == NULL)
	       Tcl_DeleteHashEntry(he);
	    else
	       Tcl_SetHashValue(he, tt->next);
	 }
	 hashtot -= (strlen(tt->func_name) + 1);
	 nfree(tt->func_name);
	 nfree(tt);
	 hashtot -= sizeof(tcl_cmd_t);
	 return 1;
      }
      last = tt;
      tt = tt->next;
   }
   return 0;			/* no match */
}

/* add command (remove old one if necessary) */
int cmd_bind PROTO4(int, typ, int, flags, char *, cmd, char *, proc)
{
   tcl_cmd_t *tt;
   int new;
   Tcl_HashEntry *he;
   Tcl_HashTable *ht;
   int stk;
   if (proc[0] == '#') {
      putlog(LOG_MISC, "*", "Note: binding to '#' is obsolete.");
      return 0;
   }
   cmd_unbind(typ, flags, cmd, proc);	/* make sure we don't dup */
   tt = (tcl_cmd_t *) nmalloc(sizeof(tcl_cmd_t));
   hashtot += sizeof(tcl_cmd_t);
   tt->flags_needed = flags;
   tt->next = NULL;
   tt->func_name = (char *) nmalloc(strlen(proc) + 1);
   hashtot += strlen(proc) + 1;
   strcpy(tt->func_name, proc);
   ht = gethashtable(typ, &stk, NULL);
   he = Tcl_CreateHashEntry(ht, cmd, &new);
   if (!new) {
      tt->next = (tcl_cmd_t *) Tcl_GetHashValue(he);
      if (!stk) {
	 /* remove old one -- these are not stackable */
	 hashtot -= (strlen(tt->next->func_name) + 1);
	 hashtot -= sizeof(tcl_cmd_t);
	 nfree(tt->next->func_name);
	 nfree(tt->next);
	 tt->next = NULL;
      }
   }
   Tcl_SetHashValue(he, tt);
   return 1;
}

/* used as the common interface to builtin commands */
int tcl_builtin STDVAR
{
   char typ[4];
   Function F = (Function) cd;
#ifdef EBUG
   char s[512];
   int i = 0;
#endif
   /* find out what kind of cmd this is */
    context;
   if (argv[0][0] != '*') {
      Tcl_AppendResult(irp, "bad builtin command call!", NULL);
      return TCL_ERROR;
   }
   strncpy(typ, &argv[0][1], 3);
   typ[3] = 0;
   if (strcmp(typ, "dcc") == 0) {
      int idx;
      BADARGS(4, 4, " hand idx param");
      idx = findidx(atoi(argv[2]));
      if (idx < 0) {
	 Tcl_AppendResult(irp, "invalid idx", NULL);
	 return TCL_ERROR;
      }
      BADARGS(4, 4, " hand idx param");
      if (F == CMD_LEAVE) {
	 Tcl_AppendResult(irp, "break", NULL);
	 return TCL_OK;
      }
#ifdef EBUG
      /* check if it's a password change, if so, don't show the password */
      strcpy(s, &argv[0][5]);
      if (strcmp(s, "newpass") == 0) {
	 if (argv[3][0])
	    debug3("tcl: builtin dcc call: %s %s %s [something]",
		   argv[0], argv[1], argv[2]);
	 else
	    i = 1;
      } else if (strcmp(s, "chpass") == 0) {
	 stridx(s, argv[3], 1);
	 if (s[0])
	    debug4("tcl: builtin dcc call: %s %s %s %s [something]",
		   argv[0], argv[1], argv[2], s);
	 else
	    i = 1;
      } else if (strcmp(s, "tcl") == 0) {
	 stridx(s, argv[3], 1);
	 if (strcmp(s, "chpass") == 0) {
	    stridx(s, argv[3], 2);
	    if (s[0])
	       debug4("tcl: builtin dcc call: %s %s %s chpass %s [something]",
		      argv[0], argv[1], argv[2], s);
	    else
	       i = 1;
	 } else
	    i = 1;
      } else
	 i = 1;
      if (i)
	 debug4("tcl: builtin dcc call: %s %s %s %s", argv[0], argv[1], argv[2],
		argv[3]);
#endif
      (F) (idx, argv[3]);
      Tcl_ResetResult(irp);
      return TCL_OK;
   }
   if (strcmp(typ, "msg") == 0) {
      BADARGS(5, 5, " nick uhost hand param");
      (F) (argv[3], argv[1], argv[2], argv[4]);
      return TCL_OK;
   }
   if (strcmp(typ, "fil") == 0) {
      int idx;
      BADARGS(4, 4, " hand idx param");
      idx = findidx(atoi(argv[2]));
      if (idx < 0) {
	 Tcl_AppendResult(irp, "invalid idx", NULL);
	 return TCL_ERROR;
      }
      if (F == CMD_LEAVE) {
	 Tcl_AppendResult(irp, "break", NULL);
	 return TCL_OK;
      }
      printf("IDX %d VALUE [%s]\n", idx, argv[3]);
      (F) (idx, argv[3]);
      Tcl_ResetResult(irp);
      return TCL_OK;
   }
   Tcl_AppendResult(irp, "non-existent builtin type", NULL);
   return TCL_ERROR;
}

/* trigger (execute) a proc */
int trigger_bind PROTO2(char *, proc, char *, param)
{
   int x;
#ifdef EBUG_TCL
   FILE *f = fopen("DEBUG.TCL", "a");
   if (f != NULL)
      fprintf(f, "eval: %s%s\n", proc, param);
#endif
   set_tcl_vars();
   context;
   x = Tcl_VarEval(interp, proc, param, NULL);
   if (x == TCL_ERROR) {
#ifdef EBUG_TCL
      if (f != NULL) {
	 fprintf(f, "done eval. error.\n");
	 fclose(f);
      }
#endif
      if (strlen(interp->result) > 400)
	 interp->result[400] = 0;
      putlog(LOG_MISC, "*", "Tcl error [%s]: %s", proc, interp->result);
      return BIND_EXECUTED;
   } else {
#ifdef EBUG_TCL
      if (f != NULL) {
	 fprintf(f, "done eval. ok.\n");
	 fclose(f);
      }
#endif
      if (strcmp(interp->result, "break") == 0)
	 return BIND_EXEC_BRK;
      return (atoi(interp->result) > 0) ? BIND_EXEC_LOG : BIND_EXECUTED;
   }
}

/* check a tcl binding and execute the procs necessary */
int check_tcl_bind PROTO5(Tcl_HashTable *, hash, char *, match, int, atr,
			  char *, param, int, match_type)
{
   Tcl_HashSearch srch;
   Tcl_HashEntry *he;
   int cnt = 0;
   char *proc = NULL;
   tcl_cmd_t *tt;
   int f = 0, atrok, x;
   context;
   for (he = Tcl_FirstHashEntry(hash, &srch); (he != NULL) && (!f);
	he = Tcl_NextHashEntry(&srch)) {
      int ok = 0;
      context;
      switch (match_type & 0x03) {
      case MATCH_PARTIAL:
	 ok = (strncasecmp(match, Tcl_GetHashKey(hash, he), strlen(match)) == 0);
	 break;
      case MATCH_EXACT:
	 ok = (strcasecmp(match, Tcl_GetHashKey(hash, he)) == 0);
	 break;
      case MATCH_MASK:
	 ok = wild_match_per(Tcl_GetHashKey(hash, he), match);
	 break;
      }
      context;
      if (ok) {
	 tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
	 switch (match_type & 0x03) {
	 case MATCH_MASK:
	    /* could be multiple triggers */
	    while (tt != NULL) {
	       if (match_type & BIND_HAS_BUILTINS)
		  atrok = flags_ok(tt->flags_needed, atr);
	       else
		  atrok = flags_eq(tt->flags_needed, atr);
	       if ((!(match_type & BIND_USE_ATTR)) || atrok) {
		  cnt++;
		  x = trigger_bind(tt->func_name, param);
		  if ((match_type & BIND_WANTRET) && !(match_type & BIND_ALTER_ARGS) &&
		      (x == BIND_EXEC_LOG))
		     return x;
		  if (match_type & BIND_ALTER_ARGS) {
		     if ((interp->result == NULL) || !(interp->result[0]))
			return x;
		     /* this is such an amazingly ugly hack: */
		     Tcl_SetVar(interp, "_a", interp->result, 0);
		  }
	       }
	       tt = tt->next;
	    }
	    break;
	 default:
	    if (match_type & BIND_HAS_BUILTINS)
	       atrok = flags_ok(tt->flags_needed, atr);
	    else
	       atrok = flags_eq(tt->flags_needed, atr);
	    if ((!(match_type & BIND_USE_ATTR)) || atrok) {
	       cnt++;
	       proc = tt->func_name;
	       if (strcasecmp(match, Tcl_GetHashKey(hash, he)) == 0) {
		  cnt = 1;
		  f = 1;	/* perfect match */
	       }
	    }
	    break;
	 }
      }
   }
   context;
   if (cnt == 0)
      return BIND_NOMATCH;
   if ((match_type & 0x03) == MATCH_MASK)
      return BIND_EXECUTED;
   if (cnt > 1)
      return BIND_AMBIGUOUS;
   return trigger_bind(proc, param);
}

/* check for tcl-bound msg command, return 1 if found */
/* msg: proc-name <nick> <user@host> <handle> <args...> */
int check_tcl_msg PROTO5(char *, cmd, char *, nick, char *, uhost, char *, hand,
			 char *, args)
{
#ifndef NO_IRC
   int x, atr;
   context;
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   x = check_tcl_bind(&H_msg, cmd, atr, " $_n $_uh $_h $_a",
		      MATCH_PARTIAL | BIND_HAS_BUILTINS | BIND_USE_ATTR);
   context;
   if (x == BIND_EXEC_LOG)
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %s", nick, uhost, hand, cmd, args);
   return ((x == BIND_MATCHED) || (x == BIND_EXECUTED) || (x == BIND_EXEC_LOG));
#else
   return 0;
#endif
}

/* check for tcl-bound dcc command, return 1 if found */
/* dcc: proc-name <handle> <sock> <args...> */
int check_tcl_dcc PROTO3(char *, cmd, int, idx, char *, args)
{
   int x, atr, chatr;
   char s[5];
   context;
   atr = get_attr_handle(dcc[idx].nick);
   chatr = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.chat->con_chan);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   sprintf(s, "%d", dcc[idx].sock);
   Tcl_SetVar(interp, "_n", dcc[idx].nick, 0);
   Tcl_SetVar(interp, "_i", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   x = check_tcl_bind(&H_dcc, cmd, atr, " $_n $_i $_a",
		      MATCH_PARTIAL | BIND_USE_ATTR | BIND_HAS_BUILTINS);
   context;
   if (x == BIND_AMBIGUOUS) {
      dprintf(idx, "Ambigious command.\n");
      return 0;
   }
   if (x == BIND_NOMATCH) {
      dprintf(idx, "What?  You need '.help'\n");
      return 0;
   }
   if (x == BIND_EXEC_BRK)
      return 1;			/* quit */
   if (x == BIND_EXEC_LOG)
      putlog(LOG_CMDS, "*", "#%s# %s %s", dcc[idx].nick, cmd, args);
   return 0;
}

int check_tcl_pub PROTO4(char *, nick, char *, from, char *, chname, char *, msg)
{
   int x, atr, chatr;
   char args[512], cmd[512], host[161], handle[21];
   context;
   strcpy(args, msg);
   nsplit(cmd, args);
   sprintf(host, "%s!%s", nick, from);
   get_handle_by_host(handle, host);
   atr = get_attr_handle(handle);
   chatr = get_chanattr_handle(handle, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", from, 0);
   Tcl_SetVar(interp, "_h", handle, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", args, 0);
   context;
   x = check_tcl_bind(&H_pub, cmd, atr, " $_n $_uh $_h $_a $_aa",
		      MATCH_EXACT | BIND_USE_ATTR);
   context;
   if (x == BIND_NOMATCH)
      return 0;
   if (x == BIND_EXEC_LOG)
      putlog(LOG_CMDS, chname, "<<%s>> !%s! %s %s", nick, handle, cmd, args);
   return 1;
}

void check_tcl_pubm PROTO4(char *, nick, char *, from, char *, chname, char *, msg)
{
   char args[512], host[161], handle[21];
   int atr, chatr;
   context;
   strcpy(args, chname);
   strcat(args, " ");
   strcat(args, msg);
   sprintf(host, "%s!%s", nick, from);
   get_handle_by_host(handle, host);
   atr = get_attr_handle(handle);
   chatr = get_chanattr_handle(handle, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", from, 0);
   Tcl_SetVar(interp, "_h", handle, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", msg, 0);
   context;
   check_tcl_bind(&H_pubm, args, atr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_msgm PROTO5(char *, cmd, char *, nick, char *, uhost, char *, hand,
			   char *, arg)
{
   int atr;
   char args[512];
   context;
   if (arg[0])
      sprintf(args, "%s %s", cmd, arg);
   else
      strcpy(args, cmd);
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   check_tcl_bind(&H_msgm, args, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_notc PROTO4(char *, nick, char *, uhost, char *, hand, char *, arg)
{
   int atr;
   context;
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", arg, 0);
   context;
   check_tcl_bind(&H_notc, arg, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_join PROTO4(char *, nick, char *, uhost, char *, hand, char *, chname)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   context;
   check_tcl_bind(&H_join, args, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_part PROTO4(char *, nick, char *, uhost, char *, hand, char *, chname)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   context;
   check_tcl_bind(&H_part, args, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_sign PROTO5(char *, nick, char *, uhost, char *, hand,
			   char *, chname, char *, reason)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", reason, 0);
   context;
   check_tcl_bind(&H_sign, args, atr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_topc PROTO5(char *, nick, char *, uhost, char *, hand,
			   char *, chname, char *, topic)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s", chname, topic);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", topic, 0);
   context;
   check_tcl_bind(&H_topc, args, atr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_nick PROTO5(char *, nick, char *, uhost, char *, hand,
			   char *, chname, char *, newnick)
{
   int atr = get_attr_handle(hand), chatr = get_chanattr_handle(hand, chname);
   char args[512];
   context;
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   sprintf(args, "%s %s", chname, newnick);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", newnick, 0);
   context;
   check_tcl_bind(&H_nick, args, atr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_kick PROTO6(char *, nick, char *, uhost, char *, hand,
			   char *, chname, char *, dest, char *, reason)
{
   char args[512];
   context;
   sprintf(args, "%s %s", chname, dest);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", dest, 0);
   Tcl_SetVar(interp, "_aaa", reason, 0);
   context;
   check_tcl_bind(&H_kick, args, 0, " $_n $_uh $_h $_a $_aa $_aaa",
		  MATCH_MASK | BIND_STACKABLE);
   context;
}

/* return 1 if processed */
#ifdef RAW_BINDS
int check_tcl_raw PROTO3(char *, from, char *, code, char *, msg)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", code, 0);
   Tcl_SetVar(interp, "_aa", msg, 0);
   context;
   x = check_tcl_bind(&H_raw, code, 0, " $_n $_a $_aa",
		      MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
   context;
   return (x == BIND_EXEC_LOG);
}
#endif

void check_tcl_bot PROTO3(char *, nick, char *, code, char *, param)
{
   context;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_h", code, 0);
   Tcl_SetVar(interp, "_a", param, 0);
   context;
   check_tcl_bind(&H_bot, code, 0, " $_n $_h $_a", MATCH_EXACT);
   context;
}

void check_tcl_mode PROTO5(char *, nick, char *, uhost, char *, hand,
			   char *, chname, char *, mode)
{
   char args[512];
   context;
   sprintf(args, "%s %s", chname, mode);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", mode, 0);
   context;
   check_tcl_bind(&H_mode, args, 0, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_STACKABLE);
   context;
}

int check_tcl_ctcp PROTO6(char *, nick, char *, uhost, char *, hand, char *, dest,
			  char *, keyword, char *, args)
{
   int atr, x;
   context;
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", dest, 0);
   Tcl_SetVar(interp, "_aa", keyword, 0);
   Tcl_SetVar(interp, "_aaa", args, 0);
   context;
   x = check_tcl_bind(&H_ctcp, keyword, atr, " $_n $_uh $_h $_a $_aa $_aaa",
		      MATCH_MASK | BIND_USE_ATTR | BIND_WANTRET);
   context;
   return (x == BIND_EXEC_LOG);
/*  return ((x==BIND_MATCHED)||(x==BIND_EXECUTED)||(x==BIND_EXEC_LOG)); */
}

int check_tcl_ctcr PROTO6(char *, nick, char *, uhost, char *, hand,
			  char *, dest, char *, keyword, char *, args)
{
   int atr;
   context;
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", dest, 0);
   Tcl_SetVar(interp, "_aa", keyword, 0);
   Tcl_SetVar(interp, "_aaa", args, 0);
   context;
   check_tcl_bind(&H_ctcr, keyword, atr, " $_n $_uh $_h $_a $_aa $_aaa",
		  MATCH_MASK | BIND_USE_ATTR);
   context;
   return 1;
}

void check_tcl_chon PROTO2(char *, hand, int, idx)
{
   int atr;
   char s[20];
   context;
   sprintf(s, "%d", idx);
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   context;
   check_tcl_bind(&H_chon, hand, atr, " $_n $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_chof PROTO2(char *, hand, int, idx)
{
   int atr;
   char s[20];
   context;
   sprintf(s, "%d", idx);
   atr = get_attr_handle(hand);
   if (op_anywhere(hand))
      atr |= USER_PSUEDOOP;
   if (master_anywhere(hand))
      atr |= USER_PSUMST;
   if (owner_anywhere(hand))
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   context;
   check_tcl_bind(&H_chof, hand, atr, " $_n $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_chat PROTO3(char *, from, int, chan, char *, text)
{
   char s[10];
   context;
   sprintf(s, "%d", chan);
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   Tcl_SetVar(interp, "_aa", text, 0);
   context;
   check_tcl_bind(&H_chat, text, 0, " $_n $_a $_aa", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_link PROTO2(char *, bot, char *, via)
{
   context;
   Tcl_SetVar(interp, "_n", bot, 0);
   Tcl_SetVar(interp, "_a", via, 0);
   context;
   check_tcl_bind(&H_link, bot, 0, " $_n $_a", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_disc PROTO1(char *, bot)
{
   context;
   Tcl_SetVar(interp, "_n", bot, 0);
   context;
   check_tcl_bind(&H_disc, bot, 0, " $_n", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_splt PROTO4(char *, nick, char *, uhost, char *, hand, char *, chname)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   context;
   check_tcl_bind(&H_splt, args, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_rejn PROTO4(char *, nick, char *, uhost, char *, hand, char *, chname)
{
   int atr, chatr;
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   atr = get_attr_handle(hand);
   chatr = get_chanattr_handle(hand, chname);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   context;
   check_tcl_bind(&H_rejn, args, atr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

char *check_tcl_filt PROTO2(int, idx, char *, text)
{
   char s[10];
   int x, atr, chatr;

   context;
   atr = get_attr_handle(dcc[idx].nick);
   sprintf(s, "%d", dcc[idx].sock);
   chatr = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.chat->con_chan);
   if (chatr & CHANUSER_OP)
      atr |= USER_PSUEDOOP;
   if (chatr & CHANUSER_MASTER)
      atr |= USER_PSUMST;
   if (chatr & CHANUSER_OWNER)
      atr |= USER_PSUOWN;
   Tcl_SetVar(interp, "_n", s, 0);
   Tcl_SetVar(interp, "_a", text, 0);
   context;
   x = check_tcl_bind(&H_filt, text, atr, " $_n $_a",
	     MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE | BIND_WANTRET |
		      BIND_ALTER_ARGS);
   context;
   if ((x == BIND_EXECUTED) || (x == BIND_EXEC_LOG)) {
      if ((interp->result == NULL) || (!interp->result[0]))
	 return "";
      else
	 return interp->result;
   } else
      return text;
}

int check_tcl_flud PROTO5(char *, nick, char *, uhost, char *, hand,
			  char *, ftype, char *, chname)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", ftype, 0);
   Tcl_SetVar(interp, "_aa", chname, 0);
   context;
   x = check_tcl_bind(&H_flud, ftype, 0, " $_n $_uh $_h $_a $_aa",
		      MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
   context;
   return (x == BIND_EXEC_LOG);
}

int check_tcl_note PROTO3(char *, from, char *, to, char *, text)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_h", to, 0);
   Tcl_SetVar(interp, "_a", text, 0);
   context;
   x = check_tcl_bind(&H_note, to, 0, " $_n $_h $_a", MATCH_EXACT);
   context;
   return ((x == BIND_MATCHED) || (x == BIND_EXECUTED) || (x == BIND_EXEC_LOG));
}

void check_tcl_act PROTO3(char *, from, int, chan, char *, text)
{
   char s[10];
   context;
   sprintf(s, "%d", chan);
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   Tcl_SetVar(interp, "_aa", text, 0);
   context;
   check_tcl_bind(&H_act, text, 0, " $_n $_a $_aa", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_listen PROTO2(char *, cmd, int, idx)
{
   char s[10];
   int x;
   context;
   sprintf(s, "%d", idx);
   Tcl_SetVar(interp, "_n", s, 0);
   set_tcl_vars();
   context;
   x = Tcl_VarEval(interp, cmd, " $_n", NULL);
   context;
   if (x == TCL_ERROR)
      putlog(LOG_MISC, "*", "error on listen: %s", interp->result);
}

int check_tcl_wall PROTO2(char *, from, char *, msg)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", msg, 0);
   context;
   x = check_tcl_bind(&H_wall, msg, 0, " $_n $_a", MATCH_MASK | BIND_STACKABLE);
   context;
   if (x == BIND_EXEC_LOG) {
      putlog(LOG_WALL, "*", "!%s! %s", from, msg);
      return 1;
   } else
      return 0;
}

void tell_binds PROTO2(int, idx, char *, name)
{
   Tcl_HashEntry *he;
   Tcl_HashSearch srch;
   Tcl_HashTable *ht;
   int i, fnd = 0;
   tcl_cmd_t *tt;
   char typ[5], *s, *proc, flg[20];
   int kind, showall = 0;
   kind = get_bind_type(name);
   if (strcasecmp(name, "all") == 0)
      showall = 1;
   for (i = 0; i < BINDS; i++)
      if ((kind == (-1)) || (kind == i)) {
	 ht = gethashtable(i, NULL, typ);
	 for (he = Tcl_FirstHashEntry(ht, &srch); (he != NULL);
	      he = Tcl_NextHashEntry(&srch)) {
	    if (!fnd) {
	       dprintf(idx, "Command bindings:\n");
	       fnd = 1;
	       dprintf(idx, "  TYPE FLGS COMMAND              BINDING (TCL)\n");
	    }
	    tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
	    s = Tcl_GetHashKey(ht, he);
	    while (tt != NULL) {
	       proc = tt->func_name;
	       flags2str(tt->flags_needed, flg);
	       if ((showall) || (proc[0] != '*') || (strcmp(s, proc + 5) != 0) ||
		   (strncmp(typ, proc + 1, 3) != 0))
		  dprintf(idx, "  %-4s %-4s %-20s %s\n", typ, flg, s, tt->func_name);
	       tt = tt->next;
	    }
	 }
      }
   if (!fnd) {
      if (kind == (-1))
	 dprintf(idx, "No command bindings.\n");
      else
	 dprintf(idx, "No bindings for %s.\n", name);
   }
}

int tcl_getbinds PROTO2(int, kind, char *, name)
{
   Tcl_HashEntry *he;
   Tcl_HashSearch srch;
   Tcl_HashTable *ht;
   char *s;
   tcl_cmd_t *tt;
   ht = gethashtable(kind, NULL, NULL);
   for (he = Tcl_FirstHashEntry(ht, &srch); (he != NULL);
	he = Tcl_NextHashEntry(&srch)) {
      s = Tcl_GetHashKey(ht, he);
      if (strcasecmp(s, name) == 0) {
	 tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
	 while (tt != NULL) {
	    Tcl_AppendElement(interp, tt->func_name);
	    tt = tt->next;
	 }
	 return TCL_OK;
      }
   }
   return TCL_OK;
}

int call_tcl_func PROTO3(char *, name, int, idx, char *, args)
{
   char s[11];
   set_tcl_vars();
   sprintf(s, "%d", idx);
   Tcl_SetVar(interp, "_n", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   if (Tcl_VarEval(interp, name, " $_n $_a", NULL) == TCL_ERROR) {
      putlog(LOG_MISC, "*", "Tcl error [%s]: %s", name, interp->result);
      return -1;
   }
   return (atoi(interp->result));
}

void check_tcl_chjn PROTO6(char *, bot, char *, nick, int, chan, char, type,
			   int, sock, char *, host)
{
   int atr;
   char s[20], t[2], u[20];
   context;
   t[0] = type;
   t[1] = 0;
   switch (type) {
   case '*':
      atr = USER_OWNER;
      break;
   case '+':
      atr = USER_MASTER;
      break;
   case '@':
      atr = USER_GLOBAL;
      break;
   default:
      atr = 0;
   }
   sprintf(s, "%d", chan);
   sprintf(u, "%d", sock);
   Tcl_SetVar(interp, "_b", bot, 0);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_c", s, 0);
   Tcl_SetVar(interp, "_a", t, 0);
   Tcl_SetVar(interp, "_s", u, 0);
   Tcl_SetVar(interp, "_h", host, 0);
   context;
   check_tcl_bind(&H_chjn, s, atr, " $_b $_n $_c $_a $_s $_h",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_chpt PROTO3(char *, bot, char *, hand, int, sock)
{
   char u[20];
   context;
   sprintf(u, "%d", sock);
   Tcl_SetVar(interp, "_b", bot, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_s", u, 0);
   context;
   check_tcl_bind(&H_chpt, hand, 0, " $_b $_h $_s",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_bcst PROTO3(char *, from, int, chan, char *, text)
{
   char s[10];
   context;
   sprintf(s, "%d", chan);
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   Tcl_SetVar(interp, "_aa", text, 0);
   context;
   check_tcl_bind(&H_bcst, s, get_attr_handle(from),
		  " $_n $_a $_aa", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_time PROTO1(struct tm *, tm)
{
   char y[100];
   context;
   sprintf(y, "%d", tm->tm_min);
   Tcl_SetVar(interp, "_m", y, 0);
   sprintf(y, "%d", tm->tm_hour);
   Tcl_SetVar(interp, "_h", y, 0);
   sprintf(y, "%d", tm->tm_mday);
   Tcl_SetVar(interp, "_d", y, 0);
   sprintf(y, "%d", tm->tm_mon);
   Tcl_SetVar(interp, "_mo", y, 0);
   sprintf(y, "%d", tm->tm_year + 1900);
   Tcl_SetVar(interp, "_y", y, 0);
   sprintf(y, "%d %d %d %d %d", tm->tm_min, tm->tm_hour, tm->tm_mday,
	   tm->tm_mon, tm->tm_year + 1900);
   check_tcl_bind(&H_time, y, 0,
		  " $_m $_h $_d $_mo $_y", MATCH_MASK | BIND_STACKABLE);
}
