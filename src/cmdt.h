/* stuff for builtin commands */
/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/
#ifndef _H_CMDT
#define _H_CMDT

#define CMD_LEAVE    (Function)(-1)
typedef int (*Function)();
typedef struct {
  char *name;
  char flag;
  Function func;
} cmd_t;

#endif
