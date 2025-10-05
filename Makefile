# Makefile for libperfmon - Performance Monitoring Library

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c99
LDFLAGS = -shared

# Library name
LIB_NAME = libperfmon
LIB_STATIC = $(LIB_NAME).a
LIB_SHARED = $(LIB_NAME).so
LIB_VERSION = 1.0.0
LIB_SHARED_FULL = $(LIB_SHARED).$(LIB_VERSION)

# Installation directories
PREFIX ?= /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

# Source files
SOURCES = perfmon.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = perfmon.h

# Examples
EXAMPLES = example_simple 
EXAMPLE_OBJECTS = $(EXAMPLES:=.o)

# Default target
all: $(LIB_STATIC) $(LIB_SHARED) examples

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(LIB_STATIC): $(OBJECTS)
	$(AR) rcs $@ $^
	@echo "Built static library: $(LIB_STATIC)"

# Build shared library
$(LIB_SHARED): $(OBJECTS)
	$(CC) $(LDFLAGS) -Wl,-soname,$(LIB_SHARED).1 -o $(LIB_SHARED_FULL) $^
	ln -sf $(LIB_SHARED_FULL) $(LIB_SHARED).1
	ln -sf $(LIB_SHARED).1 $(LIB_SHARED)
	@echo "Built shared library: $(LIB_SHARED_FULL)"

# Build examples
examples: $(EXAMPLES)

example_simple: example_simple.o $(LIB_STATIC)
	$(CC) -o $@ $< -L. -lperfmon -static
	@echo "Built example: $@"


# Install library and headers
install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 755 $(LIB_SHARED_FULL) $(DESTDIR)$(LIBDIR)/
	ln -sf $(LIB_SHARED_FULL) $(DESTDIR)$(LIBDIR)/$(LIB_SHARED).1
	ln -sf $(LIB_SHARED).1 $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)
	install -m 644 $(HEADERS) $(DESTDIR)$(INCLUDEDIR)/
	@echo "Installed to $(PREFIX)"
	@echo "Run 'ldconfig' or set LD_LIBRARY_PATH=$(LIBDIR) to use shared library"

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_STATIC)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)*
	rm -f $(DESTDIR)$(INCLUDEDIR)/perfmon.h
	@echo "Uninstalled from $(PREFIX)"

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(EXAMPLE_OBJECTS)
	rm -f $(LIB_STATIC) $(LIB_SHARED)* 
	rm -f $(EXAMPLES)
	@echo "Cleaned build artifacts"

# Test if perf is supported
test-support:
	@if [ -f example_simple ]; then \
		./example_simple --check-support; \
	else \
		echo "Please run 'make' first to build the examples"; \
	fi

# Display help
help:
	@echo "libperfmon - Performance Monitoring Library"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build static and shared libraries (default)"
	@echo "  examples         - Build example programs"
	@echo "  install          - Install library and headers (may require sudo)"
	@echo "  uninstall        - Remove installed files"
	@echo "  clean            - Remove build artifacts"
	@echo "  test-support     - Test if performance monitoring is supported"
	@echo "  help             - Display this help message"
	@echo ""
	@echo "Installation:"
	@echo "  make"
	@echo "  sudo make install"
	@echo ""
	@echo "Custom prefix:"
	@echo "  make PREFIX=/custom/path install"

.PHONY: all examples install uninstall clean test-support help

