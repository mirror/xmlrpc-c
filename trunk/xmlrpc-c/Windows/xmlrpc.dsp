# Microsoft Developer Studio Project File - Name="xmlrpc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=xmlrpc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc.mak" CFG="xmlrpc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xmlrpc - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "xmlrpc - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "xmlrpc"
# PROP Scc_LocalPath ".."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\bin\Release\xmlrpc"
# PROP Intermediate_Dir "..\bin\Release\xmlrpc"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../" /I "../lib/expat/xmlparse" /I "../lib/w3c-libwww-5.3.2/Library/src" /I "../lib/abyss/src" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /YX /FD /c
# SUBTRACT CPP /X
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\xmlrpc.lib"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\bin\Debug\xmlrpc"
# PROP Intermediate_Dir "..\bin\Debug\xmlrpc"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../" /I "../lib/expat/xmlparse" /I "../lib/w3c-libwww-5.3.2/Library/src" /I "../lib/abyss/src" /D "_DEBUG" /D "HAVE_WIN32_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\xmlrpcD.lib"

!ENDIF 

# Begin Target

# Name "xmlrpc - Win32 Release"
# Name "xmlrpc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\xmlrpc_abyss.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

# SUBTRACT CPP /X /u

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_base64.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_client.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_data.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_expat.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_parse.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_registry.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_serialize.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_struct.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_support.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\xmlrpc.h
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_abyss.h
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_client.h
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_expat.h
# End Source File
# Begin Source File

SOURCE=..\src\XmlRpcCpp.h
# End Source File
# End Group
# End Target
# End Project
