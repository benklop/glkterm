# GLK Autosave Unit Testing

This directory contains unit tests for the GLK autosave infrastructure using the Unity testing framework.

## Test Organization

- `unity/` - Unity C testing framework (version 2.6.0)
- `test_updatetags.c` - Tests for the deterministic update tag system
- `test_serialization.c` - Tests for endian-safe serialization functions
- `CMakeLists.txt` - CMake configuration for building and running tests

## Running Tests

All tests can be run using several methods:

### Using CMake (recommended)
```bash
cd build
make check           # Run all tests via custom target
ctest --verbose      # Run tests via CMake's testing system
```

### Individual tests
```bash
cd build
make test-updatetags      # Run only update tag tests
make test-serialization   # Run only serialization tests
./tests/test_updatetags   # Run update tag tests directly
./tests/test_serialization # Run serialization tests directly
```

## Test Coverage

### Update Tag Tests (`test_updatetags.c`)
- ✅ Deterministic tag generation for windows, streams, and filerefs
- ✅ Tag lookup by object (find_by_tag functionality)
- ✅ Null object handling
- ✅ Cross-session tag consistency (rock+type based)

### Serialization Tests (`test_serialization.c`)
- ✅ Big-endian uint32 serialization/deserialization
- ✅ Round-trip data integrity
- ✅ Buffer serialization with length prefixes
- ✅ Empty buffer handling
- ✅ Error handling for null file pointers

## Adding New Tests

1. Create a new test file following the pattern `test_*.c`
2. Add the test executable to `CMakeLists.txt`
3. Include Unity: `#include "../unity/unity.h"`
4. Implement `setUp()` and `tearDown()` functions
5. Write test functions starting with `test_`
6. Add tests to `main()` using `RUN_TEST()`

## Test Results

Current status: **All tests passing**
- Update tag tests: 6/6 ✅
- Serialization tests: 7/7 ✅

The test suite validates the fundamental infrastructure needed for GLK state autosave/autorestore functionality.
