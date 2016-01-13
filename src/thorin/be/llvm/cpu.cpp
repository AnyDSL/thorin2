#include "thorin/be/llvm/cpu.h"

#include <llvm/ADT/Triple.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>

namespace thorin {

CPUCodeGen::CPUCodeGen(World& world)
    : CodeGen(world, llvm::Triple(llvm::sys::getProcessTriple()).isOSWindows() ? llvm::Function::DLLExportLinkage : llvm::Function::ExternalLinkage,
              llvm::CallingConv::C, llvm::CallingConv::C, llvm::CallingConv::C)
{
    llvm::InitializeNativeTarget();
    auto triple_str = llvm::sys::getDefaultTargetTriple();
    module_->setTargetTriple(triple_str);
    llvm::EngineBuilder builder(module_);
    std::unique_ptr<llvm::TargetMachine> machine(builder.selectTarget());
    assert(machine && "can't create machine for host architecture");
    module_->setDataLayout(machine->getDataLayout()->getStringRepresentation());
}

}
