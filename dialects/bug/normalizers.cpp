#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/bug/bug.h"

namespace thorin::bug {

Ref normalize_len(Ref type, Ref callee, Ref arg) {
    auto& world    = type->world();
    auto [n, S, N] = arg->projs<3>();

    if (auto m = S->isa_lit_arity()) {
        nat_t length = 1;
        for (size_t i = 0; i != *m; i++) length *= Lit::as(S->proj(*m, i));

        return world.lit_nat(length);
    }

    return world.raw_app(type, callee, arg);
}

Ref normalize_dim(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    auto index  = arg->proj(0);

    if (auto app = callee->isa<App>()) {
        auto [n, S, N] = app->arg()->projs<3>();

        return world.extract(S, index);
    }

    return world.raw_app(type, callee, arg);
}

THORIN_bug_NORMALIZER_IMPL

} // namespace thorin::bug
