lib_LTLIBRARIES = libxmlrpc_xmltok.la

libxmlrpc_xmltok_la_SOURCES = xmltok.c xmlrole.c nametab.h
libxmlrpc_xmltok_la_LDFLAGS = @VERSION_INFO@

EXTRA_DIST = ascii.h asciitab.h dllmain.c iasciitab.h latin1tab.h utf8tab.h \
             xmldef.h xmlrole.h xmltok.dsp xmltok.h xmltok_impl.c \
             xmltok_impl.h xmltok_ns.c

BUILT_SOURCES = nametab.h

nametab.h:
	rm -f $@
	../gennmtab/gennmtab >$@

xmltok.c: nametab.h
