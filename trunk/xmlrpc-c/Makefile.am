AUTOMAKE_OPTIONS = foreign

SUBDIRS = lib src examples doc
EXTRA_DIST = NEWS BUGS CREDITS PORTING REFACTORINGS TESTING SECURITY \
	     GETTING_LIBWWW PATCHING_ABYSS 

bin_SCRIPTS = xmlrpc-c-config

dist-hook:
	cp -r $(srcdir)/conf $(distdir)/
	rm -f $(distdir)/conf/log/abyss.pid
	rm -f $(distdir)/conf/log/access.log
