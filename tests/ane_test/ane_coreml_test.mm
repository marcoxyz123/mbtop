// Apple Neural Engine Test via Core ML
// Compile: clang++ -std=c++17 -framework Foundation -framework CoreML -framework Accelerate -o ane_coreml_test ane_coreml_test.mm

#import <Foundation/Foundation.h>
#import <CoreML/CoreML.h>
#include <iostream>
#include <chrono>
#include <vector>

// Helper to measure execution time
template<typename Func>
double measureTime(Func&& func, int iterations = 10) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

// Create a simple neural network model programmatically
MLModel* createSimpleModel(MLComputeUnits computeUnits) {
    @autoreleasepool {
        // Create a simple model specification
        // This creates a basic matrix multiplication model
        
        // For a real test, you'd load an actual .mlmodelc file
        // Here we demonstrate the API structure
        
        NSError *error = nil;
        
        // Try to find any existing Core ML model in common locations
        NSArray *searchPaths = @[
            @"/Applications",
            [[NSBundle mainBundle] bundlePath],
            @"."
        ];
        
        // Configuration for compute units
        MLModelConfiguration *config = [[MLModelConfiguration alloc] init];
        config.computeUnits = computeUnits;
        
        // For demonstration, we'll return nil if no model found
        // In practice, you'd bundle or download a model
        return nil;
    }
}

void printComputeUnitInfo() {
    std::cout << "\n=== Apple Neural Engine Test ===\n\n";
    std::cout << "Available Compute Units:\n";
    std::cout << "  - MLComputeUnitsCPUOnly (0): CPU only\n";
    std::cout << "  - MLComputeUnitsCPUAndGPU (1): CPU + GPU\n";
    std::cout << "  - MLComputeUnitsAll (2): CPU + GPU + ANE\n";
    std::cout << "  - MLComputeUnitsCPUAndNeuralEngine (3): CPU + ANE\n\n";
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        printComputeUnitInfo();
        
        // Check if we're on Apple Silicon
        #if __arm64__
        std::cout << "Platform: Apple Silicon (arm64) - ANE available\n\n";
        #else
        std::cout << "Platform: Intel (x86_64) - No ANE\n\n";
        #endif
        
        // Create model configurations for different compute units
        MLModelConfiguration *configCPU = [[MLModelConfiguration alloc] init];
        configCPU.computeUnits = MLComputeUnitsCPUOnly;
        
        MLModelConfiguration *configGPU = [[MLModelConfiguration alloc] init];
        configGPU.computeUnits = MLComputeUnitsCPUAndGPU;
        
        MLModelConfiguration *configANE = [[MLModelConfiguration alloc] init];
        configANE.computeUnits = MLComputeUnitsAll;  // Includes ANE
        
        MLModelConfiguration *configCPUANE = [[MLModelConfiguration alloc] init];
        configCPUANE.computeUnits = MLComputeUnitsCPUAndNeuralEngine;
        
        std::cout << "To test ANE with a real model:\n";
        std::cout << "1. Download/create a .mlmodel file (e.g., MobileNet, ResNet)\n";
        std::cout << "2. Compile it: xcrun coremlcompiler compile Model.mlmodel .\n";
        std::cout << "3. Load with MLModel and set computeUnits = MLComputeUnitsAll\n\n";
        
        std::cout << "Example code to load and run a model:\n";
        std::cout << "----------------------------------------\n";
        std::cout << R"(
NSURL *modelURL = [NSURL fileURLWithPath:@"YourModel.mlmodelc"];
MLModelConfiguration *config = [[MLModelConfiguration alloc] init];
config.computeUnits = MLComputeUnitsAll;  // Use ANE when possible

NSError *error = nil;
MLModel *model = [MLModel modelWithContentsOfURL:modelURL
                                   configuration:config
                                           error:&error];

if (model) {
    // Create input features
    MLDictionaryFeatureProvider *input = ...;
    
    // Run inference (ANE used automatically if beneficial)
    id<MLFeatureProvider> output = [model predictionFromFeatures:input
                                                           error:&error];
}
)";
        std::cout << "----------------------------------------\n\n";
        
        std::cout << "See ane_bnns_test.cpp for a working BNNS example.\n";
        
        return 0;
    }
}
