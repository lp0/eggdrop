/*
   tclfiles.c -- handles:
   Tcl stubs for file system commands
   moved here to support modules

   dprintf'ized, 1aug96
 */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
 */

#include "../module.h"
#ifdef MODULES
#include <sys/types.h>
#include <sys/stat.h>
#include "../../files.h"
#include "filesys.h"
#else
extern char tempdir[];
#endif
#include "../../users.h"

#ifndef NO_FILE_SYSTEM
extern char dccdir[];
#ifndef MODULES
extern struct userrec *userlist;

#else

static
#endif
int tcl_getdesc STDVAR
{
   char s[301];
    BADARGS(3, 3, " dir file");
    filedb_getdesc(argv[1], argv[2], s);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setdesc STDVAR
{
   BADARGS(4, 4, " dir file desc");
   filedb_setdesc(argv[1], argv[2], argv[3]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getowner STDVAR
{
   char s[121];
    BADARGS(3, 3, " dir file");
    filedb_getowner(argv[1], argv[2], s);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setowner STDVAR
{
   BADARGS(4, 4, " dir file owner");
   filedb_setowner(argv[1], argv[2], argv[3]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getgots STDVAR
{
   int i;
   char s[10];
    BADARGS(3, 3, " dir file");
    i = filedb_getgots(argv[1], argv[2]);
    sprintf(s, "%d", i);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setlink STDVAR
{
   BADARGS(4, 4, " dir file link");
   filedb_setlink(argv[1], argv[2], argv[3]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getlink STDVAR
{
   char s[121];
    BADARGS(3, 3, " dir file");
    filedb_getlink(argv[1], argv[2], s);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setpwd STDVAR
{
   int i, idx;

    BADARGS(3, 3, " idx dir");
    i = atoi(argv[1]);
    idx = findidx(i);
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   if (dcc[idx].type != DCC_FILES) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   files_setpwd(idx, argv[2]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getpwd STDVAR
{
   int i, idx;

    BADARGS(2, 2, " idx");
    i = atoi(argv[1]);
    idx = findidx(i);
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   if (dcc[idx].type != DCC_FILES) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   Tcl_AppendResult(irp, dcc[idx].u.file->dir, NULL);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getfiles STDVAR
{
   BADARGS(2, 2, " dir");
   filedb_getfiles(irp, argv[1]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getdirs STDVAR
{
   BADARGS(2, 2, " dir");
   filedb_getdirs(irp, argv[1]);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_hide STDVAR
{
   BADARGS(3, 3, " dir file");
   filedb_change(argv[1], argv[2], FILEDB_HIDE);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_unhide STDVAR
{
   BADARGS(3, 3, " dir file");
   filedb_change(argv[1], argv[2], FILEDB_UNHIDE);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_share STDVAR
{
   BADARGS(3, 3, " dir file");
   filedb_change(argv[1], argv[2], FILEDB_SHARE);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_unshare STDVAR
{
   BADARGS(3, 3, " dir file");
   filedb_change(argv[1], argv[2], FILEDB_UNSHARE);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setflags STDVAR
{
   FILE *f;
   filedb *fdb;
   int where;
   char s[512], *p, *d;
    BADARGS(2, 3, " dir ?flags?");
    strcpy(s, argv[1]);
   if (s[strlen(s) - 1] == '/')
       s[strlen(s) - 1] = 0;
    p = strrchr(s, '/');
   if (p == NULL) {
      p = s;
      d = "";
   } else {
      *p = 0;
      p++;
      d = s;
   }
   f = filedb_open(d);
   fdb = findfile(f, p, &where);
   if (fdb == NULL)
      Tcl_AppendResult(irp, "-1", NULL);	/* no such dir */
   else if (!(fdb->stat & FILE_DIR))
      Tcl_AppendResult(irp, "-2", NULL);	/* not a dir */
   else if (argc == 3) {
      flags2str(str2flags(argv[2]), s);
      strncpy(fdb->flags_req, s, 10);
      fdb->flags_req[10] = 0;
   } else
      fdb->flags_req[0] = 0;
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
   filedb_close(f);
   Tcl_AppendResult(irp, "0", NULL);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getflags STDVAR
{
   FILE *f;
   filedb *fdb;
   int where;
   char s[512], *p, *d;
    BADARGS(2, 2, " dir");
    strcpy(s, argv[1]);
   if (s[strlen(s) - 1] == '/')
       s[strlen(s) - 1] = 0;
    p = strrchr(s, '/');
   if (p == NULL) {
      p = s;
      d = "";
   } else {
      *p = 0;
      p++;
      d = s;
   }
   f = filedb_open(d);
   fdb = findfile(f, p, &where);
   if (fdb == NULL)
      Tcl_AppendResult(irp, "", NULL);	/* no such dir */
   else if (!(fdb->stat & FILE_DIR))
      Tcl_AppendResult(irp, "", NULL);	/* not a dir */
   else {
      strcpy(s, fdb->flags_req);
      if (s[0] == '-')
	 s[0] = 0;
      Tcl_AppendResult(irp, s, NULL);
   }
   filedb_close(f);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_mkdir STDVAR
{
   char s[512], t[512], *d, *p;
   FILE *f;
   filedb *fdb;
   long where;
    BADARGS(2, 3, " dir ?required-flags?");
    strcpy(s, argv[1]);
   if (s[strlen(s) - 1] == '/')
       s[strlen(s) - 1] = 0;
    p = strrchr(s, '/');
   if (p == NULL) {
      p = s;
      d = "";
   } else {
      *p = 0;
      p++;
      d = s;
   }
   f = filedb_open(d);
   fdb = findfile(f, p, &where);
   if (fdb == NULL) {
      filedb x;
      sprintf(t, "%s%s/%s", dccdir, d, p);
      if (mkdir(t, 0755) == 0) {
	 x.version = FILEVERSION;
	 x.stat = FILE_DIR;
	 x.desc[0] = 0;
	 x.uploader[0] = 0;
	 strcpy(x.filename, argv[1]);
	 x.flags_req[0] = 0;
	 x.uploaded = time(NULL);
	 x.size = 0;
	 x.gots = 0;
	 x.sharelink[0] = 0;
	 Tcl_AppendResult(irp, "0", NULL);
	 if (argc == 3) {
	    char w[100];
	    flags2str(str2flags(argv[2]), w);
	    strncpy(x.flags_req, w, 10);
	    x.flags_req[10] = 0;
	 }
	 where = findempty(f);
	 fseek(f, where, SEEK_SET);
	 fwrite(&x, sizeof(filedb), 1, f);
	 filedb_close(f);
	 return TCL_OK;
      }
      Tcl_AppendResult(irp, "1", NULL);
      filedb_close(f);
      return TCL_OK;
   }
   /* already exists! */
   if (!(fdb->stat & FILE_DIR)) {
      Tcl_AppendResult(irp, "2", NULL);
      filedb_close(f);
      return TCL_OK;
   }
   if (argc == 3) {
      flags2str(str2flags(argv[2]), s);
      strncpy(fdb->flags_req, s, 10);
      fdb->flags_req[10] = 0;
   } else {
      fdb->flags_req[0] = 0;
   }
   Tcl_AppendResult(irp, "0", NULL);
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
   filedb_close(f);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_rmdir STDVAR
{
   FILE *f;
   filedb *fdb;
   long where;
   char s[256], t[512], *d, *p;
    BADARGS(2, 2, " dir");
   if (strlen(argv[1]) > 80)
       argv[1][80] = 0;
    strcpy(s, argv[1]);
   if (s[strlen(s) - 1] == '/')
       s[strlen(s) - 1] = 0;
    p = strrchr(s, '/');
   if (p == NULL) {
      p = s;
      d = "";
   } else {
      *p = 0;
      p++;
      d = s;
   }
   f = filedb_open(d);
   fdb = findfile(f, p, &where);
   if (fdb == NULL) {
      Tcl_AppendResult(irp, "1", NULL);
      filedb_close(f);
      return TCL_OK;
   }
   if (!(fdb->stat & FILE_DIR)) {
      Tcl_AppendResult(irp, "1", NULL);
      filedb_close(f);
      return TCL_OK;
   }
   /* erase '.filedb' and '.files' if they exist */
   sprintf(t, "%s%s/%s/.filedb", dccdir, d, p);
   unlink(t);
   sprintf(t, "%s%s/%s/.files", dccdir, d, p);
   unlink(t);
   sprintf(t, "%s%s/%s", dccdir, d, p);
   if (rmdir(t) == 0) {
      fdb->stat |= FILE_UNUSED;
      fseek(f, where, SEEK_SET);
      fwrite(fdb, sizeof(filedb), 1, f);
      filedb_close(f);
      Tcl_AppendResult(irp, "0", NULL);
      return TCL_OK;
   }
   Tcl_AppendResult(irp, "1", NULL);
   filedb_close(f);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_mv_cp PROTO4(Tcl_Interp *, irp, int, argc, char **, argv, int, copy)
{
   char *p, fn[161], oldpath[161], s[161], s1[161], newfn[161], newpath[161];
   int ok, only_first, skip_this;
   FILE *f, *g;
   filedb *fdb, x, *z;
   long where, gwhere, wherez;

   BADARGS(3, 3, " oldfilepath newfilepath");
   strcpy(fn, argv[1]);
   p = strrchr(fn, '/');
   if (p != NULL) {
      *p = 0;
      strncpy(s, fn, 160);
      s[160] = 0;
      strcpy(fn, p + 1);
      if (!resolve_dir("/", s, oldpath, USER_OWNER | USER_BOT)) {	/* tcl can do
									 * anything */
	 Tcl_AppendResult(irp, "-1", NULL);	/* invalid source */
	 return TCL_OK;
      }
   } else
      strcpy(oldpath, "/");
   strncpy(s, argv[2], 160);
   s[160] = 0;
   if (!resolve_dir("/", s, newpath, USER_OWNER | USER_BOT)) {
      /* destination is not just a directory */
      p = strrchr(s, '/');
      if (p == NULL) {
	 strcpy(newfn, s);
	 s[0] = 0;
      } else {
	 *p = 0;
	 strcpy(newfn, p + 1);
      }
      if (!resolve_dir("/", s, newpath, USER_OWNER | USER_BOT)) {
	 Tcl_AppendResult(irp, "-2", NULL);	/* invalid desto */
	 return TCL_OK;
      }
   } else
      newfn[0] = 0;
   /* stupidness checks */
   if ((strcmp(oldpath, newpath) == 0) &&
       ((!newfn[0]) || (strcmp(newfn, fn) == 0))) {
      Tcl_AppendResult(irp, "-3", NULL);	/* stoopid copy to self */
      return TCL_OK;
   }
   /* be aware of 'cp * this.file' possibility: ONLY COPY FIRST ONE */
   if (((strchr(fn, '?') != NULL) || (strchr(fn, '*') != NULL)) && (newfn[0]))
      only_first = 1;
   else
      only_first = 0;
   f = filedb_open(oldpath);
   if (strcmp(oldpath, newpath) == 0)
      g = NULL;
   else
      g = filedb_open(newpath);
   where = 0L;
   ok = 0;
   fdb = findmatch(f, fn, &where);
   if (fdb == NULL) {
      Tcl_AppendResult(irp, "-4", NULL);	/* nomatch */
      filedb_close(f);
      if (g != NULL)
	 filedb_close(g);
      return TCL_OK;
   }
   while (fdb != NULL) {
      skip_this = 0;
      if (!(fdb->stat & (FILE_HIDDEN | FILE_DIR))) {
	 sprintf(s, "%s%s%s%s", dccdir, oldpath, oldpath[0] ? "/" : "", fdb->filename);
	 sprintf(s1, "%s%s%s%s", dccdir, newpath, newpath[0] ? "/" : "", newfn[0] ?
		 newfn : fdb->filename);
	 if (strcmp(s, s1) == 0) {
	    Tcl_AppendResult(irp, "-3", NULL);	/* stoopid copy to self */
	    skip_this = 1;
	 }
	 /* check for existence of file with same name in new dir */
	 if (g == NULL)
	    z = findfile2(f, newfn[0] ? newfn : fdb->filename, &wherez);
	 else
	    z = findfile2(g, newfn[0] ? newfn : fdb->filename, &wherez);
	 if (z != NULL) {
	    /* it's ok if the entry in the new dir is a normal file (we'll */
	    /* just scrap the old entry and overwrite the file) -- but if */
	    /* it's a directory, this file has to be skipped */
	    if (z->stat & FILE_DIR) {
	       /* skip */
	       skip_this = 1;
	    } else {
	       z->stat |= FILE_UNUSED;
	       if (g == NULL) {
		  fseek(f, wherez, SEEK_SET);
		  fwrite(z, sizeof(filedb), 1, f);
	       } else {
		  fseek(g, wherez, SEEK_SET);
		  fwrite(z, sizeof(filedb), 1, g);
	       }
	    }
	 }
	 if (!skip_this) {
	    if ((fdb->sharelink[0]) || (copyfile(s, s1) == 0)) {
	       /* raw file moved okay: create new entry for it */
	       ok++;
	       if (g == NULL)
		  gwhere = findempty(f);
	       else
		  gwhere = findempty(g);
	       x.version = FILEVERSION;
	       x.stat = fdb->stat;
	       x.flags_req[0] = 0;
	       strcpy(x.filename, fdb->filename);
	       strcpy(x.desc, fdb->desc);
	       if (newfn[0])
		  strcpy(x.filename, newfn);
	       strcpy(x.uploader, fdb->uploader);
	       x.uploaded = fdb->uploaded;
	       x.size = fdb->size;
	       x.gots = fdb->gots;
	       strcpy(x.sharelink, fdb->sharelink);
	       if (g == NULL) {
		  fseek(f, gwhere, SEEK_SET);
		  fwrite(&x, sizeof(filedb), 1, f);
	       } else {
		  fseek(g, gwhere, SEEK_SET);
		  fwrite(&x, sizeof(filedb), 1, g);
	       }
	       if (!copy) {
		  unlink(s);
		  fdb->stat |= FILE_UNUSED;
		  fseek(f, where, SEEK_SET);
		  fwrite(fdb, sizeof(filedb), 1, f);
	       }
	    }
	 }
      }
      where += sizeof(filedb);
      fdb = findmatch(f, fn, &where);
      if ((ok) && (only_first))
	 fdb = NULL;
   }
   if (!ok)
      Tcl_AppendResult(irp, "-4", NULL);	/* nomatch */
   else {
      char x[30];
      sprintf(x, "%d", ok);
      Tcl_AppendResult(irp, x, NULL);
   }
   filedb_close(f);
   if (g != NULL)
      filedb_close(g);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_mv STDVAR
{
   return tcl_mv_cp(irp, argc, argv, 0);
}

#ifdef MODULES
static
#endif
int tcl_cp STDVAR
{
   return tcl_mv_cp(irp, argc, argv, 1);
}

#ifdef MODULES
static
#endif
int tcl_filesend STDVAR
{
   int i, idx;
   char s[10];

    BADARGS(3, 4, " idx filename ?nick?");
    i = atoi(argv[1]);
    idx = findidx(i);
   if (idx < 0) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   if (dcc[idx].type != DCC_FILES) {
      Tcl_AppendResult(irp, "invalid idx", NULL);
      return TCL_ERROR;
   }
   if (argc == 4)
      i = files_get(idx, argv[2], argv[3]);
   else
      i = files_get(idx, argv[2], "");
   sprintf(s, "%d", i);
   Tcl_AppendResult(irp, s, NULL);
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getuploads STDVAR
{
   struct userrec *u;
   char s[81];
    BADARGS(2, 2, " handle");
    u = get_user_by_handle(userlist, argv[1]);
   if (u == NULL)
       return TCL_OK;
    sprintf(s, "%u %lu", u->uploads, u->upload_k);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_getdnloads STDVAR
{
   struct userrec *u;
   char s[81];
    BADARGS(2, 2, " handle");
    u = get_user_by_handle(userlist, argv[1]);
   if (u == NULL)
       return TCL_OK;
    sprintf(s, "%u %lu", u->dnloads, u->dnload_k);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setuploads STDVAR
{
   BADARGS(4, 4, " handle files k");
   set_handle_uploads(userlist, argv[1], atoi(argv[2]), atoi(argv[3]));
   return TCL_OK;
}

#ifdef MODULES
static
#endif
int tcl_setdnloads STDVAR
{
   BADARGS(4, 4, " handle files k");
   set_handle_dnloads(userlist, argv[1], atoi(argv[2]), atoi(argv[3]));
   return TCL_OK;
}

#ifdef MODULES
tcl_cmds mytcls[] =
{
   {"getdesc", tcl_getdesc},
   {"getowner", tcl_getowner},
   {"setdesc", tcl_setdesc},
   {"setowner", tcl_setowner},
   {"getgots", tcl_getgots},
   {"getpwd", tcl_getpwd},
   {"setpwd", tcl_setpwd},
   {"getlink", tcl_getlink},
   {"setlink", tcl_setlink},
   {"getfiles", tcl_getfiles},
   {"getdirs", tcl_getdirs},
   {"hide", tcl_hide},
   {"unhide", tcl_unhide},
   {"share", tcl_share},
   {"unshare", tcl_unshare},
   {"filesend", tcl_filesend},
   {"getuploads", tcl_getuploads},
   {"setuploads", tcl_setuploads},
   {"getdnloads", tcl_getdnloads},
   {"setdnloads", tcl_setdnloads},
   {"mkdir", tcl_mkdir},
   {"rmdir", tcl_rmdir},
   {"cp", tcl_cp},
   {"mv", tcl_mv},
   {"getflags", tcl_getflags},
   {"setflags", tcl_setflags},
   {0, 0}
};
#endif
#endif
