# Source-make
FILENAME=share
# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../..;make

clean:
	@rm -f *.o *.so *~

modules: ../../../${FILENAME}.so

static: ../share.o

../${FILENAME}.o: ${FILENAME}.c ../module.h ../modvals.h ../../eggdrop.h
	${CC} ${CFLAGS} -DMAKING_MODS -o ../${FILENAME}.o ${FILENAME}.c

../../../${FILENAME}.so: ${FILENAME}.o ../../../transfer.so
	@mkdir modules
	@ln -s ../../../../transfer.so modules
	${LD} -o ../../../${FILENAME}.so ${FILENAME}.o modules/transfer.so
	${STRIP} ../../../${FILENAME}.so
	@rm -rf modules
        
${FILENAME}.o: ${FILENAME}.c ../module.h ../modvals.h ../../eggdrop.h
	${CC} ${CFLAGS} -DMAKING_MODS ${FILENAME}.c

../../../transfer.so:
	@cd ../transfer.mod;make modules
	
	
