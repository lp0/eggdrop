# Source-make

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ..;make

libtcle.a: ${TCLLIB}/lib${TCLLIBFN}
	@echo "[ Fixing lib${TCLLIBFN} -> libtcle.a ]"
	cp ${TCLLIB}/lib${TCLLIBFN} ./libtcle.a
	chmod u+rw libtcle.a
	ar d ./libtcle.a tclMain.o
	${RANLIB} ./libtcle.a

eggdrop: making ${OBJS} ${XREQ}
	${CC} ${STRIP} -o eggdrop ${OBJS} ${XLIBS}
	@echo "Successful compile: eggdrop"
	@echo ""

debug: making ${OBJS} ${XREQ}
	${CC} -g -o eggdrop ${OBJS} ${XLIBS}
	@echo "Successful compile: eggdrop (debug version)"
	@echo ""

making: eggdrop.h
	@echo ""
	@echo "Making eggdrop..."
	@echo "(This will take a while.  Go have a Mentos and relax.)"
	@echo ""


blowfish.o: blowfish.c eggdrop.h proto.h
	${CC} ${CFLAGS} blowfish.c

botcmd.o: eggdrop.h tandem.h users.h chan.h proto.h botcmd.c
	${CC} ${CFLAGS} botcmd.c

botnet.o: eggdrop.h tandem.h proto.h botnet.c
	${CC} ${CFLAGS} botnet.c

chan.o: eggdrop.h users.h chan.h proto.h chan.c
	${CC} ${CFLAGS} chan.c

chanprog.o: eggdrop.h users.h chan.h proto.h chanprog.c
	${CC} ${CFLAGS} chanprog.c

chanset.o: eggdrop.h users.h chan.h proto.h chanset.c
	${CC} ${CFLAGS} chanset.c

cmds.o: eggdrop.h users.h chan.h proto.h cmds.c
	${CC} ${CFLAGS} cmds.c

dcc.o: eggdrop.h chan.h proto.h dcc.c
	${CC} ${CFLAGS} dcc.c

dccutil.o: eggdrop.h chan.h proto.h dccutil.c
	${CC} ${CFLAGS} dccutil.c

filedb.o: eggdrop.h proto.h files.h filedb.c
	${CC} ${CFLAGS} filedb.c

fileq.o: eggdrop.h proto.h ../lush.h fileq.c
	${CC} ${CFLAGS} fileq.c

files.o: eggdrop.h proto.h files.h files.c
	${CC} ${CFLAGS} files.c

gotdcc.o: eggdrop.h chan.h proto.h gotdcc.c
	${CC} ${CFLAGS} gotdcc.c

hash.o: eggdrop.h cmdt.h hash.h proto.h tclegg.h ../lush.h hash.c
	${CC} ${CFLAGS} hash.c

main.o: eggdrop.h chan.h proto.h main.c
	${CC} ${CFLAGS} main.c
	@echo "--- Halfway done! ---"

match.o: match.c
	${CC} ${CFLAGS} match.c

mem.o: mem.c ../Makefile
	${CC} ${CFLAGS} mem.c

misc.o: eggdrop.h proto.h misc.c
	${CC} ${CFLAGS} misc.c

mode.o: eggdrop.h users.h chan.h proto.h mode.c
	${CC} ${CFLAGS} mode.c

msgcmds.o: eggdrop.h users.h chan.h proto.h msgcmds.c
	${CC} ${CFLAGS} msgcmds.c

msgnotice.o: eggdrop.h chan.h proto.h msgnotice.c
	${CC} ${CFLAGS} msgnotice.c

net.o: net.c eggdrop.h proto.h
	${CC} ${CFLAGS} net.c

notes.o: notes.c eggdrop.h proto.h
	${CC} ${CFLAGS} notes.c

tcl.o: eggdrop.h users.h chan.h proto.h cmdt.h tclegg.h ../lush.h tcl.c
	${CC} ${CFLAGS} tcl.c

tclchan.o: eggdrop.h users.h chan.h proto.h cmdt.h tclegg.h tclchan.c
	${CC} ${CFLAGS} tclchan.c

tcldcc.o: eggdrop.h proto.h cmdt.h tclegg.h tandem.h tcldcc.c
	${CC} ${CFLAGS} tcldcc.c

tclhash.o: eggdrop.h proto.h tclegg.h cmdt.h tclhash.c
	${CC} ${CFLAGS} tclhash.c

tclmisc.o: eggdrop.h proto.h cmdt.h tclegg.h tclmisc.c
	${CC} ${CFLAGS} tclmisc.c

tcluser.o: eggdrop.h proto.h cmdt.h tclegg.h tcluser.c
	${CC} ${CFLAGS} tcluser.c

userrec.o: eggdrop.h users.h files.h proto.h userrec.c
	${CC} ${CFLAGS} userrec.c

users.o: eggdrop.h users.h chan.h proto.h users.c
	${CC} ${CFLAGS} users.c
