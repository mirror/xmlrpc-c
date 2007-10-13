Build Instructions For XML-RPC For C/C++ On Windows
---------------------------------------------------

Xmlrpc-c release 1.13, December, 2007.

Latest Windows work was done by

  Geoff McLane
  6 October, 2007
  home: http://geoffair.net

The actual Xmlrpc-c release is a little different from what Geoff
tested, and does not actually build on Windows.  But it should take
only a small amount of programming effort to get it to do so.
07.10.10

This is a build of the xmlrpc static libary, and the examples, using
Microsoft Visual C++ 1994-98 (MSVC6), 2003 (MSVC7), and 2005 Express
(MSVC8) in Windows XP SP2. The shared libraries (DLL) are not included
and were NOT built. Also, although the DSP files are included, the
all.vcproj, configwin32.vcproj and cpptest.vcproj were exclude from
the solution.

1. Run the batch file ConfigureWin32.bat, found in the Windows
directory. This will copy four(4) headers to the appropriate folders.

2. Load xmlrpc.dsw in MSVC[6,7,8] or later, and build the Release or Debug
configurations. DLL configurations are not included, and may not compile.

This build requires that you have a Microsoft SDK, or Plaform SDK
installed, since among other things, it uses <http.h>, and
HTTPAPI.LIB, from the SDK.

Once built, the rpctest.exe, in the bin folder, should run with no errors,
and the xmlrpc_sample_add_server.exe, using port 8080, and
xmlrpc_sample_add_sync_client.exe should communicate ... proving 7+5 = 12 ;=))

Have fun.

PS: Several other batch files are included in the Windows folder ...

delsln.bat - to delete all the MSVC7 and 8 solution file.

diffcfg.bat - compare the headers in windows with the version used in
the compile. Requires diff.exe to be in the path.

updcfg.bat - copy the 3 manually maintained configuration files back
to the Windows folder (for distribution).

cleawin32.bat - deletes the headers used in the compile. That is does the
opposite of ConfigureWin32.bat.

cleanall.bat - to remove ALL the binary files created. Requires an xdelete
program which will recursively delete an entire folder.


There is some historical information in ReadMeOld.txt, which used to be
the contents of this file.  Some of it is still valid.


Developing XML-RPC For C/C++ for Windows
----------------------------------------

If you fix or enhance something in the Windows build system, please send
your updates to the Xmlrpc-c maintainer to be included in future releases
so others don't have to repeat your work.

Output of a Subversion 'diff' is usually the best way to send updates,
but you can also send complete files or just a description of the
change if that is easier.

For the project files, we distribute only MSVC6-compatible DSP and DSW
files (which are, of course, usable as input to later versions of MSVC
as well).  That means if you need to modify something in the project
files and you are not using MSVC6, you must edit the project files
manually as text files.  Modifying them via the IDE would simply
generate new files in a format that cannot be used with older MSVC.
