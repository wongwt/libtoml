CC ?= cc
AR ?= ar
ARFLAGS = rcs
RM ?= rm -f

CFLAGS ?= -std=c99 -O2 -DNDEBUG -Wall -Wextra -Werror

srcdir = src
testdir = test

SRCS := $(wildcard $(srcdir)/*.c)
HDRS := $(wildcard $(srcdir)/*.h)
OBJS := $(SRCS:.c=.o)
DBGOBJS := $(SRCS:.c=.dbg.o)

LIB := libtoml.a
DBGLIB := libtoml-dbg.a

TESTS := $(wildcard $(testdir)/test_*.c)
TESTBINS := $(TESTS:.c=)

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

.PHONY: test
test: CFLAGS = -std=c99 -O0 -g3 -Wall -Wextra -Werror
test: $(TESTBINS)
	@failures=0; \
	for bin in $(TESTBINS); do \
		echo "-- $$bin --"; \
		if ! ./$$bin; then \
			failures=$$((failures + 1)); \
		fi; \
	done; \
	if [ $$failures -gt 0 ]; then \
		echo "$$failures test binary(ies) failed"; \
		exit 1; \
	fi

$(testdir)/test_%: $(testdir)/test_%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	$(RM) -r $(srcdir)/*.o $(srcdir)/*.dbg.o $(LIB) $(DBGLIB) $(TESTBINS) $(testdir)/*.dSYM
