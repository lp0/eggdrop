/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#ifndef _H_FILES
#define _H_FILES

/* structure for file database (per directory) */
struct filler {
  char xxx[1+61+301+10+11+61+16];
  unsigned short int uuu[2];
  time_t ttt[2];
  unsigned int iii[2];
};

typedef struct {
  char version;
  unsigned short int stat;     /* misc */
  time_t timestamp;            /* last time this db was updated */
  char filename[61];
  char desc[301];              /* should be plenty */
  char uploader[10];           /* where this file came from */
  unsigned char flags_req[11]; /* (unused) from version 1.0 */
  time_t uploaded;             /* time it was uploaded */
  unsigned int size;           /* file length */
  unsigned short int gots;     /* times the file was downloaded */
  char sharelink[61];          /* points to where? */
  char flagsreq[16];           /* [v1] flags required to enter dir */
  unsigned int quota;          /* [v1] maximum k to allow in this dir */
  char unused[512-sizeof(struct filler)];
} filedb;

/* #define FILEVERSION     0x00 */
#define FILEVERSION    0x01

#define FILE_UNUSED     0x0001    /* (deleted entry) */
#define FILE_DIR        0x0002    /* it's actually a directory */
#define FILE_SHARE      0x0004    /* can be shared on the botnet */
#define FILE_HIDDEN     0x0008    /* hidden file */


/* prototypes */
filedb *findmatch();
filedb *findfile();
filedb *findmatch2();
filedb *findfile2();
#endif
