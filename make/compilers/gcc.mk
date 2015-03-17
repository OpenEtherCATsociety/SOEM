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
# Copyright (C) 2008. rt-labs AB, Sweden. All rights reserved.
#------------------------------------------------------------------------------
# $Id: gcc.mk 125 2012-04-01 17:36:17Z rtlaka $
#------------------------------------------------------------------------------

# Compiler executables
ifeq ($(ARCH),linux)
CC := $(GCC_PATH)/gcc
AS := $(GCC_PATH)/as
LD := $(GCC_PATH)/ld
AR := $(GCC_PATH)/ar
SIZE := $(GCC_PATH)/size
CPP :=$(CC) -E -xc -P
OBJCOPY := $(GCC_PATH)/objcopy
else
CC := $(GCC_PATH)/bin/$(CROSS_GCC)-gcc
AS := $(GCC_PATH)/bin/$(CROSS_GCC)-as
LD := $(GCC_PATH)/bin/$(CROSS_GCC)-ld
AR := $(GCC_PATH)/bin/$(CROSS_GCC)-ar
SIZE := $(GCC_PATH)/bin/$(CROSS_GCC)-size
CPP :=$(CC) -E -xc -P
OBJCOPY := $(GCC_PATH)/bin/$(CROSS_GCC)-objcopy
LDFLAGS = -nostartfiles -T"$(LD_SCRIPT)"
endif

# Host executables (TODO: move to host-specific settings)
RM := rm -f
MKDIR := mkdir -p

# Include paths
CC_INC_PATH = $(GCC_PATH)/$(CROSS_GCC)/include

# Compiler flags
CFLAGS = -Wall -Wextra -Wno-unused-parameter #-Werror
CFLAGS += -fomit-frame-pointer -fno-strict-aliasing
CFLAGS += -B$(GCC_PATH)/libexec/gcc

# Compiler C++ flags
CPPFLAGS = -fno-rtti -fno-exceptions

# Linker flags
LDFLAGS += -Wl,-Map=$(APPNAME).map

# Libraries
LLIBS = $(patsubst %,-l%,$(LIBS))
LIBS := -Wl,--start-group  $(LLIBS) -lc -lm -Wl,--end-group

# Directories
LIBDIR = "$(PRJ_ROOT)/lib/$(ARCH)"

