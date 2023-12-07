#include "thorin/plug/ll/ll.h"

#include <thorin/plugin.h>

#include "thorin/plug/ll/emitter.h"

using namespace thorin;

/// Heart of this Plugin.
/// Registers Pass%es in the different optimization Phase%s as well as normalizers for the Axiom%s.
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"ll", nullptr, nullptr, [](Backends& backends) { backends["ll"] = &plug::ll::emit; }};
}
