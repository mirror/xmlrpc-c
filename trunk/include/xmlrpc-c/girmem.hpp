/*============================================================================
                               girmem.hpp
==============================================================================
  This declares the user interface to memory management facilities (smart
  pointers, basically) in libxmlrpc.  They are used in interfaces to various
  classes in XML For C/C++.
  
  This file includes <windows.h>, and does it with the WIN32_LEAN_AND_MEAN
  switch so as not to interfere with other Xmlrpc-c header files that might
  be included in the same program.  However, this may itself interfere with
  other parts of your program.  If this is a problem, simply include
  <windows.h> before including this file.  (Note that if you do so without
  the WIN32_LEAN_AND_MEAN switch, some Xmlrpc-c header files won't work and
  when your compile fails, you'll have to make a choice).

  This is a sloppy way to deal with this problem.  The clean way, given that
  Microsoft made <windows.h> much too large and screwed up the introduction
  of Winsock2, would be for this file not to include <windows.h> at all and
  simply prerequire the user to do it.  But it's too late to do that, as
  lots of existing programs would stop compiling.
============================================================================*/
#ifndef GIRMEM_HPP_INCLUDED
#define GIRMEM_HPP_INCLUDED

#include <xmlrpc-c/config.h>
#include <xmlrpc-c/c_util.h>

/* The following pthread crap mirrors what is in pthreadx.h, which is
   what girmem.cpp uses to declare the lock interface.  We can't simply
   include pthreadx.h here, because it's an internal Xmlrpc-c header file,
   and this is an external one.

   This is a stopgap measure until we do something cleaner, such as expose
   pthreadx.h as an external interface (class girlock, maybe?) or create
   a lock class with virtual methods.

   The problem we're solving is that class autoObject contains 
   a pthread_mutex_t member, and on Windows, there's no such type.
*/
   
#if XMLRPC_HAVE_PTHREAD
#  include <pthread.h>
   typedef pthread_mutex_t girmem_lock;
#elif XMLRPC_HAVE_WINTHREAD
   /* See above about how this might interfere with the including program */
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
   typedef CRITICAL_SECTION girmem_lock;
#else
#  error No inter-thread locking method found
#endif

/*
  XMLRPC_LIBPP_EXPORTED marks a symbol in this file that is exported from
  libxmlrpc++.

  XMLRPC_BUILDING_LIBPP says this compilation is part of libxmlrpc++, as
  opposed to something that _uses_ libxmlrpc++.
*/
#ifdef XMLRPC_BUILDING_LIBPP
#define XMLRPC_LIBPP_EXPORTED XMLRPC_DLLEXPORT
#else
#define XMLRPC_LIBPP_EXPORTED
#endif

namespace girmem {

class XMLRPC_LIBPP_EXPORTED autoObjectPtr;

class XMLRPC_LIBPP_EXPORTED autoObject {
    friend class autoObjectPtr;

public:
    void incref();
    void decref(bool * const unreferencedP);
    
protected:
    autoObject();
    virtual ~autoObject();

private:
    girmem_lock refcountLock;
    unsigned int refcount;
};

class XMLRPC_LIBPP_EXPORTED autoObjectPtr {
public:
    autoObjectPtr();
    autoObjectPtr(girmem::autoObject * objectP);
    autoObjectPtr(girmem::autoObjectPtr const& autoObjectPtr);
    
    ~autoObjectPtr();
    
    void
    point(girmem::autoObject * const objectP);

    void
    unpoint();

    autoObjectPtr
    operator=(girmem::autoObjectPtr const& objectPtr);
    
    girmem::autoObject *
    operator->() const;
    
    girmem::autoObject *
    get() const;

protected:
    girmem::autoObject * objectP;
};

} // namespace

#endif
