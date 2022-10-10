PKGDIR	?= ../..
L4DIR		?= $(PKGDIR)/../l4re/src/l4

TARGET	= $(PKGNAME)

# list your .c or .cc files here
SRC_C		=
SRC_CC  = main.cc dataspace.cc simple-l4re-allocator.cc

# list requirements of your program here
REQUIRES_LIBS   = stdlibs libstdc++

include $(L4DIR)/mk/prog.mk
