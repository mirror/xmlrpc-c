AUTOMAKE_OPTIONS = foreign

# You can change this using 'make GPG_KEY="mykeyid" ...'.
# You will also need to set up your .rpmmacros file correctly.
GPG_KEY = "software validation key"

RPM_TOPDIR = =rpm_topdir

SUBDIRS = lib src examples doc tools
EXTRA_DIST = NEWS BUGS CREDITS PORTING REFACTORINGS TESTING SECURITY \
	     GETTING_LIBWWW PATCHING_ABYSS xmlrpc_win32_config.h

bin_SCRIPTS = xmlrpc-c-config

dist-hook: xmlrpc-c.spec
	cp $(srcdir)/xmlrpc-c.spec $(distdir)/
	cp -r $(srcdir)/debian $(distdir)/
	cp -r $(srcdir)/conf $(distdir)/
	rm -f $(distdir)/conf/log/abyss.pid
	rm -f $(distdir)/conf/log/access.log
	cp -r $(srcdir)/Windows $(distdir)/
	cp -r $(srcdir)/examples/Win32ClientTest $(distdir)/examples/
	cp -r $(srcdir)/examples/Win32ServerTest $(distdir)/examples/
	find $(distdir)/ -type d -name CVS -prune -exec rm -rf {} \;
	find $(distdir)/ -type f -name \*~ -exec rm -rf {} \;
	chmod +x $(distdir)/debian/rules

fulldist: distcheck xmlrpc-c.lsm
	rm -rf fulldist/$(VERSION)
	rm -rf $(RPM_TOPDIR)
	mkdir $(RPM_TOPDIR)
	for DIR in SPECS SOURCES BUILD RPMS SRPMS; do \
	    mkdir $(RPM_TOPDIR)/$$DIR; \
	done
	cp xmlrpc-c.spec $(RPM_TOPDIR)/SPECS
	cp xmlrpc-c-$(VERSION).tar.gz $(RPM_TOPDIR)/SOURCES
	rpm -ba --define "_topdir `pwd`/$(RPM_TOPDIR)" --clean xmlrpc-c.spec
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

fulldistsig:
	rpm --addsign fulldist/$(VERSION)/*.rpm
	gpg --default-key $(GPG_KEY) --detach-sign \
            fulldist/$(VERSION)/xmlrpc-c-$(VERSION).tar.gz 
	@echo
	@banner="fulldist/$(VERSION) has been signed with GPG"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"

debiandist:
	dpkg-buildpackage -k$(GPG_KEY) -rfakeroot

.PHONY: fulldist fulldistsig debiandist
