// Compile: clang++ -std=c++17 -framework Accelerate -o ane_bnns_test ane_bnns_test.cpp

#include <Accelerate/Accelerate.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cmath>
#include <iomanip>
#include <csignal>
#include <atomic>
#include <cstring>

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

class Timer {
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }
};

void fillRandom(std::vector<float>& v) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : v) x = dist(gen);
}

void testMatrixMultiplication(int M, int N, int K) {
    std::cout << "\n=== BNNS Matrix Multiplication (" << M << "x" << K << " * " << K << "x" << N << ") ===\n";
    
    std::vector<float> A(M * K);
    std::vector<float> B(K * N);
    std::vector<float> C(M * N, 0.0f);
    
    fillRandom(A);
    fillRandom(B);
    
    Timer timer;
    vDSP_mmul(A.data(), 1, B.data(), 1, C.data(), 1, M, N, K);
    double elapsed = timer.elapsedMs();
    
    double gflops = (2.0 * M * N * K) / (elapsed * 1e6);
    std::cout << "  Time: " << std::fixed << std::setprecision(3) << elapsed << " ms\n";
    std::cout << "  Performance: " << std::setprecision(2) << gflops << " GFLOPS\n";
    
    float sum = 0;
    vDSP_sve(C.data(), 1, &sum, M * N);
    std::cout << "  Result checksum: " << sum << "\n";
}

void testConvolution() {
    std::cout << "\n=== BNNS 2D Convolution ===\n";
    
    const size_t inputWidth = 224;
    const size_t inputHeight = 224;
    const size_t inputChannels = 3;
    const size_t kernelSize = 3;
    const size_t outputChannels = 64;
    
    std::vector<float> input(inputWidth * inputHeight * inputChannels);
    std::vector<float> weights(kernelSize * kernelSize * inputChannels * outputChannels);
    std::vector<float> bias(outputChannels, 0.0f);
    std::vector<float> output(inputWidth * inputHeight * outputChannels, 0.0f);
    
    fillRandom(input);
    fillRandom(weights);
    
    BNNSFilterParameters filterParams;
    memset(&filterParams, 0, sizeof(filterParams));
    filterParams.flags = BNNSFlagsUseClientPtr;
    filterParams.n_threads = 0;
    
    BNNSNDArrayDescriptor inputDesc;
    memset(&inputDesc, 0, sizeof(inputDesc));
    inputDesc.layout = BNNSDataLayoutImageCHW;
    inputDesc.size[0] = inputWidth;
    inputDesc.size[1] = inputHeight;
    inputDesc.size[2] = inputChannels;
    inputDesc.data = input.data();
    inputDesc.data_type = BNNSDataTypeFloat32;
    inputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor outputDesc;
    memset(&outputDesc, 0, sizeof(outputDesc));
    outputDesc.layout = BNNSDataLayoutImageCHW;
    outputDesc.size[0] = inputWidth;
    outputDesc.size[1] = inputHeight;
    outputDesc.size[2] = outputChannels;
    outputDesc.data = output.data();
    outputDesc.data_type = BNNSDataTypeFloat32;
    outputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor weightsDesc;
    memset(&weightsDesc, 0, sizeof(weightsDesc));
    weightsDesc.layout = BNNSDataLayoutConvolutionWeightsOIHW;
    weightsDesc.size[0] = kernelSize;
    weightsDesc.size[1] = kernelSize;
    weightsDesc.size[2] = inputChannels;
    weightsDesc.size[3] = outputChannels;
    weightsDesc.data = weights.data();
    weightsDesc.data_type = BNNSDataTypeFloat32;
    weightsDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor biasDesc;
    memset(&biasDesc, 0, sizeof(biasDesc));
    biasDesc.layout = BNNSDataLayoutVector;
    biasDesc.size[0] = outputChannels;
    biasDesc.data = bias.data();
    biasDesc.data_type = BNNSDataTypeFloat32;
    biasDesc.data_scale = 1.0f;
    
    BNNSLayerParametersConvolution convParams;
    memset(&convParams, 0, sizeof(convParams));
    convParams.i_desc = inputDesc;
    convParams.w_desc = weightsDesc;
    convParams.o_desc = outputDesc;
    convParams.bias = biasDesc;
    convParams.activation.function = BNNSActivationFunctionIdentity;
    convParams.x_stride = 1;
    convParams.y_stride = 1;
    convParams.x_dilation_stride = 1;
    convParams.y_dilation_stride = 1;
    convParams.x_padding = 1;
    convParams.y_padding = 1;
    convParams.groups = 1;
    
    BNNSFilter conv = BNNSFilterCreateLayerConvolution(&convParams, &filterParams);
    
    if (conv == nullptr) {
        std::cout << "  Failed to create BNNS convolution filter\n";
        return;
    }
    
    BNNSFilterApply(conv, input.data(), output.data());
    
    const int iterations = 10;
    Timer timer;
    
    for (int i = 0; i < iterations; i++) {
        BNNSFilterApply(conv, input.data(), output.data());
    }
    
    double elapsed = timer.elapsedMs() / iterations;
    
    // FLOPs = 2 * K * K * C_in * C_out * H_out * W_out (standard convolution formula)
    double flops = 2.0 * kernelSize * kernelSize * inputChannels * outputChannels * inputHeight * inputWidth;
    double gflops = flops / (elapsed * 1e6);
    
    std::cout << "  Input: " << inputWidth << "x" << inputHeight << "x" << inputChannels << "\n";
    std::cout << "  Kernel: " << kernelSize << "x" << kernelSize << ", " << outputChannels << " filters\n";
    std::cout << "  Time per inference: " << std::fixed << std::setprecision(3) << elapsed << " ms\n";
    std::cout << "  Performance: " << std::setprecision(2) << gflops << " GFLOPS\n";
    
    BNNSFilterDestroy(conv);
}

void testFullyConnected() {
    std::cout << "\n=== BNNS Fully Connected Layer ===\n";
    
    const size_t inputSize = 4096;
    const size_t outputSize = 1000;
    
    std::vector<float> input(inputSize);
    std::vector<float> weights(inputSize * outputSize);
    std::vector<float> bias(outputSize, 0.0f);
    std::vector<float> output(outputSize, 0.0f);
    
    fillRandom(input);
    fillRandom(weights);
    
    BNNSFilterParameters filterParams;
    memset(&filterParams, 0, sizeof(filterParams));
    filterParams.flags = BNNSFlagsUseClientPtr;
    filterParams.n_threads = 0;
    
    BNNSNDArrayDescriptor inputDesc;
    memset(&inputDesc, 0, sizeof(inputDesc));
    inputDesc.layout = BNNSDataLayoutVector;
    inputDesc.size[0] = inputSize;
    inputDesc.data = input.data();
    inputDesc.data_type = BNNSDataTypeFloat32;
    inputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor outputDesc;
    memset(&outputDesc, 0, sizeof(outputDesc));
    outputDesc.layout = BNNSDataLayoutVector;
    outputDesc.size[0] = outputSize;
    outputDesc.data = output.data();
    outputDesc.data_type = BNNSDataTypeFloat32;
    outputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor weightsDesc;
    memset(&weightsDesc, 0, sizeof(weightsDesc));
    weightsDesc.layout = BNNSDataLayout2DLastMajor;
    weightsDesc.size[0] = inputSize;
    weightsDesc.size[1] = outputSize;
    weightsDesc.data = weights.data();
    weightsDesc.data_type = BNNSDataTypeFloat32;
    weightsDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor biasDesc;
    memset(&biasDesc, 0, sizeof(biasDesc));
    biasDesc.layout = BNNSDataLayoutVector;
    biasDesc.size[0] = outputSize;
    biasDesc.data = bias.data();
    biasDesc.data_type = BNNSDataTypeFloat32;
    biasDesc.data_scale = 1.0f;
    
    BNNSLayerParametersFullyConnected fcParams;
    memset(&fcParams, 0, sizeof(fcParams));
    fcParams.i_desc = inputDesc;
    fcParams.w_desc = weightsDesc;
    fcParams.o_desc = outputDesc;
    fcParams.bias = biasDesc;
    fcParams.activation.function = BNNSActivationFunctionIdentity;
    
    BNNSFilter fc = BNNSFilterCreateLayerFullyConnected(&fcParams, &filterParams);
    
    if (fc == nullptr) {
        std::cout << "  Failed to create BNNS fully connected filter\n";
        return;
    }
    
    BNNSFilterApply(fc, input.data(), output.data());
    
    const int iterations = 100;
    Timer timer;
    
    for (int i = 0; i < iterations; i++) {
        BNNSFilterApply(fc, input.data(), output.data());
    }
    
    double elapsed = timer.elapsedMs() / iterations;
    double gflops = (2.0 * inputSize * outputSize) / (elapsed * 1e6);
    
    std::cout << "  Input: " << inputSize << " features\n";
    std::cout << "  Output: " << outputSize << " features\n";
    std::cout << "  Time per inference: " << std::fixed << std::setprecision(3) << elapsed << " ms\n";
    std::cout << "  Performance: " << std::setprecision(2) << gflops << " GFLOPS\n";
    
    BNNSFilterDestroy(fc);
}

void testActivation() {
    std::cout << "\n=== BNNS Activation Functions ===\n";
    
    const size_t size = 1024 * 1024;
    
    std::vector<float> input(size);
    std::vector<float> output(size);
    
    fillRandom(input);
    
    BNNSFilterParameters filterParams;
    memset(&filterParams, 0, sizeof(filterParams));
    filterParams.flags = BNNSFlagsUseClientPtr;
    filterParams.n_threads = 0;
    
    BNNSNDArrayDescriptor inputDesc;
    memset(&inputDesc, 0, sizeof(inputDesc));
    inputDesc.layout = BNNSDataLayoutVector;
    inputDesc.size[0] = size;
    inputDesc.data = input.data();
    inputDesc.data_type = BNNSDataTypeFloat32;
    inputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor outputDesc;
    memset(&outputDesc, 0, sizeof(outputDesc));
    outputDesc.layout = BNNSDataLayoutVector;
    outputDesc.size[0] = size;
    outputDesc.data = output.data();
    outputDesc.data_type = BNNSDataTypeFloat32;
    outputDesc.data_scale = 1.0f;
    
    BNNSLayerParametersActivation actParams;
    memset(&actParams, 0, sizeof(actParams));
    actParams.i_desc = inputDesc;
    actParams.o_desc = outputDesc;
    actParams.activation.function = BNNSActivationFunctionTanh;
    
    BNNSFilter act = BNNSFilterCreateLayerActivation(&actParams, &filterParams);
    
    if (act == nullptr) {
        std::cout << "  Failed to create BNNS activation filter\n";
        return;
    }
    
    BNNSFilterApply(act, input.data(), output.data());
    
    const int iterations = 100;
    Timer timer;
    
    for (int i = 0; i < iterations; i++) {
        BNNSFilterApply(act, input.data(), output.data());
    }
    
    double elapsed = timer.elapsedMs() / iterations;
    double gbps = (2.0 * size * sizeof(float)) / (elapsed * 1e6);
    
    std::cout << "  Elements: " << size << " (Tanh activation)\n";
    std::cout << "  Time per pass: " << std::fixed << std::setprecision(3) << elapsed << " ms\n";
    std::cout << "  Bandwidth: " << std::setprecision(2) << gbps << " GB/s\n";
    
    BNNSFilterDestroy(act);
}

void testPooling() {
    std::cout << "\n=== BNNS Max Pooling ===\n";
    
    const size_t inputWidth = 224;
    const size_t inputHeight = 224;
    const size_t channels = 64;
    const size_t poolSize = 2;
    const size_t outputWidth = inputWidth / poolSize;
    const size_t outputHeight = inputHeight / poolSize;
    
    std::vector<float> input(inputWidth * inputHeight * channels);
    std::vector<float> output(outputWidth * outputHeight * channels, 0.0f);
    
    fillRandom(input);
    
    BNNSFilterParameters filterParams;
    memset(&filterParams, 0, sizeof(filterParams));
    filterParams.flags = BNNSFlagsUseClientPtr;
    filterParams.n_threads = 0;
    
    BNNSNDArrayDescriptor inputDesc;
    memset(&inputDesc, 0, sizeof(inputDesc));
    inputDesc.layout = BNNSDataLayoutImageCHW;
    inputDesc.size[0] = inputWidth;
    inputDesc.size[1] = inputHeight;
    inputDesc.size[2] = channels;
    inputDesc.data = input.data();
    inputDesc.data_type = BNNSDataTypeFloat32;
    inputDesc.data_scale = 1.0f;
    
    BNNSNDArrayDescriptor outputDesc;
    memset(&outputDesc, 0, sizeof(outputDesc));
    outputDesc.layout = BNNSDataLayoutImageCHW;
    outputDesc.size[0] = outputWidth;
    outputDesc.size[1] = outputHeight;
    outputDesc.size[2] = channels;
    outputDesc.data = output.data();
    outputDesc.data_type = BNNSDataTypeFloat32;
    outputDesc.data_scale = 1.0f;
    
    BNNSLayerParametersPooling poolParams;
    memset(&poolParams, 0, sizeof(poolParams));
    poolParams.i_desc = inputDesc;
    poolParams.o_desc = outputDesc;
    poolParams.pooling_function = BNNSPoolingFunctionMax;
    poolParams.k_width = poolSize;
    poolParams.k_height = poolSize;
    poolParams.x_stride = poolSize;
    poolParams.y_stride = poolSize;
    poolParams.x_padding = 0;
    poolParams.y_padding = 0;
    
    BNNSFilter pool = BNNSFilterCreateLayerPooling(&poolParams, &filterParams);
    
    if (pool == nullptr) {
        std::cout << "  Failed to create BNNS pooling filter\n";
        return;
    }
    
    BNNSFilterApply(pool, input.data(), output.data());
    
    const int iterations = 100;
    Timer timer;
    
    for (int i = 0; i < iterations; i++) {
        BNNSFilterApply(pool, input.data(), output.data());
    }
    
    double elapsed = timer.elapsedMs() / iterations;
    double gbps = ((inputWidth * inputHeight * channels + outputWidth * outputHeight * channels) * sizeof(float)) / (elapsed * 1e6);
    
    std::cout << "  Input: " << inputWidth << "x" << inputHeight << "x" << channels << "\n";
    std::cout << "  Output: " << outputWidth << "x" << outputHeight << "x" << channels << "\n";
    std::cout << "  Pool: " << poolSize << "x" << poolSize << " max\n";
    std::cout << "  Time per pass: " << std::fixed << std::setprecision(3) << elapsed << " ms\n";
    std::cout << "  Bandwidth: " << std::setprecision(2) << gbps << " GB/s\n";
    
    BNNSFilterDestroy(pool);
}

void runLoadSimulation(int matrixSize, int threads) {
    std::cout << "\n[LOAD MODE] Simulating continuous load. Press Ctrl+C to stop.\n";
    std::cout << "Matrix size: " << matrixSize << "x" << matrixSize << ", Threads: " << threads << "\n\n";
    
    std::vector<float> A(matrixSize * matrixSize);
    std::vector<float> B(matrixSize * matrixSize);
    std::vector<float> C(matrixSize * matrixSize, 0.0f);
    
    fillRandom(A);
    fillRandom(B);
    
    uint64_t iteration = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (g_running) {
        Timer timer;
        vDSP_mmul(A.data(), 1, B.data(), 1, C.data(), 1, matrixSize, matrixSize, matrixSize);
        double elapsed = timer.elapsedMs();
        
        iteration++;
        double gflops = (2.0 * matrixSize * matrixSize * matrixSize) / (elapsed * 1e6);
        
        auto now = std::chrono::steady_clock::now();
        double totalSec = std::chrono::duration<double>(now - startTime).count();
        
        std::cout << "\r[" << std::setw(8) << iteration << "] " 
                  << std::fixed << std::setprecision(1) << elapsed << " ms | "
                  << std::setprecision(0) << gflops << " GFLOPS | "
                  << "Total: " << std::setprecision(1) << totalSec << "s" << std::flush;
    }
    
    std::cout << "\n\nLoad simulation stopped.\n";
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -l, --load          Run continuous load simulation\n";
    std::cout << "  -s, --size N        Matrix size for load mode (default: 2048)\n";
    std::cout << "  -t, --threads N     Number of threads (default: 0 = all)\n";
    std::cout << "  -h, --help          Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << "              Run benchmark once\n";
    std::cout << "  " << prog << " --load        Continuous load with 2048x2048 matrices\n";
    std::cout << "  " << prog << " -l -s 4096    Continuous load with 4096x4096 matrices\n";
}

int main(int argc, char* argv[]) {
    bool loadMode = false;
    int matrixSize = 2048;
    int threads = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--load") == 0) {
            loadMode = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
            if (i + 1 < argc) matrixSize = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) threads = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "  Apple Neural Engine / AMX Test (BNNS)\n";
    std::cout << "========================================\n";
    
    #if __arm64__
    std::cout << "\nPlatform: Apple Silicon (arm64)\n";
    std::cout << "Hardware accelerators:\n";
    std::cout << "  - AMX (Apple Matrix Coprocessor) - used by BNNS\n";
    std::cout << "  - ANE (Apple Neural Engine) - via Core ML\n";
    std::cout << "  - GPU (Metal Performance Shaders)\n";
    #else
    std::cout << "\nPlatform: Intel (x86_64) - No ANE, using CPU vectorization\n";
    #endif
    
    if (loadMode) {
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        runLoadSimulation(matrixSize, threads);
    } else {
        testMatrixMultiplication(1024, 1024, 1024);
        testMatrixMultiplication(2048, 2048, 2048);
        testConvolution();
        testFullyConnected();
        testActivation();
        testPooling();
        
        std::cout << "\n========================================\n";
        std::cout << "  Tests Complete\n";
        std::cout << "========================================\n";
        std::cout << "\nNote: BNNS uses the AMX coprocessor on Apple Silicon.\n";
        std::cout << "For direct ANE access, use Core ML with a compiled model.\n";
        std::cout << "\nRun with --load for continuous load simulation.\n\n";
    }
    
    return 0;
}
