/* This should make things properly case insensitive with respect to RFC 1459 */
/* or at least get them doing things the same way the IRCd does.              */

#define rfc_tolower(c) (rfc_tolowertab[(unsigned char)(c)])
#define rfc_toupper(c) (rfc_touppertab[(unsigned char)(c)])

extern int rfc_casecmp (char *, char *);
extern int rfc_ncasecmp (char *, char *, int);
extern unsigned char rfc_tolowertab[];
extern unsigned char rfc_touppertab[];

