#ifndef _TCL_HASH_H_
#define _TCL_HASH_H_

#define HT_STACKABLE 1

struct flag_record {
   int global;
/*   int bot; when I seperate botflags from normal flags */
   int chan;
   int match;
};
#define FR_OR  1
#define FR_AND 2

typedef struct tct {
  struct flag_record flags;
  char *func_name;
  struct tct *next;
} tcl_cmd_t;

typedef struct tcl_hash_list {
   Tcl_HashTable table;
   struct tcl_hash_list * next;
   char name[5];
   int flags;
   Function func;
} * p_tcl_hash_list;


void init_hash ();
void kill_hash ();
int expmem_tclhash ();

p_tcl_hash_list add_hash_table (char *,int,Function);
void del_hash_table (p_tcl_hash_list);

p_tcl_hash_list find_hash_table (char *);

int check_tcl_bind (p_tcl_hash_list,char *,struct flag_record *,char *,
			  int);
int check_tcl_pub (char *,char *,char *,char *);
void check_tcl_pubm (char *,char *,char *,char *);
int check_tcl_msg (char *,char *,char *,char *,char *);
void check_tcl_msgm (char *,char *,char *,char *,char *);
int check_tcl_dcc (char *,int,char *);
void check_tcl_notc (char *,char *,char *,char *);
void check_tcl_kick (char *,char *,char *,char *,char *,char *);
void check_tcl_chjn (char *,char *,int,char,int,char *);
void check_tcl_chpt (char *,char *,int);
int check_tcl_raw (char *,char *,char *);
void check_tcl_bot (char *,char *,char *);
void check_tcl_link (char *, char *);
void check_tcl_disc (char *);
char *check_tcl_filt (int, char *);
int check_tcl_flud (char *,char *,char *,char *,char *);
int check_tcl_note (char *,char *,char *);
void check_tcl_listen (char *, int);
void check_tcl_time (struct tm *);
int check_tcl_wall (char *, char *);
void tell_binds (int, char *);
int flagrec_ok (struct flag_record *, struct flag_record *);


void check_tcl_chatactbcst (char *, int, char *, p_tcl_hash_list);
#define check_tcl_chat(a,b,c) check_tcl_chatactbcst(a,b,c,H_chat)
#define check_tcl_act(a,b,c) check_tcl_chatactbcst(a,b,c,H_act)
#define check_tcl_bcst(a,b,c) check_tcl_chatactbcst(a,b,c,H_bcst)
void check_tcl_joinpart (char *,char *,char *,char *, p_tcl_hash_list);
#define check_tcl_join(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_join)
#define check_tcl_part(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_part)
#define check_tcl_splt(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_splt)
#define check_tcl_rejn(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_rejn)
void check_tcl_signtopcnickmode (char *, char *, char *, char *, char *,
			     p_tcl_hash_list);
#define check_tcl_sign(a,b,c,d,e) check_tcl_signtopcnickmode(a,b,c,d,e,H_sign)
#define check_tcl_topc(a,b,c,d,e) check_tcl_signtopcnickmode(a,b,c,d,e,H_topc)
#define check_tcl_nick(a,b,c,d,e) check_tcl_signtopcnickmode(a,b,c,d,e,H_nick)
#define check_tcl_mode(a,b,c,d,e) check_tcl_signtopcnickmode(a,b,c,d,e,H_mode)
int check_tcl_ctcpr (char *, char *, char *, char *, char *, char *,
		      p_tcl_hash_list);
#define check_tcl_ctcp(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcp)
#define check_tcl_ctcr(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcr)
void check_tcl_chonof (char *, int, p_tcl_hash_list);
#define check_tcl_chon(a,b) check_tcl_chonof(a,b,H_chon)
#define check_tcl_chof(a,b) check_tcl_chonof(a,b,H_chof)
void check_tcl_loadunld (char *, p_tcl_hash_list);
#define check_tcl_load(a) check_tcl_loadunld(a,H_load)
#define check_tcl_unld(a) check_tcl_loadunld(a,H_unld)

int rem_builtins (p_tcl_hash_list, cmd_t *);
int add_builtins (p_tcl_hash_list, cmd_t *);

int check_validity (char *,Function);
extern p_tcl_hash_list H_chat, H_act, H_bcst, H_join, H_part, H_topc, H_sign;
extern p_tcl_hash_list H_nick, H_mode, H_ctcp, H_ctcr, H_chon, H_chof, H_splt;
extern p_tcl_hash_list H_rejn, H_load, H_unld;
#endif
