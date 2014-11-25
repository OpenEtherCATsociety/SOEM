#******************************************************************************
#                *          ***                    ***
#              ***          ***                    ***
# ***  ****  **********     ***        *****       ***  ****          *****
# *********  **********     ***      *********     ************     *********
# ****         ***          ***              ***   ***       ****   ***
# ***          ***  ******  ***      ***********   ***        ****   *****
# ***          ***  ******  ***    *************   ***        ****      *****
# ***          ****         ****   ***       ***   ***       ****          ***
# ***           *******      ***** **************  *************    *********
# ***             *****        ***   *******   **  **  ******         *****
#                           t h e  r e a l t i m e  t a r g e t  e x p e r t s
#
# http://www.rt-labs.com
# Copyright (C) 2007. rt-labs AB, Sweden. All rights reserved.
#------------------------------------------------------------------------------
# $Id: setup.sh 125 2012-04-01 17:36:17Z rtlaka $
#------------------------------------------------------------------------------

arch=$1
bsp=$2

case $arch in
   
arm9e|lpc21xx|lpc23xx|lpc31xx|lpc32xx|at91|stm32|omapl1xx|efm32| \
	imx53|kinetis|imx28|am37xx|lpc17xx)
      export CROSS_GCC=arm-eabi
      ;;
   bfin)
      export CROSS_GCC=bfin-elf 
      ;;
   ppc604)
      export CROSS_GCC=powerpc-eabi 
      ;;
   mpc55xx|mpc551x)
      export CROSS_GCC=powerpc-eabispe 
      ;;
   linux)
      bsp=$arch
      export CROSS_GCC=linux
      ;;
   win32)
      ;;

   *)
      echo "Unknown architecture $arch"
      ;;
esac

export PRJ_ROOT=`pwd`
if [ "$arch" == "linux" ]; then
export GCC_PATH=${COMPILERS:-/usr/bin}
else
export GCC_PATH=${COMPILERS:-/opt/rt-tools/compilers}/$CROSS_GCC
fi
export ARCH=$arch
export BSP=$bsp

# Set path for binaries
export PATH=$GCC_PATH/bin:$PATH


