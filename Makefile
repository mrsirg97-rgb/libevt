CC := gcc
CFLAGS = -Wall -Wextra -Iinclude -O3 -g
LDLIBS := -lpthread

AR := ar
ARFLAGS := rcs

SRC_DIR   := src
TEST_DIR  := tests
OBJ_DIR := build
BIN_DIR := bin
LIB_DIR := lib

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
TESTS := $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(wildcard $(TEST_DIR)/*.c))

LIB := $(LIB_DIR)/libevt.a

all: $(LIB)

$(LIB): $(OBJS) | $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $(filter %.o,$^)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -levt $(LDLIBS) -o $@

$(OBJ_DIR) $(BIN_DIR) $(LIB_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG" all

tsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -Wextra -Iinclude -O1 -g -fsanitize=thread" LDLIBS="-lpthread -fsanitize=thread" tests

tests: $(TESTS)
	@echo "running tests..."
	@for t in $(TESTS); do \
		echo "→ $$t"; \
		$$t || exit 1; \
	done

.PHONY: all clean debug tsan tests
