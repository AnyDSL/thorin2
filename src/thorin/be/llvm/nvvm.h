#ifndef THORIN_BE_LLVM_NVVM_H
#define THORIN_BE_LLVM_NVVM_H

#include "thorin/be/llvm/llvm.h"

namespace thorin {

class Load;

class NVVMCodeGen : public CodeGen {
public:
    NVVMCodeGen(World& world, const Cont2Config&);

protected:
    void emit_function_decl_hook(Continuation*, llvm::Function*) override;
    llvm::FunctionType* convert_fn_type(Continuation*) override;
    llvm::Value* map_param(llvm::Function*, llvm::Argument*, const Def*) override;
    void emit_function_start(llvm::BasicBlock*, Continuation*) override;
    llvm::Value* emit_global(const Global*) override;
    llvm::Value* emit_load(const Load*) override;
    llvm::Value* emit_store(const Store*) override;
    llvm::Value* emit_lea(const LEA*) override;
    Continuation* emit_reserve(const Continuation*) override;
    std::string get_alloc_name() const override { return "malloc"; }

private:
    llvm::Function* get_texture_handle_fun();
    llvm::GlobalVariable* resolve_global_variable(const Def*);

    const Cont2Config& kernel_config_;
    DefMap<llvm::MDNode*> metadata_;
};

}

#endif
