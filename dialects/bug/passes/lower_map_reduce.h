#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::bug {

class LowerMapReducePass : public RWPass<LowerMapReducePass, Lam> {
public:
    LowerMapReducePass(PassMan& man)
        : RWPass(man, "lower_map_reduce") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::bug
