#ifndef MODULES

#define modmalloc nmalloc
#define modfree nfree
#define modprintf dprintf
#define modcontext context
extern int reserved_port;
extern char tempdir[];
#include "../tclegg.h"
extern Tcl_Interp * interp;
extern Tcl_HashTable H_fil, H_sent, H_rcvd;
#else
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* just include *all* the include files...it's slower but EASIER */
#include "../eggdrop.h"

#ifdef STDC_HEADERS
#define PROTO(x) x
#define PROTO1(a,b) (a b)
#define PROTO2(a1,b1,a2,b2) (a1 b1, a2 b2) 
#define PROTO3(a1,b1,a2,b2,a3,b3) (a1 b1, a2 b2, a3 b3)
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) \
              (a1 b1, a2 b2, a3 b3, a4 b4)
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5)
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (a1 b1, a2 b2, a3 b3, a4 b4, a5 b5, a6 b6)
#else
#define PROTO(x) ()
#define PROTO1(a,b) (b) a b;
#define PROTO2(a1,b1,a2,b2) (b1, b2) a1 b1; a2 b2; 
#define PROTO3(a1,b1,a2,b2,a3,b3) (b1, b2, b3) a1 b1; a2 b2; a3 b3;
#define PROTO4(a1,b1,a2,b2,a3,b3,a4,b4) (b1, b2, b3, b4) \
              a1 b1; a2 b2; a3 b3; a4 b4;
#define PROTO5(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5) \
              (b1, b2, b3, b4, b5) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5;
#define PROTO6(a1,b1,a2,b2,a3,b3,a4,b4,a5,b5,a6,b6) \
              (b1, b2, b3, b4, b5, b6) \
              a1 b1; a2 b2; a3 b3; a4 b4; a5 b5; a6 b6;
#endif

#include "modvals.h"
#include "../tclegg.h"
#include "../tandem.h"
#include "../cmdt.h"

#undef nmalloc
#undef nfree
#undef nrealloc
#undef context
#define nmalloc(x) dont_use_nmalloc_in_modules
#define context dont_use_contex_in_modules

#define modmalloc(x) (global[MOD_MALLOC])((x),MODULE_NAME,__FILE__,__LINE__)
#define modfree(x)   (global[MOD_FREE])((x),MODULE_NAME,__FILE__,__LINE__)
#define modcontext   (global[MOD_CONTEXT])(MODULE_NAME,__FILE__,__LINE__)
#define modprintf    (global[MOD_PRINTF])

#define module_register(a,b,c,d)   (global[MOD_REGISTER])(a,b,c,d)
#define module_find(a,b,c)       ((module_entry *)(global[MOD_FIND])(a,b,c))
#define module_depend(a,b,c,d)   (global[MOD_DEPEND])(a,b,c,d)
#define module_undepend(a)       (global[MOD_UNDEPEND])(a)

#define add_hook(a,b)     (global[MOD_ADD_HOOK])(a,b)
#define del_hook(a,b)     (global[MOD_DEL_HOOK])(a,b)
#define next_hook(a,b)    (global[MOD_NEXT_HOOK])(a,b)
#define call_hook_i(a,b)  (global[MOD_CALL_HOOK_I])(a,b)

#define module_load(a)    (global[MOD_LOADMOD])(a)
#define module_unload(a)  (global[MOD_UNLOADMOD])(a)

#define add_tcl_commands(a) (global[MOD_ADDTCLCOM])(a)
#define rem_tcl_commands(a) (global[MOD_REMTCLCOM])(a)
#define add_tcl_ints(s)     (global[MOD_ADDTCLINT])(s)
#define rem_tcl_ints(s)     (global[MOD_DELTCLINT])(s)
#define add_tcl_strings(s)     (global[MOD_ADDTCLSTR])(s)
#define rem_tcl_strings(s)     (global[MOD_DELTCLSTR])(s)

#define putlog              (global[MOD_PUTLOG])
#define chanout2            (global[MOD_CHANOUT2])
#define tandout             (global[MOD_TANDOUT])
#define tandout_but         (global[MOD_TANDOUT_BUT])

#define dcc                 ((struct dcc_t *)(global[MOD_DCC]))
#define nsplit(a,b)         (global[MOD_NSPLIT])(a,b)
#define add_builtins(a,b)   (global[MOD_ADD_BUILTINS])(a,b)
#define rem_builtins(a,b)   (global[MOD_REM_BUILTINS])(a,b)

#define get_attr_handle(a)  (global[MOD_GET_ATTR])(a)
#define get_chanattr_handle(a,b) (global[MOD_GET_CHANATTR])(a,b)
#define get_allattr_handle(a) (global[MOD_GET_ALLATTR])(a)

#define pass_match_by_handle(a,b) (global[MOD_PASSMATCH])(a,b)

#define check_tcl_bind(a,b,c,d,e)       (global[MOD_CHECK_TCL])(a,b,c,d,e)

#define new_dcc(a)  (global[MOD_NEW_DCC])(a)
#define new_fork(a) (global[MOD_NEW_FORK])(a)
#define lostdcc(a)  (global[MOD_LOST_DCC])(a)
#define killsock(a)  (global[MOD_KILL_SOCK])(a)

#define dcc_total       (*(int*)global[MOD_DCC_TOTAL])
#define tempdir         ((char *)(global[MOD_TEMPDIR]))
#define botnetnick      ((char *)(global[MOD_BOTNETNICK]))

#define rmspace(a)          (global[MOD_RMSPACE])(a)
#define movefile(a,b)       (global[MOD_MOVEFILE])(a,b)
#define copyfile(a,b)       (global[MOD_COPYFILE])(a,b)
#define chatout             (global[MOD_CHATOUT])
#define check_tcl_filt(a,b) (((char *(*)())(global[MOD_CHECKFILT]))(a,b))
#define detect_dcc_flood(a,b) (global[MOD_DETECTDCCFLUD])(a,b)
#define get_handle_by_host(a,b) (global[MOD_GETHANDHOST])(a,b)
#define stats_add_upload(a,b)   (global[MOD_ADDUPLOAD])(a,b)
#define stats_add_dnload(a,b)   (global[MOD_ADDDNLOAD])(a,b)
#define cancel_user_xfer(a)     (global[MOD_CANCELUSER])(a)
#define set_handle_dccdir(a,b,c)  (global[MOD_SETDCCDIR])(a,b,c)
#define userlist             (*(struct userrec **)(global[MOD_USERLIST]))
#define my_memcpy(a,b,c)            (global[MOD_MEMCPY])(a,b,c)
#define dump_resync(a,b)    (global[MOD_DUMPRESYNC])(a,b)
#define flush_tbuf(a)     (global[MOD_FLUSH_TBUF])(a)
#define answer(a,b,c,d,e)   (global[MOD_ANSWER])(a,b,c,d,e)
#define neterror(a)         (global[MOD_NETERROR])(a)

#define wild_match_file(a,b) (global[MOD_WILDMATCHFILE])(a,b)
#define flags2str(a,b)       (global[MOD_FLAGS2STR])(a,b)
#define str2flags(a)         (global[MOD_STR2FLAGS])(a)

#define flags_ok(a,b) (global[MOD_FLAGSOK])(a,b)
#define chanout       (global[MOD_CHANOUT])
#define iptolong(a)   (global[MOD_IPTOLONG])(a)
#define getmyip()    ((IP)(global[MOD_GETMYIP])())

#define tputs(a,b,c)  (global[MOD_TPUTS])(a,b,c)

#define reserved_port (*(int*)(global[MOD_RESERVEDPORT]))
#define set_files(a) (global[MOD_SETFILES])(a)
#define set_handle_uploads(a,b,c,d) (global[MOD_SET_UPLOADS])(a,b,c,d)
#define set_handle_dnloads(a,b,c,d) (global[MOD_SET_DNLOADS])(a,b,c,d)

#define is_user(a) (global[MOD_ISUSER])(a)
#define open_listen(a) (global[MOD_OPENLISTEN])(a)
#define get_attr_host(a) (global[MOD_GET_ATTR_HOST])(a)
#define my_atoul(a)      (global[MOD_MYATOUL])(a)

#define get_handle_dccdir(a,b) (global[MOD_GETDCCDIR])(a,b)
#define getsock(a)             (global[MOD_GETSOCK])(a)
#define open_telnet_dcc(a,b,c) (global[MOD_OPENTELNETDCC])(a,b,c)
#define do_boot(a,b,c)         (global[MOD_DOBOOT])(a,b,c)

#define botname                ((char *)(global[MOD_BOTNAME]))

#define show_motd(a)          (global[MOD_SHOW_MOTD])(a)
#define telltext(a,b,c)       (global[MOD_TELLTEXT])(a,b,c)
#define tellhelp(a,b,c)       (global[MOD_TELLHELP])(a,b,c)

#define splitc(a,b,c)           (global[MOD_SPLITC])(a,b,c)
#define nextbot(a)            (global[MOD_NEXTBOT])(a)
#define in_chain(a)           (global[MOD_IN_CHAIN])(a)
#define findidx(a)            (global[MOD_FINDIDX])(a)

#define interp                (*(Tcl_Interp **)(global[MOD_INTERP]))
#define get_user_by_handle(a,b) (struct userrec *)(global[MOD_GETUSERBYHAND])(a,b)
#define finish_share(a)       (global[MOD_FINISHSHARE])(a)
#define cmd_note(a,b)         (global[MOD_CMD_NOTE])(a,b)

#define H_fil         (*(Tcl_HashTable *)(global[MOD_HASH_FIL]))
#define H_rcvd         (*(Tcl_HashTable *)(global[MOD_HASH_RCVD]))
#define H_sent         (*(Tcl_HashTable *)(global[MOD_HASH_SENT]))
#define open_telnet(a,b)      (global[MOD_OPEN_TELNET])(a,b)

#define fix_colon(a)           (global[MOD_FIX_COLON])(a)
extern Function * global;

#endif
