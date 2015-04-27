# einbus - a unix bus
# See COPYING for copyright and license details.

NAME = einbus
VERSION = 0.5

APPSRCDIR = appsrc
LIBSRCDIR = libsrc
EXSRCDIR = examples

APPOBJDIR = appobj
LIBOBJDIR = libobj
EXOBJDIR = examplesobj

APPOUTDIR = appout
LIBOUTDIR = libout
EXOUTDIR = examplesout

INCDIR = include


APPDIRS = $(APPOUTDIR) $(APPOBJDIR)
LIBDIRS = $(LIBOUTDIR) $(LIBOBJDIR)
EXDIRS = $(EXOUTDIR) $(EXOBJDIR)
DIRS = $(APPDIRS) $(LIBDIRS) $(EXDIRS)


CFLAGS += -std=gnu99 -pedantic -Wall -I$(INCDIR)

ifeq (1,$(EINBUS_DEBUG))
CFLAGS += -DEINBUS_DEBUG -g -Wextra
else
CFLAGS += -O2
endif

LIBCFLAGS := $(CFLAGS) -fPIC
LIBLDFLAGS := $(LDFLAGS) -shared

LDFLAGS += -leinbus -levent -Llibout/

APPSOURCES = $(wildcard $(APPSRCDIR)/*.c)
LIBSOURCES = $(wildcard $(LIBSRCDIR)/*.c)
EXSOURCES = $(wildcard $(EXSRCDIR)/*.c)

APPOBJECTS = $(patsubst $(APPSRCDIR)/%.c,$(APPOBJDIR)/%.o,$(APPSOURCES))
LIBOBJECTS = $(patsubst $(LIBSRCDIR)/%.c,$(LIBOBJDIR)/%.o,$(LIBSOURCES))
EXOBJECTS = $(patsubst $(EXSRCDIR)/%.c,$(EXOBJDIR)/%.o,$(EXSOURCES))

APPBINS = $(patsubst %,$(APPOUTDIR)/$(NAME)-%,allinone connect invoke relay signal terminal)
LIBBIN = $(LIBOUTDIR)/lib$(NAME).so
EXBINS = $(patsubst $(EXSRCDIR)/%.c,$(EXOUTDIR)/%,$(wildcard $(EXSRCDIR)/*.c))


.PHONY: all clean default apps lib examples

default: lib

all: lib apps examples

apps: $(APPBINS)

lib: $(LIBBIN)

examples: lib $(EXBINS)

#	$(foreach var,$(EXBINS),$(CC) $(EXSRCDIR)/$(var).c $(LDFLAGS) $(CFLAGS) -o $(EXOUTDIR)/$(var); )

$(LIBBIN): % : %.$(VERSION)
# TODO -f should really not be necessary, why is the recipe run, when the file exists?
	cd $(LIBOUTDIR) ; ln -sf $(patsubst $(LIBOUTDIR)/%,%,$^) $(patsubst $(LIBOUTDIR)/%,%,$@)


$(APPBINS): $(APPOUTDIR)/% : $(APPOBJDIR)/%.o | $(APPOUTDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(LIBBIN).$(VERSION): $(LIBOBJECTS) | $(LIBOUTDIR)
	$(CC) $^ -o $@ $(LIBLDFLAGS)
	chmod 755 $@

$(EXBINS): $(EXOUTDIR)/% : $(EXOBJDIR)/%.o | $(EXOUTDIR)
	$(CC) $^ -o $@ $(LDFLAGS)


$(APPOBJECTS): $(APPOBJDIR)/%.o : $(APPSRCDIR)/%.c | $(APPOBJDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(LIBOBJECTS): $(LIBOBJDIR)/%.o : $(LIBSRCDIR)/%.c | $(LIBOBJDIR)
	$(CC) -c $< -o $@ $(LIBCFLAGS)

$(EXOBJECTS): $(EXOBJDIR)/%.o : $(EXSRCDIR)/%.c | $(EXOBJDIR)
	$(CC) -c $< -o $@ $(CFLAGS)


$(DIRS):
	mkdir -p $@

clean::
	rm -rf $(DIRS)

#ar rcs lib$(NAME).a ${OBJ}
