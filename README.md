# MiniAPL

In this assignment you will implement a tiny language
for dense matrix operations called MiniAPL. MiniAPL scripts consist of expressions that build dense tensors (N-dimensional arrays) and print them to the terminal. MiniAPL script files end with `.mapl`.

## Getting LLVM

This project has been tested and developed with LLVM 6.0, you can download it here: [https://releases.llvm.org/download.html#6.0.0](https://releases.llvm.org/download.html#6.0.0).

### Extra LLVM Info

LLVM is infamous for not being backwards compatible, when learning about it or adapting examples on line make sure you are using examples from the same LLVM version, or else be prepared to get compile errors.

Some useful LLVM resources:

* [LLVM 6.0.0 Documentation Home](https://releases.llvm.org/6.0.0/docs/index.html)
* [Kaleidoscope Tutorial](https://releases.llvm.org/6.0.0/docs/tutorial/index.html)
* [LLVM for Graduate Students](https://www.cs.cornell.edu/~asampson/blog/llvm.html)

### C++ Info

This project uses modern C++, including extensive use of `std::unique_ptr` to manage memory.  If you are not familiar with `std::unique_ptr`, see <https://shaharmike.com/cpp/unique-ptr/>. 

## Build Instructions

The project uses a simple Makefile. LLVM comes with an executable called `llvm-config` that is used to locate relevant files and generate compiler flags. Once you have installed and downloaded LLVM set `LLVM_CONFIG` to the path to the llvm-config executable. It should be located in `<path_to_llvm>/bin/llvm-config`.

To build the project and run all tests simply type:

    make

At the terminal in the home directory of this project.

## Grammar and Types

MiniAPL programs are lists of statements. Each statement
ends with a semicolon and is either an assignment, `A = add(B, C);`, or an evaluation `A;`. Evaluations print out their result to the terminal. The grammar is:

    <Program>    := <Statement> <Program>
                  | ;

    <Statement>  := <Expression> ;
                  | <Name> = <Expression> ;

    <Expression> := <Name> ( <Expression>* )
                  | <Name>
                  | <Integer>

For example the following MiniAPL program builds a 1 dimensional, 4 entry array consisting of the numbers 1, 2, 3, and 4. and then addss this array to itself and prints the result to the terminal:

    assign A = mkArray(1, 4, 1, 2, 3, 4);
    assign a = add(A, A);
    a;

The result of running this program is the printout:

    [[2][4][6][8]]

Every value is an array of integers. Each array has a fixed number of dimensions, and each
dimension is of fixed length.

The parser and type-checker have already been implemented for you. Your job is to implement the LLVM code generation
for the project.

To get you started with LLVM we have also implemented infrastructure to JIT LLVM code, generate LLVM IR that prints strings and numbers to the terminal.

## Code Generation

In addition to evaluation statement printing and assignment statements you will be expected to implement the following functions:

  * `mkArray(# of dimensions, <dimension lengths>, <values>)` - Allocate an N dimensional array with the specified dimension lengths. The lengths must be constants.  There should be `# of dimensions` `dimension lengths`, so to construct 2x3x4 array one would write `mkArray(3, 2, 3, 4, ...)`.  Values are specified incrementing the inner most dimension first so `mkArray(2, 2, 3, 0, 1, 2, 3, 4, 5)` would generate `[[[0][1][2]][[3][4][5]]]]`.   
  * `neg(<array>)` - Multiply every element by negative one.
  * `exp(<array>, power)` - Raise every number in the first argument array to the value in the scalar that is the second argument. `power` shall be greater than or equal to `0`.
  * `add(<array>, <array>)` - Add two arrays elementwise.
  * `sub(<array>, <array>)` - Subtract two arrays elementwise.
  * `reduce(<array>)` - Turn an N dimensional array into an N-1 dimensional array by adding up all numbers in the innermost dimension

## Grading and Submission

The only file you should modify is [compiler.cpp](compiler.cpp). Parsing and type-checking are already done for you. Your job is to implement methods / functions labeled with: `// STUDENTS: FILL IN THIS FUNCTION`. Your implementation should generate code whose behavior matches the examples in [./expected_results/](./expected_results/).****
The assignment will be graded as a whole out of 100 points.

### Extra Credit

As extra credit implement the following additional functions:

  * `expand(<array>, size)` - Turn an N dimensional array into an N+1 dimensional array by duplicating the elements of the last dimension `size` times.
  * `concat(<array>, <array>, dimension)` - Concatenate the two input arrays along the given dimension.






