#ifndef THORIN_DIALECTS_AFFINE_AFFINE_H
#define THORIN_DIALECTS_AFFINE_AFFINE_H

#include "thorin/world.h"

#include "dialects/affine.h"

namespace thorin::affine {

/// Returns the affine_for axiom applied with \a params.
/// See documentation for %affine.For axiom in @ref affine.
inline const Def* fn_for(World& w, Defs params) {
    return w.app(w.ax<affine::For>(), {w.lit_nat(width2mod(32)), w.lit_nat(params.size()), w.tuple(params)});
}

/// Returns a fully applied affine_for axiom.
/// See documentation for %affine.For axiom in @ref affine.
inline const Def* op_for(World& w,
                         const Def* mem,
                         const Def* begin,
                         const Def* end,
                         const Def* step,
                         Defs inits,
                         const Def* body,
                         const Def* brk) {
    DefArray types(inits.size(), [&](size_t i) { return inits[i]->type(); });
    return w.app(fn_for(w, types), {mem, begin, end, step, w.tuple(inits), body, brk});
}
} // namespace thorin::affine

#endif