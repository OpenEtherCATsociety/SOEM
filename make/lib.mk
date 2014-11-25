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
# $Id: lib.mk 452 2013-02-26 21:02:58Z smf.arthur $
#------------------------------------------------------------------------------

OBJDIR = ./obj/$(ARCH)

include $(PRJ_ROOT)/make/rules.mk
include $(PRJ_ROOT)/make/files.mk

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

all: lib$(LIBNAME).a

lib$(LIBNAME).a: $(OBJDIR) $(SUBDIRS) $(OBJDIR_OBJS)
	@echo --- Create library $@
	@test -e $(LIBDIR) || $(MKDIR) $(LIBDIR)
	$(SILENT)$(AR) crus $(LIBDIR)/$@ $(OBJDIR)/*.o

$(OBJDIR):
	@test -e $(OBJDIR) || $(MKDIR) $(OBJDIR)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	@echo --- Entering $(CURDIR)/$@
	@$(MAKE) -C $@ $(MAKECMDGOALS)
	@echo --- Leaving $(CURDIR)/$@

.PHONY: clean
clean: $(SUBDIRS)
	$(RM) $(OBJDIR)/*
	rm -rf ./obj
	$(RM) $(LIBDIR)/lib$(LIBNAME).a

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDS)
endif
