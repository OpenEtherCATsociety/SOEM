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
# $Id: subdir.mk 125 2012-04-01 17:36:17Z rtlaka $
#------------------------------------------------------------------------------

OBJDIR ?= ../obj/$(ARCH)

include $(PRJ_ROOT)/make/rules.mk
include $(PRJ_ROOT)/make/files.mk

SUBDIRS ?= $(patsubst %/,%,$(dir $(wildcard */Makefile)))
LATEDIRS ?= $(patsubst %/,%,$(dir $(wildcard */.late)))
EARLYDIRS ?= $(filter-out $(LATEDIRS), $(SUBDIRS))
ALLDIRS = $(EARLYDIRS) $(LATEDIRS)

ifneq ($(strip $(OBJS)),)
all: $(OBJDIR) $(ALLDIRS) $(OBJDIR_OBJS)
else
all: $(ALLDIRS) $(OBJDIR_OBJS)
endif

$(LATEDIRS): $(EARLYDIRS)

$(OBJDIR):
	@test -e $(OBJDIR) || $(MKDIR) $(OBJDIR)

.PHONY: $(ALLDIRS)
$(ALLDIRS):
	@echo --- Entering $(CURDIR)/$@
	@$(MAKE) --no-print-directory -C $@ $(MAKECMDGOALS)
	@echo --- Leaving $(CURDIR)/$@

.PHONY: clean
clean: $(ALLDIRS)
	$(RM) $(OBJDIR)/*

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDS)
endif

