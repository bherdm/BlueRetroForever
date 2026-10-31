all: console_cmd_test.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/console_cmd_test.o
DEPS := $(OBJS:.o=.d)

console_cmd_test.z64: N64_ROM_TITLE = "BRF Commands"

$(BUILD_DIR)/console_cmd_test.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
