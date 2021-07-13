ifneq (exec,$(notdir $(patsubst %/,%,$(dir $(CURDIR)))))
include $(CURDIR)/../mk/target.mk
else

include $(ROOTDIR)/mk/variables.mk

INCDIR := $(ROOTDIR)/src

$(warning ROOTDIR=$(ROOTDIR))
$(warning CURDIR=$(CURDIR))
$(warning SRCDIR=$(SRCDIR))
$(warning OBJDIR=$(OBJDIR))
$(warning INCDIR=$(INCDIR))
$(warning MAKECMDGOALS=$(MAKECMDGOALS))

VPATH = $(SRCDIR)

CDEFS = -DVERSION="$(VERSION)" -DPKGNAME="$(PKGNAME)" -DPKGMAINT="$(PKGMAINT)" -DARCH="$(TARGET_ARCH_TYPE)"
CXXFLAGS = $(CDEFS) -D__STDC_LIMIT_MACROS -std=gnu++17 -I$(INCDIR) -Wall -Werror -g
CFLAGS = $(CDEFS) -std=gnu17 -I$(INCDIR) -Wall -Werror -g
LDFLAGS =
LDLIBS =

.PHONY: exec
exec: $(PKGNAME)

$(PKGNAME): $(PKGNAME).o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(PKGNAME).o: $(PKGNAME).c



endif 
