/* We use pragma statements to tell the linker what we need to link
   with.  Since Curl requires Winsock, Winmm, and libcurl, and no other
   project does, we include this file into the Curl transport source code
   to tell the linker to add these libs.
   
   Alternatively, the USER can add the libraries to LINK with as
   NEEDED!
*/

#ifdef _DEBUG
#pragma comment( lib, "../lib/libcurld.lib" )
#else
#pragma comment( lib, "../lib/libcurl.lib" )
#endif

#pragma comment( lib, "Winmm.lib" )
#pragma comment( lib, "Ws2_32.lib" )
