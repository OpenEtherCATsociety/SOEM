#
# Makefile
#

CIRCLEHOME =   ../../../libs/circle-stdlib/libs/circle
NEWLIBDIR =    ../../../libs/circle-stdlib/install/arm-none-circle
SOEMBUILDDIR = ../../../build
SOEMSRCDIR =   ../../../

OBJS	= main.o kernel.o

include $(CIRCLEHOME)/Rules.mk

CFLAGS += -I "$(NEWLIBDIR)/include" -I $(STDDEF_INCPATH) \
          -I $(SOEMSRCDIR)/soem \
          -I $(SOEMSRCDIR)/osal -I $(SOEMSRCDIR)/osal/circle \
          -I $(SOEMSRCDIR)/oshw -I $(SOEMSRCDIR)/oshw/circle
LIBS := $(SOEMBUILDDIR)/libsoem.a \
    "$(NEWLIBDIR)/lib/libm.a" "$(NEWLIBDIR)/lib/libc.a" "$(NEWLIBDIR)/lib/libcirclenewlib.a" \
 	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
    $(CIRCLEHOME)/lib/usb/libusb.a \
 	$(CIRCLEHOME)/lib/input/libinput.a \
 	$(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
 	$(CIRCLEHOME)/lib/fs/libfs.a \
  	$(CIRCLEHOME)/lib/net/libnet.a \
  	$(CIRCLEHOME)/lib/sched/libsched.a \
  	$(CIRCLEHOME)/lib/libcircle.a
