@echo off

if NOT EXIST obj goto skip_obj
RMDIR /S /Q obj
:skip_obj
if NOT EXIST lib\win32 goto skip_lib
RMDIR /S /Q lib\win32
:skip_lib

echo clean done