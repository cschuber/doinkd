# Makefile for doinkd
#

# C compiler flags
CC = cc
RM = rm
INCLUDE = 

######################################################################
#
# Definitions for different unices.
#
######################################################################
#
#  If your system keeps track of processes using procfs
#  (/proc), define HAVE_PROC_FS.  This determines what
#  method is used for checking to see if xlock is running.
#  Note that the /proc directory on Linux is not the correct
#  structure.  The procfs that is necessary is for the entire
#  contents of /proc to consist solely of files, probably with
#  their names being the process id's of the current processes,
#  and otherwise somehow coded.
#
#  If you don't HAVE_PROC_FS, perhaps one of the other search
#  methos for xlock_check will work.  In that case, if you
#  have KVM stuff (ie.  <kvm.h> and lib kvm) define
#  PROC_SEARCH_1.
#
#  If zapping does not work, ie. the user does not get logged
#  out, you can try the 'ps' hack by defining PS_HACK.  The
#  "/bin/ps" line in zap.c may need to be adjusted to get
#  the proper parameters sent to ps for your system.  The
#  current setup should work fine for SunOS 4.x, perhaps others.
#
#  Define UTMPPID if your utmp file contains the process id
#  of the login shell for each entry.
#  BSD does not have it.
#
#  If you do not UTMPPID, then one of two other kill
#  methods will be attempted.  If your system has a good
#  tcgetpgrp() function, then it will be used to get the
#  pid of the shell.  If you don't, the backup method
#  will be tried.  If you know yours doesn't work (ie.
#  it always returns -1 when calling it on a different
#  tty) then you can save a couple of bytes of executable
#  space by defining BAD_PGRP.
#
#  Define UTMPHOST if your utmp file contains a ut_host entry
#  to specify where the user is logged in from.  This is required
#  to enable 'from host' killing.
#
#  If your system has an extended utmp file with utmpx.h and
#  either the ut_pid or ut_host fields are not present in
#  the normal utmp file, then define HAVE_UTMPX to use the
#  extended utmp file.
#
#  If you have setsid(), then define HAVE_SETSID to allow
#  total daemonizing of doinkd.
#
#  If you know your system has yyrestart() for its 'yacc'-ed
#  output, or the 'make' fails due to "duplicate symbol yyrestart"
#  or a similar error, then define HAVE_YYRESTART.
#
#  If you use XDM and want the X-Windows console to behave
#  decently, define XDM_HACK.  If doinkd kills the console
#  user in XDM_HACK mode, it manually kills every tty owned
#  by that user.
#
#  If you wish to mail messages to the people who doinkd logs
#  out, define MAILMESSAGE.  Make sure you also specify the
#  correct MAILPATH and MAILMESSAGEFILE paths, where MAILPATH
#  is the path to the mail program to use, and MAILMESSAGEFILE
#  is the generic file to send to the user.  These last two are
#  in doinkd.h.
#
# If you have problems with doinkd warning users but never logging them
# out, especiaally under DU 3.2, add -DDISABLE_WARNS to the defs line.
# This will prevent warns from happening, but logouts should work.

######################################################################
######################################################################
# UnixWare 7 -- SVR5 MP 
#DEFS = -DSYSV -DHAVE_PROC_FS -DUTMPPID -DHAVE_SETSID -DHAVE_UTMPX -DUTMPHOST -DUNIXWARE
DEFS = -DSYSV -DPS_HACK -DRUDE_KILL -DHAVE_SETSID -DHAVE_UTMPX -DUTMPHOST -DUNIXWARE
SPECLIBS = 
INSTTYPE = install4

# SysV application file layout
DEST    = /usr/sbin
CFDEST  = /etc/doinkd
NEWCFDEST = /etc/doinkd
MDEST   = /usr/local/man
LOGDEST = /var/adm/doinkd
INDEST  = /etc/init.d

OWNER   = root
CFOWNER = root
MOWNER  = root

GROUP   = root
CFGROUP = root

MODE    = 750
CFMODE  = 664
MMODE   = 644

######################################################################
######################################################################
# BSD  --  SunOS 4.x  
# Note: If doinkd fails to log users off, try adding -DPS_HACK on the
#       DEFS line to have doinkd use that killing method.
#DEFS += -DBSD4_2 -DUTMPHOST -DHAVE_SETSID -DPROC_SEARCH_1
#SPECLIBS = -lkvm
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# Older BSDI  ---  (See BSD/OS 2.0 for newer versions of BSDI)
# Define PROC_SEARCH_1 to enable xlock checking with the
#        "kill user immediately if xlock found" effect
#        If you add it, '-lkvm' must also be added to the SPECLIBS line
# You will need to delete parse.c before compiling!  You can either
# do so by hand, or do a 'make clean' followed by the normal 'make'.
##DEFS += -DPROC_SEARCH_1
#DEFS += -DBSDI -DHAVE_SETSID
#SPECLIBS =
##SPECLIBS += -lkvm
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# BSD/OS 2.0  ---  New BSDI
# You will need to delete parse.c before compiling!  You can either
# do so by hand, or do a 'make clean' followed by the normal 'make'.
#DEFS += -DBSD_OS2 -DUTMPHOST -DPROC_SEARCH_1 -DHAVE_SETSID -DHAVE_YYRESTART -DPS_HACK
#SPECLIBS = -lkvm
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = man
#
#GROUP   = daemon
#CFGROUP = daemon
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# FreeBSD-2.X
# Define PROC_SEARCH_1 to enable xlock checking with the
#        "kill user immediately if xlock found" effect
#        If you add it, '-lkvm' must also be added to the SPECLIBS line
# You will need to delete parse.c before compiling!  You can either
# do so by hand, or do a 'make clean' followed by the normal 'make'.
#DEFS += -O -m486
#DEFS += -DBSD_OS2 -DHAVE_SETSID -DHAVE_YYRESTART -DPS_HACK
#DEFS += -DPROC_SEARCH_1
#SPECLIBS = -lkvm
#INSTTYPE = install1a
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = man
#
#GROUP   = daemon
#CFGROUP = daemon
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 444

######################################################################
######################################################################
# Ultrix 4.4
#DEFS = -DHAVE_SETSID -DUTMPHOST
#SPECLIBS =
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /usr/adm
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = system
#CFGROUP = system
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# SVR4  --  Solaris 2.x
# Use install3 for /usr/sbin/install
# and install1 for /usr/ucb/install
#DEFS += -DSYSV -DHAVE_PROC_FS -DUTMPPID -DHAVE_SETSID -DHAVE_UTMPX -DUTMPHOST -DRUDEKILL
#SPECLIBS = 
#INSTTYPE = install3
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log/doinkd
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644
#
#######################################################################
######################################################################
# Linux
# Note:  NOT all version of Linux have yyrestart().  Remove the
#        -DHAVE_YYRESTART if you have problems.
# You should do a 'make clean' before 'make', so that parse.c
# and scan.c will be created on your system.
#DEFS += -DSYSV -DUTMPPID -DUTMPHOST -DHAVE_SETSID -DHAVE_YYRESTART
#SPECLIBS = 
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# UnixWare 2.01 -- SVR4.2 MP  (probably other UnixWare 2.x too)
#DEFS = -DSYSV -DUTMPPID -DHAVE_SETSID 
#SPECLIBS = 
#INSTTYPE = install4
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#NEWCFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/adm/doinkd
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# UnixWare 1.1.4 -- SVR4.2  (probably other UnixWare 1.x too)
#DEFS = -DSYSV -DHAVE_PROC_FS -DUTMPPID -DHAVE_SETSID
#SPECLIBS = 
#INSTTYPE = install3
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /usr/adm
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = sys
#CFGROUP = sys
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# AIX 3.2.5  (probably other AIX 3.x too)
#DEFS = -DSYSV -DAIX -DUTMPPID -DUTMPHOST -DHAVE_SETSID
#SPECLIBS = 
#INSTTYPE = install2
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = system
#CFGROUP = system
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# Dec Alpha OSF/1 V3  and  DIGITAL Unix
# If you have problems with doinkd warning users but never logging them
# out, especiaally under DU 3.2, add -DDISABLE_WARNS to the defs line.
# This will prevent warns from happening, but logouts should work.
#DEFS = -DSYSV -DHAVE_PROC_FS -DUTMPPID -DHAVE_SETSID -DUTMPHOST
#SPECLIBS = 
#INSTTYPE = install3
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
# DIGITAL Unix wants system (does OSF/1?):
##GROUP   = system
##CFGROUP = system
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# SVR3  --  Univel
#DEFS += -DHAVE_SETSID -DPROC_SEARCH_1
#SPECLIBS = -lkvm
#INSTTYPE = install1
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /var/log
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = root
#CFGROUP = root
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644

######################################################################
######################################################################
# HP-UX 9.0X  ---  Also SGI's
#DEFS = -DHAVE_SETSID -DSYSV -DUTMPPID
#SPECLIBS =
#INSTTYPE = install3
#
#DEST    = /usr/local/sbin
#CFDEST  = /etc/doinkd
#MDEST   = /usr/local/man
#LOGDEST = /usr/adm
#
#OWNER   = root
#CFOWNER = root
#MOWNER  = root
#
#GROUP   = sys
#CFGROUP = sys
#
#MODE    = 750
#CFMODE  = 664
#MMODE   = 644


######################################################################
######################################################################
######################################################################
#
# Define DEBUG to have doinkd put more debug information in its logfile.
# It will not log anyone off when compiled in debug mode!
#DEBUG = -DDEBUG=9999 -g

# programs (not including paths) that need explicit make lines, plain files
BINARY  = doinkd
COMMFILE= doinkd.cf
CFMAN   = doinkd.cf.5
PMAN    = doinkd.8

# Names of config and log files
CONFIG  = ${CFDEST}/${COMMFILE}
LOGFILE = ${LOGDEST}/doinkd.log

# HERE are the big CFLAGS
# Add -g if you want debugging
# Add -O or whatever variant for optimization
CFLAGS = ${DEFS} ${DEBUG} -DCONFIG=\"${CONFIG}\" -DLOGFILE=\"${LOGFILE}\" ${INCLUDE}

# For HP's ANSI C compiler (use -g instead of +O3 for debugging)
# CFLAGS = +O3 -Aa -D_HPUX_SOURCE ${DEFS} ${DEBUG} -DCONFIG=\"${CONFIG}\" -DLOGFILE=\"${LOGFILE}\" ${INCLUDE}

LIBS   = ${SPECLIBS}

#
#####################################################################
#  Hopefully, you shouldn't have to change anything below this line.
#####################################################################
#

# Source files to be mkdepended
SRC = insque.c list.c doinkd.c warn.c xlock_check.c zap.c linetimelist.c
SRCl = parse.y scan.l
SRCg = parse.c scan.c
MAN = $(CFMAN) $(PMAN)
HDR = doinkd.h
OBJ = insque.o list.o parse.o scan.o doinkd.o warn.o xlock_check.o zap.o linetimelist.o


all: ${BINARY} ${MAN}

clean:
	${RM} -f a.out ${OBJ} core errs lint.errs Makefile.bak *.s tags \
		${SRCg} ${BINARY} y.tab.* lex.yy.c doinkd.cf.5 doinkd.8

#depend: ${SRC} ${SRCg} ${HDR}
#	maketd -a ${DEFS} ${INCLUDE} ${SRC} ${SRCg}

# Some systems use install1, some use install2, others install3, others use ????
# Aix Prefers install2
# BSD machines in general prefer install1
# Solaris 2.x uses install1 for /usr/ucb/install
#              and install3 for /usr/sbin/install (/etc/install is linked to it)
install: ${INSTTYPE}

# BSD machines in general prefer install1
# Solaris 2.x using /usr/ucb/install also uses install1
install1: all 
	install -c -m ${MODE} -o ${OWNER} -g ${GROUP} ${BINARY} ${DEST}
	install -c -m ${CFMODE} -o ${CFOWNER} -g ${CFGROUP} ${COMMFILE}.template ${CFDEST}
	install -c -m ${MMODE} -o ${MOWNER} ${CFMAN} ${MDEST}/man5
	install -c -m ${MMODE} -o ${MOWNER} ${PMAN} ${MDEST}/man8
	@echo ""
	@echo "Be sure to edit/create the file ${CONFIG} based on"
	@echo "the needs for your system. The ${CONFIG}.template"
	@echo "file can serve as a guide, as well as the man pages."

# BSD like, but with some enhancements like: install -s -> strip binary
#                                            gzip manual pages
install1a: all
	install -s -c -m ${MODE} -o ${OWNER} -g ${GROUP} ${BINARY} ${DEST}
	install -c -m ${CFMODE} -o ${CFOWNER} -g ${CFGROUP} ${COMMFILE}.template ${CFDEST}
	install -c -m ${MMODE} -o ${MOWNER} ${CFMAN} ${MDEST}/man5
	gzip -f ${MDEST}/man5/${CFMAN}
	install -c -m ${MMODE} -o ${MOWNER} ${PMAN} ${MDEST}/man8
	gzip -f ${MDEST}/man8/${PMAN}
	@echo ""
	@echo "Be sure to edit/create the file ${CONFIG} based on"
	@echo "the needs for your system. The ${CONFIG}.template"
	@echo "file can serve as a guide, as well as the man pages."

# Aix Prefers install2
install2: all 
	install -c ${DEST} -M ${MODE} -O ${OWNER} -G ${GROUP} ${BINARY} ${DEST}
	install -c ${CFDEST} -M ${CFMODE} -O ${CFOWNER} -G ${CFGROUP} ${COMMFILE}.template ${CFDEST}
	install -c ${MDEST}/man5 -M ${MMODE} -O ${MOWNER} ${CFMAN} ${MDEST}/man5
	install -c ${MDEST}/man8 -M ${MMODE} -O ${MOWNER} ${PMAN} ${MDEST}/man8
	@echo ""
	@echo "Be sure to edit/create the file ${CONFIG} based on"
	@echo "the needs for your system. The ${CONFIG}.template"
	@echo "file can serve as a guide, as well as the man pages."

# Solaris 2.x using /usr/sbin/install uses install3
# This install 'f'orces the files into the directory, but backs up the
# 'o'ld file to OLD<file> if a file in that name was already in that directory
install3: all 
	install -o -f ${DEST} -m ${MODE} -u ${OWNER} -g ${GROUP} ${BINARY}
	install -o -f ${CFDEST} -m ${CFMODE} -u ${CFOWNER} -g ${CFGROUP} ${COMMFILE}.template
	install -o -f ${MDEST}/man5 -m ${MMODE} -u ${MOWNER} ${CFMAN}
	install -o -f ${MDEST}/man8 -m ${MMODE} -u ${MOWNER} ${PMAN}
	@echo ""
	@echo "Be sure to edit/create the file ${CONFIG} based on"
	@echo "the needs for your system. The ${CONFIG}.template"
	@echo "file can serve as a guide, as well as the man pages."

install4: all 
	if [ ! -d ${DEST} ] ; then \
		mkdir -p ${DEST}; \
	fi;
	install -o -f ${DEST} -m ${MODE} -u ${OWNER} -g ${GROUP} ${BINARY}
	if [ ! -d ${NEWCFDEST} ] ; then \
		mkdir -p ${NEWCFDEST}; \
	fi;
	if [ ! -d ${CFDEST} ] ; then \
		mkdir -p ${CFDEST}; \
	fi;
	if [ ! -d ${LOGDEST} ] ; then \
		mkdir -p ${LOGDEST}; \
	fi;
	install -o -f ${NEWCFDEST} -m ${CFMODE} -u ${CFOWNER} -g ${CFGROUP} ${COMMFILE}.template
	if [ ! -d ${MDEST} ] ; then \
		mkdir -p ${MDEST}; \
	fi;
	if [ ! -d ${MDEST}/man5 ] ; then \
		mkdir -p ${MDEST}/man5; \
	fi;
	if [ ! -d ${MDEST}/man8 ] ; then \
		mkdir -p ${MDEST}/man8; \
	fi;
	install -o -f ${MDEST}/man5 -m ${MMODE} -u ${MOWNER} ${CFMAN}
	install -o -f ${MDEST}/man8 -m ${MMODE} -u ${MOWNER} ${PMAN}

	install -o -f ${INDEST} -m ${MODE} -u ${OWNER} -g ${GROUP} init.d/doinkd
	if [ ! -f /etc/rc2.d/S95doinkd ] ; then \
		ln -s ${INDEST}/doinkd /etc/rc2.d/S95doinkd; \
	fi;
	if [ ! -f /etc/rc0.d/K95doinkd ] ; then \
		ln -s ${INDEST}/doinkd /etc/rc0.d/K95doinkd; \
	fi;

	@echo ""
	@echo "Be sure to edit/create the file ${CONFIG} based on"
	@echo "the needs for your system. The ${NEWCFDEST}/${COMMFILE}.template"
	@echo "file can serve as a guide, as well as the man pages."

man: ${MAN}

${MAN}: doinkd.man.form doinkd.cf.man.form
	echo ${CONFIG} | sed 's/\//\\\//g' > .maketmp
	echo ${LOGFILE} | sed 's/\//\\\//g' > .maketmp2
	sed -e "s/CONFIGPATH/`cat .maketmp`/" -e "s/LOGFILEPATH/`cat .maketmp2`/" doinkd.cf.man.form > ${CFMAN}
	sed -e "s/CONFIGPATH/`cat .maketmp`/" -e "s/LOGFILEPATH/`cat .maketmp2`/" doinkd.man.form > ${PMAN}
	rm -f .maketmp .maketmp2

lint: ${SRC} ${SRCg}
	lint -hxn ${DEFS} ${SRC} ${SRCg}
	
print: 
	print -n -J "doinkd Source" Makefile ${HDR} ${SRCl} ${SRC}

shar:
	shar README Makefile ${COMMFILE} ${MAN} ${SRC} ${SRCl} ${HDR} > doinkd.shar

source: ${SRC} ${SRCl} ${HDR} ${MAN} ${COMMFILE}

spotless:
	${RM} -f a.out ${OBJ} core errs lint.errs Makefile.bak y.tab.* yacc.act\
		yacc.tmp *.s ${BINARY} tags parse.c scan.c
	rcsclean ${SRC} ${SRCl} ${HDR} ${COMMFILE} ${MAN}

tags: ${SRCS} ${SRCg} tags
	ctags ${SRC} ${SRCg}

# The utmp file lines counter.  Useful, perhaps, in ensuring that
# MAXUSERS is set large enough in doinkd.h.  MAXUSERS should be larger
# than the number of lines (the output number).
utmplines: utmplinesp
	@./utmplinesp

utmplinesp: utmplines.c
	@${CC} ${DEFS} -o utmplinesp utmplines.c

# rules for everybody in ${BINARY} go here
doinkd: ${OBJ}
	${CC} ${CFLAGS} -o doinkd ${OBJ} ${LIBS}

y.tab.h: parse.c

parse.c: parse.y
	yacc -d parse.y
	mv y.tab.c parse.c

scan.c: scan.l
	lex scan.l
	mv lex.yy.c scan.c

# DO NOT DELETE THIS LINE - make depend DEPENDS ON IT

insque.o: insque.c

list.o: list.c doinkd.h y.tab.h

doinkd.o: doinkd.c doinkd.h

warn.o: warn.c doinkd.h

xlock_check.o: xlock_check.c doinkd.h

zap.o: zap.c doinkd.h

parse.o: parse.c doinkd.h

scan.o: scan.c y.tab.h

# *** Do not add anything here - It will go away. ***
