# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.so *.o *~
	
modules: blowfish.so
       
blowfish.so: blowfish.o
	${LD} -o blowfish.so blowfish.o
	${STRIP} blowfish.so
	@mv blowfish.so ..
	
blowfish.o: blowfish.c
	${CC} ${CFLAGS} -DMAKING_MODS blowfish.c
	
blowfish.o: \
   ../module.h \
   blowfish.h \
   bf_tab.h
