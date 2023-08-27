EXE := main
SRC_DIR := src
BUILD_DIR := build
DEBUG_DIR := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

CFLAGS := -Iinc -MMD -Wall
CFLAGS_DEBUG := -g
CFLAGS_RELEASE := -DNDEBUG -O3

.PHONY: all release clean

all: $(DEBUG_DIR)/$(EXE)
release: $(RELEASE_DIR)/$(EXE)

$(BUILD_DIR):
	# TODO find a better method for generating compile_flags
	@echo -Iinc > compile_flags.txt
	mkdir -p $@

# DEBUG TARGETS
$(DEBUG_DIR): | $(BUILD_DIR)
	mkdir -p $@

OBJ_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) -c $< -o $@

$(DEBUG_DIR)/$(EXE): $(OBJ_DEBUG)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(OBJ_DEBUG:.o=.d)

# RELEASE TARGETS
$(RELEASE_DIR): | $(BUILD_DIR)
	mkdir -p $@

OBJ_RELEASE := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) $(CFLAGS_RELEASE) -c $< -o $@

$(RELEASE_DIR)/$(EXE): $(OBJ_RELEASE)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) compile_flags.txt

-include $(OBJ_RELEASE:.o=.d)
