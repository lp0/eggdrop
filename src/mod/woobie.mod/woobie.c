/*
 * woobie.c	- a nonsensical command to exemplify module programming
 *
 * 		  By ButchBub - 15 July 1997	
 */

#define MAKING_WOOBIE
#define MODULE_NAME "woobie"
#include "../module.h"
#include <stdlib.h>

static int woobie_expmem()
{
   int size = 0;

   modcontext;
   return size;
}

static int cmd_woobie (int idx, char * par)
{
   modcontext;
   putlog(LOG_CMDS, "*", "#%s# woobie", dcc[idx].nick);
   modprintf(idx, "WOOBIE!\n");
   return 0;
}

/* a report on the module status */
static void woobie_report (int idx)
{
   int size = 0;

   modcontext;
   modprintf(idx, "     0 woobies using %d bytes\n",size);
}

static cmd_t mydcc[] =
{
   {"woobie", "", cmd_woobie, NULL },
   {0, 0, 0, 0 }
};

static char *woobie_close()
{
   p_tcl_hash_list H_dcc;

   modcontext;
   H_dcc = find_hash_table("dcc");
   rem_builtins(H_dcc, mydcc);
   module_undepend(MODULE_NAME);
   return NULL;
}

char *woobie_start ();

static Function woobie_table[] =
{
   (Function) woobie_start,
   (Function) woobie_close,
   (Function) woobie_expmem,
   (Function) woobie_report,
};

char *woobie_start ()
{
   p_tcl_hash_list H_dcc;

   modcontext;
   module_register(MODULE_NAME, woobie_table, 1, 0);
   module_depend(MODULE_NAME, "eggdrop", 102, 0);
   H_dcc = find_hash_table("dcc");
   add_builtins(H_dcc, mydcc);
   return NULL;
}
