# Build directory, all build files will be gathered here.
BUILD_DIR="build"

# Optional
VERBOSE="--verbose"

# Clean old build tree (if any).
rm -rf ${BUILD_DIR}

# Run cmake in configure mode using current directory as
# source tree and writing all generated build files in the
# build directory.
cmake -S . -B ${BUILD_DIR}

# Use the generated build files in build directory to actually
# compile and link the project.
cmake --build ${BUILD_DIR} ${VERBOSE}
