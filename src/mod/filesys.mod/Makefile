# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.o *.so *~

modules: filesys.so

FILESYS=dccfiles.o filedb.o filesys.o files.o tclfiles.o
       
filesys.so: ${FILESYS}
	${LD} -o filesys.so ${FILESYS}
	${STRIP} filesys.so
	@mv filesys.so ..
	
dccfiles.o: dccfiles.c
	${CC} ${CFLAGS} -DMAKING_MODS dccfiles.c
	
filedb.o: filedb.c
	${CC} ${CFLAGS} -DMAKING_MODS filedb.c
	
files.o: files.c
	${CC} ${CFLAGS} -DMAKING_MODS files.c
	
filesys.o: filesys.c
	${CC} ${CFLAGS} -DMAKING_MODS filesys.c
	
tclfiles.o: tclfiles.c
	${CC} ${CFLAGS} -DMAKING_MODS tclfiles.c
	
dccfiles.o: \
   filesys.h
filedb.o: \
   filesys.h
files.o: \
   filesys.h
filesys.o: \
   filesys.h
tclfiles.o: \
   filesys.h
