/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/
#ifndef _H_ENGLISH
#define _H_ENGLISH

#define USAGE            "Usage"
#define FAILED           "Failed.\n"

/* file area */
#define FILES_CONVERT    "Converting filesystem image in %s ..."
#define FILES_NOUPDATE   "filedb-update: can't open directory!"
#define FILES_NOCONVERT  "(!) Broken convert to filedb in %s"
#define FILES_LSHEAD1    "Filename                        Size  Sent by/Date         # Gets\n"
#define FILES_LSHEAD2    "------------------------------  ----  -------------------  ------\n"
#define FILES_NOFILES    "No files in this directory.\n"
#define FILES_NOMATCH    "No matching files.\n"
#define FILES_DIRDNE     "Directory does not exist"
#define FILES_FILEDNE    "File does not exist"
#define FILES_NOSHARE    "File is not shared"
#define FILES_REMOTE     "(remote)"
#define FILES_SENDERR    "Error trying to send file"
#define FILES_SENDING    "(sending)"
#define FILES_REMOTEREQ  "Remote request for /%s%s%s (sending)"
#define FILES_BROKEN     "\nThe file system seems to be broken right now.\n"
#define FILES_INVPATH    "(The dcc-path is set to an invalid directory.)\n"
#define FILES_CURDIR     "Current directory"
#define FILES_NEWCURDIR  "New current directory"
#define FILES_NOSUCHDIR  "No such directory.\n"
#define FILES_ILLDIR     "Illegal directory.\n"
#define FILES_BADNICK    "Be reasonable.\n"
#define FILES_NOTAVAIL   "%s isn't available right now.\n"
#define FILES_REQUESTED  "Requested %s from %s ...\n"
#define FILES_NORMAL     "%s is already a normal file.\n"
#define FILES_CHGLINK    "Changed link to %s\n"
#define FILES_NOTOWNER   "You didn't upload %s\n"
#define FILES_CREADIR    "Created directory"
#define FILES_REQACCESS  "Requires +%s to access\n"
#define FILES_CHGACCESS  "Changed %s/ to require +%s to access\n"
#define FILES_CHGNACCESS "Changes %s/ to require no flags to access\n"
#define FILES_REMDIR     "Removed directory"
#define FILES_ILLSOURCE  "Illegal source directory.\n"
#define FILES_ILLDEST    "Illegal destination directory.\n"
#define FILES_STUPID     "You can't %s files on top of themselves.\n"
#define FILES_EXISTDIR   "exists as a directory -- skipping"
#define FILES_SKIPSTUPID "onto itself?  Nuh uh."
#define FILES_DEST       "Destination"
#define FILES_COPY       "copy"
#define FILES_COPIED     "Copied"
#define FILES_MOVE       "move"
#define FILES_MOVED      "Moved"
#define FILES_CANTWRITE  "Could not write"
#define FILES_REQUIRES   "requires"
#define FILES_HID        "Hid"
#define FILES_UNHID      "Unhid"
#define FILES_SHARED     "Shared"
#define FILES_UNSHARED   "Unshared"
#define FILES_ADDLINK    "Added link"
#define FILES_CHANGED    "Changed"
#define FILES_BLANKED    "Blanked"
#define FILES_ERASED     "Erased"

#endif
