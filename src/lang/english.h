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
#define FILES_LSHEAD1    \
"Filename                        Size  Sent by/Date         # Gets\n"
#define FILES_LSHEAD2    \
"------------------------------  ----  -------------------  ------\n"
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

/* Userfile messages */
#define USERF_XFERDONE	"Userlist transfer complete; switched over"
#define USERF_BADREREAD	"Cannot open userfile to reread channel data"
#define USERF_CANTREAD	"CAN'T READ NEW USERFILE"
#define USERF_CANTSEND	"Can't send userfile to you (internal error)"
#define USERF_NOMATCH	"Can't find anyone matching that"
#define USERF_OLDFMT	"Old userfile (unencrypted); Converting..."
#define USERF_INVALID	"Invalid userfile format."
#define USERF_CORRUPT	"Corrupt user record"
#define USERF_DUPE	"Duplicate user record"
#define USERF_BROKEPASS	"Corrupted password reset for"
#define USERF_IGNBANS	"Ignored bans for channel(s):"
#define USERF_WRITING	"Writing user file..."
#define USERF_ERRWRITE	"ERROR writing user file."
#define USERF_ERRWRITE2	"ERROR writing user file to transfer."
#define USERF_NONEEDNEW	"Userfile creation not necessary--skipping"
#define USERF_REHASHING	"Rehashing..."
#define USERF_UNKNOWN	"I don't know anyone by that name.\n"
#define USERF_NOUSERREC	"No user record."
#define USERF_BACKUP	"Backing up user file..."
#define USERF_FAILEDXFER	"Failed connection; aborted userfile transfer."

/* Misc messages */
#define MISC_EXPIRED	"expired"
#define MISC_TOTAL	"total" /* e.g. "(%d total messages)" */
/* the following used for: "Erased #%d; %d left."  For erasing notes.*/
/* clean this up later for other language semantics and flexibility */
#define MISC_ERASED	"Erased" 
#define MISC_LEFT	"left"
#define MISC_ONLOCALE	"on"  /* e.g. "No longer banning <nick> ON <channel>"*/
#define MISC_MATCHING	"Matching" /* e.g. "Matching <hostmask>..." */
#define MISC_SKIPPING 	"skipping first" /*e.g. "Skipping first <n matches>" */
#define MISC_TRUNCATED	"(more than %d matches; list truncated)\n"
#define MISC_FOUNDMATCH	"--- Found %d match%s.\n"
#define MISC_AMBIGUOUS	"Ambiguous command.\n"
#define MISC_NOSUCHCMD	"What?  You need '.help'\n"
#define MISC_CMDBINDS	"Command bindings:\n"
#define MISC_RESTARTING	"Restarting..."
#define MISC_NOMODULES	"Module load attempted on non-module eggdrop."
#define MISC_LOGSWITCH	"Switching logfiles..."

/* Text for ban messages */
#define IRC_BANNED	"banned"	
#define IRC_YOUREBANNED	"You are banned"

/* BOT log messages when attempting to place a ban which matches me */
#define IRC_IBANNEDME	"Wanted to ban myself--deflected."
#define IRC_FUNKICK	"that was fun, let's do it again!"
#define IRC_HI		"Hi"
#define IRC_GOODBYE	"Goodbye"
#define IRC_BANNED2	"You're banned, goober."
#define IRC_NICKTOOLONG "Your nick is too long to add right now."
#define IRC_INTRODUCED	"Introduced to" /* "Introduced to %s" format */
#define IRC_COMMONSITE	"common site"
/* dont forget the \n's */
#define IRC_SALUT1	"NOTICE %s :Hi %s!  I'm %s, an eggdrop bot.\n"
#define IRC_SALUT1_ARGS	nick, nick, botname
#define IRC_SALUT2	\
	"NOTICE %s :I'll recognize you by hostmask '%s' from now on.\n"
#define IRC_SALUT2_ARGS	nick, host
#define IRC_SALUT2A	\
	"Since you come from a common irc site, this means you should"
#define IRC_SALUT2B	"always use this nickname when talking to me."
#define IRC_INITOWNER1	"YOU ARE THE OWNER ON THIS BOT NOW"
#define IRC_INIT1	"Bot installation complete, first master is %s"
#define IRC_INIT1_ARGS	nick
#define IRC_INITNOTE	"Welcome to Eggdrop! =]"
#define IRC_INITINTRO	"introduced to %s from %s"
#define IRC_PASS	"You have a password set."
#define IRC_NOPASS	"You don't have a password set."
#define IRC_NOPASS2	"(YOU HAVE NO PASSWORD SET )\n"
#define IRC_EXISTPASS	"You already have a password set."
#define IRC_PASSFORMAT	"Please use at least 4 characters."
#define IRC_SETPASS	"Password set to:"
#define IRC_FAILPASS	"Incorrect password."
#define IRC_CHANGEPASS	"Password changed to:"
#define IRC_FAILCOMMON	"You're at a common site; you can't IDENT."
#define IRC_MISIDENT	"NOTICE %s :You're not %s, you're %s.\n"
#define IRC_MISIDENT_ARGS	nick, nick, hand
#define IRC_MISIDENT_ARGS2	nick, who, hand
#define IRC_DENYACCESS	"Access denied."
#define IRC_RECOGNIZED	"I recognize you there."
#define IRC_ADDHOSTMASK	"Added hostmask"
#define IRC_DELMAILADDR	"Removed your email address."
#define IRC_FIELDCURRENT	"Currently:"
#define IRC_FIELDCHANGED	"Now:"
#define IRC_FIELDTOREMOVE	"To remove it:"
#define IRC_NOEMAIL	"You have no email address set."
#define IRC_INFOLOCKED	"Your info line is locked"
#define IRC_REMINFOON	"Removed your info line on" /* channel */
#define IRC_REMINFO	"Removed your info line."
#define IRC_NOINFOON	"You have no info set on"
#define IRC_NOINFO	"You have no info set."
#define IRC_NOMONITOR	"I don't monitor that channel."
#define IRC_RESETCHAN	"Resetting channel info."
#define IRC_JUMP	"Jumping servers..."
#define IRC_CHANHIDDEN	"Channel is currently hidden."
#define IRC_ONCHANNOW	"Now on channel"
#define IRC_NEVERJOINED	"Never joined one of my channels."
#define IRC_LASTSEENAT	"Last seen at"
#define IRC_DONTKNOWYOU	"I don't know you; please introduce yourself first."
#define IRC_NOHELP	"No help."
#define IRC_NOHELP2	"No help available on that."
#define IRC_NOTNORMFILE	"is not a normal file!"
#define IRC_NOTONCHAN	"Not on that channel right now."
#define IRC_GETORIGNICK	"Switching back to nick %s"
#define IRC_BADBOTNICK	"Server says my nickname is invalid."
#define IRC_BOTNICKINUSE	"NICK IN USE: Trying '%s'"
#define IRC_CANTCHANGENICK	\
		"Can't change nickname on %s.  Is my nickname banned?"
#define IRC_BOTNICKJUPED	"Nickname has been juped."
#define IRC_CHANNELJUPED	"Channel %s is juped. :("
#define IRC_NOTREGISTERED1	"%s says I'm not registered, trying next one."
#define	IRC_NOTREGISTERED2	"You have a fucked up server."
#define IRC_FLOODIGNORE1	"Flood from @%s!  Placing on ignore!"
#define IRC_FLOODIGNORE2	"CTCP flood from @%s!  Placing on ignore!"
#define IRC_FLOODIGNORE3	"JOIN flood from @%s!  Banning."
#define IRC_FLOODKICK		"Channel flood from %s -- kicking"
#define IRC_SERVERTRY		"Trying server"
#define IRC_DNSFAILED		"DNS lookup failed"
#define IRC_FAILEDCONNECT	"Failed connect to"
#define IRC_SERVERSTONED	"Server got stoned; jumping..."
#define IRC_DISCONNECTED	"Disconnected from"

/* Eggdrop command line usage */

#define EGG_USAGE	"\
Command line arguments:\n\
  -h   help\n\
  -v   print version and exit\n\
  -n   don't go into the background\n\
  -c   (with -n) display channel stats every 10 seconds\n\
  -t   (with -n) use terminal to simulate dcc-chat\n\
  -m   userfile creation mode\n\
  optional config filename (default 'egg.config')\n\
"

#define EGG_RUNNING1	"I detect %s already running from this directory.\n"
#define EGG_RUNNING2	"If this is incorrect, erase the '%s'"

#define USER_ISGLOBALOP	"  (is a global op)"
#define USER_ISBOT	"  (is a bot)"
#define USER_ISMASTER	"  (is a master)"

/* Messages used when listing with `.bans' */
#define BANS_CREATED	"Created"
#define BANS_LASTUSED	"last used"
#define BANS_INACTIVE	"inactive"
#define BANS_PLACEDBY	"placed by"
#define BANS_GLOBAL	"Global bans"
#define BANS_NOTACTIVE	"not active on"
#define BANS_BYCHANNEL	"Channel bans for"
#define BANS_NOTACTIVE2	"not active"
#define BANS_NOTBYBOT	"not placed by bot"
#define BANS_USEBANSALL	"Use '.bans all' to see the total list"
#define BANS_NOLONGER	"No longer banning"

/* Messages referring to channels */
#define CHAN_NOSUCH	"No such channel defined"
#define CHAN_BADCHANKEY	"Bogus channel key"
#define CHAN_BADCHANMODE	"* Mode change on %s for nonexistant %s!"
#define CHAN_BADCHANMODE_ARGS	chan->name, who
#define CHAN_BADCHANMODE_ARGS2	chan->name, op
#define CHAN_MASSDEOP	"Mass deop on %s by %s"
#define CHAN_MASSDEOP_ARGS	chan->name, s1
#define CHAN_MASSDEOP_KICK	"Mass deop.  Go sit in a corner."
#define CHAN_BADBAN	"Bogus ban"
#define CHAN_PERMBANNED	"%s is in my permaban list.  You need to\n\
use '-ban' in dcc chat if you want it gone for good."
#define CHAN_FORCEJOIN	"Oops.   Someone made me join %s... leaving..."
#define CHAN_FAKEMODE	"Mode change by fake op on %s!  Reversing..."
#define CHAN_FAKEMODE_KICK	"Abusing ill-gained server ops"
#define CHAN_DESYNCMODE	"Mode change by non-chanop on %s!  Reversing..."
#define CHAN_DESYNCMODE_KICK	"Abusing desync"

/* Messages referring to ignores */
#define IGN_NONE	"No ignores"
#define IGN_CURRENT	"Currently ignoring"
#define IGN_NOLONGER	"No longer ignoring"

/* Messages referring to bots */
#define BOT_NOTHERE	"That bot isn't here.\n"
#define BOT_NONOTES	"That's a bot.  You can't leave notes for a bot.\n"
#define BOT_USERAWAY	"is away"
#define BOT_NOTEUNSUPP	"Notes are not supported by this bot.\n"
#define BOT_NOTES2MANY	"Sorry, that user has too many notes already.\n"
#define BOT_NOTESERROR1	"Can't create notefile.  Sorry.\n"
#define BOT_NOTESERROR2	"Notefile unreachable!"
#define BOT_NOTEARRIVED	"Note arrived for you"
#define BOT_NOTESTORED	"Stored message"
#define BOT_NOTEDELIV	"Note delivered."
#define BOT_NOTEOUTSIDE	"Outside note"
#define BOT_NOTESUSAGE	"NOTES function must be one of:"
#define BOT_NOMESSAGES	"You have no messages"
#define BOT_NOTEEXP1	" -- EXPIRES TODAY"
#define BOT_NOTEEXP2	" -- EXPIRES IN %d DAY%s"
#define BOT_NOTEWAIT	"You have the following notes waiting"
#define BOT_NOTTHATMANY	"You don't have that many messages"
#define BOT_NOTEUSAGE	"Use '.notes read' to read them."
#define BOT_CANTMODNOTE	"Can't modify the note file"
#define BOT_NOTESERASED	"Erased all notes"
#define BOT_NOTESWAIT1	"NOTICE %s :You have %d note%s waiting on %s.\n"
#define BOT_NOTESWAIT1_ARGS	m->nick, k, k == 1 ? "" : "s", botname
#define BOT_NOTESWAIT2	"For a list:"
#define BOT_NOTESWAIT3	"### You have %d note%s waiting.\n"
#define BOT_NOTESWAIT3_ARGS k, k == 1 ? "" : "s"
#define BOT_NOTESWAIT4	"### Use '.notes read' to read them.\n"


#define BOT_MSGDIE	\
	"Daisy, Daisssyyy, give meee yourr ansssweerrrrrr dooooooooo...."

/* Messages pertaining to MODULES */
#define MOD_ALREADYLOAD	"Already loaded."
#define MOD_BADCWD	"Can't determine current directory."
#define MOD_NOSTARTDEF	"No start function defined."
#define MOD_LOADED	"Module loaded:"
#define MOD_NEEDED	"Needed by another module"
#define MOD_NOCLOSEDEF	"No close function"
#define MOD_UNLOADED	"Module unloaded: "
#define MOD_NOSUCH	"No such module"
#define MOD_NOINFO	"No infor for module:"
#define MOD_LOADERROR	"Error loading module:"
#define MOD_UNLOADERROR	"Error unloading module:"
#define MOD_CANTLOADMOD	"Can't load modules"
#define MOD_STAGNANT	"Stagnant module; there WILL be memory leaks!"
#define MOD_NOCRYPT	"\
You have installed modules but have not selected an encryption\n\
module, please consult the default config file for info.\n\
"

#define DCC_NOSTRANGERS	"I don't accept DCC chats from strangers."
#define DCC_REFUSED	"Refused DCC chat (noaccess)"
#define DCC_REFUSED2	"No access"
#define DCC_REFUSED3	"You must have a password set."
#define DCC_REFUSED4	"Refused DCC chat (no password)"
#define DCC_REFUSED5	"Refused DCC chat (+x but no file area)"
#define DCC_REFUSED6	"Refused DCC chat (file area full)"
#define DCC_TOOMANY	"Too many people are in the file area right now."
#define DCC_TRYLATER	"Please try again later."
#define DCC_REFUSEDTAND	"Refused tandem connection from %s (duplicate)"
#define DCC_NOSTRANGERFILES1	"I don't accept files from strangers."
#define DCC_NOSTRANGERFILES2	"Refused DCC SEND %s (noaccess): %s!%s"
#define DCC_TOOMANYDCCS1	"Sorry, too many DCC connections."
#define DCC_TOOMANYDCCS2	"DCC connections full: %s %s (%s!%s)"
#define DCC_DCCNOTSUPPORTED	"DCC file transfers not supported."
#define DCC_REFUSEDNODCC	"Refused DCC send %s from %s!%s"
#define DCC_REFUSEDNODCC_ARGS	param, nick, from
#define DCC_FILENAMEBADSLASH	"Filename cannot have '/' in it..."
#define DCC_MISSINGFILESIZE	"Sorry, file size info must be included."
#define DCC_FILEEXISTS		"That file already exists."
#define DCC_CREATEERROR		"Can't create that file (temp dir error)"
#define DCC_FILEBEINGSENT	"That file is already being sent."
#define DCC_REFUSEDNODCC2	"no file size"
#define DCC_REFUSEDNODCC3	"Refused DCC send %s from %s: no file size"
#define DCC_FILETOOLARGE	"Sorry, file too large."
#define DCC_FILETOOLARGE2	"Refused dcc send %s (%s): file too large"
#define DCC_CONNECTFAILED1	"Failed to connect"
#define DCC_CONNECTFAILED2	"DCC connection failed"
#define DCC_FILESYSBROKEN	"File system broken."
#define DCC_ENTERPASS		"Enter your password."
#define DCC_FLOODBOOT		"%s has been forcibly removed for flooding.\n"
#define DCC_BOOTED1	"-=- poof -=-\n"
#define DCC_BOOTED2	"You've been booted from the %s by %s%s%s\n"
#define DCC_BOOTED2_ARGS		files ? "file section" : "bot",\
				by, reason[0] ? ": " : ".", reason
#define DCC_BOOTED3	"%s booted %s from the party line%s%s\n"
#define DCC_BOOTED3_ARGS	by, dcc[idx].nick, \
				reason[0] ? ": " : ".", reason

/* CLIENTINFO default messages */
/* SED and UTC are a big lie, but they'll never know */
#define CLIENTINFO \
	"SED VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME ACTION DCC UTC PING ECHO  :Use CLIENTINFO <COMMAND> to get more specific information"
#define CLIENTINFO_SED "SED contains simple_encrypted_data"
#define CLIENTINFO_VERSION "VERSION shows client type, version and environment"
#define CLIENTINFO_CLIENTINFO \
	"CLIENTINFO gives information about available CTCP commands" 
#define CLIENTINFO_USERINFO "USERINFO returns user settable information"
#define CLIENTINFO_ERRMSG "ERRMSG returns error messages"
#define CLIENTINFO_FINGER \
	"FINGER shows real name, login name and idle time of user"
#define CLIENTINFO_TIME "TIME tells you the time on the user's host"
#define CLIENTINFO_ACTION "ACTION contains action descriptions for atmosphere"
#define CLIENTINFO_DCC "DCC requests a direct_client_connection"
#define CLIENTINFO_UTC "UTC substitutes the local timezone"
#define CLIENTINFO_PING "PING returns the arguments it receives"
#define CLIENTINFO_ECHO "ECHO returns the arguments it receives"

#endif

