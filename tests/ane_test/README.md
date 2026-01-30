# Apple Silicon Load Test Tools

Test programs for simulating load on Apple Silicon hardware accelerators (AMX and ANE).

## Quick Start

```bash
cd tests/ane_test

# AMX/CPU load (works immediately)
./ane_bnns_test --load

# ANE load (requires model setup first)
make setup              # Downloads and compiles MobileNetV2
./ane_coreml_load --load
```

## Tools Overview

| Tool | Hardware | Use Case |
|------|----------|----------|
| `ane_bnns_test` | **AMX** (Matrix Coprocessor) | CPU/AMX stress test |
| `ane_coreml_load` | **ANE** (Neural Engine) | ANE stress test |

## AMX Load Test (ane_bnns_test)

Runs continuous matrix multiplication using Apple's BNNS framework, which leverages the AMX coprocessor.

```bash
# Run continuous load (Ctrl+C to stop)
./ane_bnns_test --load

# Heavier load with larger matrices
./ane_bnns_test --load --size 4096

# Run benchmark once (no continuous load)
./ane_bnns_test
```

**Options:**
- `-l, --load` - Run continuous load simulation
- `-s, --size N` - Matrix size (default: 2048)
- `-h, --help` - Show help

**Expected output:**
```
[LOAD MODE] Simulating continuous load. Press Ctrl+C to stop.
Matrix size: 2048x2048, Threads: 0

[     100] 6.0 ms | 2850 GFLOPS | Total: 0.6s
```

**Note:** AMX usage appears as CPU load in mbtop (Apple doesn't expose separate AMX metrics).

## ANE Load Test (ane_coreml_load)

Runs continuous neural network inference using Core ML, which utilizes the Apple Neural Engine.

### Setup (one-time)

```bash
# Download and compile MobileNetV2
make setup

# Or manually:
curl -L -o MobileNetV2.mlmodel \
  'https://ml-assets.apple.com/coreml/models/Image/ImageClassification/MobileNetV2/MobileNetV2.mlmodel'
xcrun coremlcompiler compile MobileNetV2.mlmodel .
```

### Run

```bash
# Run continuous ANE load (Ctrl+C to stop)
./ane_coreml_load --load

# Run specific number of iterations
./ane_coreml_load --load --iterations 1000
```

**Options:**
- `-l, --load` - Run continuous load simulation
- `-n, --iterations N` - Number of iterations (0 = infinite)
- `-v, --verbose` - Verbose output
- `-h, --help` - Show help

**Expected output:**
```
[LOAD MODE] Running ANE inference. Press Ctrl+C to stop.

[     100] 0.6 ms | Avg: 0.7 ms | 1500.0 FPS | Total: 0.1s
```

**Note:** ANE usage is visible in mbtop's ANE meter when running this test.

## Building from Source

```bash
make                    # Build all
make ane_bnns_test      # Build AMX test only
make ane_coreml_load    # Build ANE test only
make clean              # Remove binaries
```

## Hardware Comparison

| Accelerator | Monitored by mbtop? | Access Method |
|-------------|---------------------|---------------|
| **CPU** | Yes (CPU %) | Direct code |
| **AMX** | No (shows as CPU) | BNNS/vDSP/BLAS |
| **ANE** | Yes (ANE %) | Core ML only |
| **GPU** | Yes (GPU %) | Metal/MPS |

## Typical Performance (M4 Pro)

| Test | Performance |
|------|-------------|
| AMX (2048x2048 matmul) | ~2800-3000 GFLOPS |
| ANE (MobileNetV2) | ~1500 FPS |
