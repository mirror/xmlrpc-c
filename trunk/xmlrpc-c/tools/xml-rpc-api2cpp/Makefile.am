# Turn on warnings.
CFLAGS = @CFLAGS@ -Wall

EXTRA_DIST = README

noinst_PROGRAMS = xml-rpc-api2cpp

LIBEXPAT  = ../../lib/expat/xmlparse/libxmlrpc_xmlparse.la \
            ../../lib/expat/xmltok/libxmlrpc_xmltok.la
LIBXMLRPC = ../../src/libxmlrpc.la $(LIBEXPAT)
LIBCLIENT = ../../src/libxmlrpc_client.la @LIBWWW_LDADD@ $(LIBXMLRPC)

xml_rpc_api2cpp_SOURCES = xml-rpc-api2cpp.cc
xml_rpc_api2cpp_LDADD   = ../../src/libxmlrpc_cpp.a $(LIBCLIENT)
xml_rpc_api2cpp_LDFLAGS = @LIBWWW_RPATH@
