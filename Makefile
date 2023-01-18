LLVM_CONFIG ?= llvm-config
CXX ?= clang++

SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build

CXXFLAGS := -std=c++11 -I ./src

mini-apl-tests: mini-apl
	$(BIN_DIR)/$^ ./miniapl_programs/test_file.mapl
	$(BIN_DIR)/$^ ./miniapl_programs/add_file.mapl
	$(BIN_DIR)/$^ ./miniapl_programs/reduce_file.mapl
	$(BIN_DIR)/$^ ./miniapl_programs/exp_file.mapl
	$(BIN_DIR)/$^ ./miniapl_programs/sub_file.mapl
	$(BIN_DIR)/$^ ./miniapl_programs/neg_file.mapl

mini-apl:
	@mkdir -p $(BIN_DIR)
	$(CXX) -g -O0 compiler.cpp `$(LLVM_CONFIG) --cxxflags --ldflags --system-libs --libs all` -o $(BIN_DIR)/mini-apl

clean:
	\rm -rf $(BUILD_DIR) $(BIN_DIR)

test: mini-apl-tests

docker-shell:
	docker compose run --rm -ti shell

docker-test:
	docker compose run --rm test
