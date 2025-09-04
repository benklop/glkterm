#!/bin/bash
set -e

echo "Building Rust glkcli launcher..."

# Change to the launcher_rust directory
cd "$(dirname "$0")"

# Build the Rust project
cargo build --release

echo "✓ Rust launcher built successfully!"
echo "Binary available at: target/release/glkcli"
echo ""
echo "To test the Rust launcher:"
echo "  cd launcher_rust"
echo "  ./target/release/glkcli ../path/to/game.z5"
echo ""
echo "To replace the C launcher in your build:"
echo "  cp target/release/glkcli ../build/"
