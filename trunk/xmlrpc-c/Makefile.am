AUTOMAKE_OPTIONS = foreign

RPM_TOPDIR = =rpm_topdir

SUBDIRS = lib src examples doc tools
EXTRA_DIST = NEWS BUGS CREDITS PORTING REFACTORINGS TESTING SECURITY \
	     GETTING_LIBWWW PATCHING_ABYSS 

bin_SCRIPTS = xmlrpc-c-config

dist-hook:
	cp -r $(srcdir)/conf $(distdir)/
	rm -f $(distdir)/conf/log/abyss.pid
	rm -f $(distdir)/conf/log/access.log

fulldist: distcheck xmlrpc-c.spec xmlrpc-c.lsm
	rm -rf fulldist/$(VERSION)
	rm -rf $(RPM_TOPDIR)
	mkdir $(RPM_TOPDIR)
	for DIR in SPECS SOURCES BUILD RPMS SRPMS; do \
	    mkdir $(RPM_TOPDIR)/$$DIR; \
	done
	cp xmlrpc-c.spec $(RPM_TOPDIR)/SPECS
	cp xmlrpc-c-$(VERSION).tar.gz $(RPM_TOPDIR)/SOURCES
	rpm -ba --define "_topdir `pwd`/$(RPM_TOPDIR)" xmlrpc-c.spec
	-mkdir fulldist
	mkdir fulldist/$(VERSION)
	cp xmlrpc-c.lsm fulldist/$(VERSION)
	cp xmlrpc-c-$(VERSION).tar.gz fulldist/$(VERSION)
	mv $(RPM_TOPDIR)/SRPMS/*.rpm fulldist/$(VERSION)
	mv $(RPM_TOPDIR)/RPMS/*/*.rpm fulldist/$(VERSION)
	rm -rf $(RPM_TOPDIR)
	@banner="fulldist/$(VERSION) is ready for distribution"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"


.PHONY: fulldist
