## Development Commands

```bash
# Build the suite (Release, tests ON, dedicated git-ignored build dir)
cmake -S . -B .claude-build-release -DCMAKE_BUILD_TYPE=Release -DEMERAUDE_ENABLE_TESTS=On
cmake --build .claude-build-release --target EmeraudeBaseUnitTests -j$(nproc)

# Run everything (from the build dir; ctest sets the resources/ fixture working dir)
cd .claude-build-release && ctest --output-on-failure -j$(nproc)

# Tests by category (from resources/ so fixture paths resolve)
cd resources && ../.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='MathVector*'
../.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='ThreadPool*'
../.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='TokenFormatter*'
```

See [`Testing/AGENTS.md`](../../../src/Testing/AGENTS.md) for conventions and the sanitizer gate.
