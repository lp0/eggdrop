/* just include *all* the include files...it's slower but EASIER */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../main.h"
#include "modvals.h"
#include "../tandem.h"

#undef nmalloc
#undef nfree
#undef nrealloc
#undef context
#define nmalloc(x) dont_use_nmalloc_in_modules
#define context dont_use_contex_in_modules

#define modmalloc(x) mod_malloc((x),MODULE_NAME,__FILE__,__LINE__)
#define modfree(x)   mod_free((x),MODULE_NAME,__FILE__,__LINE__)
#define modcontext   mod_context(MODULE_NAME,__FILE__,__LINE__)
#define modprintf    dprintf

void * mod_malloc (int,char *,char *,int);
void mod_free(void *,char *,char *,int);
void mod_context (char *,char *,int);

int module_register (char *, Function *, int, int);
int module_depend (char *, char *, int, int);
int module_undepend (char *);
module_entry * module_find (char *, int, int);

void add_hook (int, void *);
void del_hook (int, void *);

extern Tcl_Interp * interp;
extern const int reserved_port;
extern const char tempdir[];
extern char botnetnick[];
extern int dcc_total;
extern struct dcc_t * dcc;
extern struct userrec * userlist;
extern const char botname[];
extern const time_t now;
extern const char ver[];
extern const char origbotname[];
extern const char userfile[];
