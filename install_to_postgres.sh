#!/bin/bash
#
# Helper script to integrate libperfmon into PostgreSQL source code
#

set -e

# Default PostgreSQL source path
PG_SRC_DIR="${1:-/mydata/postgresql-16.4}"

# Check if PostgreSQL source directory exists
if [ ! -d "$PG_SRC_DIR" ]; then
    echo "Error: PostgreSQL source directory does not exist: $PG_SRC_DIR"
    echo "Usage: $0 [postgresql_source_directory_path]"
    exit 1
fi

echo "==================================="
echo "libperfmon PostgreSQL Integration Script"
echo "==================================="
echo ""
echo "PostgreSQL source directory: $PG_SRC_DIR"
echo ""

# Get current script directory (libperfmon directory)
LIBPERFMON_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Step 1/4: Building libperfmon..."
cd "$LIBPERFMON_DIR"
if [ ! -f libperfmon.a ]; then
    make clean
    make
fi
echo "✓ libperfmon build complete"
echo ""

echo "Step 2/4: Copying library files to PostgreSQL source..."
# Copy static library
cp -v libperfmon.a "$PG_SRC_DIR/src/backend/"
echo "✓ Copied libperfmon.a"

# Copy header file
mkdir -p "$PG_SRC_DIR/src/include/utils"
cp -v perfmon.h "$PG_SRC_DIR/src/include/utils/"
echo "✓ Copied perfmon.h"
echo ""

echo "Step 3/4: Backing up and modifying PostgreSQL Makefile..."
BACKEND_MAKEFILE="$PG_SRC_DIR/src/backend/Makefile"

# Backup original Makefile
if [ ! -f "${BACKEND_MAKEFILE}.perfmon.bak" ]; then
    cp "$BACKEND_MAKEFILE" "${BACKEND_MAKEFILE}.perfmon.bak"
    echo "✓ Backed up original Makefile to ${BACKEND_MAKEFILE}.perfmon.bak"
else
    echo "✓ Makefile backup already exists, skipping backup"
fi

# Check if libperfmon has already been added
if grep -q "libperfmon.a" "$BACKEND_MAKEFILE"; then
    echo "✓ Makefile already contains libperfmon configuration, skipping modification"
else
    echo "" >> "$BACKEND_MAKEFILE"
    echo "# libperfmon support" >> "$BACKEND_MAKEFILE"
    echo "OBJS += libperfmon.a" >> "$BACKEND_MAKEFILE"
    echo "✓ Modified Makefile to add libperfmon support"
fi
echo ""

