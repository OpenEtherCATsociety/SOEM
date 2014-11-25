@echo off

REM Build slave info
if NOT EXIST test\win32\slaveinfo\obj goto skip_obj
RMDIR /S /Q test\win32\slaveinfo\obj
:skip_obj
if NOT EXIST test\win32\slaveinfo\*.exe goto skip_exe
DEL /Q test\win32\slaveinfo\*.exe
:skip_exe
if NOT EXIST test\win32\slaveinfo\*.ilk goto skip_ilk
DEL /Q test\win32\slaveinfo\*.ilk
:skip_ilk
if NOT EXIST test\win32\slaveinfo\*.pdb goto skip_pdb
DEL /Q test\win32\slaveinfo\*.pdb
:skip_pdb
if NOT EXIST test\win32\slaveinfo\*.idb goto skip_idb
DEL /Q test\win32\slaveinfo\*.idb
:skip_idb

REM Simple_test info
if NOT EXIST test\win32\simple_test\obj goto skip_obj2
RMDIR /S /Q test\win32\simple_test\obj
:skip_obj2
if NOT EXIST test\win32\simple_test\*.exe goto skip_exe2
DEL /Q test\win32\simple_test\*.exe
:skip_exe2
if NOT EXIST test\win32\simple_test\*.ilk goto skip_ilk2
DEL /Q test\win32\simple_test\*.ilk
:skip_ilk2
if NOT EXIST test\win32\simple_test\*.pdb goto skip_pdb2
DEL /Q test\win32\simple_test\*.pdb
:skip_pdb2
if NOT EXIST test\win32\simple_test\*.idb goto skip_idb2
DEL /Q test\win32\simple_test\*.idb
:skip_idb2

REM eepromtool info
if NOT EXIST test\win32\eepromtool\obj goto skip_obj3
RMDIR /S /Q test\win32\eepromtool\obj
:skip_obj3
if NOT EXIST test\win32\eepromtool\*.exe goto skip_exe3
DEL /Q test\win32\eepromtool\*.exe
:skip_exe3
if NOT EXIST test\win32\eepromtool\*.ilk goto skip_ilk3
DEL /Q test\win32\eepromtool\*.ilk
:skip_ilk3
if NOT EXIST test\win32\eepromtool\*.pdb goto skip_pdb3
DEL /Q test\win32\eepromtool\*.pdb
:skip_pdb3
if NOT EXIST test\win32\eepromtool\*.idb goto skip_idb3
DEL /Q test\win32\eepromtool\*.idb
:skip_idb3

echo clean done