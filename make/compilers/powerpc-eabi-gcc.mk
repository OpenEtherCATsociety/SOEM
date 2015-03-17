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
# Copyright (C) 2006. rt-labs AB, Sweden. All rights reserved.
#------------------------------------------------------------------------------
# $Id: powerpc-eabi-gcc.mk 125 2012-04-01 17:36:17Z rtlaka $
#------------------------------------------------------------------------------

# Prefix of the cross-compiler
CROSS_GCC := powerpc-eabi

# Common settings
include $(PRJ_ROOT)/make/compilers/gcc.mk

# Machine settings
-include $(PRJ_ROOT)/make/compilers/$(ARCH).mk

# Default machine settings
# mcpu 750 use an 603,604 specific instuction stfiwx
# this inst is not implemnted OK in QEMU, cause a program expection 7
# mcpu=powerpc is a generic type used for now
#MACHINE ?= -mbig -mregnames -mcpu=750 -mcall-sysv -meabi
MACHINE ?= -mbig -mregnames -mcpu=powerpc -mcall-sysv -meabi

# Compiler flags
CFLAGS  += $(MACHINE) -fshort-wchar
LDFLAGS += $(MACHINE)

