#define HOOK_XXX0                0 /* these *were* something, but changed */
#define HOOK_XXX1                1
#define HOOK_XXX2                2
#define HOOK_READ_USERFILE       3
#define HOOK_GOT_DCC             4
#define HOOK_MINUTELY            5
#define HOOK_DAILY               6
#define HOOK_HOURLY              7
#define HOOK_USERFILE            8
#define REAL_HOOKS               9
#define HOOK_GET_ASSOC_NAME      100
#define HOOK_GET_ASSOC           101
#define HOOK_DUMP_ASSOC_BOT      102
#define HOOK_KILL_ASSOCS         103
#define HOOK_BOT_ASSOC           104
#define HOOK_SHAREOUT            105
#define HOOK_SHAREIN             106
#define HOOK_ENCRYPT_PASS        107

/* these are FIXED once they are in a relase they STAY */
/* well, unless im feeling grumpy ;) */
#define MODCALL_START  0
#define MODCALL_CLOSE  1
#define MODCALL_EXPMEM 2
#define MODCALL_REPORT 3
/* filesys */
#define FILESYS_REMOTE_REQ 4
#define FILESYS_ADDFILE    5
#define FILESYS_INCRGOTS   6
#define FILESYS_ISVALID	   7
/* share */
#define SHARE_FINISH       4
#define SHARE_DUMP_RESYNC  5

typedef struct _module_entry {
   char * name;           /* name of the module (without .so) */
   int major;             /* major version number MUST match */
   int minor;             /* minor version number MUST be >= */
#ifndef STATIC
#ifdef HPUX_HACKS
   shl_t hand;
#else
   void * hand;           /* module handle */
#endif
#endif
   struct _module_entry * next;
#ifdef EBUG_MEM
   int mem_work;
#endif
   Function * funcs;
} module_entry;
