/* Define if va_list is actually an array. */
#undef VA_LIST_IS_ARRAY

/* Define if we're using a copy of libwww with built-in SSL support. */
#undef HAVE_LIBWWW_SSL

/* We use this to mark unused variables under GCC. */
#undef ATTR_UNUSED

/* Define this if your C library provides reasonably complete and correct
** Unicode wchar_t support. */
#undef HAVE_UNICODE_WCHAR

/* The kind of system we're allegedly running on.  Used for diagnostics. */
#undef XMLRPC_HOST_TYPE

/* The path separator used by the host operating system. */
#undef PATH_SEPARATOR

/* The directory containing our source code.  This is used by some of
** the build-time test suites to locate their test data (because 
** autoconf may compile our code in a separate build directory. */
#undef TOP_SRCDIR
