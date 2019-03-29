#export ARM_GCC_DIR=/h/Qt/gcc-arm-none-eabi-8-2018-q4-major-win32
#export ARM_GCC_STDDEF_H_DIR=/h/Qt/gcc-arm-none-eabi-8-2018-q4-major-win32/lib/gcc/arm-none-eabi/8.2.1/include

# execute in a MSYS2-MINGW32 shell

export HOMEDIR=$PWD

export PATH=$PATH:$ARM_GCC_DIR/bin

cd $HOMEDIR/libs/circle-stdlib
./configure -r 3 -s $ARM_GCC_STDDEF_H_DIR
make

cd $HOMEDIR
mkdir build
cd build
cmake -D CMAKE_SYSTEM_NAME=circle ../
make

 



