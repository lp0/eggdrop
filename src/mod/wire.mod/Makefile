# Source-make
FILENAME=wire
include ../Makefile.generic

install:
	@echo Installing wire language file.
	@rm -f $(REALDEST)/language/wire.english.lang
	@cp wire.english.lang $(REALDEST)/language

wire.o: wire.h ../module.h ../../users.h ../../chan.h 
