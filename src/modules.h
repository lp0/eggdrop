/*
 * modules.h - support for code modules in eggdrop
 * by Darrin Smith (beldin@light.iinet.net.au)
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

/*
 * module related structures
 */

#ifndef _MODULE_H_
#define _MODULE_H_

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eggdrop.h"
#ifndef MAKING_MODS
#include "proto.h"
#endif
#include "tclegg.h"
#include "cmdt.h"
#include "mod/modvals.h"

typedef struct _dependancy {
   struct _module_entry * needed;
   struct _module_entry * needing;
   struct _dependancy * nexted;
   struct _dependancy * nexting;
} dependancy;

typedef struct _module_entry {
   char * name;           /* name of the module (without .so) */
   int major;             /* major version number MUST match */
   int minor;             /* minor version number MUST be >= */
   void * hand;           /* module handle */
   dependancy * needed;
   dependancy * needing;
   struct _module_entry * next;
#ifdef EBUG_MEM
   int mem_work;
#endif
   module_function * funcs;
} module_entry;

/* modules specific functions */
/* functions called by eggdrop */
void init_modules (); /* initialise it all */
int expmem_modules (); /* totaly memory used by all module code */
void do_module_report PROTO((int));

/* now these MUST be in each module , to support things */
/* they should return NULL on success or an error message otherwise */
char * module_start PROTO(());
char * module_close PROTO(());
/* this returns all the memory used by the module */
int module_expmem PROTO(());
/* a report on the module status */
void module_report PROTO((int sock));

char * load_module PROTO((char * module_name));
char * unload_module PROTO((char * module_name));
module_entry * find_module PROTO((char * name, int, int));
int depend PROTO((char *,char *, int major, int minor));
int undepend PROTO((char *,char *));
void * mod_malloc PROTO((int size,char * modname, char * filename, int line));
void mod_free PROTO((void * ptr,char * modname, char * filename, int line));
void add_hook PROTO((int hook_num, void * func));
void del_hook PROTO((int hook_num, void * func));
void * get_next_hook PROTO((int hook_num, void * func));
int call_hook ();

void add_builtins PROTO((int,cmd_t * ));
void rem_builtins PROTO((int,cmd_t * ));
void add_tcl_commands PROTO((tcl_cmds *));
void rem_tcl_commands PROTO((tcl_cmds *));
void add_tcl_strings PROTO((tcl_strings *));
void rem_tcl_strings PROTO((tcl_strings *));
void add_tcl_ints PROTO((tcl_ints *));
void rem_tcl_ints PROTO((tcl_ints *));

/* some hooks */
int new_dcc PROTO((int));
int new_fork PROTO((int));
/* since some machines dont have the headers */
void * dlsym PROTO((void *,char *));
#endif /* _MODULE_H_ */
