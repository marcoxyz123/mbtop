# Makefile for Apple Neural Engine Tests

CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
FRAMEWORKS = -framework Accelerate

OBJCXX = clang++
OBJCXXFLAGS = -std=c++17 -O3 -Wall -Wextra
COREML_FRAMEWORKS = -framework Foundation -framework CoreML -framework Accelerate -framework CoreVideo

.PHONY: all clean run setup

all: ane_bnns_test ane_coreml_test ane_coreml_load

ane_bnns_test: ane_bnns_test.cpp
	$(CXX) $(CXXFLAGS) $(FRAMEWORKS) -o $@ $<

ane_coreml_test: ane_coreml_test.mm
	$(OBJCXX) $(OBJCXXFLAGS) $(COREML_FRAMEWORKS) -o $@ $<

ane_coreml_load: ane_coreml_load.mm
	$(OBJCXX) $(OBJCXXFLAGS) $(COREML_FRAMEWORKS) -o $@ $<

run: ane_bnns_test
	@./ane_bnns_test

run-load: ane_bnns_test
	@./ane_bnns_test --load

run-ane: ane_coreml_load
	@./ane_coreml_load --load

setup:
	@echo "Downloading MobileNetV2..."
	curl -L -o MobileNetV2.mlmodel 'https://ml-assets.apple.com/coreml/models/Image/ImageClassification/MobileNetV2/MobileNetV2.mlmodel'
	@echo "Compiling model..."
	xcrun coremlcompiler compile MobileNetV2.mlmodel .
	@echo "Done! Run 'make run-ane' to test ANE."

clean:
	rm -f ane_bnns_test ane_coreml_test ane_coreml_load
	rm -rf *.mlmodelc *.mlmodel
