/*
 * tclhash.c -- rewritten tclhash.c & hash.c, handles (from tclhash.c):
 * bind and unbind
 * checking and triggering the various in-bot bindings
 * listing current bindings
 * adding/removing new binding tables
 * 
 * dprintf'ized, 4feb96
 * Now includes FREE OF CHARGE everything from hash.c, 'cause they
 * were exporting functions to each othe andr only for each other.
 * 
 * hash.c -- handles:
 * (non-Tcl) procedure lookups for msg/dcc/file commands
 * (Tcl) binding internal procedures to msg/dcc/file commands
 *
 *  dprintf'ized, 15nov95
*/

/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#include "main.h"
#include "chan.h"
#include "users.h"
#include "match.c"

extern Tcl_Interp * interp;
extern struct dcc_t * dcc;
extern int require_p;
extern struct chanset_t * chanset;
extern struct userrecord * userlist;
extern int debug_tcl;
extern int raw_binds;

static p_tcl_hash_list hash_table_list;
p_tcl_hash_list H_chat, H_act, H_bcst, H_join, H_part, H_topc, H_sign;
p_tcl_hash_list H_nick, H_mode, H_ctcp, H_ctcr, H_chon, H_chof, H_splt;
p_tcl_hash_list H_rejn, H_load, H_unld, H_link, H_disc;
static p_tcl_hash_list H_msg, H_dcc, H_pub, H_msgm, H_pubm, H_notc;
static p_tcl_hash_list H_filt, H_flud, H_note, H_wall, H_chjn, H_chpt;
static p_tcl_hash_list  H_kick, H_bot, H_time, H_raw;
p_tcl_hash_list H_rcvd, H_sent, H_fil;
int builtin_fil(), builtin_sentrcvd();
   
static int builtin_2char();
static int builtin_5char();
static int builtin_5int();
static int builtin_6char();
static int builtin_4char();
static int builtin_3char();
static int builtin_char();
static int builtin_chpt();
static int builtin_chjn();
static int builtin_idxchar();
static int builtin_charidx(); 
static int builtin_chat();
static int builtin_dcc();
static int hashtot = 0;

int expmem_tclhash () {
   struct tcl_hash_list * p = hash_table_list;
   int tot = hashtot;

   while (p) {
      tot += sizeof(struct tcl_hash_list);
      p=p->next;
   }
   return tot;
}


static void *tclcmd_alloc (int size)
{
   tcl_cmd_t *x = (tcl_cmd_t *) nmalloc(sizeof(tcl_cmd_t));
   hashtot += sizeof(tcl_cmd_t);
   x->func_name = (char *) nmalloc(size);
   hashtot += size;
   return (void *) x;
}

static void tclcmd_free (void * ptr)
{
   tcl_cmd_t *x = ptr;
   hashtot -= sizeof(tcl_cmd_t);
   hashtot -= strlen(x->func_name) + 1;
   nfree(x->func_name);
   nfree(x);
}

extern cmd_t C_dcc[], C_dcc_irc[], C_msg[], C_fil[];
static int tcl_bind();

void init_hash () {
   hash_table_list = NULL;
   context;
   Tcl_CreateCommand(interp, "bind", tcl_bind, (ClientData) 0, NULL);
   Tcl_CreateCommand(interp, "unbind", tcl_bind, (ClientData) 1, NULL);
   H_wall = add_hash_table("wall",HT_STACKABLE,builtin_2char);
   H_unld = add_hash_table("unld",HT_STACKABLE,builtin_char);
   H_time = add_hash_table("time",HT_STACKABLE,builtin_5int);
   H_topc = add_hash_table("topc",HT_STACKABLE,builtin_5char);
   H_splt = add_hash_table("splt",HT_STACKABLE,builtin_4char);
   H_sign = add_hash_table("sign",HT_STACKABLE,builtin_5char);
   H_rejn = add_hash_table("rejn",HT_STACKABLE,builtin_4char);
   H_raw = add_hash_table("raw",HT_STACKABLE,builtin_3char);
   H_pubm = add_hash_table("pubm",HT_STACKABLE,builtin_5char);
   H_pub = add_hash_table("pub",0,builtin_5char);
   H_part = add_hash_table("part",HT_STACKABLE,builtin_4char);
   H_note = add_hash_table("note",0,builtin_3char);
   H_notc = add_hash_table("notc",HT_STACKABLE,builtin_5char);
   H_nick = add_hash_table("nick",HT_STACKABLE,builtin_5char);
   H_msgm = add_hash_table("msgm",HT_STACKABLE,builtin_5char);
   H_msg = add_hash_table("msg",0,builtin_4char);
   H_mode = add_hash_table("mode",HT_STACKABLE,builtin_5char);
   H_load = add_hash_table("load",HT_STACKABLE,builtin_char);
   H_link = add_hash_table("link",HT_STACKABLE,builtin_2char);
   H_kick = add_hash_table("kick",HT_STACKABLE,builtin_6char);
   H_join = add_hash_table("join",HT_STACKABLE,builtin_4char);
   H_flud = add_hash_table("flud",HT_STACKABLE,builtin_5char);
   H_filt = add_hash_table("filt",HT_STACKABLE,builtin_idxchar);
   H_disc = add_hash_table("disc",HT_STACKABLE,builtin_char);
   H_dcc = add_hash_table("dcc",0,builtin_dcc);
   H_ctcr = add_hash_table("ctcr",0,builtin_6char);
   H_ctcp = add_hash_table("ctcp",0,builtin_6char);
   H_chpt = add_hash_table("chpt",HT_STACKABLE,builtin_chpt);
   H_chon = add_hash_table("chon",HT_STACKABLE,builtin_charidx);
   H_chof = add_hash_table("chof",HT_STACKABLE,builtin_charidx);
   H_chjn = add_hash_table("chjn",HT_STACKABLE,builtin_chjn);
   H_chat = add_hash_table("chat",HT_STACKABLE,builtin_chat); 
   H_bot = add_hash_table("bot",0,builtin_3char);
   H_bcst = add_hash_table("bsct",HT_STACKABLE,builtin_chat);
   H_act = add_hash_table("act",HT_STACKABLE,builtin_chat);
   context;
   add_builtins(H_dcc, C_dcc);
   add_builtins(H_dcc, C_dcc_irc);
#ifndef NO_IRC
   add_builtins(H_msg, C_msg);
#endif
   context;
}

void kill_hash () {
   rem_builtins(H_dcc, C_dcc);
#ifndef NO_IRC
   rem_builtins(H_msg, C_msg);
#endif
   while (hash_table_list) {
      del_hash_table(hash_table_list);
   }  
}

p_tcl_hash_list add_hash_table (char * nme,int flg,Function f) {
   p_tcl_hash_list p = hash_table_list, o = NULL;
   
   if (strlen(nme) > 4)
     nme[4] = 0;
   while (p) {
      int v = strcasecmp(p->name,nme);
      if (v == 0) 
	 /* repeat, just return old value */
	 return p;
      /* insert at start of list */
      if (v > 0) {
	 break;
      } else {
	 o = p;
	 p = p->next;
      }
   }
   p = nmalloc(sizeof(struct tcl_hash_list));
   Tcl_InitHashTable(&(p->table),TCL_STRING_KEYS);
   strcpy(p->name,nme);
   p->flags = flg;
   p->func = f;
   if (o) {
      p->next = o->next;
      o->next = p;
   } else {
      p->next = hash_table_list;
      hash_table_list = p;
   }
   putlog(LOG_DEBUG,"*","Allocated hash table %s (flags %d)",nme,flg);
   return p;
}
	
void del_hash_table (p_tcl_hash_list which) {
   p_tcl_hash_list p = hash_table_list, o = NULL;

   while (p) {
      if (p == which) {
	 Tcl_HashSearch srch;
	 Tcl_HashEntry *he;
	 tcl_cmd_t *tt, *tt1;
	 
	 context;
	 if (o) {
	    o->next = p->next;
	 } else {
	    hash_table_list = p->next;
	 }
	 /* cleanup code goes here */
	 for (he = Tcl_FirstHashEntry(&(p->table), &srch); (he != NULL);
	      he = Tcl_NextHashEntry(&srch)) {
	    tt = Tcl_GetHashValue(he);
	    while (tt != NULL) {
	       tt1 = tt->next;
	       tclcmd_free(tt);
	       tt = tt1;
	    }
	 }
	 putlog(LOG_DEBUG,"*","De-Allocated hash table %d",p->name);
	 Tcl_DeleteHashTable(&(p->table));
	 nfree(p);
	 return;
      }
      o = p;
      p = p->next;
   }
   putlog(LOG_DEBUG,"*","??? Tried to delete no listed hash table ???");
}

p_tcl_hash_list find_hash_table (char * nme) {
   p_tcl_hash_list p = hash_table_list;
   
   while (p) {
      int v = strcasecmp(p->name,nme);
      if (v == 0)
	return p;
      if (v > 0)
	return NULL;
      p = p->next;
   }
   return NULL;
}

static void dump_hash_tables (Tcl_Interp * irp) {
   p_tcl_hash_list p = hash_table_list;
   int i = 0;
   
   while (p) {
      if (i)
	Tcl_AppendResult(irp,", ",NULL);
      else
	i++;
      Tcl_AppendResult(irp,p->name,NULL);
      p = p->next;
   }
}

static int unbind_hash_entry (p_tcl_hash_list typ,
			  char * flags,
			  char * cmd, 
			  char * proc)
{
   tcl_cmd_t *tt, *last;
   Tcl_HashEntry *he;
   he = Tcl_FindHashEntry(&(typ->table), cmd);
   if (he == NULL)
      return 0;			/* no such binding */
   tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
   last = NULL;
   while (tt != NULL) {
      /* if procs are same, erase regardless of flags */
      if (strcasecmp(tt->func_name, proc) == 0) {
	 /* erase it */
	 if (last != NULL) {
	    last->next = tt->next;
	 } else if (tt->next == NULL) {
	   Tcl_DeleteHashEntry(he);
	 } else {
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
static int bind_hash_entry (p_tcl_hash_list typ,
			    char * flags,
			    char * cmd, 
			    char * proc)
{
   tcl_cmd_t *tt;
   int new;
   Tcl_HashEntry *he;
   Tcl_HashTable *ht;
   int stk = 0;
   char * p, c;
   
   if (proc[0] == '#') {
      putlog(LOG_MISC, "*", "Note: binding to '#' is obsolete.");
      return 0;
   }
   context;
   unbind_hash_entry(typ, flags, cmd, proc);	/* make sure we don't dup */
   tt = tclcmd_alloc(strlen(proc)+1);
   tt->flags.match = FR_OR;
   tt->flags.global = 0;
   tt->flags.chan = 0;
   if (flags) {
      c = 0;
      if ((p = strchr(flags,'|'))) {
	 c = '|';
      } else if ((p = strchr(flags,'&'))) {
	 tt->flags.match = FR_AND;
	 c = '&';
      }
      if (p) {
	 char x[100];
	 tt->flags.chan = str2chflags(p+1);
	 strncpy(x,flags,p-flags);
	 x[p-flags] = 0;
	 tt->flags.global = str2flags(x);
      } else 
	tt->flags.global = str2flags(flags);
   }
   tt->next = NULL;
   strcpy(tt->func_name, proc);
   ht = &(typ->table);
   he = Tcl_CreateHashEntry(ht, cmd, &new);
   if (!new) {
      tt->next = (tcl_cmd_t *) Tcl_GetHashValue(he);
      if (!stk) {
	 /* remove old one -- these are not stackable */
	 tclcmd_free(tt->next);
	 tt->next = NULL;
      }
   }
   context;
   Tcl_SetHashValue(he, tt);
   return 1;
}

static int tcl_getbinds (p_tcl_hash_list kind, char * name)
{
   Tcl_HashEntry *he;
   Tcl_HashSearch srch;
   Tcl_HashTable *ht;
   
   char *s;
   tcl_cmd_t *tt;
   ht = &(kind->table);
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

static int tcl_bind STDVAR
{
   p_tcl_hash_list tp;
  
   if ((long int) cd == 1) {
      BADARGS(5, 5, " type flags cmd/mask procname")
   } else {
      BADARGS(4, 5, " type flags cmd/mask ?procname?")
   }
   tp = find_hash_table(argv[1]);
   if (!tp) {
      Tcl_AppendResult(irp, "bad type, should be one of: ",NULL);
      dump_hash_tables(irp);
      return TCL_ERROR;
   }
   if ((long int) cd == 1) {
      if (!unbind_hash_entry(tp, argv[2], argv[3], argv[4])) {
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
      bind_hash_entry(tp, argv[2], argv[3], argv[4]);
   }
   Tcl_AppendResult(irp, argv[3], NULL);
   return TCL_OK;
}

int check_validity (char * nme,Function f) {
   char * p;
   p_tcl_hash_list t;
   
   if (*nme != '*')
     return 0;
   if (!(p = strchr(nme+1,':'))) 
     return 0;
   *p = 0;
   t = find_hash_table(nme+1);
   *p = ':';
   if (!t)
     return 0;
   if (t->func != f)
     return 0;
   return 1;
}

#define CHECKVALIDITY(a) if (!check_validity(argv[0],a)) { \
Tcl_AppendResult(irp, "bad builtin command call!", NULL); \
return TCL_ERROR; \
}

static int builtin_2char STDVAR {
   Function F = (Function) cd;

   BADARGS(3, 3, " nick msg");
   CHECKVALIDITY(builtin_2char);
   F(argv[1], argv[2]);
   return TCL_OK;
}

static int builtin_5char STDVAR {
   Function F = (Function) cd;

   BADARGS(6, 6, " nick user@host handle channel text");
   CHECKVALIDITY(builtin_5char);
   F(argv[1], argv[2],argv[3],argv[4],argv[5]);
   return TCL_OK;
}

static int builtin_5int STDVAR {
   Function F = (Function) cd;

   BADARGS(6, 6, " min hrs dom mon year");
   CHECKVALIDITY(builtin_5int);
   F(atoi(argv[1]), atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]));
   return TCL_OK;
}

static int builtin_6char STDVAR {
   Function F = (Function) cd;

   BADARGS(7, 7, " nick user@host handle desto/chan keyword/nick text");
   CHECKVALIDITY(builtin_6char);
   F(argv[1], argv[2],argv[3],argv[4],argv[5],argv[6]);
   return TCL_OK;
}

static int builtin_4char STDVAR {
   Function F = (Function) cd;

   BADARGS(5, 5, " nick uhost hand chan/param");
   CHECKVALIDITY(builtin_4char);
   F(argv[1], argv[2],argv[3],argv[4]);
   return TCL_OK;
}

static int builtin_3char STDVAR {
   Function F = (Function) cd;

   BADARGS(4, 4, " from type/to args");
   CHECKVALIDITY(builtin_3char);
   F(argv[1], argv[2],argv[3]);
   return TCL_OK;
}

static int builtin_char STDVAR {
   Function F = (Function) cd;

   BADARGS(2, 2, " handle");
   CHECKVALIDITY(builtin_char);
   F(argv[1]);
   return TCL_OK;
}

static int builtin_chpt STDVAR {
   Function F = (Function) cd;
   
   BADARGS(3, 3, " bot nick sock");
   CHECKVALIDITY(builtin_chpt);
   F(argv[1], argv[2],atoi(argv[3]));
   return TCL_OK;
}


static int builtin_chjn STDVAR {
   Function F = (Function) cd;
   
   BADARGS(6, 6, " bot nick chan# flag&sock host");
   CHECKVALIDITY(builtin_chjn);
   F(argv[1], argv[2],atoi(argv[3]),argv[4][0],argv[4][0]?atoi(argv[4]+1):0,
     argv[5]);
   return TCL_OK;
}

static int builtin_idxchar STDVAR {
   Function F = (Function) cd;
   int idx;
   char * r;
   
   BADARGS(3, 3, " idx args");
   CHECKVALIDITY(builtin_idxchar);
   idx = findidx(atoi(argv[1]));
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   r = (((char *(*)())F)(idx,argv[2]));
   Tcl_ResetResult(irp);
   Tcl_AppendResult(irp,r,NULL);
   return TCL_OK;
}

static int builtin_charidx STDVAR {
   Function F = (Function) cd;
   int idx;
   
   BADARGS(3, 3, " handle idx");
   CHECKVALIDITY(builtin_charidx);
   idx = findidx(atoi(argv[2]));
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   F(argv[1], idx);
   return TCL_OK;
}

static int builtin_chat STDVAR {
   Function F = (Function) cd;
   int ch;
   
   BADARGS(4, 4, " handle channel# text");
   CHECKVALIDITY(builtin_chat);
   ch = atoi(argv[2]);
   F(argv[1],ch,argv[3]);
   return TCL_OK;
}

static int builtin_dcc STDVAR {
   int idx;
   Function F = (Function) cd;
#ifdef EBUG
   int i;
   char s[1024];
#endif
   
   context;
   BADARGS(4, 4, " hand idx param");
   idx = findidx(atoi(argv[2]));
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   if (F == 0) {
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

/* trigger (execute) a proc */
static int trigger_bind (char * proc, char * param)
{
   int x;
   FILE * f = 0;
   
   if (debug_tcl) {
      f = fopen("DEBUG.TCL", "a");
      if (f != NULL)
	fprintf(f, "eval: %s%s\n", proc, param);
   }
   set_tcl_vars();
   context;
   x = Tcl_VarEval(interp, proc, param, NULL);
   context;
   if (x == TCL_ERROR) {
      if (debug_tcl && (f != NULL)) {
	 fprintf(f, "done eval. error.\n");
	 fclose(f);
      }
      if (strlen(interp->result) > 400)
	 interp->result[400] = 0;
      putlog(LOG_MISC, "*", "Tcl error [%s]: %s", proc, interp->result);
      return BIND_EXECUTED;
   } else {
      if (debug_tcl && (f != NULL)) {
	 fprintf(f, "done eval. ok.\n");
	 fclose(f);
      }
      if (strcmp(interp->result, "break") == 0)
	 return BIND_EXEC_BRK;
      return (atoi(interp->result) > 0) ? BIND_EXEC_LOG : BIND_EXECUTED;
   }
}

int flagrec_ok (struct flag_record * req,
		struct flag_record * have)
{
   if (!req->chan && !req->global)
     return 1;
   if (req->match == FR_AND) {
      if ((req->global & have->global) != req->global)
	return 0;
      if ((req->chan & have->chan) != req->chan)
	return 0;
      return 1;
   } else if (req->match == FR_OR) {
      int hav = have->global;
      
      if ((hav & USER_OWNER) && !(req->global & USER_BOT))
	return 1;
      if ((hav & USER_MASTER) && !(req->global & (USER_OWNER | USER_BOT)))
	return 1;
      if ((!require_p) && ((hav & USER_GLOBAL) || (have->chan & CHANUSER_OWNER)))
	hav |= USER_PARTY;
      if (hav & req->global)
	return 1;
      if (have->chan & req->chan)
	return 1;
      return 0;
   }
   return 1;
}

static int flagrec_eq (struct flag_record * req, struct flag_record * have)
{
   if (!req->chan && !req->global)
     return 1;
   if (req->match == FR_AND) {
      if ((req->global & have->global) != req->global)
	return 0;
      if ((req->chan & have->chan) != req->chan)
	return 0;
      return 1;
   } else if (req->match == FR_OR) {
      if (have->global & req->global)
	return 1;
      if (have->chan & req->chan)
	return 1;
      return 0;
   }
   return 1;
}

/* check a tcl binding and execute the procs necessary */
int check_tcl_bind (p_tcl_hash_list hash, char * match,
		    struct flag_record * atr,
		    char * param, int match_type)
{
   Tcl_HashSearch srch;
   Tcl_HashEntry *he;
   int cnt = 0;
   char *proc = NULL;
   tcl_cmd_t *tt;
   int f = 0, atrok, x;
   context;
   for (he = Tcl_FirstHashEntry(&(hash->table), &srch); (he != NULL) && (!f);
	he = Tcl_NextHashEntry(&srch)) {
      int ok = 0;
      context;
      switch (match_type & 0x03) {
      case MATCH_PARTIAL:
	 ok = (strncasecmp(match, Tcl_GetHashKey(&(hash->table), he), strlen(match)) == 0);
	 break;
      case MATCH_EXACT:
	 ok = (strcasecmp(match, Tcl_GetHashKey(&(hash->table), he)) == 0);
	 break;
      case MATCH_MASK:
	 ok = wild_match_per(Tcl_GetHashKey(&(hash->table), he), match);
	 break;
      }
      context;
      if (ok) {
	 tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
	 switch (match_type & 0x03) {
	 case MATCH_MASK:
	    /* could be multiple triggers */
	    while (tt != NULL) {
	       if (match_type & BIND_USE_ATTR) {
		  if (match_type & BIND_HAS_BUILTINS)
		    atrok = flagrec_ok(&tt->flags, atr);
		  else
		    atrok = flagrec_eq(&tt->flags, atr);
	       } else 
		 atrok = 1;
	       if (atrok) {
		  cnt++;
		  Tcl_SetVar(interp,"lastbind", match, TCL_GLOBAL_ONLY);
		  x = trigger_bind(tt->func_name, param);
		  if ((match_type & BIND_WANTRET) 
		      && !(match_type & BIND_ALTER_ARGS) &&
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
	    if (match_type & BIND_USE_ATTR) {
	       if (match_type & BIND_HAS_BUILTINS)
		 atrok = flagrec_ok(&tt->flags, atr);
	       else
		 atrok = flagrec_eq(&tt->flags, atr);
	    } else
	      atrok = 1;
	    if (atrok) {
	       cnt++;
	       proc = tt->func_name;
	       if (strcasecmp(match, Tcl_GetHashKey(&(hash->table), he)) == 0) {
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
   Tcl_SetVar(interp,"lastbind", match, TCL_GLOBAL_ONLY);
   return trigger_bind(proc, param);
}

static int flags_anywhere (char * handle)
{
   struct chanset_t * chan;
   int r = get_attr_handle(handle) & USER_CHANMASK;
   
   chan = chanset;
   while (chan != NULL) {
      r |= get_chanattr_handle(handle, chan->name);
      chan = chan->next;
   }
   return r;
}

void get_allattr_handle (char * handle,struct flag_record * fr)
{
   struct userrec *u;
   u = get_user_by_handle(userlist, handle);
   if (u) {
      fr->global = u->flags;
      fr->chan = flags_anywhere(handle);
   }
}

/* check for tcl-bound msg command, return 1 if found */
/* msg: proc-name <nick> <user@host> <handle> <args...> */
int check_tcl_msg (char * cmd, char * nick, 
		   char * uhost, char * hand,
		   char * args)
{
#ifndef NO_IRC
   struct flag_record fr = {0,0,0};
   int x;
   context;
   fr.global = get_attr_handle(hand);
   fr.chan = flags_anywhere(hand);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   x = check_tcl_bind(H_msg, cmd, &fr, " $_n $_uh $_h $_a",
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
int check_tcl_dcc (char * cmd, int idx, char * args)
{
   struct flag_record fr = {0,0,0};
   int x;
   char s[5];
   context;
   
   fr.global = get_attr_handle(dcc[idx].nick);
   fr.chan = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.chat->con_chan);
   sprintf(s, "%ld", dcc[idx].sock);
   Tcl_SetVar(interp, "_n", dcc[idx].nick, 0);
   Tcl_SetVar(interp, "_i", s, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   x = check_tcl_bind(H_dcc, cmd, &fr, " $_n $_i $_a",
		      MATCH_PARTIAL | BIND_USE_ATTR | BIND_HAS_BUILTINS);
   context;
   if (x == BIND_AMBIGUOUS) {
      dprintf(idx, MISC_AMBIGUOUS);
      return 0;
   }
   if (x == BIND_NOMATCH) {
      dprintf(idx, MISC_NOSUCHCMD);
      return 0;
   }
   if (x == BIND_EXEC_BRK)
      return 1;			/* quit */
   if (x == BIND_EXEC_LOG)
      putlog(LOG_CMDS, "*", "#%s# %s %s", dcc[idx].nick, cmd, args);
   return 0;
}

int check_tcl_pub (char * nick, char * from, char * chname, char * msg)
{
   struct flag_record fr = {0,0,0};
   int x;
   char args[512], cmd[512], host[161], handle[21];
   context;
   strcpy(args, msg);
   nsplit(cmd, args);
   sprintf(host, "%s!%s", nick, from);
   get_handle_by_host(handle, host);
   fr.global = get_attr_handle(handle);
   fr.chan = get_chanattr_handle(handle, chname);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", from, 0);
   Tcl_SetVar(interp, "_h", handle, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", args, 0);
   context;
   x = check_tcl_bind(H_pub, cmd, &fr, " $_n $_uh $_h $_a $_aa",
		      MATCH_EXACT | BIND_USE_ATTR);
   context;
   if (x == BIND_NOMATCH)
      return 0;
   if (x == BIND_EXEC_LOG)
      putlog(LOG_CMDS, chname, "<<%s>> !%s! %s %s", nick, handle, cmd, args);
   return 1;
}

void check_tcl_pubm (char * nick, char * from, char * chname, char * msg)
{
   char args[512], host[161], handle[21];
   struct flag_record fr = {0,0,0};
   context;
   strcpy(args, chname);
   strcat(args, " ");
   strcat(args, msg);
   sprintf(host, "%s!%s", nick, from);
   get_handle_by_host(handle, host);
   fr.global = get_attr_handle(handle);
   fr.chan = get_chanattr_handle(handle, chname);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", from, 0);
   Tcl_SetVar(interp, "_h", handle, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", msg, 0);
   context;
   check_tcl_bind(H_pubm, args, &fr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_notc (char * nick, char * uhost, char * hand, char * arg)
{
   struct flag_record fr = {0,0,0};
   context;
   fr.global = get_attr_handle(hand);
   fr.chan = flags_anywhere(hand);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", arg, 0);
   context;
   check_tcl_bind(H_notc, arg, &fr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_msgm (char * cmd, char * nick,
		     char * uhost, char * hand,
		     char * arg)
{
   struct flag_record fr = {0,0,0};
   char args[512];
   context;
   if (arg[0])
      sprintf(args, "%s %s", cmd, arg);
   else
      strcpy(args, cmd);
   fr.global = get_attr_handle(hand);
   fr.chan = flags_anywhere(hand);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", args, 0);
   context;
   check_tcl_bind(H_msgm, args, &fr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_joinpart (char * nick, char * uhost, char * hand, char * chname,
			 p_tcl_hash_list table)
{
   struct flag_record fr = {0,0,0};
   char args[512];
   context;
   sprintf(args, "%s %s!%s", chname, nick, uhost);
   fr.global = get_attr_handle(hand);
   fr.chan = get_chanattr_handle(hand, chname);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   context;
   check_tcl_bind(table, args, &fr, " $_n $_uh $_h $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_signtopcnickmode (char * nick, char * uhost, char * hand,
			     char * chname, char * reason,
			     p_tcl_hash_list table)
{
   struct flag_record fr = {0,0,0};
   char args[512];
   context;
   if (table == H_sign)
     sprintf(args, "%s %s!%s", chname, nick, uhost);
   else
     sprintf(args, "%s %s", chname, reason);
   fr.global = get_attr_handle(hand);
   fr.chan = get_chanattr_handle(hand, chname);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", reason, 0);
   context;
   check_tcl_bind(table, args, &fr, " $_n $_uh $_h $_a $_aa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_kick (char * nick, char * uhost, char * hand,
		     char * chname, char * dest, char * reason)
{
   struct flag_record fr = {0,0,0};
   char args[512];
   context;
   fr.global = get_attr_handle(hand);
   fr.chan = get_chanattr_handle(hand, chname);
   sprintf(args, "%s %s", chname, dest);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", chname, 0);
   Tcl_SetVar(interp, "_aa", dest, 0);
   Tcl_SetVar(interp, "_aaa", reason, 0);
   context;
   check_tcl_bind(H_kick, args, &fr, " $_n $_uh $_h $_a $_aa $_aaa",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

/* return 1 if processed */
int check_tcl_raw (char * from, char * code, char * msg)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", code, 0);
   Tcl_SetVar(interp, "_aa", msg, 0);
   context;
   x = check_tcl_bind(H_raw, code, 0, " $_n $_a $_aa",
		      MATCH_EXACT | BIND_STACKABLE | BIND_WANTRET);
   context;
   return (x == BIND_EXEC_LOG);
}

void check_tcl_bot (char * nick, char * code, char * param)
{
   context;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_h", code, 0);
   Tcl_SetVar(interp, "_a", param, 0);
   context;
   check_tcl_bind(H_bot, code, 0, " $_n $_h $_a", MATCH_EXACT);
   context;
}

int check_tcl_ctcpr (char * nick, char * uhost, char * hand, char * dest,
		    char * keyword, char * args, p_tcl_hash_list table)
{
   struct flag_record fr = {0,0,0};
   int x;
   context;
   fr.global = get_attr_handle(hand);
   fr.chan = flags_anywhere(hand);
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", dest, 0);
   Tcl_SetVar(interp, "_aa", keyword, 0);
   Tcl_SetVar(interp, "_aaa", args, 0);
   context;
   x = check_tcl_bind(table, keyword, &fr, " $_n $_uh $_h $_a $_aa $_aaa",
		      MATCH_MASK | BIND_USE_ATTR | 
		      (table == H_ctcr) ?BIND_WANTRET : 0);
   context;
   return (x == BIND_EXEC_LOG) || (table == H_ctcr);
/*  return ((x==BIND_MATCHED)||(x==BIND_EXECUTED)||(x==BIND_EXEC_LOG)); */
}

void check_tcl_chonof (char * hand, int idx, p_tcl_hash_list table)
{
   struct flag_record fr = {0,0,0};
   char s[20];
   context;
   sprintf(s, "%d", idx);
   fr.global = get_attr_handle(hand);
   fr.chan = flags_anywhere(hand);
   Tcl_SetVar(interp, "_n", hand, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   context;
   check_tcl_bind(table, hand, &fr, " $_n $_a",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_chatactbcst (char * from, int chan, char * text,
			    p_tcl_hash_list ht)
{
   char s[10];
   context;
   sprintf(s, "%d", chan);
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", s, 0);
   Tcl_SetVar(interp, "_aa", text, 0);
   context;
   check_tcl_bind(ht, text, 0, " $_n $_a $_aa", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_link (char * bot, char * via)
{
   context;
   Tcl_SetVar(interp, "_n", bot, 0);
   Tcl_SetVar(interp, "_a", via, 0);
   context;
   check_tcl_bind(H_link, bot, 0, " $_n $_a", MATCH_MASK | BIND_STACKABLE);
   context;
}

int check_tcl_wall (char * from, char * msg)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_a", msg, 0);
   context;
   x = check_tcl_bind(H_wall, msg, 0, " $_n $_a", MATCH_MASK | BIND_STACKABLE);
   context;
   if (x == BIND_EXEC_LOG) {
      putlog(LOG_WALL, "*", "!%s! %s", from, msg);
      return 1;
   } else
      return 0;
}

void check_tcl_disc (char * bot)
{
   context;
   Tcl_SetVar(interp, "_n", bot, 0);
   context;
   check_tcl_bind(H_disc, bot, 0, " $_n", MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_loadunld (char * mod, p_tcl_hash_list table)
{
   context;
   Tcl_SetVar(interp, "_n", mod, 0);
   context;
   check_tcl_bind(table, mod, 0, " $_n", MATCH_MASK | BIND_STACKABLE);
   context;
}

char *check_tcl_filt (int idx, char * text)
{
   char s[10];
   int x;
   struct flag_record fr = {0,0,0};


   context;
   fr.global = get_attr_handle(dcc[idx].nick);
   sprintf(s, "%ld", dcc[idx].sock);
   fr.chan = get_chanattr_handle(dcc[idx].nick, dcc[idx].u.chat->con_chan);
   Tcl_SetVar(interp, "_n", s, 0);
   Tcl_SetVar(interp, "_a", text, 0);
   context;
   x = check_tcl_bind(H_filt, text, &fr, " $_n $_a",
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

int check_tcl_flud (char * nick, char * uhost, char * hand,
		    char * ftype, char * chname)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", nick, 0);
   Tcl_SetVar(interp, "_uh", uhost, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_a", ftype, 0);
   Tcl_SetVar(interp, "_aa", chname, 0);
   context;
   x = check_tcl_bind(H_flud, ftype, 0, " $_n $_uh $_h $_a $_aa",
		      MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
   context;
   return (x == BIND_EXEC_LOG);
}

int check_tcl_note (char * from, char * to, char * text)
{
   int x;
   context;
   Tcl_SetVar(interp, "_n", from, 0);
   Tcl_SetVar(interp, "_h", to, 0);
   Tcl_SetVar(interp, "_a", text, 0);
   context;
   x = check_tcl_bind(H_note, to, 0, " $_n $_h $_a", MATCH_EXACT);
   context;
   return ((x == BIND_MATCHED) || (x == BIND_EXECUTED) || (x == BIND_EXEC_LOG));
}

void check_tcl_listen (char * cmd, int idx)
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

void check_tcl_chjn (char * bot, char * nick, int chan, char type,
			   int sock, char * host)
{
   struct flag_record fr = {0,0,0};
   char s[20], t[2], u[20];
   context;
   t[0] = type;
   t[1] = 0;
   switch (type) {
   case '*':
      fr.global = USER_OWNER;
      break;
   case '+':
      fr.global = USER_MASTER;
      break;
   case '@':
      fr.global = USER_GLOBAL;
      break;
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
   check_tcl_bind(H_chjn, s, &fr, " $_b $_n $_c $_a $_s $_h",
		  MATCH_MASK | BIND_STACKABLE);
   context;
}

void check_tcl_chpt (char * bot, char * hand, int sock)
{
   char u[20];
   context;
   sprintf(u, "%d", sock);
   Tcl_SetVar(interp, "_b", bot, 0);
   Tcl_SetVar(interp, "_h", hand, 0);
   Tcl_SetVar(interp, "_s", u, 0);
   context;
   check_tcl_bind(H_chpt, hand, 0, " $_b $_h $_s",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
   context;
}

void check_tcl_time (struct tm * tm)
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
   context;
   check_tcl_bind(H_time, y, 0,
		  " $_m $_h $_d $_mo $_y", MATCH_MASK | BIND_STACKABLE);
   context;
}

void tell_binds (int idx, char * name) {
   Tcl_HashEntry *he;
   Tcl_HashSearch srch;
   Tcl_HashTable *ht;
   p_tcl_hash_list p, kind;
   int fnd = 0;
   tcl_cmd_t *tt;
   char *s, *proc, flg[100];
   int showall = 0;
  
   context;
   s = strchr(name,' ');
   if (s) {
      *s = 0;
      s++;
   } else {
      s = name;
   }
   kind = find_hash_table(name);
   if (strcasecmp(s, "all") == 0)
      showall = 1;
   for (p  = hash_table_list;p; p = p->next)
      if (!kind || (kind == p)) {
	 ht = &(p->table);
	 for (he = Tcl_FirstHashEntry(ht, &srch); (he != NULL);
	      he = Tcl_NextHashEntry(&srch)) {
	    if (!fnd) {
	       dprintf(idx, MISC_CMDBINDS);
	       fnd = 1;
	       dprintf(idx, "  TYPE FLGS     COMMAND              BINDING (TCL)\n");
	    }
	    tt = (tcl_cmd_t *) Tcl_GetHashValue(he);
	    s = Tcl_GetHashKey(ht, he);
	    while (tt != NULL) {
	       proc = tt->func_name;
	       flags2str(tt->flags.global, flg);
	       switch(tt->flags.match) {
		case FR_OR:
		  strcat(flg,"|");
		  break;
		case FR_AND:
		  strcat(flg,"&");
		  break;
	       }
	       chflags2str(tt->flags.chan, flg+strlen(flg));
	       context;
	       if ((showall) || (proc[0] != '*') || (strcmp(s, proc + 5) != 0) ||
		   (strncmp(p->name, proc + 1, 3) != 0))
		  dprintf(idx, "  %-4s %-8s %-20s %s\n", p->name, flg, s, tt->func_name);
	       tt = tt->next;
	    }
	 }
      }
   if (!fnd) {
      if (!kind)
	 dprintf(idx, "No command bindings.\n");
      else
	 dprintf(idx, "No bindings for %s.\n", name);
   }
}

/* bring the default msg/dcc/fil commands into the Tcl interpreter */
int add_builtins (p_tcl_hash_list table, cmd_t * cc)
{
   int i,k;
   char p[1024],*l;

   context;
   i = 0;
   while (cc[i].name != NULL) {
      sprintf(p,"*%s:%s",table->name,cc[i].funcname?cc[i].funcname:cc[i].name);
      l = (char *)nmalloc(Tcl_ScanElement(p,&k));
      Tcl_ConvertElement(p,l,k|TCL_DONT_USE_BRACES);
      Tcl_CreateCommand(interp, p, table->func,
			(ClientData) cc[i].func, NULL);
      bind_hash_entry(table,cc[i].flags,cc[i].name,l);
      nfree(l);
      /* create command entry in Tcl interpreter */
      i++;
   }
   return i;
}

/* bring the default msg/dcc/fil commands into the Tcl interpreter */
int rem_builtins (p_tcl_hash_list table, cmd_t * cc)
{
   int i,k;
   char p[1024], *l;

   i = 0;
   while (cc[i].name != NULL) {
      sprintf(p,"*%s:%s",table->name,cc[i].funcname?cc[i].funcname:cc[i].name);
      l = (char *)nmalloc(Tcl_ScanElement(p,&k));
      Tcl_ConvertElement(p,l,k|TCL_DONT_USE_BRACES);
      Tcl_DeleteCommand(interp, p);
      unbind_hash_entry(table,cc[i].flags,cc[i].name,l);
      nfree(l);
      i++;
   }
   return i;
}
