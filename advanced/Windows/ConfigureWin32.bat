@echo off
echo creating Win32 header files...
copy .\win32_config.h ..\include\xmlrpc-c\config.h
copy .\xmlrpc_win32_config.h ..\xmlrpc_config.h
copy .\transport_config_win32.h ..\transport_config.h
echo completed creating win32 header files.
pause
