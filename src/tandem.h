/*
   This file is part of the eggdrop source code
   copyright (c) 1997 Robey Pointer
   and is distributed according to the GNU general public license.
   For full details, read the top of 'main.c' or the file called
   COPYING that was distributed with this code.
*/

#ifndef _H_TANDEM
#define _H_TANDEM

/* keep track of tandem-bots in the chain */
typedef struct {
  char bot[10];
  char via[10];
  char next[10];
} tand_t;

/* keep track of channel associations */
typedef struct travis {
  char name[21];
  unsigned int channel;
  struct travis *next;
} assoc_t;

/* keep track of party-line members */
typedef struct {
  char nick[10];
  char bot[10];
  int sock;
  int chan;
  char from[41];
  char flag;
  char status;
  time_t timer;      /* track idle time */
  char away[61];
} party_t;

/* status: */
#define PLSTAT_AWAY   0x01

#endif
