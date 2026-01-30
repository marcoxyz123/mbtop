// Compile: clang++ -std=c++17 -O3 -framework Foundation -framework CoreML -framework Accelerate -framework CoreVideo -o ane_coreml_load ane_coreml_load.mm

#import <Foundation/Foundation.h>
#import <CoreML/CoreML.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstring>
#include <vector>

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

API_AVAILABLE(macos(12.0))
MLModel* createConvolutionModel(MLComputeUnits computeUnits) {
    @autoreleasepool {
        NSError *error = nil;
        
        // Model dimensions optimized for ANE
        const int batchSize = 1;
        const int inputChannels = 64;
        const int outputChannels = 128;
        const int height = 224;
        const int width = 224;
        const int kernelSize = 3;
        
        // Create model configuration
        MLModelConfiguration *config = [[MLModelConfiguration alloc] init];
        config.computeUnits = computeUnits;
        
        // Build a simple convolution model using MLProgram (macOS 13+)
        // For older macOS, we'll use a different approach
        
        if (@available(macOS 14.0, *)) {
            // Use compute plan for newer macOS
        }
        
        // Create model spec programmatically
        // We'll build a NeuralNetwork model specification
        
        NSDictionary *modelSpec = @{
            @"specificationVersion": @5,
            @"description": @{
                @"input": @[@{
                    @"name": @"input",
                    @"type": @{
                        @"multiArrayType": @{
                            @"shape": @[@(batchSize), @(inputChannels), @(height), @(width)],
                            @"dataType": @"FLOAT32"
                        }
                    }
                }],
                @"output": @[@{
                    @"name": @"output",
                    @"type": @{
                        @"multiArrayType": @{
                            @"shape": @[@(batchSize), @(outputChannels), @(height), @(width)],
                            @"dataType": @"FLOAT32"
                        }
                    }
                }],
                @"metadata": @{
                    @"shortDescription": @"ANE Load Test Model"
                }
            },
            @"neuralNetwork": @{
                @"layers": @[
                    @{
                        @"name": @"conv1",
                        @"input": @[@"input"],
                        @"output": @[@"conv1_out"],
                        @"convolution": @{
                            @"outputChannels": @(outputChannels),
                            @"kernelChannels": @(inputChannels),
                            @"nGroups": @1,
                            @"kernelSize": @[@(kernelSize), @(kernelSize)],
                            @"stride": @[@1, @1],
                            @"dilationFactor": @[@1, @1],
                            @"valid": @{
                                @"paddingAmounts": @{
                                    @"borderAmounts": @[
                                        @{@"startEdgeSize": @1, @"endEdgeSize": @1},
                                        @{@"startEdgeSize": @1, @"endEdgeSize": @1}
                                    ]
                                }
                            },
                            @"isDeconvolution": @NO,
                            @"hasBias": @NO,
                            @"weights": @{
                                @"floatValue": [NSMutableArray array]
                            }
                        }
                    },
                    @{
                        @"name": @"relu1",
                        @"input": @[@"conv1_out"],
                        @"output": @[@"relu1_out"],
                        @"activation": @{
                            @"ReLU": @{}
                        }
                    },
                    @{
                        @"name": @"conv2",
                        @"input": @[@"relu1_out"],
                        @"output": @[@"output"],
                        @"convolution": @{
                            @"outputChannels": @(outputChannels),
                            @"kernelChannels": @(outputChannels),
                            @"nGroups": @1,
                            @"kernelSize": @[@(kernelSize), @(kernelSize)],
                            @"stride": @[@1, @1],
                            @"dilationFactor": @[@1, @1],
                            @"valid": @{
                                @"paddingAmounts": @{
                                    @"borderAmounts": @[
                                        @{@"startEdgeSize": @1, @"endEdgeSize": @1},
                                        @{@"startEdgeSize": @1, @"endEdgeSize": @1}
                                    ]
                                }
                            },
                            @"isDeconvolution": @NO,
                            @"hasBias": @NO,
                            @"weights": @{
                                @"floatValue": [NSMutableArray array]
                            }
                        }
                    }
                ]
            }
        };
        
        // Initialize weights
        NSMutableArray *weights1 = modelSpec[@"neuralNetwork"][@"layers"][0][@"convolution"][@"weights"][@"floatValue"];
        NSMutableArray *weights2 = modelSpec[@"neuralNetwork"][@"layers"][2][@"convolution"][@"weights"][@"floatValue"];
        
        int numWeights1 = outputChannels * inputChannels * kernelSize * kernelSize;
        int numWeights2 = outputChannels * outputChannels * kernelSize * kernelSize;
        
        for (int i = 0; i < numWeights1; i++) {
            [weights1 addObject:@(0.01f * (float)(rand() % 100 - 50) / 50.0f)];
        }
        for (int i = 0; i < numWeights2; i++) {
            [weights2 addObject:@(0.01f * (float)(rand() % 100 - 50) / 50.0f)];
        }
        
        // Convert to JSON data
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:modelSpec options:0 error:&error];
        if (error) {
            NSLog(@"JSON error: %@", error);
            return nil;
        }
        
        // Write to temp file as .mlmodel spec
        NSString *tempDir = NSTemporaryDirectory();
        NSString *specPath = [tempDir stringByAppendingPathComponent:@"ane_test_model.mlmodel.json"];
        [jsonData writeToFile:specPath atomically:YES];
        
        return nil; // This approach won't work directly, need compiled model
    }
}

API_AVAILABLE(macos(11.0))
void runANELoadTest(uint64_t iterations, bool verbose) {
    @autoreleasepool {
        NSError *error = nil;
        
        // Model dimensions - optimized for ANE (powers of 2, reasonable sizes)
        const int inputChannels = 3;
        const int height = 224;
        const int width = 224;
        
        // Check for bundled or downloaded model
        NSString *modelPath = nil;
        NSArray *searchPaths = @[
            @"./MobileNetV2.mlmodelc",
            @"./ResNet50.mlmodelc",
            @"./SqueezeNet.mlmodelc",
            [[NSBundle mainBundle] pathForResource:@"MobileNetV2" ofType:@"mlmodelc"],
        ];
        
        for (NSString *path in searchPaths) {
            if (path && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
                modelPath = path;
                break;
            }
        }
        
        if (!modelPath) {
            std::cout << "\nNo Core ML model found. Download one first:\n\n";
            std::cout << "  # Download MobileNetV2 (small, fast)\n";
            std::cout << "  curl -L -o MobileNetV2.mlmodel \\\n";
            std::cout << "    'https://ml-assets.apple.com/coreml/models/Image/ImageClassification/MobileNetV2/MobileNetV2.mlmodel'\n\n";
            std::cout << "  # Compile it\n";
            std::cout << "  xcrun coremlcompiler compile MobileNetV2.mlmodel .\n\n";
            std::cout << "  # Then run this test again\n";
            std::cout << "  ./ane_coreml_load --load\n\n";
            return;
        }
        
        std::cout << "Loading model: " << [modelPath UTF8String] << "\n";
        
        // Configure for ANE
        MLModelConfiguration *config = [[MLModelConfiguration alloc] init];
        config.computeUnits = MLComputeUnitsAll; // Prefer ANE when possible
        
        NSURL *modelURL = [NSURL fileURLWithPath:modelPath];
        MLModel *model = [MLModel modelWithContentsOfURL:modelURL configuration:config error:&error];
        
        if (error || !model) {
            std::cout << "Failed to load model: " << [[error description] UTF8String] << "\n";
            return;
        }
        
        std::cout << "Model loaded successfully!\n";
        std::cout << "Compute units: MLComputeUnitsAll (CPU + GPU + ANE)\n\n";
        
        // Get input description
        MLModelDescription *desc = model.modelDescription;
        NSDictionary *inputs = desc.inputDescriptionsByName;
        
        if (verbose) {
            std::cout << "Model inputs:\n";
            for (NSString *name in inputs) {
                MLFeatureDescription *input = inputs[name];
                std::cout << "  " << [name UTF8String] << ": " << [[input description] UTF8String] << "\n";
            }
            std::cout << "\n";
        }
        
        // Find the image input
        NSString *inputName = nil;
        MLFeatureDescription *inputDesc = nil;
        for (NSString *name in inputs) {
            MLFeatureDescription *desc = inputs[name];
            if (desc.type == MLFeatureTypeImage || desc.type == MLFeatureTypeMultiArray) {
                inputName = name;
                inputDesc = desc;
                break;
            }
        }
        
        if (!inputName) {
            std::cout << "Could not find suitable input\n";
            return;
        }
        
        // Create input data
        MLMultiArray *inputArray = nil;
        CVPixelBufferRef pixelBuffer = nil;
        
        if (inputDesc.type == MLFeatureTypeImage) {
            // Create a pixel buffer for image input
            MLImageConstraint *constraint = inputDesc.imageConstraint;
            int imgWidth = (int)constraint.pixelsWide;
            int imgHeight = (int)constraint.pixelsHigh;
            
            NSDictionary *attrs = @{
                (id)kCVPixelBufferCGImageCompatibilityKey: @YES,
                (id)kCVPixelBufferCGBitmapContextCompatibilityKey: @YES,
                (id)kCVPixelBufferMetalCompatibilityKey: @YES
            };
            
            CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault,
                                                   imgWidth, imgHeight,
                                                   kCVPixelFormatType_32BGRA,
                                                   (__bridge CFDictionaryRef)attrs,
                                                   &pixelBuffer);
            
            if (status != kCVReturnSuccess) {
                std::cout << "Failed to create pixel buffer\n";
                return;
            }
            
            // Fill with random data
            CVPixelBufferLockBaseAddress(pixelBuffer, 0);
            uint8_t *baseAddr = (uint8_t *)CVPixelBufferGetBaseAddress(pixelBuffer);
            size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
            for (int y = 0; y < imgHeight; y++) {
                for (int x = 0; x < imgWidth * 4; x++) {
                    baseAddr[y * bytesPerRow + x] = rand() % 256;
                }
            }
            CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
            
            std::cout << "Created " << imgWidth << "x" << imgHeight << " image input\n";
            
        } else if (inputDesc.type == MLFeatureTypeMultiArray) {
            // Create multi-array input
            MLMultiArrayConstraint *constraint = inputDesc.multiArrayConstraint;
            inputArray = [[MLMultiArray alloc] initWithShape:constraint.shape
                                                   dataType:MLMultiArrayDataTypeFloat32
                                                      error:&error];
            if (error) {
                std::cout << "Failed to create input array: " << [[error description] UTF8String] << "\n";
                return;
            }
            
            // Fill with random data
            float *ptr = (float *)inputArray.dataPointer;
            for (NSInteger i = 0; i < inputArray.count; i++) {
                ptr[i] = (float)(rand() % 1000) / 1000.0f;
            }
            
            std::cout << "Created multi-array input with " << inputArray.count << " elements\n";
        }
        
        std::cout << "\n[LOAD MODE] Running ANE inference. Press Ctrl+C to stop.\n\n";
        
        uint64_t iteration = 0;
        auto startTime = std::chrono::steady_clock::now();
        double totalInferenceTime = 0;
        
        while (g_running && (iterations == 0 || iteration < iterations)) {
            @autoreleasepool {
                auto inferStart = std::chrono::high_resolution_clock::now();
                
                // Create feature provider
                id<MLFeatureProvider> input = nil;
                
                if (pixelBuffer) {
                    MLFeatureValue *imageValue = [MLFeatureValue featureValueWithPixelBuffer:pixelBuffer];
                    input = [[MLDictionaryFeatureProvider alloc] 
                             initWithDictionary:@{inputName: imageValue} error:&error];
                } else if (inputArray) {
                    MLFeatureValue *arrayValue = [MLFeatureValue featureValueWithMultiArray:inputArray];
                    input = [[MLDictionaryFeatureProvider alloc] 
                             initWithDictionary:@{inputName: arrayValue} error:&error];
                }
                
                if (error || !input) {
                    std::cout << "Failed to create input: " << [[error description] UTF8String] << "\n";
                    break;
                }
                
                // Run inference
                id<MLFeatureProvider> output = [model predictionFromFeatures:input error:&error];
                
                auto inferEnd = std::chrono::high_resolution_clock::now();
                double inferMs = std::chrono::duration<double, std::milli>(inferEnd - inferStart).count();
                
                if (error) {
                    std::cout << "Inference error: " << [[error description] UTF8String] << "\n";
                    break;
                }
                
                iteration++;
                totalInferenceTime += inferMs;
                
                auto now = std::chrono::steady_clock::now();
                double totalSec = std::chrono::duration<double>(now - startTime).count();
                double avgMs = totalInferenceTime / iteration;
                double fps = 1000.0 / avgMs;
                
                std::cout << "\r[" << std::setw(8) << iteration << "] "
                          << std::fixed << std::setprecision(1) << inferMs << " ms | "
                          << "Avg: " << std::setprecision(1) << avgMs << " ms | "
                          << std::setprecision(1) << fps << " FPS | "
                          << "Total: " << std::setprecision(1) << totalSec << "s" << std::flush;
            }
        }
        
        if (pixelBuffer) {
            CVPixelBufferRelease(pixelBuffer);
        }
        
        std::cout << "\n\nANE load test stopped.\n";
        std::cout << "Total iterations: " << iteration << "\n";
        std::cout << "Average inference time: " << std::fixed << std::setprecision(2) 
                  << (totalInferenceTime / iteration) << " ms\n";
    }
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -l, --load          Run continuous ANE load simulation\n";
    std::cout << "  -n, --iterations N  Number of iterations (0 = infinite, default)\n";
    std::cout << "  -v, --verbose       Verbose output\n";
    std::cout << "  -h, --help          Show this help\n\n";
    std::cout << "Requirements:\n";
    std::cout << "  A compiled Core ML model (.mlmodelc) in the current directory.\n\n";
    std::cout << "Setup:\n";
    std::cout << "  # Download MobileNetV2\n";
    std::cout << "  curl -L -o MobileNetV2.mlmodel \\\n";
    std::cout << "    'https://ml-assets.apple.com/coreml/models/Image/ImageClassification/MobileNetV2/MobileNetV2.mlmodel'\n\n";
    std::cout << "  # Compile it\n";
    std::cout << "  xcrun coremlcompiler compile MobileNetV2.mlmodel .\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " --load        Run continuous ANE load\n";
    std::cout << "  " << prog << " -l -n 100     Run 100 iterations\n";
}

int main(int argc, char* argv[]) {
    bool loadMode = false;
    uint64_t iterations = 0;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--load") == 0) {
            loadMode = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 < argc) iterations = std::strtoull(argv[++i], nullptr, 10);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "  Apple Neural Engine (ANE) Load Test\n";
    std::cout << "========================================\n";
    
    #if __arm64__
    std::cout << "\nPlatform: Apple Silicon (arm64) - ANE available\n";
    #else
    std::cout << "\nPlatform: Intel (x86_64) - No ANE available\n";
    return 1;
    #endif
    
    if (!loadMode) {
        printUsage(argv[0]);
        return 0;
    }
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (@available(macOS 11.0, *)) {
        runANELoadTest(iterations, verbose);
    } else {
        std::cout << "Requires macOS 11.0 or later\n";
        return 1;
    }
    
    return 0;
}
