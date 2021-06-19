-include mk.conf
#CFLAGS+=-Wall -Werror

SRC = gstthetauvc.c gstthetauvcsrc.c thetauvc.c

PKG_CONFIGS= gstreamer-1.0 gstreamer-base-1.0 libuvc
ifdef WITH_TRANSFORM_FILTER
PKG_CONFIGS += gstreamer-gl-1.0
CFLAGS += -DWITH_TRANSFORM_FILTER
SRC += gstthetatransform.c gstglutils.c
endif

CFLAGS += -g -Og
OBJDIR=./obj
DEPDIR=./.depend
OBJ := $(addprefix $(OBJDIR)/,$(SRC:.c=.o))
CFLAGS += $(shell pkg-config --cflags ${PKG_CONFIGS})
LDFLAGS += $(shell pkg-config --libs ${PKG_CONFIGS})

vpath %.o $(OBJDIR)

all: $(OBJDIR) gstthetauvc.so

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

gstthetauvc.so: $(OBJ)
	$(CC) -shared -o $@ $^  $(LDFLAGS)


.PHONY: clean depend
clean:
	$(RM) -r $(OBJDIR) *.o *.so

mostlyclean: clean
	$(RM) -r *~ $(DEPDIR)

$(DEPDIR):
	mkdir $(DEPDIR)

depend: $(DEPDIR)
	for f in $(SRC); do \
		$(CC) $(CFLAGS) -c -MM -o $(DEPDIR)/$${f%.c}.d -MT $(OBJDIR)/$${f%.c}.o $$f ;\
	done

-include $(DEPDIR)/*.d
