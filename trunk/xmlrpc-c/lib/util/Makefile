ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
BUILDDIR = $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

include $(SRCDIR)/Makefile.common

all: casprintf.o cmdline_parser.o

INCLUDES = -Iinclude -I$(BUILDDIR)

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

%.o:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(CFLAGS) $<

include Makefile.depend

clean: clean-common

dep: dep-common

install: