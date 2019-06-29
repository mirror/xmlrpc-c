ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/openssl

include $(BLDDIR)/config.mk

default: all

MODS := xmlrpc_openssl_thread

TARGET_MODS = $(MODS)

TARGET_LIBRARY_NAMES := libxmlrpc_openssl

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_openssl.a

SHARED_LIBS_TO_BUILD := libxmlrpc_openssl
SHARED_LIBS_TO_INSTALL := libxmlrpc_openssl

PKGCONFIG_FILES_TO_INSTALL := xmlrpc_openssl.pc

MAJ := 1

OPENSSL_INCLUDES := $(shell pkg-config openssl --cflags)
OPENSSL_LIBS     := $(shell pkg-config openssl --libs)

include $(SRCDIR)/common.mk

OPENSSL_SHLIB = $(call shlibfn,libxmlrpc_openssl)
#OPENSSL_SHLIB is e.g. libxmlrpc_openssl.so.3.1
OPENSSL_SHLIBLE = $(call shliblefn,libxmlrpc_openssl)
#OPENSSL_SHLIBLE is e.g. libxmlrpc_openssl.so

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

.PHONY: all
all: libxmlrpc_openssl.a \
  $(TARGET_SHARED_LIBRARIES) \
  $(TARGET_SHARED_LE_LIBS) \
  $(PKGCONFIG_FILES_TO_INSTALL) \

# Rules for the above dependencies are in common.mk,
# courtesy of TARGET_MODS.

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES
$(OPENSSL_SHLIB): $(TARGET_MODS:%=%.osh) \
	$(LIBXMLRPC_UTIL)
$(OPENSSL_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh)
$(OPENSSL_SHLIB): LIBDEP = $(LIBXMLRPC_UTIL_LIBDEP) $(OPENSSL_LIBS)

# Rule for this is in common.mk, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_openssl.a: $(TARGET_MODS:%=%.o)
libxmlrpc_openssl.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include \
  $(OPENSSL_INCLUDES)

#-----------------------------------------------------------------------------
# RULES TO MAKE PKGCONFIG FILES
#
# (These are files used by the 'pkg-config' program to get information about
# using the libraries we build)
#-----------------------------------------------------------------------------

xmlrpc_openssl.pc:
	rm -f $@
	@echo "Echoes to '$@' suppressed here ..."
	@echo "Name:        xmlrpc_openssl"                                >>$@
	@echo "Description: Openssl convenience function from Xmlrpc-c package"                 >>$@
	@echo "Version:     $(XMLRPC_VERSION_STRING)"                      >>$@
	@echo	                                                           >>$@
	@echo "Requires: xmlrpc_util"                                      >>$@
	@echo "Libs:     -L$(LIBDESTDIR) -lxmlrpc_openssl"                 >>$@
	@echo "Cflags:   -I$(HEADERDESTDIR)"                               >>$@

#-----------------------------------------------------------------------------
#  MISCELLANEOUS
#-----------------------------------------------------------------------------

.PHONY: clean
clean: clean-common

.PHONY: distclean
distclean: clean distclean-common

.PHONY: tags
tags: TAGS

.PHONY: distdir
distdir:

.PHONY: install
install: install-common

.PHONY: uninstall
uninstall: uninstall-common

.PHONY: dep
dep: dep-common

include depend.mk

# Need this dependency for those who don't use depend.mk.
# Without it, version.h doesn't get created.
xmlrpc_curl_transport.o xmlrpc_curl_transport.osh: version.h
