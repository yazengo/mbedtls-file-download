# Makefile for HTTPS Download Library
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -D_POSIX_C_SOURCE=199309L
LDFLAGS = -lmbedtls -lmbedx509 -lmbedcrypto -lrt

# Directories
SRCDIR = .
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = system_abstraction_linux.c https_download.c
TEST_SOURCES = test_download.c
TOOL_SOURCES = download_tool.c
HEADERS = system_abstraction.h https_download.h

# Object files
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:%.c=$(OBJDIR)/%.o)
TOOL_OBJECTS = $(TOOL_SOURCES:%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = $(BINDIR)/test_download
DOWNLOAD_TOOL = $(BINDIR)/download
LIBRARY = $(BINDIR)/libhttps_download.a

# Default target
all: directories $(LIBRARY) $(TARGET) $(DOWNLOAD_TOOL)

# Create directories
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Compile library
$(LIBRARY): $(OBJECTS)
	@echo "Creating static library..."
	ar rcs $@ $^

# Compile test executable
$(TARGET): $(TEST_OBJECTS) $(LIBRARY)
	@echo "Linking test executable..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile download tool
$(DOWNLOAD_TOOL): $(TOOL_OBJECTS) $(LIBRARY)
	@echo "Linking download tool..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Install mbedtls development files (Ubuntu/Debian)
install-deps:
	@echo "Installing mbedTLS development libraries..."
	sudo apt-get update
	sudo apt-get install -y libmbedtls-dev

# Check if mbedtls is installed
check-deps:
	@echo "Checking mbedTLS installation..."
	@pkg-config --exists mbedtls && echo "mbedTLS found" || (echo "mbedTLS not found. Run 'make install-deps' to install."; exit 1)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJDIR) $(BINDIR)

# Run tests
test: $(TARGET)
	@echo "Running tests..."
	./$(TARGET)

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: all

# Release build
release: CFLAGS += -DNDEBUG -O3
release: all

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build library, test executable and download tool (default)"
	@echo "  $(LIBRARY)   - Build static library only"
	@echo "  $(TARGET)    - Build test executable only"
	@echo "  $(DOWNLOAD_TOOL) - Build download tool only"
	@echo "  install-deps - Install mbedTLS development libraries"
	@echo "  check-deps   - Check if mbedTLS is installed"
	@echo "  clean        - Remove build artifacts"
	@echo "  test         - Build and run tests"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized release version"
	@echo "  help         - Show this help message"

.PHONY: all directories install-deps check-deps clean test debug release help
