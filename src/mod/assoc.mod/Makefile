# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.o *.so *~

modules: assoc.so

assoc.so: assoc.o
	${LD} -o assoc.so assoc.o
	${STRIP} assoc.so
	@mv assoc.so ..
       
assoc.o: assoc.c
	${CC} ${CFLAGS} -DMAKING_MODS assoc.c

