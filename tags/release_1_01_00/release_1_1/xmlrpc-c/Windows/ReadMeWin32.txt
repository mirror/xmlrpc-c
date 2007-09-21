XMLRPC-C 1.0 Win32 WinInet
--------------------------

This release contains significant changes from the 1.0 release of xmlrpc-c for the Windows platform.  Please read this file in its entirety for configuration instructions.  Note that the only tested configuration for the transport layer is WinInet.

To create the three headers required for Win32 WinInet compilation, run the ConfigureWin32.bat found in the Windows directory.  Open the xmlrpc.dsw file in Visual Studio 6 or greater.  The project will convert and work fine in Visual Studio 2003 as well - other versions of Visual Studio were not tested.

Changes from the 1.0 release for Win32:
1) Project files now reflect static linking for the expat XML library.
2) Example projects were created/updated to keep them in sync with the distribution.  The project files
   were moved into the .\Windows directory
3) Projects for the rpc and cpp tests were created.  The xmlrpc_win32_config.h defines the directory for
   the test files relative to the output directory
4) Major refactoring of the Wininet Transport.

Suggested testing for evaluation of the library involves a few projects.  Here is a quick getting started guide:

1) Set the Active Project to query_meerkat and build it in release or debug modes.  The dependent projects will be built automatically.  In the project settings dialog, add the argument for what you wish to query meerkat for - "Windows" is a good query.  Run the project.  This will query the meerkat server for articles related to windows and output the results to the console.

2) Set the Active Project to xmlrpc_sample_add_server and build it in release or debug modes.  The dependent projects will be built automatically.  In the project settings dialog, add the argument for the abyss.conf file as "..\examples\abyss.conf"  This will run the server sample which adds two numbers and returns a result.  You should run this from a command prompt instead of through Visual Studio so you may run the client as well.

3) Set the Active Project to xmlrpc_sample_add_sync_client or xmlrpc_sample_add_async_client and build it in release or debug modes.  The dependent projects will be built automatically.  This will run the client sample which submits two numbers to be added to the server application as described above and displays the result.  Note that the client example comes in the sync and async varieties.

Steven Bone
January 1, 2005
sbone@pobox.com
