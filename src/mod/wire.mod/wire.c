/*
 * wire.c   - An encrypted partyline communication.
 *            Compatible with wire.tcl.
 *
 *            by ButchBub - Scott G. Taylor (staylor@mrynet.com) 
 *
 *      REQUIRED: Eggdrop Module version 1.2.0
 *
 *	1.0	1997-07-17	Initial.
 *	1.1	1997-07-28	Release version.
 *
 *   This module does not support wire usage in the files area.
 *
 *   TODO:  Reduce char[] sizes and clean up superfluous char[] usage.
 *
 */

#define MAKING_WIRE
#define MODULE_NAME "wire"
#include "../module.h"
#include <time.h>

typedef struct wire_t {
   int    idx;
   char   *crypt;
   char   *key;
   struct wire_t *next;
} wire_list;

wire_list *wirelist;

static cmd_t wire_bot[] =
{
   {0, 0, 0, 0}, /* Saves having to malloc :P */
   {0, 0, 0, 0}
};

static void wire_leave ();
static void wire_join ();
static void wire_display ();
char *encrypt_string(), *decrypt_string();
char geticon ();

static int wire_expmem()
{
wire_list *w = wirelist;
int size = 0;

   modcontext;
   while (w) {
      size += sizeof(wirelist);
      size += strlen(w->crypt);
      size += strlen(w->key);
      w = w->next;
   }
   return size;
}

static void wire_filter (char *from, char *cmd, char *param)
{
char   wirecrypt[512];
char   wirewho[512];
char   wiretmp2[512];
char   wiretmp[512];
char   wirereq[512];
wire_list *w = wirelist;
char   reqidx;
time_t now2 = time(NULL);
char   idle[20];
char   *enctmp;

   modcontext;
   strcpy(wirecrypt, &cmd[5]);
   strcpy(wiretmp, param);
   nsplit(wirereq, param);

/*
 * !wire<crypt"wire"> !wirereq <destbotidx> <crypt"destbotnick">
 * -----  wirecrypt    wirereq    wirewho         param
 */

   if (!strcmp(wirereq, "!wirereq")) {
      nsplit(wirewho, param);
      while(w) { 
         if (!strcmp(w->crypt, wirecrypt)) {

            reqidx = atoi(wirewho);
            if (now2 - dcc[w->idx].timeval > 300) {
               unsigned long days, hrs, mins;
               days = (now2 - dcc[w->idx].timeval) / 86400;
               hrs = ((now2 - dcc[w->idx].timeval) - (days * 86400)) / 3600;
               mins = ((now2 - dcc[w->idx].timeval) - (hrs * 3600)) / 60;
               if (days > 0)
                  sprintf(idle, " \\[idle %lud%luh\\]", days, hrs);
               else if (hrs > 0)
                  sprintf(idle, " \\[idle %luh%lum\\]", hrs, mins);
               else
                  sprintf(idle, " \\[idle %lum\\]", mins);
            } else
               idle[0] = 0;

            sprintf(wirereq, "----- %c%-9s %-9s  %s%s", 
                                          geticon(w->idx), dcc[w->idx].nick,
                                          botnetnick, dcc[w->idx].host, idle);
            enctmp=encrypt_string(w->key, wirereq);
            strcpy(wiretmp, enctmp);
            modfree(enctmp);
            sprintf(wirereq, "zapf %s %s !wire%s !wireresp %s %s %s",
               botnetnick, from, wirecrypt, wirewho, param, wiretmp);
            modprintf(nextbot(from),"%s\n", wirereq);

            if(dcc[w->idx].u.chat->away) {
               sprintf(wirereq, "-----    AWAY: %s\n", 
                                              dcc[w->idx].u.chat->away);
               enctmp=encrypt_string(w->key, wirereq);
               strcpy(wiretmp, enctmp);
               modfree(enctmp);
               sprintf(wirereq, "zapf %s %s !wire%s !wireresp %s %s %s",
                  botnetnick, from, wirecrypt, wirewho, param, wiretmp);
               modprintf(nextbot(from),"%s\n", wirereq);
            }
         }
         w = w->next;
      }
      return;
   }
   if (!strcmp(wirereq, "!wireresp")) {
      nsplit(wirewho, param);
      reqidx = atoi(wirewho);
      w = wirelist;
      nsplit(wiretmp2, param);
      while(w) {
         if(w->idx == reqidx) {
            enctmp = decrypt_string(w->key, wiretmp2);
            strcpy(wirewho, enctmp);
            modfree(enctmp);
            if (!strcmp(dcc[reqidx].nick, wirewho)) {
               enctmp = decrypt_string(w->key, param);
               modprintf(reqidx, "%s\n", enctmp);
               modfree(enctmp);
               return;
            }
         }
         w = w->next;
      }
      return;
   }
   while(w) {
      if (!strcmp(wirecrypt, w->crypt))
         wire_display(w->idx, w->key, wirereq, param);
      w = w->next;
   }
}

static void wire_display (int idx, char *key, char *from, char *message)
{
   char   *enctmp;
   
   enctmp = decrypt_string(key, message);
   if(from[0] == '!') 
      modprintf(idx, "----- > %s %s\n", &from[1], enctmp+1);
   else
      modprintf(idx, "----- <%s> %s\n", from, enctmp);
   modfree(enctmp);
}

static int cmd_wirelist (int idx, char *par)
{
wire_list *w = wirelist;
int      entry = 0;

modcontext;
   modprintf(idx, "Current Wire table:  (Base table address = %U)\n", w);
   while(w) {
      modprintf(idx, "entry %d: w=%U  idx=%d  next=%U\n",
                         ++entry, w, w->idx, w->next);
      w = w->next;
   }
   return 0;
}

static int cmd_onwire (int idx, char *par)
{
wire_list *w, *w2;
char   wiretmp[512], wirecmd[512], idxtmp[512];
char   idle[20], *enctmp;
time_t now2 = time(NULL);

modcontext;
   w = wirelist;
   while(w) {
      if (w->idx == idx) break;
      w = w->next;
   }
   if (!w) {
      modprintf(idx, "You aren't on a wire.\n");
      return 0;
   }
   modprintf(idx, "----- Currently on wire '%s':\n", w->key);
   modprintf(idx, "----- Nick       Bot        Host\n");
   modprintf(idx, "----- ---------- ---------- ------------------------------\n");

   enctmp=encrypt_string(w->key, "wire");
   sprintf(wirecmd, "!wire%s", enctmp);
   modfree(enctmp);

   enctmp=encrypt_string(w->key, dcc[idx].nick);
   strcpy(wiretmp, enctmp);
   modfree(enctmp);
   sprintf(idxtmp, "%s !wirereq %d %s", wirecmd, idx, wiretmp);
   tandout("zapf-broad %s %s\n", botnetnick, idxtmp);

   w2 = wirelist;
   while(w2) {
      if (!strcmp(w2->key, w->key)) {
         if (now2 - dcc[w2->idx].timeval > 300) {
            unsigned long days, hrs, mins;
            days = (now2 - dcc[w2->idx].timeval) / 86400;
            hrs = ((now2 - dcc[w2->idx].timeval) - (days * 86400)) / 3600;
            mins = ((now2 - dcc[w2->idx].timeval) - (hrs * 3600)) / 60;
            if (days > 0)
               sprintf(idle, " \\[idle %lud%luh\\]", days, hrs);
            else if (hrs > 0)
               sprintf(idle, " \\[idle %luh%lum\\]", hrs, mins);
            else
               sprintf(idle, " \\[idle %lum\\]", mins);
         } else
            idle[0] = 0;

         modprintf(idx, "----- %c%-9s %-9s  %s%s\n", 
                           geticon(w2->idx), dcc[w2->idx].nick,
                           botnetnick, dcc[w2->idx].host, idle);
         if(dcc[w2->idx].u.chat->away) 
            modprintf(idx, "-----    AWAY: %s\n", dcc[w2->idx].u.chat->away);
      }
      w2 = w2->next;
   }
   return 0;
}

static int cmd_wire (int idx, char *par)
{
   wire_list *w = wirelist;

   if(!par[0]) {
      modprintf(idx, "Usage: .wire [<encrypt-key>|OFF|info]\n");
      return 0;
   }

   while (w) {
      if (w->idx == idx) break;
      w = w->next;
   }

   if(!strcmp(par, "OFF") || !strcmp(par, "off")) {
      if (w) {
         wire_leave(w->idx);
         return 0;
      }
      modprintf(idx, "You are not on a wire.\n");
      return 0;
   }

   if(!strcmp(par, "info") || !strcmp(par, "INFO")) {
      if (w) 
         modprintf(idx, "You are currently on wire '%s'.\n", w->key);
      else
         modprintf(idx, "You are not on a wire.\n");
      return 0;
   }

   if(w) {
      modprintf(idx, "Changing wire encryption key to %s...\n", par);
      wire_leave(w->idx);
   } else {
      modprintf(idx, 
                "----- All text starting with ; will now go over the wire.\n");
      modprintf(idx, "----- To see who's on your wire, type '.onwire'.\n");
      modprintf(idx, "----- To leave, type '.wire off'.\n");
   }
    
   wire_join(idx, par);
   cmd_onwire(idx, "");
   return 0;
}

static char *chof_wire (char *from, int idx)
{
   wire_leave(idx);
   return NULL;
}

static void wire_join (int idx, char *key)
{
char   wirecmd[512];
char   wiremsg[512];
char   wiretmp[512];
char   *enctmp;
wire_list *w = wirelist, *w2;
p_tcl_hash_list H_dcc;

modcontext;
   while (w) {
      if (w->next == 0) break;
      w = w->next;
   }

   if (!wirelist) {
      wirelist = (wire_list *) modmalloc(sizeof(wire_list));
      w = wirelist;
   } else {
      w->next = (wire_list *) modmalloc(sizeof(wire_list));
      w = w->next;
   }

   w->idx = idx;
   w->key = (char *) modmalloc(strlen(key)+1);
   strcpy(w->key, key);
   w->next = 0;

modcontext;
   enctmp=encrypt_string(w->key, "wire");
   strcpy(wiretmp, enctmp);
   modfree(enctmp);
   w->crypt = (char *) modmalloc(strlen(wiretmp)+1);
   strcpy(w->crypt, wiretmp);
   sprintf(wirecmd, "!wire%s", wiretmp);

   sprintf(wiremsg, "%s joined wire '%s'", dcc[idx].nick, key);
   enctmp=encrypt_string(w->key, wiremsg);
   strcpy(wiretmp, enctmp);
   modfree(enctmp);

   tandout("zapf-broad %s %s %s %s\n", 
                     botnetnick, wirecmd, botnetnick, wiretmp);

   w2 = wirelist;
   while(w2) {
      if(!strcmp(w2->key, w->key))
         modprintf(w2->idx, "----- %s joined wire '%s'.\n", 
                                             dcc[w2->idx].nick, w2->key);
      w2 = w2->next;
   }
   
   w2 = wirelist;
   while (w2) {   /* Is someone using this key here already? */
      if (w2 != w)
         if (!strcmp(w2->key, w->key))
            break;
      w2 = w2->next;
   }

   if (!w2) { /* Someone else is NOT using this key, so we add a bind */
      wire_bot[0].name = wirecmd;
      wire_bot[0].flags = "";
      wire_bot[0].func = (Function) wire_filter;
      H_dcc = find_hash_table("bot");
      add_builtins(H_dcc, wire_bot);
   }
}
   
static void wire_leave (int idx)
{
char   wirecmd[513];
char   wiremsg[513];
char   wiretmp[513];
char   *enctmp;
wire_list *w = wirelist;
wire_list *w2 = wirelist;
wire_list *wlast = wirelist;

p_tcl_hash_list H_dcc;

   while(w) {
      if (w->idx == idx) break;
      w = w->next;
   }

   if(!w) return;
	
   enctmp=encrypt_string(w->key, "wire");
   strcpy(wirecmd, enctmp);
   modfree(enctmp);

   sprintf(wiretmp, "%s left the wire.", dcc[w->idx].nick);
   enctmp=encrypt_string(w->key, wiretmp);
   strcpy(wiremsg, enctmp);
   modfree(enctmp);

   tandout("zapf-broad %s !wire%s %s %s\n", botnetnick, wirecmd, botnetnick, 
                                       wiremsg);

   w2 = wirelist;
   while(w2) {
      if(!strcmp(w2->key, w->key))
         modprintf(w2->idx, "----- %s left the wire.\n", dcc[w2->idx].nick);
      w2 = w2->next;
   }

   /* Check to see if someone else is using this wire key.   If so, */
   /* then don't remove the wire filter binding */

   w2 = wirelist;
   while (w2) {
      if(w2 != w && !strcmp(w2->key, w->key) ) break;
      w2 = w2->next;
   }

   if (!w2) { /* Someone else is NOT using this key */
      wire_bot[0].name = wirecmd;
      wire_bot[0].flags = "";
      wire_bot[0].func = (Function) wire_filter;
      H_dcc = find_hash_table("bot");
      rem_builtins(H_dcc, wire_bot);
   }

   w2 = wirelist;
   
   wlast = 0;
   while (w2) {
      if (w2 == w) break;
      wlast = w2;
      w2 = w2->next;
   }

   if (wlast) 
      if (w->next)
         wlast->next = w->next;
      else
         wlast->next = 0;
   else
      if (!w->next)
         wirelist = 0;
      else
         wirelist = w->next;

modcontext;
   modfree(w->crypt);
modcontext;
   modfree(w->key);
modcontext;
   modfree(w);
modcontext;

}

static char *cmd_putwire (int idx, char *message)
{
wire_list *w = wirelist;
wire_list *w2 = wirelist;
int    wiretype;
char   wirecmd[512];
char   wiremsg[512];
char   wiretmp[512];
char   wiretmp2[512];
char   *enctmp;

   while(w) {
      if (w->idx == idx) break;
      w = w->next;
   }

   if (!w) return "";

   if (!message[1]) return "";

   if (strlen(message) > 2 && !strncmp(&message[1], "me", 2)) {
      sprintf(wiretmp2,"!%s@%s", dcc[idx].nick, botnetnick);
      enctmp=encrypt_string(w->key, &message[3]);
      wiretype = 1;
   } else {
      sprintf(wiretmp2, "%s@%s", dcc[idx].nick, botnetnick);
      enctmp=encrypt_string(w->key, &message[1]);
      wiretype = 0;
   }
   strcpy(wiremsg, enctmp);
   modfree(enctmp);

   enctmp=encrypt_string(w->key, "wire");
   strcpy(wirecmd, enctmp);
   modfree(enctmp);
   sprintf(wiretmp, "!wire%s %s %s", wirecmd, wiretmp2, wiremsg);
   tandout("zapf-broad %s %s\n", botnetnick, wiretmp);
   sprintf(wiretmp, "%s%s", wiretype ? "!" : "", dcc[w2->idx].nick); 
   while(w2) {
      if (!strcmp(w2->key, w->key))
         wire_display(w2->idx, w2->key, wiretmp, wiremsg);
      w2 = w2->next;
   }
   return "";
}

/* a report on the module status */
static void wire_report (int idx)
{
   int wiretot = 0; 
   wire_list *w = wirelist;
   while(w) {
   wiretot++;
   w = w->next;
   }

   modprintf(idx, "   %d wire%s using %d bytes\n", wiretot, 
                            wiretot == 1 ? "" : "s", wire_expmem());
}

static cmd_t wire_dcc[] =
{
   {"wire",   "", cmd_wire, 0},
   {"onwire", "", cmd_onwire, 0},
   {"wirelist","n", cmd_wirelist, 0},
   {0, 0, 0, 0}
};

static cmd_t wire_chof[] =
{
   {"*", "", (Function) chof_wire, "wire:chof" },
   {0, 0, 0, 0}
};

static cmd_t wire_filt[] =
{
{";*", "", (Function) cmd_putwire, "wire:filt"},
   {0, 0, 0, 0}
};

static char *wire_close()
{
wire_list   *w = wirelist;
p_tcl_hash_list H_dcc;
char   wiretmp[512];
char   *enctmp;

    /* Remove any current wire encrypt bindings */
    /* for now, don't worry about duplicate unbinds */
    
modcontext;
   while(w) {
      enctmp = encrypt_string(w->key, "wire");
      sprintf(wiretmp, "!wire%s", enctmp);
      modfree(enctmp);
      wire_bot[0].name = wiretmp;
      wire_bot[0].flags = "";
      wire_bot[0].func = (Function) wire_filter;
      H_dcc = find_hash_table("bot");
      rem_builtins(H_dcc, wire_bot);
      w = w->next;
   }
   w = wirelist;
   while(w && w->idx) {
      modprintf(w->idx, "----- WIRE module being unloaded.\n");
      modprintf(w->idx, "----- No longer on wire '%s'.\n", w->key);
      wire_leave(w->idx);
      w = wirelist;
   }
   H_dcc = find_hash_table("dcc");
   rem_builtins(H_dcc, wire_dcc);
   H_dcc = find_hash_table("filt");
   rem_builtins(H_dcc, wire_filt);
   H_dcc = find_hash_table("chof");
   rem_builtins(H_dcc, wire_chof);
   module_undepend(MODULE_NAME);
   return NULL;
}

char *wire_start ();

static Function wire_table[] =
{
   (Function) wire_start,
   (Function) wire_close,
   (Function) wire_expmem,
   (Function) wire_report,
};

char *wire_start ()
{
p_tcl_hash_list H_dcc;
   modcontext;
   module_register(MODULE_NAME, wire_table, 1, 1);
   if(!module_depend(MODULE_NAME, "eggdrop", 102, 0)) 
      return "MODULE wire cannot be loaded on Eggdrops prior to version 1.2.0";
   H_dcc = find_hash_table("dcc");
   add_builtins(H_dcc, wire_dcc);
   H_dcc = find_hash_table("filt");
   add_builtins(H_dcc, wire_filt);
   H_dcc = find_hash_table("chof");
   add_builtins(H_dcc, wire_chof);
   wirelist = 0; 
   return NULL;
}
