# SPDX-License-Identifier: MIT

include ../config.mk
include ../libversion.mk

SRCFILES := $(wildcard *.c)
TARGETS = $(SRCFILES:%.c=$(O_DIR)/%)
DEPFILES = $(SRCFILES:%.c=$(O_DIR)/%.d)
COMMANDS = evl
HELPER_SCRIPTS = $(wildcard *.evl)
HELPER_PROGRAMS = ps

CMD_CPPFLAGS := $(BASE_CPPFLAGS) -I. -I../include -I$(O_DIR)/../include
CMD_CFLAGS := $(CMD_CPPFLAGS) $(BASE_CFLAGS)
override CFLAGS := $(CMD_CFLAGS) $(CFLAGS)

CMD_LDFLAGS := $(O_DIR)/../lib/libevl.so.$(EVL_IVERSION) -lpthread -lrt
override LDFLAGS := $(CMD_LDFLAGS) $(LDFLAGS)

$(TARGETS):

all: output-Makefile $(TARGETS)

install: all
	$(call inst-cmd,utilities,						\
		for bin in $(COMMANDS); do					\
		$(INSTALL) -D $(O_DIR)/$$bin $(DESTDIR)/$(bindir)/$$bin; done)
	$(call inst-cmd,command-helpers,					\
		for hlp in $(HELPER_SCRIPTS); do				\
		name=evl-$$(basename $$hlp .evl);				\
		$(INSTALL) -D $$hlp $(DESTDIR)/$(libexecdir)/$$name; done;	\
		for hlp in $(HELPER_PROGRAMS); do				\
		name=evl-$$hlp;							\
		$(INSTALL) -D $(O_DIR)/$$hlp $(DESTDIR)/$(libexecdir)/$$name; done)

clean clobber mrproper: output-Makefile
	$(Q)$(RM) -f $(TARGETS) $(DEPFILES)

$(O_DIR)/%: %.c
	$(call ccld-cmd,$@,$(Q)$(CC) -o $(@) $< $(CFLAGS) $(LDFLAGS))

-include $(DEPFILES)
