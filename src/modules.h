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

#include "main.h"
#include "mod/modvals.h"

/* modules specific functions */
/* functions called by eggdrop */
void init_modules (int); /* initialise it all */
int expmem_modules (int); /* totaly memory used by all module code */
void do_module_report (int);

/* now these MUST be in each module , to support things */
/* they should return NULL on success or an error message otherwise */
char * module_start (void);
char * module_close (void);
/* this returns all the memory used by the module */
int module_expmem (void);
/* a report on the module status */
void module_report (int sock);

const char * module_load (char * module_name);
char * module_unload (char * module_name,char * nick);
module_entry * module_find (char * name, int, int);
int module_depend (char *,char *, int major, int minor);
int module_undepend (char *);
void * mod_malloc (int size,char * modname, char * filename, int line);
void mod_free (void * ptr,char * modname, char * filename, int line);
void add_hook (int hook_num, void * func);
void del_hook (int hook_num, void * func);
void * get_next_hook (int hook_num, void * func);
int call_hook (int);
int call_hook_cccc (int,char *,char*,char*,char*);

typedef struct _dependancy {
   struct _module_entry * needed;
   struct _module_entry * needing;
   struct _dependancy * next;
   int major;
   int minor;
} dependancy;
extern dependancy * dependancy_list;
#endif /* _MODULE_H_ */
