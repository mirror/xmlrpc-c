ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  ABYSSDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(ABYSSDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/abyss/src

include $(SRCDIR)/Makefile.config

CFLAGS = $(CFLAGS_COMMON)
CFLAGS += -D_UNIX
ifeq ($(ENABLE_ABYSS_THREADS),yes)
  THREAD_MODULE = thread_pthread
else
  THREAD_MODULE = thread_fork
endif
CFLAGS += $(CFLAGS_PERSONAL) $(CADD)
LIBLDFLAGS = $(LDFLAGS_VERSINFO) -rpath $(LIBINST_DIR)
LIBLDFLAGS += $(LADD)

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/lib/util/include

LTLIBRARIES_TO_INSTALL = libxmlrpc_abyss.la

default: all

.PHONY: all
all: libxmlrpc_abyss.la


ABYSS_OBJS = \
  channel.lo \
  chanswitch.lo \
  conf.lo \
  conn.lo \
  data.lo \
  date.lo \
  file.lo \
  http.lo \
  init.lo \
  response.lo \
  server.lo \
  session.lo \
  socket.lo \
  socket_unix.lo \
  token.lo \
  $(THREAD_MODULE).lo \
  trace.lo \

libxmlrpc_abyss.la: $(ABYSS_OBJS)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) $^

$(ABYSS_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) \
	  $(CFLAGS) $<

# Need this dependency for those who don't use Makefile.depend.
# Without it, version.h doesn't get created.
response.lo: version.h

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

include $(SRCDIR)/Makefile.common

include Makefile.depend
