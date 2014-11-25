REM @echo off

rem give path as arg
if "%~1"=="" goto exit_err_arg
if NOT EXIST %1 goto exit_err_arg

rem call cvarsall to load the env
call "%~1\vcvarsall.bat" %2

rem cd to folder containing .bat file
cd /d "%0\.."

rem compile and build library
if EXIST obj goto skip_obj
MKDIR obj
:skip_obj
cl.exe @make\cl_libsoem.rsp /errorReport:prompt
if EXIST lib\win32 goto skip_lib
MKDIR lib\win32
:skip_lib
lib.exe @make\lib_libsoem.rsp /nologo /errorReport:prompt

echo make done
goto :eof

:exit_err_arg
echo supply path to MSVC folder that contain vcvarsall.bat as ARG to batch file and ARCH
echo "Ex. make_libsoem_lib.bat "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC" x86
goto :eof

:usage
echo Error in script usage. The correct usage is:
echo     %0 [option]
echo where [option] is: x86 ^| ia64 ^| amd64 ^| x86_amd64 ^| x86_ia64
echo:
echo For example:
echo     %0 x86_ia64
goto :eof


