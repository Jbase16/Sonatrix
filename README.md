# Sonatrix

Sonatrix is a macOS-native, production-grade backing track creation environment that consolidates guitar strumming, intelligent piano comping, bass generation, string arrangement, and drum groove generation into a single coherent suite purpose-built for vocal production.

## Core Architecture
- **Language**: C++20 for strict real-time DSP, Swift/SwiftUI for UI.
- **Invariants**: Lock-free audio thread, deterministic event scheduling, immutable pattern data.

## Build Instructions (macOS)
This project uses CMake to generate Xcode projects for both the Standalone App and the AUv3 plugin targets.

```bash
mkdir build
cd build
cmake -G Xcode ..
cmake --build .
```
