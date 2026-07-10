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
SANTESTBINS := $(TESTS:.c=.san)
RELTESTBINS := $(TESTS:.c=.rel)

IS_CLANG := $(shell $(CC) --version 2>/dev/null | grep -qi clang && echo 1)
SANFLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all
ifeq ($(IS_CLANG),1)
SANFLAGS += -fsanitize=integer
endif

CPPCHECK ?= cppcheck
VALGRIND ?= valgrind

# $(1): List of test suites
# $(2): Optional command prefix
define run_tests
	failures=0; \
	for bin in $(1); do \
		echo "-- $$bin --"; \
		if ! $(2) ./$$bin; then \
			failures=$$((failures + 1)); \
		fi; \
	done; \
	if [ $$failures -gt 0 ]; then \
		echo "$$failures test binary(ies) failed"; \
		exit 1; \
	fi
endef

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
	@$(call run_tests,$(TESTBINS))

$(testdir)/test_%: $(testdir)/test_%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: test-cppcheck
test-cppcheck:
	@$(CPPCHECK) --enable=warning,style,performance,portability --error-exitcode=1 \
		--suppressions-list=cppcheck-suppressions.txt --inline-suppr \
		$(srcdir)

.PHONY: test-sanitize
test-sanitize: CFLAGS = -std=c99 -O0 -g3 -Wall -Wextra -Werror $(SANFLAGS)
test-sanitize: $(SANTESTBINS)
	@$(call run_tests,$(SANTESTBINS),ASAN_OPTIONS=detect_stack_use_after_return=1)

$(testdir)/test_%.san: $(testdir)/test_%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: test-valgrind
test-valgrind: CFLAGS = -std=c99 -O0 -g3 -Wall -Wextra -Werror
test-valgrind: $(TESTBINS)
	@$(call run_tests,$(TESTBINS),$(VALGRIND) --error-exitcode=1 --leak-check=full)

.PHONY: test-release
test-release: $(RELTESTBINS)
	@$(call run_tests,$(RELTESTBINS))

$(testdir)/test_%.rel: $(testdir)/test_%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	$(RM) -r $(srcdir)/*.o $(srcdir)/*.dbg.o $(LIB) $(DBGLIB)
	$(RM) -r $(TESTBINS) $(SANTESTBINS) $(RELTESTBINS) $(testdir)/*.dSYM
