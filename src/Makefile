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

eggdrop: making ${OBJS} ${XREQ}
	${LD} -o eggdrop ${OBJS} ${XLIBS}
	${STRIP} eggdrop
	@echo "Successful compile: eggdrop"
	@echo ""

debug: making ${OBJS} ${XREQ}
	${LD} -g -o eggdrop ${OBJS} ${XLIBS}
	@echo "Successful compile: eggdrop (debug version)"
	@echo ""

making: eggdrop.h
	@echo ""
	@echo "Making eggdrop..."
	@echo "(This will take a while.  Pour yourself an ice cold coke and chill.)"
	@echo ""

blowfish.o: blowfish.c
	${CC} ${CFLAGS} blowfish.c
	
botcmd.o: botcmd.c
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
	
dcc.o: dcc.c
	${CC} ${CFLAGS} dcc.c
	
dccutil.o: dccutil.c
	${CC} ${CFLAGS} dccutil.c

gotdcc.o: gotdcc.c
	${CC} ${CFLAGS} gotdcc.c

hash.o: hash.c
	${CC} ${CFLAGS} hash.c
	
main.o: main.c
	${CC} ${CFLAGS} main.c
	
match.o: match.c
	${CC} ${CFLAGS} match.c

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

blowfish.o: \
   blowfish.h \
   bf_tab.h \
   eggdrop.h \
   proto.h
botcmd.o: \
   eggdrop.h \
   tandem.h \
   users.h \
   chan.h \
   tclegg.h
botnet.o: \
   eggdrop.h \
   tandem.h \
   proto.h \
   mod/assoc.c
chan.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h
chanprog.o: \
   eggdrop.h \
   users.h \
   chan.h \
   tclegg.h \
   modules.h
chanset.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h
cmds.o: \
   eggdrop.h \
   users.h \
   chan.h \
   tclegg.h \
   mod/filesys.c
dcc.o: \
   eggdrop.h \
   chan.h \
   proto.h \
   modules.h \
   mod/dccfiles.c
dccutil.o: \
   eggdrop.h \
   chan.h \
   proto.h \
   modules.h
gotdcc.o: \
   eggdrop.h \
   chan.h \
   proto.h \
   modules.h
hash.o: \
   eggdrop.h \
   cmdt.h \
   hash.h \
   tclegg.h
main.o: \
   eggdrop.h \
   chan.h \
   tclegg.h \
   modules.h \
   ../lush.h \
   files.h \
   mod/filedb.c \
   mod/files.c \
   mod/fileq.c
misc.o: \
   eggdrop.h \
   proto.h
mode.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h
modules.o: \
   modules.h
msgcmds.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h
msgnotice.o: \
   eggdrop.h \
   chan.h \
   proto.h
net.o: \
   eggdrop.h \
   proto.h
notes.o: \
   eggdrop.h \
   proto.h
tcl.o: \
   eggdrop.h \
   tclegg.h \
   cmdt.h
tclchan.o: \
   eggdrop.h \
   tclegg.h \
   users.h \
   chan.h \
   cmdt.h
tcldcc.o: \
   eggdrop.h \
   tclegg.h \
   tandem.h \
   cmdt.h
tclhash.o: \
   eggdrop.h \
   cmdt.h \
   tclegg.h
tclmisc.o: \
   eggdrop.h \
   cmdt.h \
   tclegg.h \
   files.h \
   mod/tclfiles.c
tcluser.o: \
   eggdrop.h \
   users.h \
   cmdt.h \
   tclegg.h \
   chan.h
userrec.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h
users.o: \
   eggdrop.h \
   users.h \
   chan.h \
   proto.h \
   modules.h

#safety hash
