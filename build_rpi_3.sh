
#execute in a MSYS2-MINGW32 shell, with arm-none-eabi-gcc already in path.
#export PATH=$PATH:/h/Qt/gcc-arm-none-eabi-8-2018-q4-major-win32/bin

if [ "$1" == "" ]; then
	#/h/Qt/gcc-arm-none-eabi-8-2018-q4-major-win32/lib/gcc/arm-none-eabi/8.2.1/include
    echo "Usage: build_rpi_3.sh <path to stddef.h>"
	exit
fi

export HOMEDIR=$PWD

cd $HOMEDIR/libs/circle-stdlib
./configure -r 3 -s $1
make

cd $HOMEDIR
mkdir build
cd build
cmake -D CMAKE_SYSTEM_NAME=circle ../
make

 



