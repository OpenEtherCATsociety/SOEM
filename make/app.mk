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
# $Id: app.mk 452 2013-02-26 21:02:58Z smf.arthur $
#------------------------------------------------------------------------------

OBJDIR = obj/$(ARCH)
LIBS += oshw osal soem

include $(PRJ_ROOT)/make/rules.mk
include $(PRJ_ROOT)/make/files.mk

ifeq ($(ARCH),linux)
LIBS += -lpthread -lrt
endif

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

# Use .PHONY so link step always occurs. This is a simple way
# to avoid computing dependencies on libs
.PHONY: $(APPNAME)
$(APPNAME): $(OBJDIR) $(SUBDIRS) $(OBJDIR_OBJS) $(EXTRA_OBJS)
	@echo --- Linking $@
	$(SILENT)$(CC) $(LDFLAGS) $(LD_PATHS) $(OBJDIR)/*.o -o $@ $(LIBS)

$(OBJDIR):
	@test -e $(OBJDIR) || $(MKDIR) $(OBJDIR)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	@echo --- Entering $(CURDIR)/$@
	@$(MAKE) -C $@ $(MAKECMDGOALS)
	@echo --- Leaving $(CURDIR)/$@

.PHONY: clean
clean: $(SUBDIRS)
	@$(RM) $(OBJDIR)/*
	rm -rf obj/
	@$(RM) $(APPNAME)
	@$(RM) $(APPNAME).elf 
	@$(RM) $(APPNAME).map

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDS)
endif
