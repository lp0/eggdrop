/*
 * assoc.c - the assoc module, moved here mainly from botnet.c for module
 * work
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#define MAKING_ASSOC
#define MODULE_NAME "assoc"
#include "../module.h"
#include <stdlib.h>

/* channel name-number associations */
#ifdef MODULES
Function *global;

static
#endif
assoc_t *assoc = NULL;

#ifdef MODULES
static int assoc_expmem()
{
#else
int expmem_assoc()
{
#endif
   assoc_t *a = assoc;
   int size = 0;

   modcontext;
   while (a != NULL) {
      size += sizeof(assoc_t);
      a = a->next;
   }
   return size;
}

#ifdef MODULES
static
#endif
void dump_bot_assoc(int idx)
{
   assoc_t *a = assoc;

   modcontext;
   while (a != NULL) {
      if (a->name[0])
	 modprintf(idx, "assoc Y %d %s\n", a->channel, a->name);
      a = a->next;
   }
}

#ifdef MODULES
static
#endif
void kill_assoc PROTO1(int, chan)
{
   assoc_t *a = assoc, *last = NULL;

   modcontext;
   while (a != NULL) {
      if (a->channel == chan) {
	 if (last != NULL)
	    last->next = a->next;
	 else
	    assoc = a->next;
	 modfree(a);
	 a = NULL;
      } else {
	 last = a;
	 a = a->next;
      }
   }
}

#ifdef MODULES
static
#endif
void kill_all_assoc()
{
   assoc_t *a = assoc, *x;

   modcontext;
   while (a != NULL) {
      x = a;
      a = a->next;
      modfree(x);
   }
   assoc = NULL;
}

#ifdef MODULES
static
#endif
void add_assoc PROTO2(char *, name, int, chan)
{
   assoc_t *a = assoc, *b, *old = NULL;

   modcontext;
   while (a != NULL) {
      if ((name[0] != 0) && (strcasecmp(a->name, name) == 0)) {
	 kill_assoc(a->channel);
	 add_assoc(name, chan);
	 return;
      }
      if (a->channel == chan) {
	 strncpy(a->name, name, 20);
	 a->name[20] = 0;
	 return;
      }
      a = a->next;
   }
   /* add in numerical order */
   a = assoc;
   while (a != NULL) {
      if (a->channel > chan) {
	 b = (assoc_t *) modmalloc(sizeof(assoc_t));
	 b->next = a;
	 b->channel = chan;
	 strncpy(b->name, name, 20);
	 b->name[20] = 0;
	 if (old == NULL)
	    assoc = b;
	 else
	    old->next = b;
	 return;
      }
      old = a;
      a = a->next;
   }
   /* add at the end */
   b = (assoc_t *) modmalloc(sizeof(assoc_t));
   b->next = NULL;
   b->channel = chan;
   strncpy(b->name, name, 20);
   b->name[20] = 0;
   if (old == NULL)
      assoc = b;
   else
      old->next = b;
}

#ifdef MODULES
static
#endif
int get_assoc PROTO1(char *, name)
{
   assoc_t *a = assoc;

   modcontext;
   while (a != NULL) {
      if (strcasecmp(a->name, name) == 0)
	 return a->channel;
      a = a->next;
   }
   return -1;
}

#ifdef MODULES
static
#endif
char *get_assoc_name PROTO1(int, chan)
{
   assoc_t *a = assoc;

   modcontext;
   while (a != NULL) {
      if (a->channel == chan)
	 return a->name;
      a = a->next;
   }
   return NULL;
}

#ifdef MODULES
static
#endif
void dump_assoc PROTO1(int, idx)
{
   assoc_t *a = assoc;

   modcontext;
   if (a == NULL) {
      modprintf(idx, "No channel names.\n");
      return;
   }
   modprintf(idx, " Chan  Name\n");
   while (a != NULL) {
      if (a->name[0])
	 modprintf(idx, "%c%5d %s\n", (a->channel < 100000) ? ' ' : '*', a->channel % 100000,
		   a->name);
      a = a->next;
   }
   return;
}


#ifdef MODULES
static
#endif
int cmd_assoc PROTO2(int, idx, char *, par)
{
   char num[512];
   int chan;

   modcontext;
   if (!par[0]) {
      putlog(LOG_CMDS, "*", "#%s# assoc", dcc[idx].nick);
      dump_assoc(idx);
      return 0;
   } else if (!(get_attr_handle(dcc[idx].nick) & (USER_BOTMAST | USER_MASTER))) {
      modprintf(idx, "What? You need '.help'.\n");
      return 0;
   }
   nsplit(num, par);
   if (num[0] == '*') {
      chan = 100000 + atoi(num + 1);
      if (chan < 100000 || chan > 199999) {
	 modprintf(idx, "Channel # out of range: must be *0-*99999\n");
	 return 0;
      }
   } else {
      chan = atoi(num);
      if (chan == 0) {
	 modprintf(idx, "You can't name the main party line; it's just a party line.\n");
	 return 0;
      }
      if ((chan < 1) || (chan > 99999)) {
	 modprintf(idx, "Channel # out of range: must be 1-99999\n");
	 return 0;
      }
   }
   if (!par[0]) {
      /* remove an association */
      if (get_assoc_name(chan) == NULL) {
	 modprintf(idx, "Channel %s%d has no name.\n",
		   (chan < 100000) ? "" : "*", chan % 100000);
	 return 0;
      }
      kill_assoc(chan);
      putlog(LOG_CMDS, "*", "#%s# assoc %d", dcc[idx].nick, chan);
      modprintf(idx, "Okay, removed name for channel %s%d.\n",
		(chan < 100000) ? "" : "*", chan % 100000);
      chanout2(chan, "%s removed this channel's name.\n", dcc[idx].nick);
      if (chan < 100000)
	 tandout_but(-1, "assoc %d 0\n", chan);
      return 0;
   }
   if (strlen(par) > 20) {
      modprintf(idx, "Channel's name can't be that long (20 chars max).\n");
      return 0;
   }
   if ((par[0] >= '0') && (par[0] <= '9')) {
      modprintf(idx, "First character of the channel name can't be a digit.\n");
      return 0;
   }
   add_assoc(par, chan);
   putlog(LOG_CMDS, "*", "#%s# assoc %d %s", dcc[idx].nick, chan, par);
   modprintf(idx, "Okay, channel %s%d is '%s' now.\n",
	     (chan < 100000) ? "" : "*", chan % 100000, par);
   chanout2(chan, "%s named this channel '%s'\n", dcc[idx].nick, par);
   if (chan < 100000)
      tandout("assoc %d %s\n", chan, par);
   return 0;
}

#ifdef MODULES
static
#endif
int tcl_killassoc STDVAR
{
   int chan;

    modcontext;
    BADARGS(2, 2, " chan");
    chan = atoi(argv[1]);
   if ((chan < 1) || (chan > 199999)) {
      Tcl_AppendResult(irp, "invalid channel #", NULL);
      return TCL_ERROR;
   }
   kill_assoc(chan);
   tandout("assoc %d 0\n", chan);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_assoc STDVAR
{
   int chan;
   char name[21], *p;

    modcontext;
    BADARGS(2, 3, " chan ?name?");
   if ((argc == 2) && ((argv[1][0] < '0') || (argv[1][0] > '9'))) {
      chan = get_assoc(argv[1]);
      if (chan == -1)
	 Tcl_AppendResult(irp, "", NULL);
      else {
	 sprintf(name, "%d", chan);
	 Tcl_AppendResult(irp, name, NULL);
      } return TCL_OK;
   }
   chan = atoi(argv[1]);
   if ((chan < 1) || (chan > 199999)) {
      Tcl_AppendResult(irp, "invalid channel #", NULL);
      return TCL_ERROR;
   }
   if (argc == 3) {
      strncpy(name, argv[2], 20);
      name[20] = 0;
      add_assoc(name, chan);
      tandout("assoc %d %s\n", chan, name);
   }
   p = get_assoc_name(chan);
   if (p == NULL)
      name[0] = 0;
   else
      strcpy(name, p);
   Tcl_AppendResult(irp, name, NULL);
   return TCL_OK;
}

#ifdef MODULES
static void do_bot_assoc PROTO2(int, idx, char *, par)
#else
void bot_assoc PROTO2(int, idx, char *, par)
#endif
{
   char s[1024], *s1;
   int linking = 0;

   modcontext;
   nsplit(s, par);
   if (s[0] == 'Y') {
      linking = 1;
      nsplit(s, par);
   }
   if ((atoi(s) < 1) || (atoi(s) > 99999))
      return;
   s1 = get_assoc_name(atoi(s));
   if (linking && ((s1 == NULL) || (s1[0] == 0) ||
		   (get_attr_handle(dcc[idx].nick) & BOT_HUB))) {
      add_assoc(par, atoi(s));
      tandout_but(idx, "assoc %s %s\n", s, par);
   } else if (par[0] == '0') {
      s1 = get_assoc_name(atoi(s));
      if (s1 != NULL) {
	 if (s1[0] == 0)
	    kill_assoc(atoi(s));
	 else
	    add_assoc("", atoi(s));
      }
      tandout_but(idx, "assoc %s 0\n", s);
   } else if (get_assoc(par) != atoi(s)) {
      /* new one i didn't know about -- pass it on */
      s1 = get_assoc_name(atoi(s));
      if (s1 != NULL) {
	 if (s1[0] == 0) {
	    /* recently killed assoc */
	    tandout_but(idx, "assoc %s 0\n", s);
	    kill_assoc(atoi(s));
	    return;
	 }
      }
      add_assoc(par, atoi(s));
      tandout_but(idx, "assoc %s %s\n", s, par);
   }
}

#ifdef MODULES
/* a report on the module status */
static void assoc_report PROTO1(int, idx)
{
   assoc_t *a = assoc;
   int size = 0, count = 0;;

   modcontext;
   while (a != NULL) {
      count++;
      size += sizeof(assoc_t);
      a = a->next;
   }
   modprintf(idx, "     %d assocs using %d bytes\n",
	     count, size);
}

static cmd_t mydcc[] =
{
   {"assoc", '-', cmd_assoc},
   {0, 0, 0}
};

static tcl_cmds mytcl[] =
{
   {"assoc", tcl_assoc},
   {"killassoc", tcl_killassoc},
   {0, 0}
};

static char *assoc_close()
{
   modcontext;
   rem_builtins(BUILTIN_DCC, mydcc);
   module_undepend(MODULE_NAME);
   rem_tcl_commands(mytcl);
   del_hook(HOOK_GET_ASSOC_NAME, get_assoc_name);
   del_hook(HOOK_GET_ASSOC, get_assoc);
   del_hook(HOOK_DUMP_ASSOC_BOT, dump_bot_assoc);
   del_hook(HOOK_KILL_ASSOCS, kill_all_assoc);
   del_hook(HOOK_BOT_ASSOC, do_bot_assoc);
   return NULL;
}

char *assoc_start PROTO((Function *));

static Function assoc_table[] =
{
   (Function) assoc_start,
   (Function) assoc_close,
   (Function) assoc_expmem,
   (Function) assoc_report,
};

char *assoc_start PROTO1(Function *, egg_func_table)
{
   global = egg_func_table;
   modcontext;
   module_register(MODULE_NAME, assoc_table, 1, 0);
   module_depend(MODULE_NAME, "eggdrop", 101, 4);
   add_hook(HOOK_GET_ASSOC_NAME, get_assoc_name);
   add_hook(HOOK_GET_ASSOC, get_assoc);
   add_hook(HOOK_DUMP_ASSOC_BOT, dump_bot_assoc);
   add_hook(HOOK_KILL_ASSOCS, kill_all_assoc);
   add_hook(HOOK_BOT_ASSOC, do_bot_assoc);
   add_builtins(BUILTIN_DCC, mydcc);
   add_tcl_commands(mytcl);
   return NULL;
}
#endif
