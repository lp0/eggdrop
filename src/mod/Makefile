# Source-make

GMAKE=${MAKE} 'LD=${LD}' 'STRIP=${STRIP}' 'CC=${CC}' 'CFLAGS=${CFLAGS}'

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../..;make
	
clean:
	@for x in *.mod; do cd $$x;make clean;cd ..;done
	@rm -f *.so *~
	
modules:
	@for x in *.mod; do cd $$x;${GMAKE} modules;cd ..; done
	@echo "Successful compiled modules"
	@echo ""
