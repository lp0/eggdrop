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
#ifndef STATIC 
#ifdef HPUX_HACKS
#include <dl.h>
#else
#ifdef OSF1_HACKS
#include <loader.h>
#else
#if DLOPEN_1
char *dlerror();
void *dlopen (const char *, int);
int dlclose (void *);
void * dlsym (void *,char *);
#define DLFLAGS 1
#else
#include <dlfcn.h>
#ifdef RTLD_GLOBAL
#define DLFLAGS RTLD_NOW|RTLD_GLOBAL
#else
#define DLFLAGS RTLD_NOW
#endif /* RTLD_GLOBAL */
#endif /* DLOPEN_1 */
#endif /* OSF1_HACKS */
#endif /* HPUX_HACKS */

#endif /* STATIC */
extern struct dcc_t * dcc;
#include "users.h"
int cmd_note();

/* from other areas */
extern int egg_numver;
extern Tcl_Interp *interp;
extern struct userrec *userlist;
extern int dcc_total;
extern char tempdir[];
extern char botnetnick[];
extern int reserved_port;
extern char botname[];
extern time_t now;
/* only used here, we'll cheat */
int check_validity (char *, Function);

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

static void null_share (int idx, char * x) {
   tprintf(dcc[idx].sock, "share uf-no Not sharing userfile.\n");
}

/* these are obscure ones that I hope to neaten eventually :/ */
void (*kill_all_assoc) () = null_func;
void (*dump_bot_assoc) (int) = null_func;
char *(*get_assoc_name) (int) = charp_func;
int (*get_assoc) (char *) = minus_func;
void (*do_bot_assoc) (int, char *) = null_func;
void (*encrypt_pass) (char *, char *) = 0;
void (*shareout)() = null_func;
void (*sharein)(int,char *) = null_share;

module_entry *module_list;
dependancy * dependancy_list = NULL;

void init_modules(int x)
{
   int i;

   context;
   if (x) {
      module_list = nmalloc(sizeof(module_entry));
      module_list->name = nmalloc(8);
      strcpy(module_list->name, "eggdrop");
      module_list->major = (egg_numver) / 10000;
      module_list->minor = ((egg_numver) / 100) % 100;
#ifndef STATIC
      module_list->hand = NULL;
#endif
      module_list->next = NULL;
      module_list->funcs = NULL;
   }
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

void mod_context (char * module, char * file, int line)
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

int module_register (char * name, Function * funcs,
		     int major, int minor)
{
   module_entry *p = module_list;
#ifdef STATIC 
   p = nmalloc(sizeof(module_entry));
   if (p == NULL)
      return 0;
   p->next = module_list;
   module_list = p;
   module_list->name = nmalloc(strlen(name) + 1);
   strcpy(module_list->name, name);
   module_list->major = 0;
   module_list->minor = 0;
   check_tcl_load(name);
   putlog(LOG_MISC, "*", "%s %s", MOD_LOADED, name);
#endif
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

const char *module_load (char * name)
{
#ifndef STATIC
   module_entry *p;
   char workbuf[1024];
#ifdef HPUX_HACKS
   shl_t hand;
#else
#ifdef OSF1_HACKS
   ldr_module_t hand;
#else
   void *hand;
#endif
#endif
   char *e;
   Function f;

   context;
   if (module_find(name, 0, 0) != NULL)
      return MOD_ALREADYLOAD;
   if (getcwd(workbuf, 1024) == NULL)
      return MOD_BADCWD;
   sprintf(&(workbuf[strlen(workbuf)]), "/modules/%s.so", name);
#ifdef HPUX_HACKS
   hand = shl_load(workbuf, BIND_IMMEDIATE, 0);
   if (!hand)
     return "Can't load module.";
#else
#ifdef OSF1_HACKS
   hand = (Tcl_PackageInitProc *) load (workbuf, LDR_NOFLAGS);
   if (hand == LDR_NULL_MODULE)
     return "Can't load module.";
#else
   hand = dlopen(workbuf, DLFLAGS);
   if (!hand)
      return dlerror(); 
#endif
#endif
     
   sprintf(workbuf, "%s_start", name);
#ifdef HPUX_HACKS
   if (!shl_findsym(hand, procname, (short) TYPE_PROCEDURE, (void *)f))
     f = NULL;
#else
#ifdef OSF1_HACKS
   f = ldr_lookup_package(hand, workbuf);
#else
   f = dlsym(hand, workbuf);
#endif
#endif
   if (f == NULL) {		/* some OS's need the _ */
      sprintf(workbuf, "_%s_start", name);
#ifdef HPUX_HACKS
      if (!shl_findsym(hand, procname, (short) TYPE_PROCEDURE, (void *)f))
	f = NULL;
#else
#ifdef OSF1_HACKS
      f = ldr_lookup_package(hand, workbuf);
#else
      f = dlsym(hand, workbuf);
#endif
#endif
      if (f == NULL) {
	 return MOD_NOSTARTDEF;
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
   e =  (((char *(*)())f)(0));
   if (e != NULL)
      return e;
   check_tcl_load(name);
   putlog(LOG_MISC, "*", "%s %s", MOD_LOADED, name);
   return NULL;
#else
   return "Can't load modules, statically linked.";
#endif
}

char *module_unload (char * name,char * user)
{
#ifndef STATIC
   module_entry *p = module_list, *o = NULL;
   char *e;
   Function *f;
   context;
   while (p) {
      if ((p->name != NULL) && (strcmp(name, p->name) == 0)) {
	 dependancy *d = dependancy_list;
	 
	 while (d!=NULL) {
	    if (d->needed == p) {
	       return MOD_NEEDED;
	    }
	    d=d->next;
	 }
	 f = p->funcs;
	 if ((f != NULL) && (f[MODCALL_CLOSE] == NULL))
	    return MOD_NOCLOSEDEF;
	 if (f != NULL) {
	    check_tcl_unld(name);
	    e =  (((char *(*)())f[MODCALL_CLOSE]) (user));
	    if (e != NULL)
	       return e;
#ifdef HPUX_HACKS
	    shl_unload(hand);
#else
#ifdef OSF1_HACKS
#else
	    dlclose(p->hand);
#endif
#endif
	 }
	 nfree(p->name);
	 if (o == NULL) {
	    module_list = p->next;
	 } else {
	    o->next = p->next;
	 }
	 nfree(p);
	 putlog(LOG_MISC, "*", "%s %s", MOD_UNLOADED, name);
	 return NULL;
      }
      o = p;
      p = p->next;
   }
   return MOD_NOSUCH;
#else
   return "Can't unload modules, statically linked.";
#endif /* STATIC */
}

module_entry *module_find (char * name, int major, int minor)
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

int module_depend (char * name1, char * name2, int major, int minor)
{
   module_entry *p = module_find(name2, major, minor);
   module_entry *o = module_find(name1, 0, 0);
   dependancy *d;

   context;
#ifndef STATIC
   if (p == NULL) {
      if (module_load(name2) != NULL)
	return 0;
      p = module_find(name2, major, minor);
   }
#endif
   if ((p == NULL) || (o == NULL))
      return 0;
   d = nmalloc(sizeof(dependancy));

   d->needed = p;
   d->needing = o;
   d->next = dependancy_list;
   d->major = major;
   d->minor = minor;
   dependancy_list = d;
   return 1;
}

int module_undepend (char * name1)
{
   int ok = 0;
#ifndef STATIC
   module_entry *p = module_find(name1, 0, 0);
   dependancy *d = dependancy_list, *o = NULL;

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
#endif
   return ok;
}

void *mod_malloc (int size, char * modname, char * filename, int line)
{
   char x[100];
   sprintf(x, "%s:%s", modname, filename);
   x[15] = 0;
   return n_malloc(size, x, line);
}

void mod_free (void * ptr, char * modname, char * filename, int line)
{
   char x[100];
   sprintf(x, "%s:%s", modname, filename);
   x[15] = 0;
   n_free(ptr, x, line);
}

/* hooks, various tables of functions to call on ceratin events */
void add_hook (int hook_num, void * func)
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
      case HOOK_SHAREOUT:
         shareout = func;
	 break;
      case HOOK_SHAREIN:
         sharein = func;
	 break;	 
      }				/* ignore unsupported stuff a.t.m. :) */
}

void del_hook (int hook_num, void * func)
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
      case HOOK_SHAREOUT:
         if (shareout == func)
	   shareout = null_func;
	 break;
      case HOOK_SHAREIN:
         if (sharein == func)
	   sharein = null_share;
	 break;	 
      }				/* ignore unsupported stuff a.t.m. :) */
}

void *get_next_hook (int hook_num, void * func)
{
   return NULL;
   /* we dont use this YET */
}

int call_hook (int hooknum)
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

int call_hook_cccc (int hooknum, char * a, char * b, char * c, char * d)
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

void do_module_report (int idx)
{
   module_entry *p = module_list;
   if (p != NULL)
#ifdef STATIC
     dprintf(idx, "MODULES STATICALLY LINKED:\n");
#else
     dprintf(idx, "MODULES LOADED:\n");
#endif
   while (p) {
      dependancy *d = dependancy_list;
      dprintf(idx, "Module: %s, v %d.%d\n", p->name ? p->name : "CORE",
	      p->major, p->minor);
      while (d != NULL) {
	 if (d->needing == p) 
	   dprintf(idx, "    requires: %s, v %d.%d\n", d->needed->name,
		   d->major, d->minor);
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
