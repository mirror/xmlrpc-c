ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR = $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

include $(SRCDIR)/Makefile.common

.PHONY: all
all: casprintf.o cmdline_parser.o

INCLUDES = -Iinclude -I$(BUILDDIR)

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

%.o:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(CFLAGS) $<

include Makefile.depend

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

install: