# Source-make

GMAKE = ${MAKE} 'CC=${CC}' 'CFLAGS=${CFLAGS}' 

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

../eggdrop:  ${OBJS} ${XREQ} eggdrop.h
	@echo ""
	@echo "Linking eggdrop..."
	@echo ""
	${LD} -o ../eggdrop ${OBJS} ${XLIBS}
	${STRIP} ../eggdrop
	@echo "Successful compile: eggdrop"
	@echo ""
	
eggdrop: ../eggdrop

static: ${OBJS} ${XREQ}
	@echo ""
	@echo "Linking eggdrop...(statically)"
	@echo ""
	${LD} -o ../eggdrop ${OBJS} ${XLIBS} mod/*.o
	${STRIP} ../eggdrop
	@echo "Successful compile: eggdrop"
	@echo ""

debug: ${OBJS} ${XREQ}
	@echo ""
	@echo "Linking eggdrop (debug version)..."
	@echo ""
	${LD} -g -o ../eggdrop ${OBJS} ${XLIBS}
	@echo "Successful compile: eggdrop (debug version)"
	@echo ""

blowfish.o: blowfish.c
	${CC} ${CFLAGS} blowfish.c
	
botcmd.o: botcmd.c
	@echo "(This will take a while.  Pour yourself an ice cold coke and chill.)"
	${CC} ${CFLAGS} botcmd.c
	
botnet.o: botnet.c
	${CC} ${CFLAGS} botnet.c
	
chan.o: chan.c
	${CC} ${CFLAGS} chan.c
	
chanprog.o: chanprog.c
	${CC} ${CFLAGS} chanprog.c
	
chanset.o: chanset.c
	${CC} ${CFLAGS} chanset.c
	
cmds.o: cmds.c
	${CC} ${CFLAGS} cmds.c

cmdsirc.o: cmdsirc.c
	${CC} ${CFLAGS} cmdsirc.c

dcc.o: dcc.c
	${CC} ${CFLAGS} dcc.c
	
dccutil.o: dccutil.c
	${CC} ${CFLAGS} dccutil.c

gotdcc.o: gotdcc.c
	${CC} ${CFLAGS} gotdcc.c

hash.o: hash.c
	${CC} ${CFLAGS} hash.c
	
main.o: main.c patch.h
	${CC} ${CFLAGS} main.c
	
mem.o: mem.c
	${CC} ${CFLAGS} mem.c
	
misc.o: misc.c
	${CC} ${CFLAGS} misc.c
	@echo "---------- Yeesh! 1/2 way there!!... ----------"

mode.o: mode.c
	${CC} ${CFLAGS} mode.c

modules.o: modules.c
	${CC} ${CFLAGS} modules.c

msgcmds.o: msgcmds.c
	${CC} ${CFLAGS} msgcmds.c
	
msgnotice.o: msgnotice.c
	${CC} ${CFLAGS} msgnotice.c

net.o: net.c
	${CC} ${CFLAGS} net.c
	
notes.o: notes.c
	${CC} ${CFLAGS} notes.c

tcl.o: tcl.c
	${CC} ${CFLAGS} tcl.c
	
tclchan.o: tclchan.c
	${CC} ${CFLAGS} tclchan.c

tcldcc.o: tcldcc.c
	${CC} ${CFLAGS} tcldcc.c

tclhash.o: tclhash.c
	${CC} ${CFLAGS} tclhash.c
	
tclmisc.o: tclmisc.c
	${CC} ${CFLAGS} tclmisc.c

tcluser.o: tcluser.c
	${CC} ${CFLAGS} tcluser.c

userrec.o: userrec.c
	${CC} ${CFLAGS} userrec.c

users.o: users.c
	${CC} ${CFLAGS} users.c

# I orginially had this in another file, but some lame make's can't cope :/ 
botcmd.o: \
   main.h \
   tandem.h \
   users.h \
   chan.h \
   modules.h
botnet.o: \
   main.h \
   tandem.h
chan.o: \
   main.h \
   users.h \
   chan.h
chanprog.o: \
   main.h \
   users.h \
   chan.h \
   modules.h
chanset.o: \
   main.h \
   chan.h \
   users.h
cmds.o: \
   main.h \
   modules.h \
   chan.h
cmdsirc.o: \
   main.h \
   chan.h \
   users.h \
   modules.h
dcc.o: \
   main.h \
   chan.h \
   modules.h
dccutil.o: \
   main.h \
   chan.h \
   modules.h
gotdcc.o: \
   main.h \
   chan.h \
   modules.h
main.o: \
   main.h \
   chan.h \
   modules.h \
   patch.h
mem.o: \
   mod/modvals.h
misc.o: \
   main.h \
   chan.h
mode.o: \
   main.h \
   users.h \
   chan.h
modules.o: \
   modules.h \
   users.h
msgcmds.o: \
   main.h \
   chan.h
msgnotice.o: \
   main.h \
   chan.h
net.o: \
   main.h
notes.o: \
   main.h
tcl.o: \
   main.h
tclchan.o: \
   eggdrop.h \
   tclegg.h \
   users.h \
   chan.h \
   cmdt.h
tcldcc.o: \
   main.h \
   tandem.h
tclhash.o: \
   main.h \
   chan.h \
   users.h \
   match.c
tclmisc.o: \
   main.h \
   modules.h
tcluser.o: \
   main.h \
   users.h \
   chan.h
userrec.o: \
   main.h \
   users.h \
   chan.h \
   modules.h
users.o: \
   main.h \
   users.h \
   chan.h \
   modules.h
#safety hash
