/*
 * modules.c - support for code modules in eggdrop
 * by Darrin Smith (beldin@light.iinet.net.au)
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "modules.h"
#ifndef HAVE_DLOPEN
#include "you/need/dlopen/to/compile/with/dynamic/modules"
#else
#ifdef DLOPEN_MUST_BE_1
#define RTLD_FLAGS 1
#else
#define RTLD_FLAGS 2
#endif
void *dlopen PROTO((const char *, int));
char *dlerror();
int dlclose PROTO((void *));
extern struct dcc_t dcc[];
#endif
#include "users.h"
int cmd_note();

/* from other areas */
extern int egg_numver;
extern Tcl_Interp *interp;
extern Tcl_HashTable H_fil, H_rcvd, H_sent;
extern struct userrec *userlist;
extern int dcc_total;
extern char tempdir[];
extern char botnetnick[];
extern int reserved_port;
extern char botname[];

/* the null functions */
void null_func()
{
}

char *charp_func()
{
   return NULL;
}
int minus_func()
{
   return -1;
}

/* various hooks & things */
/* the REAL hooks, when these are called, a return of 0 indicates unhandled
 * 1 is handled */
struct hook_entry {
   struct hook_entry *next;
   int (*func) ();
} *hook_list[REAL_HOOKS];

/* these are obscure ones that I hope to neaten eventually :/ */
void (*kill_all_assoc) () = null_func;
void (*dump_bot_assoc) PROTO((int)) = null_func;
char *(*get_assoc_name) PROTO((int)) = charp_func;
int (*get_assoc) PROTO((char *)) = minus_func;
void (*do_bot_assoc) PROTO((int, char *)) = null_func;
void (*encrypt_pass) PROTO((char *, char *)) = 0;

module_entry *module_list;
dependancy * dependancy_list = NULL;

void init_modules()
{
   int i;

   context;
   module_list = nmalloc(sizeof(module_entry));
   module_list->name = nmalloc(8);
   strcpy(module_list->name, "eggdrop");
   module_list->major = (egg_numver) / 10000;
   module_list->minor = ((egg_numver) / 100) % 100;
   module_list->hand = NULL;
   module_list->next = NULL;
   module_list->funcs = NULL;
   for (i = 0; i < REAL_HOOKS; i++)
      hook_list[i] = NULL;
}

int expmem_modules(int y)
{
   int c = 0;
   int i;
   module_entry *p = module_list;
   dependancy *d = dependancy_list;
   Function *f;

   context;
   for (i = 0; i < REAL_HOOKS; i++) {
      struct hook_entry *q = hook_list[i];
      while (q) {
	 c += sizeof(struct hook_entry);
	 q = q->next;
      }
   }
   while (d) {
      c += sizeof(dependancy);
      d = d->next;
   }
   while (p) {
      c += sizeof(module_entry);
      c += strlen(p->name) + 1;
      f = p->funcs;
      if ((f != NULL) && !y)
	 c += (int) (f[MODCALL_EXPMEM] ());
      p = p->next;
   }
   return c;
}

void mod_context PROTO3(char *, module, char *, file, int, line)
{
   char x[100];
   sprintf(x, "%s:%s", module, file);
   x[30] = 0;
#ifdef EBUG
   cx_ptr = ((cx_ptr + 1) & 15);
   strcpy(cx_file[cx_ptr], x);
   cx_line[cx_ptr] = line;
#else
   strcpy(cx_file, x);
   cx_line = line;
#endif
}

int register_module PROTO4(char *, name, Function *, funcs,
			   int, major, int, minor)
{
   module_entry *p = module_list;
   context;
   while (p) {
      if ((p->name != NULL) && (strcasecmp(name, p->name) == 0)) {
	 p->major = major;
	 p->minor = minor;
	 p->funcs = funcs;
	 return 1;
      }
      p = p->next;
   }
   return 0;
}

Function global_funcs[] =
{
   (Function) dprintf,
   (Function) mod_context,
   (Function) mod_malloc,
   (Function) mod_free,

   (Function) register_module,
   (Function) find_module,
   (Function) depend,
   (Function) undepend,

   (Function) add_hook,
   (Function) del_hook,
   (Function) get_next_hook,
   (Function) call_hook_i,

   (Function) load_module,
   (Function) unload_module,

   (Function) add_tcl_commands,
   (Function) rem_tcl_commands,
   (Function) add_tcl_ints,
   (Function) rem_tcl_ints,
   (Function) add_tcl_strings,
   (Function) rem_tcl_strings,

   (Function) putlog,
   (Function) chanout2,
   (Function) tandout,
   (Function) tandout_but,

   (Function) dcc,
   (Function) nsplit,
   (Function) add_builtins,
   (Function) rem_builtins,

   (Function) get_attr_handle,
   (Function) get_chanattr_handle,
   (Function) get_allattr_handle,

   (Function) pass_match_by_handle,

   (Function) new_dcc,
   (Function) new_fork,
   (Function) lostdcc,
   (Function) killsock,

   (Function) check_tcl_bind,
   (Function) & dcc_total,
   (Function) tempdir,
   (Function) botnetnick,

   (Function) rmspace,
   (Function) movefile,
   (Function) copyfile,
   (Function) check_tcl_filt,

   (Function) detect_dcc_flood,
   (Function) get_handle_by_host,
   (Function) stats_add_upload,
   (Function) stats_add_dnload,

   (Function) cancel_user_xfer,
   (Function) set_handle_dccdir,
   (Function) & userlist,
   (Function) my_memcpy,

   (Function) dump_resync,
   (Function) flush_tbuf,
   (Function) answer,
   (Function) neterror,

   (Function) tputs,
   (Function) wild_match_file,
   (Function) flags2str,
   (Function) str2flags,

   (Function) flags_ok,
   (Function) chatout,
   (Function) iptolong,
   (Function) getmyip,

   (Function) & reserved_port,
   (Function) set_files,
   (Function) set_handle_uploads,
   (Function) set_handle_dnloads,

   (Function) is_user,
   (Function) open_listen,
   (Function) get_attr_host,
   (Function) my_atoul,

   (Function) get_handle_dccdir,
   (Function) getsock,
   (Function) open_telnet_dcc,
   (Function) do_boot,

   (Function) botname,
   (Function) show_motd,
   (Function) telltext,
   (Function) tellhelp,

   (Function) splitc,
   (Function) nextbot,
   (Function) in_chain,
   (Function) findidx,

   (Function) & interp,
   (Function) get_user_by_handle,
   (Function) finish_share,
   (Function) cmd_note,

   (Function) & H_fil,
   (Function) & H_rcvd,
   (Function) & H_sent,
   (Function) open_telnet,

   (Function) fixcolon,
};

char *load_module PROTO1(char *, name)
{
   module_entry *p;
   char workbuf[1024];
   void *hand;
   char *e;
   Function f;

   context;
   if (find_module(name, 0, 0) != NULL)
      return "Already loaded.";
   if (getcwd(workbuf, 1024) == NULL)
      return "can't determine current directory.";
   sprintf(&(workbuf[strlen(workbuf)]), "/%s.so", name);
   hand = dlopen(workbuf, RTLD_FLAGS);
   if (hand == NULL)
      return dlerror();
   sprintf(workbuf, "%s_start", name);
   f = dlsym(hand, workbuf);
   if (f == NULL) {		/* some OS's need the _ */
      sprintf(workbuf, "_%s_start", name);
      f = dlsym(hand, workbuf);
      if (f == NULL) {
	 return "No start function defined.";
      }
   }
   p = nmalloc(sizeof(module_entry));
   if (p == NULL)
      return "Malloc error";
   p->next = module_list;
   module_list = p;
   module_list->name = nmalloc(strlen(name) + 1);
   strcpy(module_list->name, name);
   module_list->major = 0;
   module_list->minor = 0;
   module_list->hand = hand;
   e = (char *) (f(global_funcs));
   if (e != NULL)
      return e;
   putlog(LOG_MISC, "*", "Module %s loaded", name);
   return NULL;
}

char *unload_module PROTO2(char *, name,char *,user)
{
   module_entry *p = module_list, *o = NULL;
   char *e;
   Function *f;

   context;
   while (p) {
      if ((p->name != NULL) && (strcmp(name, p->name) == 0)) {
	 dependancy *d = dependancy_list;
	 
	 while (d!=NULL) {
	    if (d->needed == p) {
	       return "Needed by another module";
	    }
	    d=d->next;
	 }
	 f = p->funcs;
	 if ((f != NULL) && (f[MODCALL_CLOSE] == NULL))
	    return "No close function";
	 if (f != NULL) {
	    e = (char *) (f[MODCALL_CLOSE] (user));
	    if (e != NULL)
	       return e;
	    dlclose(p->hand);
	 }
	 nfree(p->name);
	 if (o == NULL) {
	    module_list = p->next;
	 } else {
	    o->next = p->next;
	 }
	 nfree(p);
	 putlog(LOG_MISC, "*", "Module %s unloaded", name);
	 return NULL;
      }
      o = p;
      p = p->next;
   }
   return "No such module";
}

module_entry *find_module PROTO3(char *, name, int, major, int, minor)
{
   module_entry *p = module_list;
   while (p) {
      if ((p->name != NULL) && (strcasecmp(name, p->name) == 0) &&
	  ((major == p->major) || (major == 0))
	  && (minor <= p->minor))
	 return p;
      p = p->next;
   }
   return NULL;
}

int depend PROTO4(char *, name1, char *, name2, int, major, int, minor)
{
   module_entry *p = find_module(name2, major, minor);
   module_entry *o = find_module(name1, 0, 0);
   dependancy *d;

   context;
   if (p == NULL) {
      if (load_module(name2) != NULL)
	 return 0;
      p = find_module(name2, major, minor);
   }
   if ((p == NULL) || (o == NULL))
      return 0;
   d = nmalloc(sizeof(dependancy));

   d->needed = p;
   d->needing = o;
   d->next = dependancy_list;
   dependancy_list = d;
   return 1;
}

int undepend PROTO1(char *, name1)
{
   module_entry *p = find_module(name1, 0, 0);
   dependancy *d = dependancy_list, *o = NULL;
   int ok = 0;

   context;
   if (p == NULL)
      return 0;
   while (d!=NULL) {
      if (d->needing == p) {
	 if (o == NULL) {
	    dependancy_list = d->next;
	 } else {
	    o->next = d->next;
	 }
	 nfree(d);
	 if (o == NULL)
	    d = dependancy_list;
	 else
	    d = o->next;
	 ok++;
      } else {
	 o = d;
	 d = d->next;
      }
   }
   return ok;
}

void *mod_malloc PROTO4(int, size, char *, modname, char *, filename, int, line)
{
   char x[100];
   sprintf(x, "%s:%s", modname, filename);
   x[15] = 0;
   return n_malloc(size, x, line);
}

void mod_free PROTO4(void *, ptr, char *, modname, char *, filename, int, line)
{
   char x[100];
   sprintf(x, "%s:%s", modname, filename);
   x[15] = 0;
   n_free(ptr, x, line);
}

/* add/remove tcl commands */
void add_tcl_commands PROTO1(tcl_cmds *, tab)
{
   int i;
   for (i = 0; tab[i].name; i++)
      Tcl_CreateCommand(interp, tab[i].name, tab[i].func, NULL, NULL);
}

void rem_tcl_commands PROTO1(tcl_cmds *, tab)
{
   int i;
   for (i = 0; tab[i].name; i++)
      Tcl_DeleteCommand(interp, tab[i].name);
}
/* hooks, various tables of functions to call on ceratin events */
void add_hook PROTO2(int, hook_num, void *, func)
{
   context;
   if (hook_num < REAL_HOOKS) {
      struct hook_entry *p = nmalloc(sizeof(struct hook_entry));
      p->next = hook_list[hook_num];
      hook_list[hook_num] = p;
      p->func = func;
   } else
      switch (hook_num) {
      case HOOK_GET_ASSOC_NAME:
	 get_assoc_name = func;
	 break;
      case HOOK_GET_ASSOC:
	 get_assoc = func;
	 break;
      case HOOK_DUMP_ASSOC_BOT:
	 dump_bot_assoc = func;
	 break;
      case HOOK_KILL_ASSOCS:
	 kill_all_assoc = func;
	 break;
      case HOOK_BOT_ASSOC:
	 do_bot_assoc = func;
	 break;
      case HOOK_ENCRYPT_PASS:
	 encrypt_pass = func;
	 break;
      }				/* ignore unsupported stuff a.t.m. :) */
}

void del_hook PROTO2(int, hook_num, void *, func)
{
   context;
   if (hook_num < REAL_HOOKS) {
      struct hook_entry *p = hook_list[hook_num], *o = NULL;
      while (p) {
	 if (p->func == func) {
	    if (o == NULL)
	       hook_list[hook_num] = p->next;
	    else
	       o->next = p->next;
	    nfree(p);
	    break;
	 }
	 o = p;
	 p = p->next;
      }
   } else
      switch (hook_num) {
      case HOOK_GET_ASSOC_NAME:
	 if (get_assoc_name == func)
	    get_assoc_name = charp_func;
	 break;
      case HOOK_GET_ASSOC:
	 if (get_assoc == func)
	    get_assoc = minus_func;
	 break;
      case HOOK_DUMP_ASSOC_BOT:
	 if (dump_bot_assoc == func)
	    dump_bot_assoc = null_func;
	 break;
      case HOOK_KILL_ASSOCS:
	 if (kill_all_assoc == func)
	    kill_all_assoc = null_func;
	 break;
      case HOOK_BOT_ASSOC:
	 if (do_bot_assoc == func)
	    do_bot_assoc = null_func;
	 break;
      case HOOK_ENCRYPT_PASS:
	 if (encrypt_pass == func)
	    encrypt_pass = null_func;
	 break;
      }				/* ignore unsupported stuff a.t.m. :) */
}

void *get_next_hook PROTO2(int, hook_num, void *, func)
{
   return NULL;
   /* we dont use this YET */
}

void cmd_modulestat PROTO2(int, idx, char *, par)
{
   context;
   putlog(LOG_CMDS, "*", "#%s# modulestat", dcc[idx].nick);
   if (par && par[0]) {
      module_entry * m = find_module(par,0,0);
      if (!m) {
	 dprintf(idx,"No such module.\n");
      } else if (!m->funcs || !m->funcs[MODCALL_REPORT]){
	 dprintf(idx,"No info for module %s.",par);
      } else {
	 m->funcs[MODCALL_REPORT](idx);
      }
   }
   do_module_report(idx);
}

void cmd_loadmodule PROTO2(int, idx, char *, par)
{
   char *p;

   context;
   if (!par[0]) {
      dprintf(idx, "Usage: loadmodule <module>\n");
   } else {
      p = load_module(par);
      if (p != NULL)
	 dprintf(idx, "Error in loading module %s: %s\n", par, p);
      else {
	 putlog(LOG_CMDS, "*", "#%s# loadmodule %s", dcc[idx].nick, par);
	 dprintf(idx, "Module %s loaded successfully\n", par);
      }
   }
   context;
}

void cmd_unloadmodule PROTO2(int, idx, char *, par)
{
   char *p;

   context;
   if (!par[0]) {
      dprintf(idx, "Usage: unloadmodule <module>\n");
   } else {
      p = unload_module(par,dcc[idx].nick);
      if (p != NULL)
	 dprintf(idx, "Error in removing module %s: %s\n", par, p);
      else {
	 putlog(LOG_CMDS, "*", "#%s# unloadmodule %s", dcc[idx].nick, par);
	 dprintf(idx, "Module %s removed successfully\n", par);
      }
   }
}

int call_hook PROTO1(int, hooknum)
{
   struct hook_entry *p;

   if (hooknum >= REAL_HOOKS)
      return 0;
   p = hook_list[hooknum];
   context;
   while (p != NULL) {
      p->func();
      p = p->next;
   }
   return 0;
}


int call_hook_i PROTO2(int, hooknum, int, idx)
{
   struct hook_entry *p;
   int f = 0;

   if (hooknum >= REAL_HOOKS)
      return 0;
   p = hook_list[hooknum];
   context;
   while ((p != NULL) && !f) {
      f = p->func(idx);
      p = p->next;
   }
   return f;
}

int call_hook_ici PROTO4(int, hooknum, int, idx, char *, buf, int, len)
{
   struct hook_entry *p;
   int f = 0;

   if (hooknum >= REAL_HOOKS)
      return 0;
   p = hook_list[hooknum];
   context;
   while ((p != NULL) && !f) {
      f = p->func(idx, buf, len);
      p = p->next;
   }
   return f;
}

int call_hook_cccc PROTO5(int, hooknum, char *, a, char *, b, char *, c, char *, d)
{
   struct hook_entry *p;
   int f = 0;

   if (hooknum >= REAL_HOOKS)
      return 0;
   p = hook_list[hooknum];
   context;
   while ((p != NULL) && !f) {
      f = p->func(a, b, c, d);
      p = p->next;
   }
   return f;
}

int tcl_loadmodule STDVAR
{
   char *p;

    context;
    BADARGS(2, 2, " module-name");
    p = load_module(argv[1]);
   if ((p != NULL) && strcmp(p, "Already loaded."))
       putlog(LOG_MISC, "*", "Can't load modules %s: %s", argv[1], p);
    Tcl_AppendResult(irp, p, NULL);
    return TCL_OK;
}

int tcl_unloadmodule STDVAR
{
   context;
   BADARGS(2, 2, " module-name");
   Tcl_AppendResult(irp, unload_module(argv[1],botname), NULL);
   return TCL_OK;
}

void do_module_report PROTO1(int, idx)
{
   module_entry *p = module_list;
   if (p != NULL)
      dprintf(idx, "MODULES LOADED:\n");
   while (p) {
      dependancy *d = dependancy_list;
      dprintf(idx, "Module: %s, v %d.%d\n", p->name ? p->name : "CORE",
	      p->major, p->minor);
      while (d != NULL) {
	 if (d->needing == p) 
	   dprintf(idx, "    requires: %s\n", d->needed->name);
	 d = d->next;
      }
      if (p->funcs != NULL) {
	 Function f = p->funcs[MODCALL_REPORT];
	 if (f != NULL)
	    f(idx);
      }
      p = p->next;
   }
}
