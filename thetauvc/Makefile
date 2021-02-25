#CFLAGS+=-Wall -Werror
CFLAGS += -g -Og
OBJDIR=./obj
SRC = gstthetauvc.c gstthetauvcsrc.c thetauvc.c
OBJ := $(addprefix $(OBJDIR)/,$(SRC:.c=.o))
CFLAGS += $(shell pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0 libuvc)
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0 gstreamer-base-1.0 libuvc)

vpath %.o $(OBJDIR)

all: $(OBJDIR) gstthetauvc.so

$(OBJDIR):
	mkdir $(OBJDIR)
#gstthetauvc.o: gstthetauvc.c gstthetauvc.h
#	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

gstthetauvc.so: $(OBJ)
	$(CC) -shared -o $@ $^  $(LDFLAGS)


.PHONY: clean
clean:
	$(RM) -r $(OBJDIR) *~ *.o *.so
