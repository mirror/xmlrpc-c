###############################################################################
# This directory builds libxmlrpc_util, which contains utility
# functions that are used by the Xmlprc-c libraries, and also
# directly by Xmlrpc-c programs.  Some of them are documented for use
# by Xmlrpc-c users, as facilities of the libxmlrpc library (which
# prerequires libxmlrpc_util).
#
# The functions in this library are characterized by being general purpose
# programming functions, such as one might wish were in the standard C
# library, which have nothing in particular to do with XML-RPC.
###############################################################################

ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/libutil

include $(BLDDIR)/config.mk

default: all

TARGET_LIBRARY_NAMES := libxmlrpc_util

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_util.a

SHARED_LIBS_TO_BUILD := libxmlrpc_util
SHARED_LIBS_TO_INSTALL := libxmlrpc_util

ifeq ($(HAVE_PTHREAD),yes)
  LOCK_PTHREAD = lock_pthread
else
  LOCK_PTHREAD =
endif

ifeq ($(MSVCRT),yes)
  LOCK_WINDOWS = lock_windows
else
  LOCK_WINDOWS =
endif

TARGET_MODS = \
  asprintf \
  base64 \
  error \
  lock_platform \
  $(LOCK_PTHREAD) \
  $(LOCK_WINDOWS) \
  lock_none \
  make_printable \
  memblock \
  mempool \
  select \
  sleep \
  string_number \
  time \
  utf8 \

OMIT_LIBXMLRPC_UTIL_RULE=Y
MAJ=4
  # Major number of shared libraries in this directory

include $(SRCDIR)/common.mk

INCLUDES = -I$(BLDDIR) -Isrcdir \
           -I$(BLDDIR)/include -Isrcdir/include -Isrcdir/lib/util/include

UTIL_SHLIB = $(call shlibfn,libxmlrpc_util)
#UTIL_SHLIB is e.g. libxmlrpc_util.so.3.1
UTIL_SHLIBLE = $(call shliblefn,libxmlrpc_util)
#UTIL_SHLIBLE is e.g. libxmlrpc_util.so

PKGCONFIG_FILES_TO_INSTALL := xmlrpc_util.pc

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

.PHONY: all
all: libxmlrpc_util.a \
  $(TARGET_SHARED_LIBRARIES) \
  $(TARGET_SHARED_LE_LIBS) \
  $(PKGCONFIG_FILES_TO_INSTALL) \

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES:
$(UTIL_SHLIB): $(TARGET_MODS:%=%.osh)
$(UTIL_SHLIB): LIBOBJECTS = $(TARGET_MODS:%=%.osh)
$(UTIL_SHLIB): LIBDEP += $(SOCKET_LIBOPT) $(THREAD_LIBS)

# Rule for this is in common.mk, courtesy of TARGET_LIBRARY_NAMES:

libxmlrpc_util.a: $(TARGET_MODS:%=%.o)
libxmlrpc_util.a: LIBOBJECTS = $(TARGET_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules to compile object modules from which to build the static and shared
# library are in common.mk, courtesy of TARGET_MODS.

#-----------------------------------------------------------------------------
# RULES TO MAKE PKGCONFIG FILES
#
# (These are files used by the 'pkg-config' program to get information about
# using the libraries we build)
#-----------------------------------------------------------------------------

xmlrpc_util.pc:
	rm -f $@
	@echo "Echoes to '$@' suppressed here ..."
	@echo -e '$(PKGCONFIG_VAR_SET)'                                    >>$@
	@echo	                                                           >>$@
	@echo "Name:        xmlrpc_util"                                   >>$@
	@echo "Description: Xmlrpc-c utility functions library"            >>$@
	@echo "Version:     $(XMLRPC_VERSION_STRING)"                      >>$@
	@echo	                                                           >>$@
	@echo "Requires: "                                                 >>$@
	@echo 'Libs:     -L$${libdir} -lxmlrpc_util'                       >>$@
	@echo 'Cflags:   -I$${includedir}'                                 >>$@

.PHONY: install
install: install-common

.PHONY: uninstall
uninstall: uninstall-common

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include depend.mk
