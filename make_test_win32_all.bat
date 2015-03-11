@echo off

rem compile and build library
if EXIST obj goto skip_obj
MKDIR obj
:skip_obj
cl.exe @make\cl_libsoem.rsp /errorReport:prompt
if EXIST lib\win32 goto skip_lib
MKDIR lib\win32
:skip_lib
lib.exe @make\lib_libsoem.rsp /nologo /errorReport:prompt

rem compile and build test applications
if EXIST test\win32\slaveinfo\obj goto skip_obj
MKDIR test\win32\slaveinfo\obj
:skip_obj
cl.exe @make\cl_slaveinfo.rsp /errorReport:prompt
link.exe @make\link_slaveinfo.rsp /nologo /errorReport:prompt

if EXIST test\win32\simple_test\obj goto skip_obj2
MKDIR test\win32\simple_test\obj
:skip_obj2
cl.exe @make\cl_simple_test.rsp /errorReport:prompt
link.exe @make\link_simple_test.rsp /nologo /errorReport:prompt

if EXIST test\win32\eepromtool\obj goto skip_obj3
MKDIR test\win32\eepromtool\obj
:skip_obj3
cl.exe @make\cl_eepromtool.rsp /errorReport:prompt
link.exe @make\link_eepromtool.rsp /nologo /errorReport:prompt

echo make done
goto :eof
