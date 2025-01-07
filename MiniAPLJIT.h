#ifndef LLVM_EXECUTIONENGINE_ORC_MINIAPLJIT_H
#define LLVM_EXECUTIONENGINE_ORC_MINIAPLJIT_H

#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include <memory>
#include <string_view>

namespace llvm {
namespace orc {

// Houses the components for the Mini APL JIT. (This is really just a wrapper
// for the LLJIT, which was initially a lot more complex to set up.) We refer
// curious readers to the OrcV2 documentation: https://llvm.org/docs/ORCv2.html
class MiniAPLJIT {
public:
  MiniAPLJIT(std::unique_ptr<LLJIT> J) : JIT(std::move(J)) {}

  // Creates a mini-APL JIT from the LLJIT builder.
  static Expected<MiniAPLJIT> Create() {
    // Initialize LLVM's native target and ASM printer.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::Expected<std::unique_ptr<LLJIT>> J = LLJITBuilder().create();
    if (!J) {
      return J.takeError();
    }
    MiniAPLJIT miniJIT(std::move(*J));
    return miniJIT;
  }

  // Returns the LLVM target triple, i.e., <arch>-<vendor>-<sys>-<abi>
  llvm::Triple getTargetTriple() { return JIT->getTargetTriple(); }

  // Adds this thread safe-module to the JIT. Returns error upon failure.
  Error addIRModule(ThreadSafeModule TSM) {
    return JIT->addIRModule(std::move(TSM));
  }

  // Find a symbol in the JIT. If none is found, return error.
  Expected<ExecutorAddr> lookup(std::string_view name) {
    return JIT->lookup(name);
  }

private:
  std::unique_ptr<LLJIT> JIT;
};

} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_MINIAPLJIT_H