#define MOD_PRINTF        0
#define MOD_CONTEXT       1
#define MOD_MALLOC        2
#define MOD_FREE          3

#define MOD_REGISTER      4
#define MOD_FIND          5
#define MOD_DEPEND        6
#define MOD_UNDEPEND      7

#define MOD_ADD_HOOK      8
#define MOD_DEL_HOOK      9
#define MOD_NEXT_HOOK    10
#define MOD_CALL_HOOK_I  11

#define MOD_LOADMOD      12
#define MOD_UNLOADMOD    13

#define MOD_ADDTCLCOM    14
#define MOD_REMTCLCOM    15
#define MOD_ADDTCLINT    16
#define MOD_DELTCLINT    17
#define MOD_ADDTCLSTR    18
#define MOD_DELTCLSTR    19

#define MOD_PUTLOG       20
#define MOD_CHANOUT2     21
#define MOD_TANDOUT      22
#define MOD_TANDOUT_BUT  23

#define MOD_DCC          24
#define MOD_NSPLIT       25
#define MOD_ADD_BUILTINS 26
#define MOD_REM_BUILTINS 27

#define MOD_GET_ATTR     28
#define MOD_GET_CHANATTR 29
#define MOD_GET_ALLATTR  30

#define MOD_PASSMATCH    31

#define MOD_NEW_DCC      32
#define MOD_NEW_FORK     33
#define MOD_LOST_DCC     34
#define MOD_KILL_SOCK    35

#define MOD_CHECK_TCL    36

#define MOD_DCC_TOTAL    37
#define MOD_TEMPDIR      38
#define MOD_BOTNETNICK   39

#define MOD_RMSPACE      40
#define MOD_MOVEFILE     41
#define MOD_COPYFILE     42
#define MOD_CHECKFILT    43

#define MOD_DETECTDCCFLUD 44
#define MOD_GETHANDHOST  45
#define MOD_ADDUPLOAD    46
#define MOD_ADDDNLOAD    47

#define MOD_CANCELUSER   48
#define MOD_SETDCCDIR    49
#define MOD_USERLIST     50
#define MOD_MEMCPY       51

#define MOD_DUMPRESYNC   52
#define MOD_FLUSH_TBUF   53
#define MOD_ANSWER       54
#define MOD_NETERROR     55

#define MOD_TPUTS        56
#define MOD_WILDMATCHFILE 57
#define MOD_FLAGS2STR    58
#define MOD_STR2FLAGS    59

#define MOD_FLAGSOK      60
#define MOD_CHATOUT      61
#define MOD_IPTOLONG     62
#define MOD_GETMYIP      63

#define MOD_RESERVEDPORT 64
#define MOD_SETFILES     65
#define MOD_SET_UPLOADS  66
#define MOD_SET_DNLOADS  67

#define MOD_ISUSER       68
#define MOD_OPENLISTEN   69
#define MOD_GET_ATTR_HOST 70
#define MOD_MYATOUL      71

#define MOD_GETDCCDIR    72
#define MOD_GETSOCK      73
#define MOD_OPENTELNETDCC 74
#define MOD_DOBOOT       75

#define MOD_BOTNAME      76
#define MOD_SHOW_MOTD    77
#define MOD_TELLTEXT     78
#define MOD_TELLHELP     79

#define MOD_SPLITC       80
#define MOD_NEXTBOT      81
#define MOD_IN_CHAIN     82
#define MOD_FINDIDX      83

#define MOD_INTERP       84
#define MOD_GETUSERBYHAND 85
#define MOD_FINISHSHARE  86
#define MOD_CMD_NOTE     87

#define MOD_HASH_FIL     88
#define MOD_HASH_RCVD    89
#define MOD_HASH_SENT    90
#define MOD_OPEN_TELNET  91

#define MOD_FIX_COLON    92
#define MOD_MAX          93

#define HOOK_ACTIVITY            0
#define HOOK_EOF                 1
#define HOOK_TIMEOUT             2
#define HOOK_CONNECT             3
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
#define HOOK_ENCRYPT_PASS        107

/* these are FIXED once they are in a relase they STAY */
/* well, unless im feeling grumpy ;) */
#define MODCALL_START  0
#define MODCALL_CLOSE  1
#define MODCALL_EXPMEM 2
#define MODCALL_REPORT 3
/* transfer */
#define TRANSFER_RAW_DCC     4
#define TRANSFER_FILEQCANCEL 5
#define TRANSFER_ATLIMIT     6
#define TRANSFER_QUEUEFILE   7

#define TRANSFER_SHOWQUEUED  8
#define TRANSFER_COPYTOTMP   9
#define TRANSFER_WIPETMPFILE 10
/* filesys */
#define FILESYS_REMOTE_REQ 4
#define FILESYS_ADDFILE    5
#define FILESYS_INCRGOTS   6

typedef struct _module_entry {
   char * name;           /* name of the module (without .so) */
   int major;             /* major version number MUST match */
   int minor;             /* minor version number MUST be >= */
   void * hand;           /* module handle */
   struct _module_entry * next;
#ifdef EBUG_MEM
   int mem_work;
#endif
   Function * funcs;
} module_entry;
