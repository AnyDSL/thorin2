#include "foo.h"

#include <thorin/config.h>
#include <thorin/dialect.h>

#include "thorin/pass/pipelinebuilder.h"

namespace thorin {

const Def* Foo::rewrite(const Def* def) {
    std::cout << "hi from: '" << name() << "'\n";
    def->dump();
    return def;
}

} // namespace thorin

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"foo", [](thorin::PipelineBuilder& builder) {
                builder.extend_opt_phase([](thorin::PassMan& man) { man.add<thorin::Foo>(); });
            }};
}