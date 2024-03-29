#pragma once

#include <thorin/pass/eta_exp.h>
#include <thorin/pass/pass.h>

#include "thorin/plug/clos/clos.h"

namespace thorin::plug::clos {

class LowerTypedClosPrep : public FPPass<LowerTypedClosPrep, Lam> {
public:
    LowerTypedClosPrep(PassMan& man)
        : FPPass<LowerTypedClosPrep, Lam>(man, "lower_typed_clos_prep") {}

private:
    Ref rewrite(Ref) override;
    undo_t analyze(Ref) override;

    bool is_esc(Ref def) {
        if (auto [_, lam] = ca_isa_var<Lam>(def); lam && !lam->is_set()) return true;
        return esc_.contains(def);
    }
    undo_t set_esc(Ref);

    DefSet esc_;
};

} // namespace thorin::plug::clos
