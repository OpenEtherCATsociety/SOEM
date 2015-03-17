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
# $Id: bfin-elf-gcc.mk 125 2012-04-01 17:36:17Z rtlaka $
#------------------------------------------------------------------------------

# Prefix of the cross-compiler
CROSS_GCC := bfin-elf

# Common settings
include $(PRJ_ROOT)/make/compilers/gcc.mk

# Machine settings
-include $(PRJ_ROOT)/make/compilers/$(ARCH).mk

# Default machine settings
MACHINE ?= -mcpu=bf537

# Compiler flags
CFLAGS  += $(MACHINE) -fshort-wchar
LDFLAGS += $(MACHINE)
