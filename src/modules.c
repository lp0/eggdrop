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
#include <varargs.h>
#ifndef HAVE_DLOPEN
#include "you/need/dlopen/to/compile/with/dynamic/modules"
#else
#ifdef DLOPEN_MUST_BE_1
#define RTLD_FLAGS 1
#else
#define RTLD_FLAGS 2
#endif
void * dlopen PROTO((const char *,int));
char * dlerror ();
int dlclose PROTO((void *));
extern struct dcc_t dcc[];
#endif
#include "users.h"
int cmd_note();

/* from other areas */
extern int egg_numver;
extern Tcl_Interp * interp;
extern Tcl_HashTable H_fil, H_rcvd, H_sent;
extern struct userrec *userlist;
extern int dcc_total;
extern char tempdir[];
extern char botnetnick[];
extern int reserved_port;
extern char botname[];

/* the null functions */
void null_func () {
}
char * charp_func () {
   return NULL;
}
int minus_func () {
   return -1;
}
int one_func () {
   return 1;
}

/* various hooks & things */
/* the REAL hooks, when these are called, a return of 0 indicates unhandled
 * 1 is handled */
struct hook_entry {
   struct hook_entry * next;
   int (*func)();
} * hook_list[REAL_HOOKS];

/* these are obscure ones that I hope to neaten eventually :/ */
void (*kill_all_assoc) () = null_func;
void (*dump_bot_assoc) PROTO((int)) = null_func;
char * (*get_assoc_name) PROTO((int)) = charp_func;
int (*get_assoc) PROTO((char *)) = minus_func;
void (*do_bot_assoc) PROTO((int, char *)) = null_func;
void (*remote_filereq) PROTO((int, char*, char*)) = NULL;
void (*encrypt_pass) PROTO((char *, char *)) = 0;
int (*raw_dcc_send) PROTO((char *,char *,char *,char *)) = one_func;

module_entry * module_list;

void init_modules () {
   int i;
   
   context;
   module_list = nmalloc(sizeof(module_entry));
   module_list->name = nmalloc(8);
   strcpy(module_list->name,"eggdrop");
   module_list->major = (egg_numver)/10000;
   module_list->minor = ((egg_numver)/100)%100;
   module_list->hand = NULL;
   module_list->needed = NULL;
   module_list->needing = NULL;
   module_list->next = NULL;
   module_list->funcs = NULL;
   for (i = 0;i < REAL_HOOKS;i++) 
      hook_list[i] = NULL;
}

int expmem_modules (int y) {
   int c = 0;
   int i;
   module_entry * p = module_list;
   module_function * f;
   
   context;
   for (i = 0;i < REAL_HOOKS;i++) {
      struct hook_entry * q = hook_list[i];
      while (q) {
	 c+=sizeof(struct hook_entry);
	 q=q->next;
      }
   }
   while (p) {
      dependancy * d = p->needed;
      c+=sizeof(module_entry);
      c+=strlen(p->name)+1;
      while (d) {
	 c+=sizeof(dependancy);
	 d=d->nexted;
      }
      f = p->funcs;
      if ((f != NULL) && !y)
	c+= (int)(f[MODCALL_EXPMEM]());
      p=p->next;
   }
   return c;
}

void mod_context PROTO3(char *,module,char *,file, int,line) {
   char x[100];
   sprintf(x,"%s:%s",module,file);
   x[30] = 0;
#ifdef EBUG
   cx_ptr=((cx_ptr + 1) & 15);
   strcpy(cx_file[cx_ptr],x);
   cx_line[cx_ptr]=line;
#else
   strcpy(cx_file,x);
   cx_line=line;
#endif
}

int register_module PROTO4(char *,name,module_function *,funcs,
			   int,major,int,minor)
{
   module_entry * p = module_list;
   context;
   while (p) {
      if ((p->name != NULL) && (strcasecmp(name,p->name)==0)) {
	 p->major = major;
	 p->minor = minor;
	 p->funcs = funcs;
	 return 1;
      }
      p = p->next;
   }
   return 0;
}

module_function global_funcs[] = {
     (module_function)dprintf,
     (module_function)mod_context,
     (module_function)mod_malloc,
     (module_function)mod_free,
     
     (module_function)register_module,
     (module_function)find_module,
     (module_function)depend,
     (module_function)undepend,
     
     (module_function)add_hook,
     (module_function)del_hook,
     (module_function)get_next_hook,
     (module_function)call_hook,
     
     (module_function)load_module,
     (module_function)unload_module,
     
     (module_function)add_tcl_commands,
     (module_function)rem_tcl_commands,
     (module_function)add_tcl_ints,
     (module_function)rem_tcl_ints,
     (module_function)add_tcl_strings,
     (module_function)rem_tcl_strings,
     
     (module_function)putlog,
     (module_function)chanout2,
     (module_function)tandout,
     (module_function)tandout_but,

     (module_function)dcc,
     (module_function)nsplit,
     (module_function)add_builtins,
     (module_function)rem_builtins,
     
     (module_function)get_attr_handle,
     (module_function)get_chanattr_handle,
     (module_function)get_allattr_handle,
     
     (module_function)pass_match_by_handle,

     (module_function)new_dcc,
     (module_function)new_fork,
     (module_function)lostdcc,
     (module_function)killsock,

     (module_function)check_tcl_bind,
     (module_function)&dcc_total,
     (module_function)tempdir,
     (module_function)botnetnick,
     
     (module_function)rmspace,
     (module_function)movefile,
     (module_function)copyfile,
     (module_function)check_tcl_filt,
     
     (module_function)detect_dcc_flood,
     (module_function)get_handle_by_host,
     (module_function)stats_add_upload,
     (module_function)stats_add_dnload,
     
     (module_function)cancel_user_xfer,
     (module_function)set_handle_dccdir,
     (module_function)&userlist,
     (module_function)my_memcpy,
     
     (module_function)dump_resync,
     (module_function)flush_tbuf,
     (module_function)answer,
     (module_function)neterror,
     
     (module_function)tputs,
     (module_function)wild_match_file,
     (module_function)flags2str,
     (module_function)str2flags,
     
     (module_function)flags_ok,
     (module_function)chatout,
     (module_function)iptolong,
     (module_function)getmyip,
     
     (module_function)&reserved_port,
     (module_function)set_files,
     (module_function)set_handle_uploads,
     (module_function)set_handle_dnloads,
     
     (module_function)is_user,
     (module_function)open_listen,
     (module_function)get_attr_host,
     (module_function)my_atoul,
     
     (module_function)get_handle_dccdir,
     (module_function)getsock,
     (module_function)open_telnet_dcc,
     (module_function)do_boot,
     
     (module_function)botname,
     (module_function)show_motd,
     (module_function)telltext,
     (module_function)showhelp,
     
     (module_function)splitc,
     (module_function)nextbot,
     (module_function)in_chain,
     (module_function)findidx,
     
     (module_function)&interp,
     (module_function)get_user_by_handle,
     (module_function)finish_share,
     (module_function)cmd_note,
     
     (module_function)&H_fil,
     (module_function)&H_rcvd,
     (module_function)&H_sent,
};

char * load_module PROTO1(char *,name) {
   module_entry * p;
   char workbuf[1024];
   void * hand;
   char * e;
   module_function f;
   
   context;
   if (find_module(name,0,0)!=NULL)
     return "Already loaded.";
   if(getcwd(workbuf,1024)==NULL) 
      return "can't determine current directory.";
   sprintf(&(workbuf[strlen(workbuf)]),"/%s.so",name);
   hand = dlopen(workbuf,RTLD_FLAGS);
   if (hand == NULL)
     return dlerror();
   p = nmalloc(sizeof(module_entry));
   if (p == NULL)
     return "Malloc error";
   p->next = module_list;
   module_list = p;
   module_list->name = nmalloc(strlen(name)+1);
   strcpy(module_list->name,name);
   module_list->major = 0;
   module_list->minor = 0;
   module_list->needed = NULL;
   module_list->needing = NULL;
   module_list->hand = hand; 
   sprintf(workbuf,"%s_start",name);
   f = dlsym(hand,workbuf);
   if (f == NULL) { /* some OS's need the _ */
      sprintf(workbuf,"_%s_start",name);
      f = dlsym(hand,workbuf);
      if (f == NULL) {
	 return "No start function defined.";
      }
   }
   e = (char *)(f(global_funcs));
   if (e != NULL)
     return e;
   putlog(LOG_MISC,"*","Module %s loaded",name);
   return NULL;
}

char * unload_module PROTO1(char *,name) {
   module_entry * p = module_list, *o = NULL;
   char * e;
   module_function * f;

   context;
   while (p) {
      if ((p->name != NULL) && (strcasecmp(name,p->name)==0)) {
	 if (p->needing != NULL) {
	    return "Needed by another module";
	 }
	 f = p->funcs;
	 if ((f == NULL) || (f[MODCALL_CLOSE]==NULL))
	   return "No close function";
	 e = (char *)(f[MODCALL_CLOSE]());
	 if (e != NULL)
	   return e;
	 dlclose(p->hand);
	 nfree(p->name);
	 if (o == NULL) {
	    module_list = p->next;
	 } else {
	    o->next = p->next;
	 }
	 nfree(p);
	 putlog(LOG_MISC,"*","Module %s unloaded",name);
	 return NULL;
      }
      o = p;
      p = p->next;
   }
   return "No such module";
}

module_entry * find_module PROTO3(char *,name, int,major,int,minor)
{
   module_entry * p = module_list;
   while (p) {
      if ((p->name != NULL) && (strcasecmp(name,p->name)==0) &&
	  ((major == p->major) || (major == 0))
	  && (minor <= p->minor))
	return p;
      p = p->next;
   }
   return NULL;
}

int depend PROTO4(char *,name1,char *,name2, int,major, int,minor) 
{
   module_entry * p = find_module(name2,major,minor);
   module_entry * o = find_module(name1,0,0);
   dependancy * d;
   
   context;
   if (p == NULL) {
      if (load_module(name2)!=NULL)
	return 0;
      p = find_module(name2,major,minor);
   }
   if ((p == NULL) || (o == NULL))
     return 0;
   d = nmalloc(sizeof(dependancy));

   d->needed = p;
   d->nexted = o->needed;
   o->needed = d;

   d->needing = o;
   d->nexting = p->needing;
   p->needing = d;
   return 1;
}

int undepend PROTO2(char *,name1, char *,name2) 
{
   module_entry * p = find_module(name2,0,0);
   module_entry * o = find_module(name1,0,0);
   dependancy * d, * d1;
   int ok = 0;
   
   context;
   if ((p == NULL) || (o == NULL))
     return 0;
   d = p->needed;
   d1 = NULL;
   while (d) {
      if (d->needed == o) {
	 if (d1 == NULL) {
	    p->needed = d->nexted;
	 } else {
	    d1->nexted = d->nexted;
	 }
	 ok++;
      }
      d1 = d;
      d = d->nexted;
   }
   d = p->needing;
   d1 = NULL;
   while (d) {
      if (d->needing == o) {
	 if (d1 == NULL) {
	    p->needing = d->nexting;
	 } else {
	    d1->nexting = d->nexting;
	 }
	 ok++;
      }
      d1 = d;
      d = d->nexting;
   }
   return ok;
}

void * mod_malloc PROTO4(int,size,char *,modname, char *,filename, int,line) 
{
   char x[100];
   sprintf(x,"%s:%s",modname,filename);
   x[15] = 0;
   return n_malloc(size,x,line);
}
       
void mod_free PROTO4(void *,ptr,char *,modname, char *,filename, int,line)
{
   char x[100];
   sprintf(x,"%s:%s",modname,filename);
   x[15] = 0;
   n_free(ptr,x,line);
}

/* add/remove tcl commands */
void add_tcl_commands PROTO1(tcl_cmds *,tab)
{
   int i;
   for (i = 0;tab[i].name;i++)
     Tcl_CreateCommand(interp,tab[i].name,tab[i].func,NULL,NULL);
}

void rem_tcl_commands PROTO1(tcl_cmds *,tab)
{
   int i;
   for (i = 0;tab[i].name;i++)
     Tcl_DeleteCommand(interp,tab[i].name);
}
/* hooks, various tables of functions to call on ceratin events */
void add_hook PROTO2(int,hook_num, void *,func) 
{
   context;
   if (hook_num < REAL_HOOKS) {
      struct hook_entry * p = nmalloc(sizeof(struct hook_entry));
      p->next = hook_list[hook_num];
      hook_list[hook_num] = p;
      p->func = func;
   } else switch (hook_num) {
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
    case HOOK_REMOTE_FILEREQ:
      remote_filereq = func;
      break;
    case HOOK_RAW_DCC:
      raw_dcc_send = func;
      break;
    case HOOK_ENCRYPT_PASS:
      encrypt_pass = func;
      break;
   } /* ignore unsupported stuff a.t.m. :) */
}
   
void del_hook PROTO2(int,hook_num, void *,func) 
{
   context;
   if (hook_num < REAL_HOOKS) {
      struct hook_entry * p = hook_list[hook_num],
	*o = NULL;
      while (p) {
	 if (p->func == func) {
	    if (o==NULL)
	      hook_list[hook_num] = p->next;
	    else
	      o->next = p->next;
	    nfree(p);
	    break;
	 }
	 o = p;
	 p = p->next;
      }
   } else switch (hook_num) {
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
    case HOOK_REMOTE_FILEREQ:
      if (remote_filereq == func)
	remote_filereq = NULL;
      break;
    case HOOK_RAW_DCC:
      if (raw_dcc_send == func)
	raw_dcc_send = one_func;
      break;
    case HOOK_ENCRYPT_PASS:
      if (encrypt_pass == func)
	encrypt_pass = null_func;
      break;
   } /* ignore unsupported stuff a.t.m. :) */
}
   
void * get_next_hook PROTO2(int,hook_num, void *,func) 
{
   return NULL;
   /* we dont use this YET */
}

void cmd_modules PROTO2(int,idx,char *,par) {
   context;
   putlog(LOG_CMDS,"*","#%s# modules",dcc[idx].nick);
   do_module_report(idx);
}

void cmd_pls_module PROTO2(int,idx,char *,par) {
   char * p;
   
   context;
   if (!par[0]) {
      dprintf(idx,"Usage: +module <module>\n");
   } else {
      p = load_module(par);
      if (p!=NULL) 
	dprintf(idx,"Error in loading module %s: %s\n",par,p);
      else { 
	 putlog(LOG_CMDS,"*","#%s# loadmodule %s",dcc[idx].nick,par);
	 dprintf(idx,"Module %s loaded successfully\n",par);
      }
   }
   context;
}

void cmd_mns_module PROTO2(int,idx,char *,par) {
   char * p;
   
   context;
   if (!par[0]) {
      dprintf(idx,"Usage: -module <module>\n");
   } else {
      p = unload_module(par);
      if (p!=NULL) 
	dprintf(idx,"Error in removing module %s: %s\n",par,p);
      else { 
	 putlog(LOG_CMDS,"*","#%s# unloadmodule %s",dcc[idx].nick,par);
	 dprintf(idx,"Module %s removed successfully\n",par);
      }
   }
}

int call_hook (va_alist)
va_dcl
{
   va_list va;
   unsigned int hooknum,ret = 0;
   int idx = 0,len = 0; 
   char * c1 = 0, * c2 = 0, * c3 = 0, * c4 = 0;
   struct hook_entry * p;
   
   va_start(va);
   hooknum=va_arg(va,int);
   if (hooknum >= REAL_HOOKS) 
     return 0;
   p = hook_list[hooknum];
   switch (hooknum) {
    case HOOK_GOT_DCC:
      c1 = va_arg(va,char *);
      c2 = va_arg(va,char *);
      c3 = va_arg(va,char *);
      c4 = va_arg(va,char *);
      break;
    case HOOK_ACTIVITY:
      idx = va_arg(va,int);
      c1 = va_arg(va,char *);
      len = va_arg(va,int);
      break;
    default:
      idx = va_arg(va,int);
   }
   va_end(va);
   context;
   while ((p!=NULL) && !ret) {
      switch (hooknum) {
       case HOOK_GOT_DCC:
	 ret = p->func(c1,c2,c3,c4);
	 break;
       case HOOK_ACTIVITY:
	 ret = p->func(idx,c1,len);
	 break;
       default:
	 ret = p->func(idx);
      }
      p = p->next;
   }
   return ret;
}
      
int tcl_loadmodule STDVAR
{
   char * p;
 
   context;
   BADARGS(2,2," module-name");
   p = load_module(argv[1]);
   if ((p!=NULL) && strcmp(p, "Already loaded."))
     putlog(LOG_MISC,"*","Can't load modules %s: %s",argv[1],p);
   Tcl_AppendResult(irp,p,NULL);
   return TCL_OK;
}

int tcl_unloadmodule STDVAR
{
   context;
   BADARGS(2,2," module-name");
   Tcl_AppendResult(irp,unload_module(argv[1]),NULL);
   return TCL_OK;
}

void do_module_report PROTO1(int,idx) {
   module_entry * p = module_list;
   if (p != NULL)
     dprintf(idx,"MODULES LOADED:\n");
   while (p) {
      dependancy * d = p->needed;
      dprintf(idx,"Module: %s, v %d.%d\n",p->name?p->name:"CORE",
	      p->major,p->minor);
      while (d!=NULL) {
	 dprintf(idx,"    requires: %s\n",d->needed->name);
	 d=d->nexted;
      }
      if (p->funcs != NULL) {
	 module_function f = p->funcs[MODCALL_REPORT];
	 if (f != NULL) 
	   f(idx);
      }
      p = p->next;
   }
}
