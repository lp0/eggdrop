/*
 * language.c - language support code.
 */

/*
 * DOES:
 *		Nothing <- typical BB code :)
 *
 * WILL DO:
 *		Upon loading:
 *		o	default loads english.lang, if possible.
 *		Commands:
 *		DCC .language <language>
 *		DCC .ldump
 *
 * FILE FORMAT: language.lang
 *		<textidx>,<text>
 * TEXT MESSAGE USAGE:
 *		get_language(<textidx> [,<PARMS>])
 *
 * ADDING LANGUAGES:
 *		o	Copy an existing language.lang to a new .lang
 *			file and modify as needed.  Use %1...%n, where
 *			necessary, for plug-in insertions of parameters
 *			(see language.lang).
 *		o	Ensure <language>.lang is in the lang directory.
 *		o	.language <language>
 *
 */

#include "main.h"

extern struct dcc_t * dcc;

static int langloaded = 0;

typedef struct lang_t {
   int    idx;
   char   *text;
   struct lang_t *next;
} lang_tab;

static lang_tab langtab = { 0, "MSG%.3x", 0};

static int add_message(int lidx, char *ltext)
{
   lang_tab *l = &langtab;

   while (l) {
      if (l->idx && (l->idx == lidx)) {
         nfree(l->text);
         l->text = nmalloc(strlen(ltext) + 1);
         strcpy(l->text, ltext);
         return 1;
      }
      if (!l->next) 
	break;
      l = l->next;
   }
   l->next = nmalloc(sizeof(lang_tab));
   l = l->next;
   l->idx = lidx;
   l->text = nmalloc(strlen(ltext) + 1);
   strcpy(l->text, ltext);
   l->next = 0;
   return 0;
}

static int cmd_loadlanguage (struct userrec * u,int idx, char *par)
{ 
FILE	*FLANG;
char	langfile[100];
char	*lread;
int	lidx;
char	ltext[512];
char	lbuf[512];
int	lline = 0;
int	lskip;
int	ltexts = 0;
int	ladd = 0, lupdate = 0;
char	*ctmp, *ctmp1;

   context;
   langloaded = 0;
   if(!par || !par[0]) {
      dprintf(idx, "Usage: language <language>\n");
      return 0;
   }

   if (idx != DP_LOG)
     putlog(LOG_CMDS, "*", "#%s# language %s", dcc[idx].nick, par);
   if(par[0] == '.' && par[0] == '/')
      strcpy(langfile, par);
   else
      sprintf(langfile, "./language/%s.lang", par);

   FLANG = fopen(langfile, "r");
   if(FLANG == NULL) {
      dprintf(idx, "Can't load language module: %s\n", langfile);
      return 0;
   }

   lskip = 0;
   while ((lread=fgets(lbuf, 511, FLANG))) {
      lline++;
      if(lbuf[0] != '#' || lskip) {
         if(sscanf(lbuf, "%s", ltext) != EOF ) {
            if(sscanf(lbuf, "0x%x,%500c", &lidx, ltext) != 2) {
               putlog(LOG_MISC, "*", "Malformed text line in %s at %d.", 
                                  langfile, lline);
            } else {
               ltexts++;
               *strchr( ltext, '\n') = 0;
               while(ltext[strlen(ltext) - 1] == '\\') {
                  ltext[strlen(ltext) - 1] = 0;
                  if(fgets(lbuf, 511, FLANG)) {
                     lline++;
                     *(strchr(lbuf, '\n') ) = 0;
                     if(strlen(lbuf) + strlen(ltext) > 511) {
                        putlog(LOG_MISC, "*", 
                           "Language: Message 0x%lx in %s at line %d too long.",
                                    lidx, langfile, lline);
                        lskip = 1;
                     } else 
                     strcpy(strchr(ltext, 0), lbuf);
                  } 
               }
            }

            /* We gotta fix \n's here as, being arguments to sprintf(), */
            /* they won't get translated */

	    ctmp = ltext;
            ctmp1= ltext;

            while(*ctmp1) {
               if(*ctmp1 == '\\' && *(ctmp1+1) == 'n') {
                  *ctmp = '\n';
                  ctmp1++;
               } else 
                  *ctmp = *ctmp1;
               ctmp++;
               ctmp1++;
            }
            *ctmp = '\0';

            if(add_message(lidx, ltext)) {
               lupdate++;
            } else
               ladd++;
         }
      } else {
         if (lskip && (strlen(lbuf)==1  || *(strchr(lbuf, '\n')-1) != '\\'))
            lskip = 0;
      }
   }

   fclose(FLANG);

   putlog(LOG_MISC, "*", "LANG: %d messages of %d lines loaded from %s.", 
                        ltexts, lline, langfile);
   putlog(LOG_MISC, "*", "LANG: %d adds, %d updates to message table.",
                        ladd, lupdate);
   langloaded = 1;
   return 0;
}

static int cmd_languagedump (struct userrec * u,int idx, char *par)
{
lang_tab *l = &langtab;
char	ltext2[512];
int	idx2;

   context;
   putlog(LOG_CMDS, "*", "#%s# ldump %s", dcc[idx].nick, par);

   if(par[0]) {
/* atoi (hence strtol) don't work right here for hex */
      if(strlen(par) > 2 && par[0] == '0' && par[1] == 'x')
         sscanf(par, "%x", &idx2);
      else
         idx2 = (int) strtol(par, (char **)NULL, 10);
      strcpy(ltext2, get_language(idx2));
      dprintf(idx, "0x%x: %s\n", idx2, ltext2);
      return 0;
   }
   
   dprintf(idx, " LANGIDX TEXT\n");
   while(l) {
      dprintf(idx, "0x%x   %s\n", l->idx, l->text);
      l = l->next;
   }
   return 0;
}

static char	text[512];
char * get_language (int idx)
{
lang_tab *l = &langtab;
   
   if(!idx) 
      return "MSG-0-";
   while(l) {
      if (idx == l->idx) return l->text;
      l = l->next;
   }
   sprintf(text, langtab.text, idx);
   return text;
}

int expmem_language()
{
lang_tab *l = langtab.next;

int size = 0;

   while (l) {
      size += sizeof(lang_tab);
      size += (strlen(l->text) + 1);
      l = l->next;
   }
   return size;
}

/* a report on the module status - not sure if we need this now :/ */
static int cmd_languagestatus (struct userrec * u,int idx,char * par)
{
int ltexts = 0;

   context;
   dprintf(idx, "language code report:\n");
   dprintf(idx, "   Table size: %d bytes\n", expmem_language());
   dprintf(idx, "   Text messages: %d\n", ltexts);
   return 0;
}

static int tcl_language STDVAR
{
   BADARGS(2, 2, " language");
   (void) cmd_loadlanguage(0, DP_LOG, argv[1]);
   if(!langloaded) {
      Tcl_AppendResult(irp, "Load failed.", NULL);
      return TCL_ERROR;
   }
   return TCL_OK;
}

static cmd_t langdcc[] =
{  
   {"language",   "n", cmd_loadlanguage,  NULL },
   {"ldump",      "n", cmd_languagedump, NULL },
   {"lstat",      "n", cmd_languagestatus, NULL },
};

static tcl_cmds langtcls[] =
{
   {"language", tcl_language},
   {0, 0}
};

void init_language (char * default_lang)
{
   context;
   if (default_lang) 
     cmd_loadlanguage(0,DP_LOG,default_lang); /* and robey said super-dprintf
					* was silly :) */
   else {
      add_tcl_commands(langtcls);
      add_builtins(H_dcc, langdcc,3);
   }
}
