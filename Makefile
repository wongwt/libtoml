CC ?= cc
AR ?= ar
ARFLAGS = rcs
RM ?= rm -f

CFLAGS ?= -std=c99 -O2 -DNDEBUG -Wall -Wextra -Werror

srcdir = src

SRCS := $(wildcard $(srcdir)/*.c)
HDRS := $(wildcard $(srcdir)/*.h)
OBJS := $(SRCS:.c=.o)
DBGOBJS := $(SRCS:.c=.dbg.o)

LIB := libtoml.a
DBGLIB := libtoml-dbg.a

.PHONY: all
all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(DBGLIB): CFLAGS = -std=c99 -O0 -g3 -Wall -Wextra -Werror
$(DBGLIB): $(DBGOBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

%.dbg.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(srcdir)/*.o $(srcdir)/*.dbg.o $(LIB) $(DBGLIB)
