FROM ubuntu:18.04

ARG LLVM_VERSION=6.0

RUN apt-get update && \
    apt-get install -y \
    make \
    g++-7 \
    llvm-${LLVM_VERSION} \
    llvm-${LLVM_VERSION}-dev \
    clang-${LLVM_VERSION}

# Create an alias from llvm-config-${LLVM_VERSION} to llvm-config
RUN ln -s /usr/bin/llvm-config-${LLVM_VERSION} /usr/bin/llvm-config

# Create an alias from clang++-${LLVM_VERSION} to clang++
RUN ln -s /usr/bin/clang++-${LLVM_VERSION} /usr/bin/clang++

# Create an alias from g++-7 to g++
RUN ln -s /usr/bin/g++-7 /usr/bin/g++
