# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../..;make

clean:
	@rm -f *.o *.so *~
	
SOS=assoc.so blowfish.so filesys.so
FILESYS=dccfiles.o filedb.o fileq.o filesys.o files.o tclfiles.o
modules: ${SOS}
	@echo "Successful compiled modules"
	@echo ""

assoc.so: assoc.o
	${LD} -o assoc.so assoc.o
	${STRIP} assoc.so
       
blowfish.so: blowfish.o
	${LD} -o blowfish.so blowfish.o
	${STRIP} blowfish.so
       
filesys.so: ${FILESYS}
	${LD} -o filesys.so ${FILESYS}
	${STRIP} filesys.so
	
assoc.o: assoc.c
	${CC} ${CFLAGS} -DMAKING_MODS assoc.c

blowfish.o: blowfish.c
	${CC} ${CFLAGS} -DMAKING_MODS blowfish.c

dccfiles.o: dccfiles.c
	${CC} ${CFLAGS} -DMAKING_MODS dccfiles.c
	
filedb.o: filedb.c
	${CC} ${CFLAGS} -DMAKING_MODS filedb.c
	
fileq.o: fileq.c
	${CC} ${CFLAGS} -DMAKING_MODS fileq.c
	
files.o: files.c
	${CC} ${CFLAGS} -DMAKING_MODS files.c
	
filesys.o: filesys.c
	${CC} ${CFLAGS} -DMAKING_MODS filesys.c
	
tclfiles.o: tclfiles.c
	${CC} ${CFLAGS} -DMAKING_MODS tclfiles.c
	
assoc.o: \
   module.h \
   ../tandem.h \
   ../cmdt.h \
   ../tclegg.h
blowfish.o: \
   module.h \
   blowfish.h \
   bf_tab.h
dccfiles.o: \
   module.h \
   filesys.h \
   ../tclegg.h
filedb.o: \
   module.h \
   ../files.h \
   filesys.h
fileq.o: \
   module.h \
   filesys.h
files.o: \
   module.h \
   ../files.h \
   filesys.h \
   ../cmdt.h
filesys.o: \
   module.h \
   filesys.h \
   ../tandem.h \
   ../files.h \
   ../users.h \
   ../cmdt.h
tclfiles.o: \
   module.h \
   ../files.h \
   filesys.h \
   ../users.h
