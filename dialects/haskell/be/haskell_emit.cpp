#include "dialects/haskell/be/haskell_emit.h"

#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ranges>
#include <string_view>

#include "thorin/axiom.h"
#include "thorin/def.h"
#include "thorin/tuple.h"

#include "thorin/analyses/cfg.h"
#include "thorin/analyses/deptree.h"
#include "thorin/be/emitter.h"
#include "thorin/fe/tok.h"
#include "thorin/util/assert.h"
#include "thorin/util/print.h"
#include "thorin/util/sys.h"

#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

using namespace std::string_literals;

namespace thorin::haskell {

/*
 * helper
 */

static Def* isa_decl(const Def* def) {
    if (auto nom = def->isa_nom()) {
        if (nom->is_external() || nom->isa<Lam>() || (!nom->name().empty() && nom->name() != "_"s)) return nom;
    }
    return nullptr;
}

static std::string id(const Def* def) {
    if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->name();
    return def->unique_name();
}

static std::string_view external(const Def* def) {
    // if (def->is_external()) return ".extern "sv;
    // return ""sv;
    if (def->is_external()) return "(* external *)\n";
    return "";
}

/*
 * Inline + LRPrec
 */

/// This is a wrapper to dump a Def "inline" and print it with all of its operands.
struct Inline2 {
    Inline2(const Def* def, int dump_gid)
        : def_(def)
        , dump_gid_(dump_gid) {}
    Inline2(const Def* def)
        : Inline2(def, def->world().flags().dump_gid) {}

    const Def* operator->() const { return def_; };
    const Def* operator*() const { return def_; };
    explicit operator bool() const {
        if (def_->dep_const()) return true;

        if (auto nom = def_->isa_nom()) {
            if (isa_decl(nom)) return false;
            return true;
        }

        if (auto app = def_->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axiom>()) { return app->callee_type()->num_doms() <= 1; }
            return false;
        }

        return true;
    }

    friend std::ostream& operator<<(std::ostream&, Inline2);

private:
    const Def* def_;
    const int dump_gid_;
};

// TODO prec is currently broken
template<bool L>
struct LRPrec2 {
    LRPrec2(const Def* l, const Def* r)
        : l(l)
        , r(r) {}

    friend std::ostream& operator<<(std::ostream& os, const LRPrec2& p) {
        if constexpr (L) {
            if (Inline2(p.l) && fe::Tok::prec(fe::Tok::prec(p.r))[0] > fe::Tok::prec(p.r))
                return print(os, "({})", Inline2(p.l));
            return print(os, "{}", Inline2(p.l));
        } else {
            if (Inline2(p.r) && fe::Tok::prec(p.l) > fe::Tok::prec(fe::Tok::prec(p.l))[1])
                return print(os, "({})", Inline2(p.r));
            return print(os, "{}", Inline2(p.r));
        }
    }

private:
    const Def* l;
    const Def* r;
};

using LPrec = LRPrec2<true>;
using RPrec = LRPrec2<false>;

std::ostream& operator<<(std::ostream& os, Inline2 u) {
    // return print(os, "Int");
    if (u.dump_gid_ == 2 || (u.dump_gid_ == 1 && !u->isa<Var>() && u->num_ops() != 0)) print(os, "/*{}*/", u->gid());

    if (auto app = u->isa<App>()) {
        auto callee = app->callee();
        if (callee->isa<Idx>()) { return print(os, "int"); }
        if (auto axiom = callee->isa<Axiom>()) {
            if (axiom->name() == "%mem.Ptr") {
                auto type = app->arg()->proj(2, 0_s);
                return print(os, "({} ref)", Inline2(type));
            }
        }

        // if (app->type()->isa<Type>()) { return print(os, "({} {})", Inline2(app->arg()), Inline2(app->callee())); }

        return print(os, "({} {})", Inline2(app->callee()), Inline2(app->arg()));
        // return print(os, "{} {}", LPrec(app->callee(), app), RPrec(app, app->arg()));
        // return print(os, "app");
    }

    if (auto type = u->isa<Type>()) {
        auto level = as_lit(type->level()); // TODO other levels
        return print(os, level == 0 ? "★" : "□");
    } else if (u->isa<Nat>()) {
        return print(os, "int");
    } else if (u->isa<Idx>()) {
        unreachable();
        // return print(os, "Int");
    } else if (auto bot = u->isa<Bot>()) {
        // return print(os, "<⊥:{}>", bot->type());
        // TODO: handle other types
        if (bot->type()->isa<Type>()) return print(os, "unit");
        return print(os, "()");
    } else if (auto top = u->isa<Top>()) {
        // TODO: handle other types
        // return print(os, "<⊤:{}>", top->type());
        if (top->type()->isa<Type>()) return print(os, "unit");
        return print(os, "()");
    } else if (auto axiom = u->isa<Axiom>()) {
        const auto& name = axiom->name();
        // if (name.find("mem.M") != std::string::npos) return print(os, "()");
        if (name == "%mem.M") return print(os, "()");
        return print(os, "{}{}", name[0] == '%' ? "" : "%", name);
    } else if (auto lit = u->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) return print(os, "{}", lit->get());
        if (lit->type()->isa<App>())
            return print(os, "{}:({})", lit->get(), Inline2(lit->type())); // HACK prec magic is broken
        return print(os, "{}:{}", lit->get(), Inline2(lit->type()));
    } else if (auto ex = u->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return print(os, "{}", ex->unique_name());
        if (ex->tuple()->type()->isa<Arr>()) {
            return print(os, "List.nth {} {}", ex->tuple(), ex->index());
        } else {
            // TODO: extract from tuple (const index)
            return print(os, "{}#{}", ex->tuple(), ex->index());
        }
    } else if (auto var = u->isa<Var>()) {
        return print(os, "{}", var->unique_name());
    } else if (auto pi = u->isa<Pi>()) {
        // if (pi->is_cn()) return print(os, "{} -> ()", Inline2(pi->dom()));
        if (auto nom = pi->isa_nom<Pi>(); nom && nom->var())
            // TODO: just give error?
            return print(os, "Π {}: {} → {}", nom->var(), pi->dom(), pi->codom());
        // return print(os, "Π {} → {}", pi->dom(), pi->codom());
        return print(os, "{} -> {}", Inline2(pi->dom()), Inline2(pi->codom()));
    } else if (auto lam = u->isa<Lam>()) {
        // return print(os, "{}, {}", lam->filter(), lam->body());
        return print(os, "{}", lam->body());
        // } else if (auto app = u->isa<App>()) {
    } else if (auto sigma = u->isa<Sigma>()) {
        if (auto nom = sigma->isa_nom<Sigma>(); nom && nom->var()) {
            // TODO:
            size_t i = 0;
            return print(os, "({, })", Elem(sigma->ops(), [&](auto op) {
                             if (auto v = nom->var(i++))
                                 print(os, "{}: {}", v, Inline2(op));
                             else
                                 print(os, "_: {}", Inline2(op));
                             //  os << op;
                         }));
        }
        // return print(os, "({, })", sigma->ops());
        return print(os, "({, })", Elem(sigma->ops(), [&](auto op) { print(os, "{}", Inline2(op)); }));
        // return print(os, "({, })", Elem(sigma->ops(), [&](auto op) { print(os, "A"); }));
    } else if (auto tuple = u->isa<Tuple>()) {
        if (tuple->type()->isa<Arr>()) {
            print(os, "[{, }]", tuple->ops());
            return os;
            // TODO: nom
            // return tuple->type()->isa_nom() ? print(os, ":{}", tuple->type()) : os;
        } else {
            print(os, "({, })", tuple->ops());
            return os;
            // return tuple->type()->isa_nom() ? print(os, ":{}", tuple->type()) : os;
        }
    } else if (auto arr = u->isa<Arr>()) {
        if (auto nom = arr->isa_nom<Arr>(); nom && nom->var())
            // TODO: impossible
            return print(os, "«{}: {}; {}»", nom->var(), nom->shape(), nom->body());
        // return print(os, "«{}; {}»", arr->shape(), arr->body());
        // return print(os, "[ {} ]", arr->body());
        return print(os, "({} list)", arr->body());
    } else if (auto pack = u->isa<Pack>()) {
        // TODO: generate list
        if (auto nom = pack->isa_nom<Pack>(); nom && nom->var())
            return print(os, "‹{}: {}; {}›", nom->var(), nom->shape(), nom->body());
        return print(os, "‹{}; {}›", pack->shape(), pack->body());
    } else if (auto proxy = u->isa<Proxy>()) {
        // TODO:
        // return print(os, ".proxy#{}#{} {, }", proxy->pass(), proxy->tag(), proxy->ops());
        assert(0);
    } else if (auto bound = u->isa<Bound>()) {
        // TODO:
        assert(0);
        // auto op = bound->isa<Join>() ? "∪" : "∩";
        // if (auto nom = u->isa_nom()) print(os, "{}{}: {}", op, nom->unique_name(), nom->type());
        // return print(os, "{}({, })", op, bound->ops());
    }

    // if (u->node_name().find("mem.M") != std::string::npos) { return print(os, "()"); }

    // other
    if (u->flags() == 0) return print(os, ".{} ({, })", u->node_name(), u->ops());
    return print(os, ".{}#{} ({, })", u->node_name(), u->flags(), u->ops());
}

/*
 * Dumper
 */

/// This thing operates in two modes:
/// 1. The output of decls is driven by the DepTree.
/// 2. Alternatively, decls are output as soon as they appear somewhere during recurse%ing.
///     Then, they are pushed to Dumper::noms.
class Dumper {
public:
    Dumper(std::ostream& os, const DepTree* dep = nullptr)
        : os(os)
        , dep(dep) {}

    void dump(Def*);
    void dump(Lam*);
    void dump_let(const Def*);
    void dump_ptrn(const Def*, const Def*, bool toplevel = true);
    void recurse(const DepNode*);
    void recurse(const Def*, bool first = false);

    std::ostream& os;
    const DepTree* dep;
    Tab tab;
    unique_queue<NomSet> noms;
    DefSet defs;
};

void Dumper::dump(Def* nom) {
    if (auto lam = nom->isa<Lam>()) {
        dump(lam);
        return;
    }

    auto nom_prefix = [&](const Def* def) {
        if (def->isa<Sigma>()) return ".Sigma";
        if (def->isa<Arr>()) return ".Arr";
        if (def->isa<Pack>()) return ".pack";
        if (def->isa<Pi>()) return ".Pi";
        unreachable();
    };

    auto nom_op0 = [&](const Def* def) -> std::ostream& {
        if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
        if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->shape());
        if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->shape());
        if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
        unreachable();
    };

    if (!nom->is_set()) {
        tab.print(os, "{}: {} = {{ <unset> }};", id(nom), nom->type());
        return;
    }

    tab.print(os, "{} {}{}: {}", nom_prefix(nom), external(nom), id(nom), nom->type());
    nom_op0(nom);
    if (nom->var()) { // TODO rewrite with dedicated methods
        if (auto e = nom->num_vars(); e != 1) {
            print(os, "{, }", Elem(nom->vars(), [&](auto def) {
                      if (def)
                          os << def->unique_name();
                      else
                          os << "<TODO>";
                  }));
        } else {
            print(os, ", @{}", nom->var()->unique_name());
        }
    }
    tab.println(os, " = {{");
    ++tab;
    if (dep) recurse(dep->nom2node(nom));
    recurse(nom);
    tab.print(os, "{, }\n", nom->ops());
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump(Lam* lam) {
    // TODO filter
    auto ptrn = [&](auto&) { dump_ptrn(lam->var(), lam->type()->dom()); };

    // if (lam->type()->is_cn()) {
    //     tab.println(os, "let {}{} {} = {{", external(lam), id(lam), ptrn);
    // } else {

    // TODO: handle mutual recursion
    tab.println(os, "{}let rec {} {} = ", external(lam), id(lam), ptrn); //, lam->type()->codom());
    // }

    ++tab;
    if (lam->is_set()) {
        if (dep) recurse(dep->nom2node(lam));
        recurse(lam->filter());
        recurse(lam->body(), true);
        // TODO:
        // if (lam->body()->isa_nom())
        //     tab.print(os, "{}\n", lam->body());
        // else
        tab.print(os, "{}\n", Inline2(lam->body()));
    } else {
        tab.print(os, " <unset>\n");
    }
    --tab;
    // tab.print(os, "}};\n");

    // TODO: not for toplevel
    tab.print(os, "in\n");
}

void Dumper::dump_let(const Def* def) {
    // TODO: type vs Inline type
    tab.print(os, "let {}: {} = {} in\n", def->unique_name(), Inline2(def->type()), Inline2(def, 0));
    // tab.print(os, "let {}: {} = {} in\n", def->unique_name(), def->type(), Inline2(def, 0));
}

void Dumper::dump_ptrn(const Def* def, const Def* type, bool toplevel) {
    if (!def) {
        // os << type;
        os << "_";
    } else {
        auto projs = def->projs();
        if ((projs.size() == 1 || std::ranges::all_of(projs, [](auto def) { return !def; }))) {
            // print(os, "({}: {})", def->unique_name(), type);
            // print(os, "({}: {})", def->unique_name(), Inline2(type));
            // print(os, "A");
            if (toplevel) {
                print(os, "({} : {})", def->unique_name(), Inline2(type));
            } else {
                print(os, "{}", def->unique_name());
            }
        } else {
            size_t i = 0;
            // print(os, "{}::[{, }]", def->unique_name(),
            //       Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++)); }));
            print(os, "((({, }) as {}): {})", Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++), false); }),
                  def->unique_name(), Inline2(type));
        }
    }
}

void Dumper::recurse(const DepNode* node) {
    for (auto child : node->children()) {
        if (auto nom = isa_decl(child->nom())) dump(nom);
    }
}

void Dumper::recurse(const Def* def, bool first /*= false*/) {
    if (auto nom = isa_decl(def)) {
        if (!dep) noms.push(nom);
        return;
    }

    if (!defs.emplace(def).second) return;

    for (auto op : def->partial_ops().skip_front()) { // ignore dbg
        if (!op) continue;
        recurse(op);
    }

    if (!first && !Inline2(def)) dump_let(def);
}

/*
 * Def
 */

/// This will stream @p def as an operand.
/// This is usually `id(def)` unless it can be displayed Inline.
// std::ostream& operator<<(std::ostream& os, const Def* def) {
//     if (def == nullptr) return os << "<nullptr>";
//     if (Inline(def)) return os << Inline(def);
//     return os << id(def);
// }

// std::ostream& Def::stream(std::ostream& os, int max) const {
//     auto freezer = World::Freezer(world());
//     auto dumper  = Dumper(os);

//     if (max == 0) {
//         os << this << std::endl;
//     } else if (auto nom = isa_decl(this)) {
//         dumper.noms.push(nom);
//     } else {
//         dumper.recurse(this);
//         dumper.tab.print(os, "{}\n", Inline(this));
//         --max;
//     }

//     for (; !dumper.noms.empty() && max > 0; --max) dumper.dump(dumper.noms.pop());

//     return os;
// }

// void Def::dump() const { std::cout << this << std::endl; }
// void Def::dump(int max) const { stream(std::cout, max) << std::endl; }

// void Def::write(int max, const char* file) const {
//     auto ofs = std::ofstream(file);
//     stream(ofs, max);
// }

// void Def::write(int max) const {
//     auto file = id(this) + ".thorin"s;
//     write(max, file.c_str());
// }

/*
 * World
 */

// void World::dump(std::ostream& os) const {
//     auto freezer = World::Freezer(*this);
//     auto old_gid = curr_gid();

//     if (flags().dump_recursive) {
//         auto dumper = Dumper(os);
//         for (const auto& [_, nom] : externals()) dumper.noms.push(nom);
//         while (!dumper.noms.empty()) dumper.dump(dumper.noms.pop());
//     } else {
//         auto dep    = DepTree(*this);
//         auto dumper = Dumper(os, &dep);

//         for (const auto& import : imported()) print(os, ".import {};\n", import);
//         dumper.recurse(dep.root());
//     }

//     assertf(old_gid == curr_gid(), "new nodes created during dump. old_gid: {}; curr_gid: {}", old_gid, curr_gid());
// }

// void World::dump() const { dump(std::cout); }

// void World::debug_dump() const {
//     if (log().level == Log::Level::Debug) dump(*log().ostream);
// }

// void World::write(const char* file) const {
//     auto ofs = std::ofstream(file);
//     dump(ofs);
// }

// void World::write() const {
//     auto file = std::string(name()) + ".thorin"s;
//     write(file.c_str());
// }

void emit(World& w, std::ostream& os) {
    auto freezer = World::Freezer(w);
    auto old_gid = w.curr_gid();

    // if (w.flags().dump_recursive) {
    //     auto dumper = Dumper(os);
    //     for (const auto& [_, nom] : externals()) dumper.noms.push(nom);
    //     while (!dumper.noms.empty()) dumper.dump(dumper.noms.pop());
    // } else {
    auto dep    = DepTree(w);
    auto dumper = Dumper(os, &dep);

    // for (const auto& import : w.imported()) print(os, ".import {};\n", import);
    dumper.recurse(dep.root());
    // }

    // assertf(old_gid == curr_gid(), "new nodes created during dump. old_gid: {}; curr_gid: {}", old_gid, curr_gid());
}

} // namespace thorin::haskell
