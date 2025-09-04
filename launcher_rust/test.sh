#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "Testing Rust glkcli launcher..."

# Build first if not already built
if [ ! -f "target/release/glkcli" ]; then
    echo "Building Rust launcher first..."
    cargo build --release
fi

GLKCLI="./target/release/glkcli"

echo "✓ Testing help output:"
$GLKCLI --help

echo ""
echo "✓ Testing version:"
$GLKCLI --version

echo ""
echo "✓ Testing format detection (with non-existent file, should show error):"
$GLKCLI --format nonexistent.z5 || echo "Expected error for non-existent file"

echo ""
echo "✓ All basic tests passed!"
echo ""
echo "To test with a real game file:"
echo "  $GLKCLI path/to/your/game.z5"
