#include "thorin/plug/autodiff/pass/autodiff_eval.h"

#include <iostream>

#include <thorin/lam.h>

#include "thorin/plug/affine/affine.h"
#include "thorin/plug/autodiff/autodiff.h"
#include "thorin/plug/core/core.h"
#include "thorin/plug/mem/mem.h"

namespace thorin::plug::autodiff {

// TODO: maybe use template (https://codereview.stackexchange.com/questions/141961/memoization-via-template) to memoize
Ref AutoDiffEval::augment(Ref def, Lam* f, Lam* f_diff) {
    if (auto i = augmented.find(def); i != augmented.end()) return i->second;
    augmented[def] = augment_(def, f, f_diff);
    return augmented[def];
}

Ref AutoDiffEval::derive(Ref def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
}

Ref AutoDiffEval::rewrite(Ref def) {
    if (auto ad_app = match<ad>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);

        if (arg->isa<Lam>()) return derive(arg);

        // TODO: handle operators analogous

        assert(0 && "not implemented");
        return def;
    }

    return def;
}

} // namespace thorin::plug::autodiff
