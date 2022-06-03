#include "thorin/be/h/h.h"

#include <sstream>

#include "thorin/axiom.h"
#include "thorin/error.h"

#include "thorin/util/print.h"

namespace thorin::h {

void Bootstrapper::emit(std::ostream& h) {
    tab.print(h, "#ifndef THORIN_{}_H\n", dialect_);
    tab.print(h, "#define THORIN_{}_H\n\n", dialect_);
    tab.print(h, "#include \"thorin/axiom.h\"\n"
                 "#include \"thorin/dialects.h\"\n"
                 "#include \"thorin/tables.h\"\n\n");

    tab.print(h, "namespace thorin {{\nnamespace {} {{\n\n", dialect_);

    // todo: can we assume mangle is successful?
    dialect_t dialect_id = *Axiom::mangle(dialect_);
    std::vector<std::ostringstream> normalizers;

    h << std::hex;
    tab.print(h, "static constexpr dialect_t id = 0x{};\n\n", dialect_id);

    tag_t tag = 0;
    for (const auto& ax : axioms) {
        tab.print(h, "enum class {} : flags_t {{\n", ax.tag);
        ++tab;
        flags_t ax_id = dialect_id | (tag++ << 8u);
        if (auto& subs = ax.subs; !subs.empty()) {
            tab.print(h, "base_ = 0x{},\n", ax_id);
            for (const auto& aliases : subs) {
                const auto& sub = aliases.front();
                tab.print(h, "{} = 0x{},\n", sub, ax_id++);
                for (size_t i = 1; i < aliases.size(); ++i) tab.print(h, "{} = {},\n", aliases[i], sub);

                if (!ax.normalizer.empty())
                    print(normalizers.emplace_back(), "normalizers[flags_t({}::{})] = &{}<{}::{}>;\n", ax.tag, sub,
                          ax.normalizer, ax.tag, sub);
            }
        } else {
            tab.print(h, "id_ = 0x{},\n", ax_id);

            if (!ax.normalizer.empty())
                print(normalizers.emplace_back(), "normalizers[flags_t({}::id_)] = &{};\n", ax.tag, ax.normalizer);
        }
        --tab;
        tab.print(h, "}};\n\n");

        tab.print(h,
                  "inline bool operator==({} enm, flags_t flags) {{ return static_cast<flags_t>(enm) == flags; }}\n\n",
                  ax.tag);

        if (!ax.normalizer.empty()) {
            if (auto& subs = ax.subs; !subs.empty()) {
                tab.print(h, "template<{}>\nconst Def* {}(const Def*, const Def*, const Def*, const Def*);\n\n", ax.tag,
                          ax.normalizer);
            } else {
                tab.print(h, "const Def* {}(const Def*, const Def*, const Def*, const Def*);\n\n", ax.normalizer);
            }
        }
    }

    if (!normalizers.empty()) {
        tab.print(h, "inline void register_normalizers(Normalizers& normalizers) {{\n");
        ++tab;
        for (const auto& normalizer : normalizers) tab.print(h, "{}", normalizer.str());
        --tab;
        tab.print(h, "}}\n\n");
    }

    tab.print(h, "}} // namespace {}\n\n", dialect_);
    tab.print(h, "namespace detail {{\n");

    for (const auto& ax : axioms)
        if (!ax.pi)
            tab.print(h,
                      "template<>\n"
                      "struct Enum2DefImpl<{}::{}> {{\n"
                      "    using type = Axiom;\n"
                      "}};\n",
                      ax.dialect, ax.tag);

    tab.print(h, "}} // namespace detail\n");
    tab.print(h, "}} // namespace thorin\n\n");

    tab.print(h, "#endif\n");
}

} // namespace thorin::h
