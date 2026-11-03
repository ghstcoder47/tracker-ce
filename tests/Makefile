ROOT_DIR := ..
BUILD_DIR := build
TARGET := $(BUILD_DIR)/file-transfer-copy-test
HOST_CXX ?= c++

SOURCES := fileTransferCopyTest.cpp \
	$(ROOT_DIR)/core/FileManager/fileTransferCopy.cpp
INCLUDES := -I$(ROOT_DIR)/core/include -I$(ROOT_DIR)/core/FileManager
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -pedantic

.PHONY: file-transfer-copy clean

file-transfer-copy: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(HOST_CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

clean:
	rm -rf $(BUILD_DIR)
