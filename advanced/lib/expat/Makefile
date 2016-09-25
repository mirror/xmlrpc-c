ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/expat

include $(BLDDIR)/config.mk

default: all

SUBDIRS = gennmtab xmlparse xmltok

PKGCONFIG_FILES_TO_INSTALL := xmlrpc_expat.pc

include $(SRCDIR)/common.mk

.PHONY: all
all: $(SUBDIRS:%=%/all) $(PKGCONFIG_FILES_TO_INSTALL)

# Extra dependencies to make parallel make work in spite of all the submakes
# (See top level make file for details)
xmlparse/all: gennmtab/all xmltok/all
xmltok/all: gennmtab/all

#-----------------------------------------------------------------------------
# RULES TO MAKE PKGCONFIG FILES
#
# (These are files used by the 'pkg-config' program to get information about
# using the libraries we build)
#-----------------------------------------------------------------------------

xmlrpc_expat.pc:
	rm -f $@
	@echo "Echoes to '$@' suppressed here ..."
	@echo "Name:        xmlrpc_expat"                                  >>$@
	@echo "Description: Xmlrpc-c XML parsing library"                  >>$@
	@echo "Version:     $(XMLRPC_VERSION_STRING)"                      >>$@
	@echo	                                                           >>$@
	@echo "Requires: xmlrpc_util"                                      >>$@
	@echo "Libs:     -L$(LIBDESTDIR) -lxmlrpc_xmlparse -lxmlrpc_xmltok" >>$@
	@echo "Cflags:   -I$(HEADERDESTDIR)"                               >>$@

.PHONY: clean
clean: $(SUBDIRS:%=%/clean) clean-common

.PHONY: distclean
distclean: $(SUBDIRS:%=%/distclean) distclean-common

.PHONY: tags
tags: $(SUBDIRS:%=%/tags) TAGS

DISTFILES = 

.PHONY: distdir
distdir: distdir-common

.PHONY: install
install: install-common $(SUBDIRS:%=%/install)

.PHONY: uninstall
uninstall: uninstall-common $(SUBDIRS:%=%/uninstall)

.PHONY: check
check:

.PHONY: dep
dep: $(SUBDIRS:%=%/dep)

