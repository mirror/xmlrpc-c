#
# Makefile
#
# This file is part of the ABYSS Web server project.
#
# Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

# Change the following lines to meet your system configuration

# If you are using a SystemV-based operating system, such as
# Solaris, you will need to uncomment these two lines.
#SYSV_LIBS=-lnsl -lsocket
#DEFS=-DBSD_COMP

#If you want the fork system, leave the two following lines uncommented
CC=gcc -D_UNIX -D_FORK $(DEFS)
LIB=

#If you want the threading system, uncomment the two following lines
#Note: The threading system is still unreliable and under developpement
#CC=cc -D_REENTRANT -D_UNIX -D_THREAD $(DEFS)
#LIB=-lpthread

#CFLAGS=-g
CFLAGS=-O

#Do not change the lines below

BIN=../bin/abyss
OBJ=conf.o data.o trace.o file.o main.o thread.o http.o socket.o server.o conn.o

all: $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LIB) $(SYSV_LIBS)

clean:
	$(RM) core *.o $(BIN)
