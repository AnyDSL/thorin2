#include "dialects/bug/bug.h"

#include <thorin/pass/pipelinebuilder.h>
#include <thorin/plugin.h>

#include "dialects/bug/passes/lower_map_reduce.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"bug", [](Normalizers& normalizers) { bug::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass<bug::lower_map_reduce, bug::LowerMapReducePass>(passes);
                // ...
            },
            nullptr};
}
