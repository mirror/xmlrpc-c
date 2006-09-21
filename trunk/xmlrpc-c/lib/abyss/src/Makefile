ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  ABYSSDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(ABYSSDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/abyss/src

include $(SRCDIR)/Makefile.config

default: all

TARGET_LIBRARY_NAMES := libxmlrpc_abyss

STATIC_LIBRARIES_TO_INSTALL = libxmlrpc_abyss.a

SHARED_LIBS_TO_BUILD := libxmlrpc_abyss
SHARED_LIBS_TO_INSTALL := libxmlrpc_abyss

include $(SRCDIR)/Makefile.common

CFLAGS = $(CFLAGS_COMMON)
CFLAGS += -D_UNIX
ifeq ($(ENABLE_ABYSS_THREADS),yes)
  THREAD_MODULE = thread_pthread
else
  THREAD_MODULE = thread_fork
endif
CFLAGS += $(CFLAGS_PERSONAL) $(CADD)

INCLUDES = -Isrcdir -Isrcdir/include -Isrcdir/lib/util/include

ABYSS_SHLIB = $(call shlibfn,libxmlrpc_abyss)
#ABYSS_SHLIB is e.g. libxmlrpc_abyss.so.3.1
ABYSS_SHLIBLE = $(call shliblefn,libxmlrpc_abyss)
#ABYSS_SHLIBLE is e.g. libxmlrpc_abyss.so

ifneq ($(SHARED_LIB_TYPE),NONE)
  TARGET_SHARED_LIBS := $(ABYSS_SHLIB) $(ABYSS_SHLIBLE)
  endif


# This 'Makefile.common' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/Makefile.common: srcdir blddir

.PHONY: all
all: libxmlrpc_abyss.a $(TARGET_SHARED_LIBS) $(TARGET_SHARED_LE_LIBS)


ABYSS_OBJS = \
  channel.o \
  chanswitch.o \
  conf.o \
  conn.o \
  data.o \
  date.o \
  file.o \
  http.o \
  init.o \
  response.o \
  server.o \
  session.o \
  socket.o \
  socket_unix.o \
  token.o \
  $(THREAD_MODULE).o \
  trace.o \


# Rule for this is in Makefile.common, courtesy of TARGET_SHARED_LIBRARIES:
$(ABYSS_SHLIB): $(ABYSS_OBJS)
$(ABYSS_SHLIB): OBJECTS = $(ABYSS_OBJS)

# Rule for this is in Makefile.common, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_abyss.a: $(ABYSS_OBJS)
libxmlrpc_abyss.a: OBJECTS = $(ABYSS_OBJS)

$(ABYSS_OBJS):%.o:%.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $<

# Need this dependency for those who don't use Makefile.depend.
# Without it, version.h doesn't get created.
response.o: version.h

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

.PHONY: dep
dep: dep-common

include Makefile.depend
