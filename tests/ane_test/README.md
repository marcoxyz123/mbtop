# Apple Neural Engine (ANE) Test Programs

Test programs demonstrating how to utilize Apple's neural hardware accelerators.

## Apple Silicon Accelerators

| Accelerator | Access Method | Best For |
|-------------|---------------|----------|
| **ANE** (Neural Engine) | Core ML | ML inference, CNN, transformers |
| **AMX** (Matrix Coprocessor) | BNNS/vDSP/BLAS | Matrix ops, dense layers |
| **GPU** | Metal/MPS | Parallel compute, rendering |

## Files

- `ane_bnns_test.cpp` - BNNS tests (matrix mult, convolution, fully connected)
- `ane_coreml_test.mm` - Core ML example (Objective-C++)
- `Makefile` - Build system

## Build

```bash
# Build all
make

# Build individually
make ane_bnns_test
make ane_coreml_test
```

## Run

```bash
# Run BNNS test (recommended - works standalone)
make run
# or
./ane_bnns_test

# Run Core ML test (informational)
make run-coreml
```

## Sample Output (M1/M2/M3)

```
========================================
  Apple Neural Engine / AMX Test (BNNS)
========================================

Platform: Apple Silicon (arm64)
Hardware accelerators available:
  - AMX (Apple Matrix Coprocessor) - for BLAS/BNNS
  - ANE (Apple Neural Engine) - via Core ML
  - GPU (Metal Performance Shaders)

=== BNNS Matrix Multiplication (1024x1024 * 1024x1024) ===
  Time: 2.345 ms
  Performance: 915.23 GFLOPS

=== BNNS 2D Convolution ===
  Input: 224x224x3
  Kernel: 3x3, 64 filters
  Time per inference: 1.234 ms
  Performance: 89.45 GFLOPS
...
```

## Using the Actual ANE (via Core ML)

The ANE is accessed through Core ML. To use it:

1. **Get a model** (e.g., from Apple's model gallery or convert from PyTorch/TensorFlow):
   ```bash
   # Download MobileNet from Apple
   curl -O https://ml-assets.apple.com/coreml/models/MobileNetV2.mlmodel
   ```

2. **Compile the model**:
   ```bash
   xcrun coremlcompiler compile MobileNetV2.mlmodel .
   ```

3. **Load with Core ML** and set compute units:
   ```objc
   MLModelConfiguration *config = [[MLModelConfiguration alloc] init];
   config.computeUnits = MLComputeUnitsAll;  // Use ANE when beneficial
   
   MLModel *model = [MLModel modelWithContentsOfURL:modelURL
                                      configuration:config
                                              error:&error];
   ```

## Understanding the Results

### BNNS Performance
- Uses **AMX** (Apple Matrix Coprocessor) internally
- AMX is separate from ANE but also very fast
- Great for: dense matrix ops, BLAS, signal processing

### When ANE is Used
Apple's Core ML automatically decides when to use ANE based on:
- Model architecture (CNNs, transformers work well)
- Operation types (some ops aren't ANE-compatible)
- Batch size (ANE likes larger batches)
- Data types (FP16/INT8 preferred on ANE)

### Verifying ANE Usage
Use Instruments.app with the "Core ML" template to see which accelerator is used.

## Rust Alternative

For Rust, use the `accelerate` crate:

```rust
// Cargo.toml
[dependencies]
accelerate = "0.1"

// main.rs
use accelerate::blas;
// BLAS operations will use AMX on Apple Silicon
```

Or use `metal` crate for GPU/MPS access.
