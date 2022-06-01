#include "dialects/mem/passes/rw/alloc2malloc.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    if (auto alloc = mem::isa<mem::mem_alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return op_malloc(pointee, alloc->arg(), alloc->dbg());
    } else if (auto slot = mem::isa<mem::mem_slot>(def)) {
        auto [pointee, addr_space] = slot->decurry()->args<2>();
        auto [mem, id]             = slot->args<2>();
        return op_mslot(pointee, mem, id, slot->dbg());
    }

    return def;
}

PassTag* Alloc2Malloc::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::mem
