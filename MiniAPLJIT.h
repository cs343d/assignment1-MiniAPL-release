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

class MiniAPLJIT {
public:
  MiniAPLJIT(std::unique_ptr<LLJIT> J) : JIT(std::move(J)) {}

  // Creates a mini-APL JIT from the LLJIT builder.
  static Expected<MiniAPLJIT> Create() {
    // Initialize LLVM's native target and asm printer.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // Create LLJIT instance.
    llvm::Expected<std::unique_ptr<LLJIT>> J = LLJITBuilder().create();
    if (!J) {
      return J.takeError();
    }
    MiniAPLJIT miniJIT(std::move(*J));
    return miniJIT;
  }

  // Returns the LLVM target triple, i.e., <arch>-<vendor>-<sys>-<abi>
  llvm::Triple getTargetTriple() { return JIT->getTargetTriple(); }

  Error addIRModule(ThreadSafeModule TSM) {
    return JIT->addIRModule(std::move(TSM));
  }

  // Find a symbol in the JIT.
  Expected<ExecutorAddr> lookup(std::string_view name) {
    return JIT->lookup(name);
  }

private:
  std::unique_ptr<LLJIT> JIT;
};

} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_MINIAPLJIT_H