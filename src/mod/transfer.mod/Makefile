# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.o *.so *~
	
modules: transfer.so
       
transfer.so: transfer.o
	${LD} -o transfer.so transfer.o
	${STRIP} transfer.so
	@mv transfer.so ..
		
transfer.o: transfer.c
	${CC} ${CFLAGS} -DMAKING_MODS transfer.c
