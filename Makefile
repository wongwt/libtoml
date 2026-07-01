BIN        := libtoml
CC         ?= cc
CFLAGS     ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS   += -MMD -MP -Isrc
LDFLAGS    ?=

prefix     ?= /usr/local
DESTDIR    ?=
libdir     := $(prefix)/lib
includedir := $(prefix)/include

BUILD_DIR  := build
SRC_DIR    := src
TEST_DIR   := test

SRCS       := $(wildcard $(SRC_DIR)/*.c)
OBJS       := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/$(SRC_DIR)/%.o)
PIC_OBJS   := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/$(SRC_DIR)/%.pic.o)
LIB        := $(BUILD_DIR)/$(BIN).a
SHLIB      := $(BUILD_DIR)/$(BIN).so

TEST_SRCS  := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS  := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/$(TEST_DIR)/%.o)
TEST_BINS  := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/$(TEST_DIR)/%)

DEPS := $(OBJS:.o=.d) $(PIC_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all
all: lib

.PHONY: lib
lib: $(LIB) $(SHLIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

$(SHLIB): $(PIC_OBJS)
	$(CC) -shared $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/$(SRC_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(SRC_DIR)/%.pic.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/$(SRC_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

.PHONY: install
install: lib
	install -d $(DESTDIR)$(includedir)
	install -m 644 $(SRC_DIR)/toml.h $(DESTDIR)$(includedir)/
	install -d $(DESTDIR)$(libdir)
	install -m 644 $(LIB) $(DESTDIR)$(libdir)/
	install -m 755 $(SHLIB) $(DESTDIR)$(libdir)/

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(includedir)/toml.h
	rm -f $(DESTDIR)$(libdir)/$(BIN).a
	rm -f $(DESTDIR)$(libdir)/$(BIN).so

.PHONY: test
test: $(TEST_BINS)

$(BUILD_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)/$(TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -c $< -o $@

$(BUILD_DIR)/$(TEST_DIR)/%: $(BUILD_DIR)/$(TEST_DIR)/%.o
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

$(BUILD_DIR)/$(SRC_DIR) $(BUILD_DIR)/$(TEST_DIR):
	mkdir -p $@

.PHONY: check
check: test
	@status=0; \
	for bin in $(TEST_BINS); do \
		echo "Running $$bin"; \
		"./$$bin" || status=1; \
	done; \
	exit $$status

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
