This is the 1.2.x branch of the Eggdrop Bot.

*** NOTE:

    This bot is NOT intended for users of very limited MEMORY or DISK
    shell accounts.  The development of this latest design of Eggdrop
    is geared towards features and flexibility.  If you have restraints
    on the resources allowed to run your bot, then you should instead
    consider the 1.0 Eggdrop version.

***

This latest development of Eggdrop is now compilable in either of two
configurations:

MODULE - dynamic

   The MODULE version of Eggdrop 1.2.x provides for the on-the-fly
   loading of extensions to the bot code without having to recompile
   the entire bot.  This is available on OS platforms which support
   dynamically linked/shared libraries in their run-time and TCL
   libraries.  It allows for functionality of the bot to be loaded
   and unloaded at any time.

   The base loadable modules distributed with this Eggdrop version
   are:
	filesys		This module performs the file-system
			operation for DCC file transfers to and from
			the BOT over via IRC DCC commands.
	transfer	This module performs the bot-to-bot userfile
			sharing necessary for sharebots.  It is
			loaded automatically when filesys is loaded.
	blowfish	The standard Eggdrop encrypting routines
			for passwords and other encrytions.  This
			can be replaced with a user-written module
			to perform their own encrypting algorithms.
	assoc		This is the functionality of the `assoc'
			command for naming party `chat' lines.  It 
			also serves as an example for writing your
			own modules.

MODULE - static

   The STATIC-MODULE version of Eggdrop 1.2.x is pretty much the same
   as the pre 1.2.x Eggdrop.  This is, all the functionality exists
   in the bot, unless specifically compiled out using DEFINEs in
   the C source configuration file.  You must recompile the bot sources
   to make changes to the bot's functionality, whereas you only need to 
   load or unload a module in the MODULE version.

   Each individual module is linked into the executable when you compile.
   To prevent a module from being compile in, remove it from src/mod/
   (or rename it to anything but modulename.mod)
   
More functionality is already planned to be moved from being hard-coded, 
in to the MODULE version, and placed in loadable modules.  Additionally,
modules for added functionality may be found that can be added to the
MODULE version of eggdrop.

See the file doc/MODULES for more specific MODULE information.
