# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.o *.so *~

modules: ../../../filesys.so

../../../transfer.so:
	cd ../transfer.mod;make modules
	
FILESYS=filedb.o filesys.o files.o tclfiles.o
       
../../../filesys.so: ${FILESYS} ../../../transfer.so
	@mkdir modules
	@ln -s ../../../../transfer.so modules/
	${LD} -o ../../../filesys.so ${FILESYS} modules/transfer.so
	${STRIP} ../../../filesys.so
	@rm -rf modules

static: ${FILESYS}
	@for x in ${FILESYS}; do rm -f ../$$x;ln -s filesys.mod/$$x ..; done 

filedb.o: filedb.c
	${CC} ${CFLAGS} -DMAKING_MODS filedb.c
	
files.o: files.c
	${CC} ${CFLAGS} -DMAKING_MODS files.c
	
filesys.o: filesys.c
	${CC} ${CFLAGS} -DMAKING_MODS filesys.c
	
tclfiles.o: tclfiles.c
	${CC} ${CFLAGS} -DMAKING_MODS tclfiles.c
	
filedb.o: \
   ../module.h \
   files.h \
   filesys.h
files.o: \
   files.h \
   filesys.h
filesys.o: \
   ../module.h \
   filesys.h \
   ../../tandem.h \
   files.h \
   ../../users.h \
   ../../cmdt.h
filesys.h: \
   ../../lang/english.h \
   ../transfer.mod/transfer.h
	@touch filesys.h
tclfiles.o: \
   ../module.h \
   files.h \
   filesys.h \
   ../../users.h
