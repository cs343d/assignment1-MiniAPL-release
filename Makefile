LLVM_CONFIG ?= llvm-config
CXX ?= clang++

SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build

CXXFLAGS := -std=c++17 -I ./src

mini-apl-tests: mini-apl-build
	$(BIN_DIR)/$^ ./miniapl_programs/test.mapl > yours.txt
	@if diff yours.txt ./expected_results/test.txt; then echo "Success!"; else echo "Error: test diff mismatch"; fi; echo "\n"
	$(BIN_DIR)/$^ ./miniapl_programs/add.mapl > yours.txt
	@if diff yours.txt ./expected_results/add.txt; then echo "Success!"; else echo "Error: add diff mismatch"; fi; echo "\n"
	$(BIN_DIR)/$^ ./miniapl_programs/reduce.mapl > yours.txt
	@if diff yours.txt ./expected_results/reduce.txt; then echo "Success!"; else echo "Error: reduce diff mismatch"; fi; echo "\n"
	$(BIN_DIR)/$^ ./miniapl_programs/exp.mapl > yours.txt
	@if diff yours.txt ./expected_results/exp.txt; then echo "Success!"; else echo "Error: exp diff mismatch"; fi; echo "\n"
	$(BIN_DIR)/$^ ./miniapl_programs/sub.mapl > yours.txt
	@if diff yours.txt ./expected_results/sub.txt; then echo "Success!"; else echo "Error: sub diff mismatch"; fi; echo "\n"
	$(BIN_DIR)/$^ ./miniapl_programs/neg.mapl > yours.txt
	@if diff yours.txt ./expected_results/neg.txt; then echo "Success!"; else echo "Error: neg diff mismatch"; fi; echo "\n"
	@rm yours.txt

mini-apl-build:
	@mkdir -p $(BIN_DIR)
	$(CXX) -g -O0 compiler.cpp `$(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs all` -o $(BIN_DIR)/mini-apl-build

mini-apl-debug-build:
	@mkdir -p $(BIN_DIR)
	$(CXX) -g -O0 compiler.cpp `$(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs all` -fsanitize=address -o $(BIN_DIR)/mini-apl-debug-build

# Example: 
#   make mini-apl-debug program=sub
mini-apl-debug: mini-apl-debug-build
	$(BIN_DIR)/$^ ./miniapl_programs/$(program).mapl -d

clean:
	\rm -rf $(BUILD_DIR) $(BIN_DIR)

test: mini-apl-tests

docker-shell:
	docker compose run --rm -ti shell

docker-test:
	docker compose run --rm test
