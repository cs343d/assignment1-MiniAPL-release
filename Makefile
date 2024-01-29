LLVM_CONFIG ?= llvm-config
CXX ?= clang++

SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build

CXXFLAGS := -std=c++11 -I ./src

mini-apl-tests: mini-apl
	$(BIN_DIR)/$^ ./miniapl_programs/test_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/test_file_output.txt; then echo "Success!"; else echo "test diff mismatch"; fi;
	$(BIN_DIR)/$^ ./miniapl_programs/add_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/add_file_output.txt; then echo "Success!"; else echo "add diff mismatch"; fi;
	$(BIN_DIR)/$^ ./miniapl_programs/reduce_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/reduce_file_output.txt; then echo "Success!"; else echo "reduce diff mismatch"; fi;
	$(BIN_DIR)/$^ ./miniapl_programs/exp_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/exp_file_output.txt; then echo "Success!"; else echo "exp diff mismatch"; fi;
	$(BIN_DIR)/$^ ./miniapl_programs/sub_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/sub_file_output.txt; then echo "Success!"; else echo "sub diff mismatch"; fi;
	$(BIN_DIR)/$^ ./miniapl_programs/neg_file.mapl > temp.txt
	@if diff -q temp.txt ./expected_results/neg_file_output.txt; then echo "Success!"; else echo "neg diff mismatch"; fi;
	@rm temp.txt

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
