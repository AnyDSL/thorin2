#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

class CPS2DS : public RWPass<CPS2DS, Lam> {
public:
    CPS2DS(PassMan& man)
        : RWPass(man, "cps2ds") {}

    void enter() override;

private:
    Def2Def rewritten_lams;
    Def2Def rewritten_;
    // Def2Def rewritten_bodies_;
    Lam* curr_lam_ = nullptr;

    const Def* rewrite_lam(Lam* lam);
    const Def* rewrite_body(const Def*);

    // void rewrite_lam(Lam* lam);
    // const Def* rewrite_(const Def*);
    // const Def* rewrite_inner(const Def*);
};

} // namespace thorin::direct