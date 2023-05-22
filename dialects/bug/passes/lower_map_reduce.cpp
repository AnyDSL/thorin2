#include "dialects/bug/passes/lower_map_reduce.h"

#include "dialects/bug/bug.h"

namespace thorin::bug {

// TODO: Implement
Ref LowerMapReducePass::rewrite(Ref def) {
    auto& world = def->world();

    std::cerr << "LowerMapReducePass" << std::endl;

    if (auto map_reduce = match<bug::map_reduce>(def)) std::cerr << "LowerMapReducePass :: match" << std::endl;

    return def;
}

} // namespace thorin::bug
