#
#   $Id: Makefile.am,v 1.1 2009/08/22 07:50:59 y42304 Exp $
#
#   Authors:
#    Lars Fenneberg		<lf@elemental.net>	 
#
#   This software is Copyright 1996,1997 by the above mentioned author(s), 
#   All Rights Reserved.
#
#   The license which is distributed with this software in the file COPYRIGHT
#   applies to this software. If your distribution is missing this file, you
#   may request it from <pekkas@netcore.fi>.
#

AUTOMAKE_OPTIONS = 1.2 foreign

DEFS=  -DPATH_RADVD_CONF=\"$(PATH_RADVD_CONF)\" \
	-DPATH_RADVD_LOG=\"$(PATH_RADVD_LOG)\" -DLOG_FACILITY=$(LOG_FACILITY) \
	-DPATH_RADVD_PID=\"$(PATH_RADVD_PID)\" \
	-DVERSION=\"$(VERSION)\" -DINET6=1
INCLUDES=-I$(srcdir) -I.

########################################################################

COMMON_SRC = log.c socket.c recv.c util.c radvd.h defaults.h pathnames.h \
	includes.h
ifdef RADVD_DUMP
sbin_PROGRAMS = radvd \
    radvdump
else
sbin_PROGRAMS = radvd
endif

radvd_SOURCES = $(COMMON_SRC) radvd.c timer.c send.c process.c interface.c \
	device.c device-common.c privsep.c gram.y gram.h scanner.l
radvd_LDADD = -lfl
EXTRA_radvd_SOURCES = device-linux.c device-bsd44.c privsep-linux.c privsep-bsd44.c
YFLAGS = -d
CFLAGS = @CFLAGS@ -Wall -Wpointer-arith  -Wcast-qual -Wcast-align \
	-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
ifdef RADVD_DUMP
radvdump_SOURCES = $(COMMON_SRC) radvdump.c
endif
ifdef RADVD_DUMP
man_MANS = radvd.8 radvd.conf.5 \
    radvdump.8
EXTRA_DIST = radvd.8.man radvdump.8.man radvd.conf.5.man VERSION CHANGES \
	COPYRIGHT INTRO.html copyright.blurb radvd.conf.example\
else
man_MANS = radvd.8 radvd.conf.5
EXTRA_DIST = radvd.8.man radvd.conf.5.man VERSION CHANGES \
	COPYRIGHT INTRO.html copyright.blurb radvd.conf.example\
endif

BUILT_SOURCES = device.c

ifdef RADVD_DUMP
CLEANFILES = radvd.8 radvd.conf.5 \
	radvdump.8
else
CLEANFILES = radvd.8 radvd.conf.5
endif
DISTCLEANFILES = device.c

SUFFIXES = .man

.man:
	$(SED) 	-e 's,@''VERSION''@,$(VERSION),g' \
		-e 's,@''sbindir''@,$(sbindir),g' \
		-e 's,@''PATH_RADVD_CONF''@,$(PATH_RADVD_CONF),g' \
		-e 's,@''PATH_RADVD_PID''@,$(PATH_RADVD_PID),g' \
		-e 's,@''PATH_RADVD_LOG''@,$(PATH_RADVD_LOG),g' \
		-e 's,@''LOG_FACILITY''@,$(LOG_FACILITY),g' \
		$< > $@

dist-hook:
	rm -f $(distdir)/device.c	
	mkdir $(distdir)/redhat
	cp -p $(srcdir)/redhat/radvd.init $(srcdir)/redhat/radvd.spec \
		$(srcdir)/redhat/radvd.sysconfig $(distdir)/redhat
