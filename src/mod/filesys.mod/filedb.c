/*
   filedb.c -- handles:
   low-level manipulation of the filesystem database files
   reaction to remote requests for files 

   dprintf'ized, 25feb96
   english, 5mar96
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../../files.h"
#include "filesys.h"
#else
extern int copy_to_tmp;
#endif

#ifndef NO_FILE_SYSTEM

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

extern char dccdir[];

/* where to put the filedb, if not in a hidden '.filedb' file in */
/* each directory */
char filedb_path[121] = "";

/* lock the file, using fcntl */
static void lockfile PROTO1(FILE *, f)
{
   struct flock fl;
   fl.l_type = F_WRLCK;
   fl.l_start = 0;
   fl.l_whence = SEEK_SET;
   fl.l_len = 0;
   /* block on lock: */
   fcntl(fileno(f), F_SETLKW, &fl);
}

/* unlock the file */
static void unlockfile PROTO1(FILE *, f)
{
   struct flock fl;
   fl.l_type = F_UNLCK;
   fl.l_start = 0;
   fl.l_whence = SEEK_SET;
   fl.l_len = 0;
   fcntl(fileno(f), F_SETLKW, &fl);
}

/* use a where of 0 to start out, then increment 1 space for each next */
filedb *findmatch PROTO3(FILE *, f, char *, lookfor, long *, where)
{
   static filedb fdb;
   char match[256];
   strncpy(match, lookfor, 255);
   match[255] = 0;
   /* clip any trailing / */
   if ((match[0]) && (match[strlen(match) - 1] == '/'))
      match[strlen(match) - 1] = 0;
   fseek(f, *where, SEEK_SET);
   while (!feof(f)) {
      *where = ftell(f);
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if (!(fdb.stat & FILE_UNUSED) && (wild_match_file(match, fdb.filename)))
	    return &fdb;
      }
   }
   return NULL;
}

filedb *findfile PROTO3(FILE *, f, char *, name, long *, where)
{
   filedb *fdb;
   long w = 0L;			/* force a rewind */
   fdb = findmatch(f, name, &w);
   if (where != NULL)
      *where = w;
   return fdb;
}

/* alternate version so the buffers don't get overwritten */
filedb *findmatch2 PROTO3(FILE *, f, char *, lookfor, long *, where)
{
   static filedb fdb;
   char match[256];
   strcpy(match, lookfor);
   /* clip any trailing / */
   if ((match[0]) && (match[strlen(match) - 1] == '/'))
      match[strlen(match) - 1] = 0;
   fseek(f, *where, SEEK_SET);
   while (!feof(f)) {
      *where = ftell(f);
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if (!(fdb.stat & FILE_UNUSED) && (wild_match_file(match, fdb.filename)))
	    return &fdb;
      }
   }
   return NULL;
}

filedb *findfile2 PROTO3(FILE *, f, char *, name, long *, where)
{
   filedb *fdb;
   long w = 0L;			/* force a rewind */
   fdb = findmatch2(f, name, &w);
   if (where != NULL)
      *where = w;
   return fdb;
}

long findempty PROTO1(FILE *, f)
{
   long where = 0L;
   filedb fdb;
   rewind(f);
   while (!feof(f)) {
      where = ftell(f);
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if (fdb.stat & FILE_UNUSED)
	    return where;
      }
   }
   fseek(f, 0L, SEEK_END);
   where = ftell(f);
   return where;
}

void filedb_timestamp PROTO1(FILE *, f)
{
   filedb fdb;
   int x;
   /* read 1st filedb entry if it's there */
   rewind(f);
   x = fread(&fdb, sizeof(filedb), 1, f);
   if (x < 1) {
      /* MAKE 1st filedb entry then! */
      fdb.version = FILEVERSION;
      fdb.stat = FILE_UNUSED;
   }
   fdb.timestamp = time(NULL);
   rewind(f);
   fwrite(&fdb, sizeof(filedb), 1, f);
}

/* return 1 if i find a '.files' and convert it */
int convert_old_db PROTO2(char *, path, char *, newfiledb)
{
   FILE *f, *g;
   char s[256], fn[61], nick[20], tm[20];
   filedb fdb;
   int in_file = 0;
   long where;
   struct stat st;
   sprintf(s, "%s/.files", path);
   f = fopen(s, "r");
   if (f == NULL)
      return 0;
   g = fopen(newfiledb, "w+b");
   if (g == NULL) {
      putlog(LOG_MISC, "(!) Can't create filedb in %s", newfiledb);
      fclose(f);
      return 0;
   }
   putlog(LOG_FILES, "*", FILES_CONVERT, path);
   where = ftell(g);
   /* scan contents of .files and painstakingly create .filedb entries */
   while (!feof(f)) {
      fgets(s, 120, f);
      if (s[strlen(s) - 1] == '\n')
	 s[strlen(s) - 1] = 0;
      if (!feof(f)) {
	 nsplit(fn, s);
	 rmspace(fn);
	 if ((fn[0]) && (fn[0] != ';') && (fn[0] != '#')) {
	    /* not comment */
	    if (fn[0] == '-') {
	       /* adjust comment for current file */
	       if (in_file) {
		  rmspace(s);
		  if (strlen(s) + strlen(fdb.desc) <= 600) {
		     strcat(fdb.desc, "\n");
		     strcat(fdb.desc, s);
		     fseek(g, where, SEEK_SET);
		     fwrite(&fdb, sizeof(filedb), 1, g);
		  }
	       }
	    } else {
	       in_file = 1;
	       where = ftell(g);
	       rmspace(s);
	       nsplit(nick, s);
	       rmspace(nick);
	       rmspace(s);
	       nsplit(tm, s);
	       rmspace(tm);
	       rmspace(s);
	       if (fn[strlen(fn) - 1] == '/')
		  fn[strlen(fn) - 1] = 0;
	       fdb.version = FILEVERSION;
	       fdb.stat = 0;
	       fdb.desc[0] = 0;
	       strcpy(fdb.filename, fn);
	       strcpy(fdb.uploader, nick);
	       fdb.gots = atoi(s);
	       fdb.sharelink[0] = 0;
	       fdb.uploaded = atol(tm);
	       fdb.flags_req[0] = 0;
	       sprintf(s, "%s/%s", path, fn);
	       if (stat(s, &st) == 0) {
		  /* file is okay */
		  if (S_ISDIR(st.st_mode)) {
		     fdb.stat |= FILE_DIR;
		     if (nick[0] == '+') {
			char x[100];
			flags2str(str2flags(&nick[1]), x);	/* we only want valid flags */
			strncpy(fdb.flags_req, x, 10);
			fdb.flags_req[10] = 0;
		     }
		  }
		  fdb.size = st.st_size;
		  fwrite(&fdb, sizeof(filedb), 1, g);
	       } else
		  in_file = 0;	/* skip */
	    }
	 }
      }
   }
   fseek(g, 0, SEEK_END);
   fclose(g);
   fclose(f);
   return 1;
}

void filedb_update PROTO2(char *, path, FILE *, f)
{
   struct dirent *dd;
   DIR *dir;
   filedb *fdb, fdb1;
   char name[61];
   long where;
   struct stat st;
   char s[512];
   /* FIRST: make sure every real file is in the database */
   dir = opendir(path);
   if (dir == NULL) {
      putlog(LOG_MISC, "*", FILES_NOUPDATE);
      return;
   }
   dd = readdir(dir);
   while (dd != NULL) {
      strncpy(name, dd->d_name, 60);
      name[60] = 0;
      if (NAMLEN(dd) <= 60)
	 name[NAMLEN(dd)] = 0;
      else {
	 /* truncate name on disk */
	 char s1[512], s2[256];
	 strcpy(s1, path);
	 strcat(s1, "/");
	 strncat(s1, dd->d_name, NAMLEN(dd));
	 s1[strlen(path) + NAMLEN(dd) + 1] = 0;
	 sprintf(s2, "%s/%s", path, name);
	 movefile(s1, s2);
      }
      if (name[0] != '.') {
	 sprintf(s, "%s/%s", path, name);
	 stat(s, &st);
	 fdb = findfile(f, name, &where);
	 if (fdb == NULL) {
	    /* new file! */
	    where = findempty(f);
	    fseek(f, where, SEEK_SET);
	    fdb1.version = FILEVERSION;
	    fdb1.stat = 0;	/* by default, visible regular file */
	    strcpy(fdb1.filename, name);
	    fdb1.desc[0] = 0;
	    strcpy(fdb1.uploader, botnetnick);
	    fdb1.gots = 0;
	    fdb1.flags_req[0] = 0;
	    fdb1.uploaded = time(NULL);
	    fdb1.size = st.st_size;
	    fdb1.sharelink[0] = 0;
	    if (S_ISDIR(st.st_mode))
	       fdb1.stat |= FILE_DIR;
	    fwrite(&fdb1, sizeof(filedb), 1, f);
	 } else {
	    /* update size if needed */
	    fdb->size = st.st_size;
	    fseek(f, where, SEEK_SET);
	    fwrite(fdb, sizeof(filedb), 1, f);
	 }
      }
      dd = readdir(dir);
   }
   closedir(dir);
   /* SECOND: make sure every db file is real */
   rewind(f);
   while (!feof(f)) {
      where = ftell(f);
      fread(&fdb1, sizeof(filedb), 1, f);
      if ((!feof(f)) && !(fdb1.stat & FILE_UNUSED) && !(fdb1.sharelink[0])) {
	 sprintf(s, "%s/%s", path, fdb1.filename);
	 if (stat(s, &st) != 0) {
	    /* gone file */
	    fseek(f, where, SEEK_SET);
	    fdb1.stat |= FILE_UNUSED;
	    fwrite(&fdb1, sizeof(filedb), 1, f);
	    /* sunos and others will puke bloody chunks if you write the */
	    /* last record in a file and then attempt to read to EOF: */
	    fseek(f, where, SEEK_SET);
	 }
      }
   }
   /* write new timestamp */
   filedb_timestamp(f);
}

int count = 0;

FILE *filedb_open PROTO1(char *, path)
{
   char s[DIRLEN], npath[DIRLEN];
   FILE *f;
   filedb fdb;
   struct stat st;
   if (count >= 2) {
      putlog(LOG_MISC, "*", "(@) warning: %d open filedb's", count);
   }
   sprintf(npath, "%s%s", dccdir, path);
   /* use alternate filename if requested */
   if (filedb_path[0]) {
      char s2[DIRLEN], *p;
      strcpy(s2, path);
      p = s2;
      while (*p++)
	 if (*p == '/')
	    *p = '.';
      sprintf(s, "%sfiledb.%s", filedb_path, s2);
      if (s[strlen(s) - 1] == '.')
	 s[strlen(s) - 1] = 0;
   } else
      sprintf(s, "%s/.filedb", npath);
   f = fopen(s, "r+b");
   if (f == NULL) {
      /* attempt to convert */
      if (convert_old_db(npath, s)) {
	 f = fopen(s, "r+b");
	 if (f == NULL) {
	    putlog(LOG_MISC, FILES_NOCONVERT, npath);
	    return NULL;
	 }
	 lockfile(f);
	 filedb_update(npath, f);	/* make it correct */
	 count++;
	 return f;
      }
      /* create new database and fix it up */
      f = fopen(s, "w+b");
      if (f == NULL)
	 return NULL;
      lockfile(f);
      filedb_update(npath, f);
      count++;
      return f;
   }
   /* lock it from other bots: */
   lockfile(f);
   /* check the timestamp... */
   fread(&fdb, sizeof(filedb), 1, f);
   stat(npath, &st);
   /* update filedb if: */
   /*  + it's been 6 hours since it was last updated */
   /*  + the directory has been visibly modified since then */
   /* (6 hours may be a bit often) */
   if (((time(NULL) - fdb.timestamp) > (6 * 3600)) || (fdb.timestamp < st.st_mtime) ||
       (fdb.timestamp < st.st_ctime)) {
      /* file database isn't up-to-date! */
      filedb_update(npath, f);
   }
   count++;
   return f;
}

void filedb_close PROTO1(FILE *, f)
{
   filedb_timestamp(f);
   fseek(f, 0L, SEEK_END);
   count--;
   unlockfile(f);
   fclose(f);
}

void filedb_add PROTO3(FILE *, f, char *, filename, char *, nick)
{
   unsigned long where;
   filedb *fdb;
   /* when the filedb was opened, a record was already created */
   fdb = findfile(f, filename, &where);
   if (fdb == NULL)
      return;
   strcpy(fdb->uploader, nick);
   fdb->uploaded = time(NULL);
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
}

/* fills fdb if can find a match and returns 1, else returns 0 */
int filedb_match PROTO4(FILE *, f, char *, match, filedb *, fdb, int, first)
{
   if (first)
      rewind(f);
   while (!feof(f)) {
      fread(fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if (wild_match_file(match, fdb->filename))
	    return 1;
      }
   }
   return 0;
}

void filedb_ls PROTO5(FILE *, f, int, idx, int, atr, char *, mask, int, showall)
{
   filedb fdb;
   int ok = 0, cnt = 0, is = 0;
   char s[81], s1[81], *p;
   rewind(f);
   while (!feof(f)) {
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 ok = 1;
	 if (fdb.stat & FILE_UNUSED)
	    ok = 0;
	 if (fdb.stat & FILE_DIR) {
	    /* check permissions */
	    if (!flags_ok(str2flags(fdb.flags_req), atr))
	       ok = 0;
	 }
	 if (ok)
	    is = 1;
	 if (!wild_match_file(mask, fdb.filename))
	    ok = 0;
	 if ((fdb.stat & FILE_HIDDEN) && !(showall))
	    ok = 0;
	 if (ok) {
	    /* display it! */
	    if (cnt == 0) {
	       modprintf(idx, FILES_LSHEAD1);
	       modprintf(idx, FILES_LSHEAD2);
	    }
	    if (fdb.stat & FILE_DIR) {
	       char s2[50];
	       /* too long? */
	       if (strlen(fdb.filename) > 45) {
		  modprintf(idx, "%s/\n", fdb.filename);
		  s2[0] = 0;
		  /* causes filename to be displayed on its own line */
	       } else
		  sprintf(s2, "%s/", fdb.filename);
	       if ((fdb.flags_req[0]) && (atr & (USER_MASTER | USER_JANITOR))) {
		  modprintf(idx, "%-45s <DIR%s>  (%s +%s)\n", s2, fdb.stat & FILE_SHARE ?
			    " SHARE" : "", FILES_REQUIRES, fdb.flags_req);
	       } else
		  modprintf(idx, "%-45s <DIR>\n", s2);
	    } else {
	       char s2[41];
	       s2[0] = 0;
	       if (showall) {
		  if (fdb.stat & FILE_SHARE)
		     strcat(s2, " (shr)");
		  if (fdb.stat & FILE_HIDDEN)
		     strcat(s2, " (hid)");
	       }
	       strcpy(s, ctime(&fdb.uploaded));
	       s[10] = 0;
	       s[7] = 0;
	       s[24] = 0;
	       strcpy(s, &s[8]);
	       strcpy(&s[2], &s[4]);
	       strcpy(&s[5], &s[22]);
	       if (fdb.size < 1024)
		  sprintf(s1, "%5d", fdb.size);
	       else
		  sprintf(s1, "%4dk", (int) (fdb.size / 1024));
	       if (fdb.sharelink[0])
		  strcpy(s1, "     ");
	       /* too long? */
	       if (strlen(fdb.filename) > 30) {
		  modprintf(idx, "%s\n", fdb.filename);
		  fdb.filename[0] = 0;
		  /* causes filename to be displayed on its own line */
	       }
	       modprintf(idx, "%-30s %s  %-9s (%s)  %6d%s\n", fdb.filename, s1,
			 fdb.uploader, s, fdb.gots, s2);
	       if (fdb.sharelink[0]) {
		  modprintf(idx, "   --> %s\n", fdb.sharelink);
	       }
	    }
	    if (fdb.desc[0]) {
	       p = strchr(fdb.desc, '\n');
	       while (p != NULL) {
		  *p = 0;
		  if (fdb.desc[0])
		     modprintf(idx, "   %s\n", fdb.desc);
		  strcpy(fdb.desc, p + 1);
		  p = strchr(fdb.desc, '\n');
	       }
	       if (fdb.desc[0])
		  modprintf(idx, "   %s\n", fdb.desc);
	    }
	    cnt++;
	 }
      }
   }
   if (is == 0)
      modprintf(idx, FILES_NOFILES);
   else if (cnt == 0)
      modprintf(idx, FILES_NOMATCH);
   else
      modprintf(idx, "--- %d file%s.\n", cnt, cnt > 1 ? "s" : "");
}

void remote_filereq PROTO3(int, idx, char *, from, char *, file)
{
   char *p, what[256], dir[256], s[256], s1[256];
   FILE *f;
   filedb *fdb;
   int i;

   strcpy(what, file);
   p = strrchr(what, '/');
   if (p == NULL)
      dir[0] = 0;
   else {
      *p = 0;
      strcpy(dir, what);
      strcpy(what, p + 1);
   }
   f = filedb_open(dir);
   if (f == NULL) {
      modprintf(idx, "filereject %s:%s/%s %s %s\n", botnetnick, dir, what,
		from, FILES_DIRDNE);
      return;
   }
   fdb = findfile(f, what, NULL);
   if (fdb == NULL) {
      modprintf(idx, "filereject %s:%s/%s %s %s\n", botnetnick, dir, what,
		from, FILES_FILEDNE);
      filedb_close(f);
      return;
   }
   if ((!(fdb->stat & FILE_SHARE)) || (fdb->stat & (FILE_HIDDEN | FILE_DIR))) {
      modprintf(idx, "filereject %s:%s/%s %s %s\n", botnetnick, dir, what,
		from, FILES_NOSHARE);
      filedb_close(f);
      return;
   }
   filedb_close(f);
   /* copy to /tmp if needed */
   sprintf(s1, "%s%s%s%s", dccdir, dir, dir[0] ? "/" : "", what);
   if (copy_to_tmp) {
      sprintf(s, "%s%s", tempdir, what);
      copyfile(s1, s);
   } else
      strcpy(s, s1);
   i = raw_dcc_send(s, "*remote", FILES_REMOTE, s);
   if (i > 0) {
      wipe_tmp_filename(s, -1);
      modprintf(idx, "filereject %s:%s/%s %s %s\n", botnetnick, dir, what,
		from, FILES_SENDERR);
      return;
   }
   /* grab info from dcc struct and bounce real request across net */
   i = dcc_total - 1;
   modprintf(idx, "filesend %s:%s/%s %s %lu %d %lu\n", botnetnick, dir,
    what, from, iptolong(getmyip()), dcc[i].port, dcc[i].u.xfer->length);
   putlog(LOG_FILES, "*", FILES_REMOTEREQ, dir, dir[0] ? "/" : "", what);
}

/*** for tcl: ***/

void filedb_getdesc PROTO3(char *, dir, char *, fn, char *, desc)
{
   FILE *f;
   filedb *fdb;
   f = filedb_open(dir);
   if (f == NULL) {
      desc[0] = 0;
      return;
   }
   fdb = findfile(f, fn, NULL);
   filedb_close(f);
   if (fdb == NULL) {
      desc[0] = 0;
      return;
   }
   strcpy(desc, fdb->desc);
   return;
}

void filedb_getowner PROTO3(char *, dir, char *, fn, char *, owner)
{
   FILE *f;
   filedb *fdb;
   f = filedb_open(dir);
   if (f == NULL) {
      owner[0] = 0;
      return;
   }
   fdb = findfile(f, fn, NULL);
   filedb_close(f);
   if (fdb == NULL) {
      owner[0] = 0;
      return;
   }
   strcpy(owner, fdb->uploader);
   return;
}

int filedb_getgots PROTO2(char *, dir, char *, fn)
{
   FILE *f;
   filedb *fdb;
   f = filedb_open(dir);
   if (f == NULL)
      return 0;
   fdb = findfile(f, fn, NULL);
   filedb_close(f);
   if (fdb == NULL)
      return 0;
   return fdb->gots;
}

void filedb_setdesc PROTO3(char *, dir, char *, fn, char *, desc)
{
   FILE *f;
   filedb *fdb;
   long where;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   fdb = findfile(f, fn, &where);
   if (fdb == NULL) {
      filedb_close(f);
      return;
   }
   strncpy(fdb->desc, desc, 300);
   fdb->desc[300] = 0;
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
   filedb_close(f);
   return;
}

void filedb_setowner PROTO3(char *, dir, char *, fn, char *, owner)
{
   FILE *f;
   filedb *fdb;
   long where;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   fdb = findfile(f, fn, &where);
   if (fdb == NULL) {
      filedb_close(f);
      return;
   }
   strncpy(fdb->uploader, owner, 9);
   fdb->uploader[9] = 0;
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
   filedb_close(f);
   return;
}

void filedb_setlink PROTO3(char *, dir, char *, fn, char *, link)
{
   FILE *f;
   filedb fdb, *x;
   long where;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   x = findfile(f, fn, &where);
   if (x != NULL) {
      /* change existing one? */
      if ((x->stat & FILE_DIR) || !(x->sharelink[0]))
	 return;
      if (!link[0]) {
	 /* erasing file */
	 x->stat |= FILE_UNUSED;
      } else {
	 strncpy(x->sharelink, link, 60);
	 x->sharelink[60] = 0;
      }
      fseek(f, where, SEEK_SET);
      fwrite(x, sizeof(filedb), 1, f);
      filedb_close(f);
      return;
   }
   fdb.version = FILEVERSION;
   fdb.stat = 0;
   fdb.desc[0] = 0;
   strcpy(fdb.uploader, botnetnick);
   strncpy(fdb.filename, fn, 30);
   fdb.filename[30] = 0;
   fdb.flags_req[0] = 0;
   fdb.uploaded = time(NULL);
   fdb.size = 0;
   fdb.gots = 0;
   strncpy(fdb.sharelink, link, 60);
   fdb.sharelink[60] = 0;
   where = findempty(f);
   fseek(f, where, SEEK_SET);
   fwrite(&fdb, sizeof(filedb), 1, f);
   filedb_close(f);
}

void filedb_getlink PROTO3(char *, dir, char *, fn, char *, link)
{
   FILE *f;
   filedb *fdb;
   f = filedb_open(dir);
   link[0] = 0;
   if (f == NULL)
      return;
   fdb = findfile(f, fn, NULL);
   if (fdb == NULL) {
      filedb_close(f);
      return;
   }
   if (fdb->stat & FILE_DIR) {
      filedb_close(f);
      return;
   }
   strcpy(link, fdb->sharelink);
   filedb_close(f);
   return;
}

void filedb_getfiles PROTO2(Tcl_Interp *, irp, char *, dir)
{
   FILE *f;
   filedb fdb;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   rewind(f);
   while (!feof(f)) {
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if (!(fdb.stat & (FILE_DIR | FILE_UNUSED)))
	    Tcl_AppendElement(irp, fdb.filename);
      }
   }
   filedb_close(f);
}

void filedb_getdirs PROTO2(Tcl_Interp *, irp, char *, dir)
{
   FILE *f;
   filedb fdb;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   rewind(f);
   while (!feof(f)) {
      fread(&fdb, sizeof(filedb), 1, f);
      if (!feof(f)) {
	 if ((!(fdb.stat & FILE_UNUSED)) && (fdb.stat & FILE_DIR))
	    Tcl_AppendElement(irp, fdb.filename);
      }
   }
   filedb_close(f);
}

void filedb_change PROTO3(char *, dir, char *, fn, int, what)
{
   FILE *f;
   filedb *fdb;
   long where;
   f = filedb_open(dir);
   if (f == NULL)
      return;
   fdb = findfile(f, fn, &where);
   if (fdb == NULL) {
      filedb_close(f);
      return;
   }
   if (fdb->stat & FILE_DIR) {
      filedb_close(f);
      return;
   }
   switch (what) {
   case FILEDB_HIDE:
      fdb->stat |= FILE_HIDDEN;
      break;
   case FILEDB_UNHIDE:
      fdb->stat &= ~FILE_HIDDEN;
      break;
   case FILEDB_SHARE:
      fdb->stat |= FILE_SHARE;
      break;
   case FILEDB_UNSHARE:
      fdb->stat &= ~FILE_SHARE;
      break;
   }
   fseek(f, where, SEEK_SET);
   fwrite(fdb, sizeof(filedb), 1, f);
   filedb_close(f);
   return;
}

#endif				/* !NO_FILE_SYSTEM */
