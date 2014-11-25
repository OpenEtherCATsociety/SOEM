@echo off

if EXIST test\win32\status_test\obj goto skip_obj2
MKDIR test\win32\status_test\obj
:skip_obj2
cl.exe @make\cl_status_test.rsp /errorReport:prompt
link.exe @make\link_status_test.rsp /nologo /errorReport:prompt

echo make done
goto :eof
