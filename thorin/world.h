#ifndef THORIN_WORLD_H
#define THORIN_WORLD_H

#include <memory>
#include <sstream>
#include <string>

#include "thorin/def.h"
#include "thorin/tables.h"
#include "thorin/util/iterator.h"
#include "thorin/util/symbol.h"
#include "thorin/check.h"

namespace thorin {

class TypeError : public std::logic_error {
public:
    TypeError(std::string&& what)
        : std::logic_error(std::move(what))
    {}
};

class World;
const Def* infer_shape(World&, const Def* def);
std::array<const Def*, 2> infer_width_and_shape(World&, const Def*);

//------------------------------------------------------------------------------

class World {
public:
    struct DefHash {
        static uint64_t hash(const Def* def) { return def->hash(); }
        static bool eq(const Def* d1, const Def* d2) { return d2->equal(d1); }
        static const Def* sentinel() { return (const Def*)(1); }
    };

    typedef HashSet<const Def*, DefHash> DefSet;

    struct BreakHash {
        static uint64_t hash(size_t i) { return i; }
        static bool eq(size_t i1, size_t i2) { return i1 == i2; }
        static size_t sentinel() { return size_t(-1); }
    };

    typedef HashSet<size_t, BreakHash> Breakpoints;

    World& operator=(const World&) = delete;
    World(const World&) = delete;

    World(Debug dbg = {});
    ~World();

    //@{ get Debug information
    Debug& debug() const { return debug_; }
    Loc loc() const { return debug_; }
    Symbol name() const { return debug().name(); }
    //@}

    //@{ create universe and kinds
    const Universe* universe() const { return universe_; }
    const Kind* kind(Def::Tag tag, const Def* q) {
        assert(tag == Def::Tag::KindArity || tag == Def::Tag::KindMulti || tag == Def::Tag::KindStar);
        return unify<Kind>(1, *this, tag, q);
    }
    const Axiom* kind_qualifier() const { return kind_qualifier_; }
    const Kind* kind_arity(Qualifier q = Qualifier::u) const { return kind_arity_[size_t(q)]; }
    const Kind* kind_multi(Qualifier q = Qualifier::u) const { return kind_multi_[size_t(q)]; }
    const Kind* kind_star (Qualifier q = Qualifier::u) const { return kind_star_ [size_t(q)]; }
    const Kind* kind_arity(const Def* def) { auto q = get_qualifier(def); return q ? kind_arity(*q) : kind(Def::Tag::KindArity, def); }
    const Kind* kind_multi(const Def* def) { auto q = get_qualifier(def); return q ? kind_multi(*q) : kind(Def::Tag::KindMulti, def); }
    const Kind* kind_star (const Def* def) { auto q = get_qualifier(def); return q ? kind_star (*q) : kind(Def::Tag::KindStar,  def); }
    //@}

    //@{ create Pi
    const Pi* pi(const Def* domain, const Def* codomain, Debug dbg = {}) {
        return pi(lit(Qualifier::u), domain, codomain, dbg);
    }
    const Pi* pi(const Def* q, const Def* domain, const Def* codomain, Debug dbg = {});
    const Pi* cn(const Def* domain, Debug dbg = {}) { return pi(domain, bot(kind_star()), dbg); }
    const Pi* cn(Defs domain, Debug dbg = {}) { return cn(sigma(domain), dbg); }
    //@}

    //@{ create Lambda
    const Def* lambda(const Def* domain, const Def* body, Debug dbg = {}) {
        return lambda(nullptr, domain, lit_true(), body, dbg);
    }
    const Def* lambda(const Def* q, const Def* domain, const Def* filter, const Def* body, Debug dbg = {});
    /// @em nominal lambda --- may be recursive
    Lambda* lambda(const Pi* type, Debug dbg = {}) {
        assertf(type->free_vars().none_begin(1),
                "function type {} of a nominal lambda may not contain free variables", type);
        //TODO
        //return insert<Lambda>(2, type, dbg)->Def::set(0, lit_false())->Def::set(1, bottom(unit()))->as_lambda();
        return insert<Lambda>(2, type, dbg);
    }
    const Param* param(const Lambda* lambda, Debug dbg = {}) { return unify<Param>(1, lambda->domain(), lambda, dbg); }
    //@}

    //@{ create App
    const Def* app(const Def* callee, Defs args, Debug dbg = {}) { return app(callee, tuple(args, dbg), dbg); }
    const Def* app(const Def* callee, const Def* arg, Debug dbg = {});
    const Def* raw_app(const Def* callee, Defs args, Debug dbg = {}) { return raw_app(callee, tuple(args, dbg), dbg); }
    const Def* raw_app(const Def* callee, const Def* arg, Debug dbg = {}) {
        auto type = callee->type()->as<Pi>()->apply(arg);
        return unify<App>(2, type, callee, arg, dbg);
    }
    //@}

    //@{ create Units
    const Def* unit(Qualifier q = Qualifier::u) { return unit_[size_t(q)]; }
    const Def* unit(const Def* def) { auto q = get_qualifier(def); return q ? unit(*q) : lit_arity(def, 1); }
    const Def* unit_kind(Qualifier q = Qualifier::u) { return variadic(lit_arity(0), kind_star(q)); }
    const Def* unit_kind(const Def* q) { return variadic(lit_arity(0), kind_star(q)); }
    //@}

    //@{ create Sigma
    /// @em structural Sigma types or kinds
    const Def* sigma(Defs defs, Debug dbg = {}) { return sigma(nullptr, defs, dbg); }
    const Def* sigma(const Def* q, Defs, Debug dbg = {});
    /// Nominal sigma types or kinds
    Sigma* sigma(const Def* type, size_t num_ops, Debug dbg = {}) { return insert<Sigma>(num_ops, type, num_ops, dbg); }
    /// @em nominal Sigma of type Star
    Sigma* sigma_type(size_t num_ops, Debug dbg = {}) { return sigma_type(lit(Qualifier::u), num_ops, dbg); }
    /// @em nominal Sigma of type Star
    Sigma* sigma_type(const Def* q, size_t num_ops, Debug dbg = {}) { return sigma(kind_star(q), num_ops, dbg); }
    /// @em nominal Sigma of type Universe
    Sigma* sigma_kind(size_t num_ops, Debug dbg = {}) { return insert<Sigma>(num_ops, universe(), num_ops, dbg); }
    //@}

    //@{ create Variadic
    const Def* variadic(const Def* arities, const Def* body, Debug dbg = {});
    const Def* variadic(Defs arities, const Def* body, Debug dbg = {});
    const Def* variadic(u64 a, const Def* body, Debug dbg = {}) { return variadic(lit_arity(Qualifier::u, a, dbg), body, dbg); }
    const Def* variadic(ArrayRef<u64> a, const Def* body, Debug dbg = {}) {
        return variadic(DefArray(a.size(), [&](auto i) { return lit_arity(Qualifier::u, a[i], dbg); }), body, dbg);
    }
    //@}

    //@{ create unit values
    const Def* val_unit(Qualifier q = Qualifier::u) { return unit_val_[size_t(q)]; }
    const Def* val_unit(const Def* def) { auto q = get_qualifier(def); return q ? val_unit(*q) : index_zero(lit_arity(def, 1)); }
    const Def* val_unit_kind(Qualifier q = Qualifier::u) { return pack(lit_arity(0), unit(q)); }
    const Def* val_unit_kind(const Def* q) { return pack(lit_arity(0), unit(q)); }
    //@}

    //@{ create Pack
    const Def* pack(const Def* arities, const Def* body, Debug dbg = {});
    const Def* pack(Defs arities, const Def* body, Debug dbg = {});
    const Def* pack(u64 a, const Def* body, Debug dbg = {}) { return pack(lit_arity(Qualifier::u, a, dbg), body, dbg); }
    const Def* pack(ArrayRef<u64> a, const Def* body, Debug dbg = {}) {
        return pack(DefArray(a.size(), [&](auto i) { return lit_arity(Qualifier::u, a[i], dbg); }), body, dbg);
    }
    //@}

    //@{ create Extract
    const Def* extract(const Def* def, const Def* index, Debug dbg = {});
    const Def* extract(const Def* def, u64 index, Debug dbg = {});
    const Def* extract(const Def* def, u64 index, u64 arity, Debug dbg = {});
    //@}

    //@{ create Insert
    const Def* insert(const Def* def, const Def* index, const Def* value, Debug dbg = {});
    const Def* insert(const Def* def, u64 index, const Def* value, Debug dbg = {});
    //@}

    //@{ index eliminator
    const Def* index_zero() const { return index_zero_; }
    const Def* index_zero(const Def* arity, Loc loc = {});
    const Def* index_succ() const { return index_succ_; }
    const Def* index_succ(const Def* index, Debug dbg = {});
    const Axiom* index_eliminator() const { return index_eliminator_; };
    //@}

    //@{ arity eliminator and recursors
    const Axiom* arity_succ() { return arity_succ_; }
    const Def*   arity_succ(const Def* arity, Debug dbg = {});
    const Axiom* arity_eliminator() const { return arity_eliminator_; }
    const Axiom* arity_recursor_to_arity() const { return arity_recursor_to_arity_; }
    const Axiom* arity_recursor_to_multi() const { return arity_recursor_to_multi_; }
    const Axiom* arity_recursor_to_star() const { return arity_recursor_to_star_; }
    const Axiom* multi_recursor() const { return multi_recursor_; }
    const Def* rank() { return rank_; }
    const Def* rank(const Def* type) { return app(app(rank(), type->qualifier()), type); }
    //@}

    //@{ axioms
    Axiom* axiom(const Def* type, Normalizer, Debug dbg = {});
    Axiom* axiom(const Def* type, Debug dbg = {}) { return axiom(type, nullptr, dbg); }
    Axiom* axiom(Symbol name, const char* s, Normalizer = nullptr);
    const Axiom* lookup_axiom(Symbol name) { return find(axioms_, name); }
    //@}

    //@{ pick, intersection and match, variant
    template<class T> const Def* join(const Def* type, Defs ops, Debug dbg = {});
    template<class T> const Def* join(Defs defs, Debug dbg = {}) { return join<T>(type_bound<T, true>(nullptr, defs), defs, dbg); }
    const Def* intersection(const Def* type, Defs defs, Debug dbg = {}) { return join<Intersection>(type, defs, dbg); }
    const Def* variant     (const Def* type, Defs defs, Debug dbg = {}) { return join<Variant     >(type, defs, dbg); }
    const Def* intersection(Defs defs, Debug dbg = {}) { return join<Intersection>(defs, dbg); }
    const Def* variant     (Defs defs, Debug dbg = {}) { return join<Variant     >(defs, dbg); }
    Intersection* intersection(const Def* type, size_t num_ops, Debug dbg = {}) { return insert<Intersection>(num_ops, type, num_ops, dbg); }
    Variant*      variant     (const Def* type, size_t num_ops, Debug dbg = {}) { return insert<Variant     >(num_ops, type, num_ops, dbg); }
    const Def* pick(const Def* type, const Def* def, Debug dbg = {});
    const Def* match(const Def* def, Defs handlers, Debug dbg = {});
    //@}

    //@{ misc factory methods
    const Def* singleton(const Def* def, Debug dbg = {});
    const Def* tuple(Defs defs, Debug dbg = {});
    const Var* var(Defs types, u64 index, Debug dbg = {}) { return var(sigma(types), index, dbg); }
    const Var* var(const Def* type, u64 index, Debug dbg = {}) { return unify<Var>(0, type, index, dbg); }
    Unknown* unknown(Loc loc = {});
    //@}

    //@{ top/bottom
    const BotTop* bot_top(const Def* type, bool b) { return unify<BotTop>(0, type, b); }
    const BotTop* bot(const Def* type) { return bot_top(type, false); }
    const BotTop* top(const Def* type) { return bot_top(type,  true); }
    //@}

    //@{ standard types
    const Axiom* type_bool() { return type_bool_; }
    const Axiom* type_nat() { return type_nat_; }
    const Axiom* type_qualifier() { return type_qualifier_; }
    //@}

    //@{ literals
    const Lit* lit(const Def* type, Box box, Debug dbg = {}) { return unify<Lit>(0, type, box, dbg); }
    const Lit* lit(Qualifier q) const { return qualifier_[size_t(q)]; }
    const Lit* lit_arity(const Def* q, u64 a, Loc loc = {});
    const Lit* lit_arity(Qualifier q, u64 a, Loc loc = {}) { return lit_arity(lit(q), a, loc); }
    const Lit* lit_arity(u64 a, Loc loc = {}) { return lit_arity(Qualifier::u, a, loc); }
    const Lit* lit_index(u64 arity, u64 idx, Loc loc = {}) { return lit_index(lit_arity(arity), idx, loc); }
    const Lit* lit_index(const Lit* arity, u64 index, Loc loc = {});
    const Lit* lit_nat(int64_t val, Loc loc = {});
    const Lit* lit_nat_0 () { return lit_nat_0_; }
    const Lit* lit_nat_1 () { return lit_nat_[0]; }
    const Lit* lit_nat_2 () { return lit_nat_[1]; }
    const Lit* lit_nat_4 () { return lit_nat_[2]; }
    const Lit* lit_nat_8 () { return lit_nat_[3]; }
    const Lit* lit_nat_16() { return lit_nat_[4]; }
    const Lit* lit_nat_32() { return lit_nat_[5]; }
    const Lit* lit_nat_64() { return lit_nat_[6]; }
    const Lit* lit(bool val) { return lit_bool_[size_t(val)]; }
    const Lit* lit_false() { return lit_bool_[0]; }
    const Lit* lit_true()  { return lit_bool_[1]; }
    //@}

    //@{ intrinsics (AKA built-in Cont%inuations)
    const Axiom* cn_br()    const { return cn_br_; }
    Lambda* cn_end()   const { return cn_end_; }
    //@}

    //@{ boolean operations
    template<BOp o> const Axiom* op() { return BOp_[size_t(o)]; }
    template<BOp o> const Def* op(const Def* a, const Def* b, Debug dbg = {}) {
        auto shape = infer_shape(*this, a);
        return op<o>(shape, a, b, dbg);
    }
    template<BOp o> const Def* op(const Def* shape, const Def* a, const Def* b, Debug dbg = {}) {
        return app(app(op<o>(), shape), {a, b}, dbg);
    }
    const Def* op_bnot(const Def* a, Debug dbg = {}) { return op_bnot(infer_shape(*this, a), a, dbg); }
    const Def* op_bnot(const Def* shape, const Def* a, Debug dbg = {}) {
        return app(app(op<BOp::bxor>(), shape), {variadic(shape, lit_true()), a}, dbg);
    }
    //@}

    //@{ nat operations
    template<NOp o> const Axiom* op() { return NOp_[size_t(o)]; }
    template<NOp o> const Def* op(const Def* a, const Def* b, Debug dbg = {}) {
        return op<o>(infer_shape(*this, a), a, b, dbg);
    }
    template<NOp o> const Def* op(const Def* shape, const Def* a, const Def* b, Debug dbg = {}) {
        return app(app(op<o>(), shape), {a, b}, dbg);
    }
    //@}

    //@{ externals
    const SymbolMap<const Def*>& externals() const { return externals_; }
    void make_external(const Def* def) {
        auto [i, success] = externals_.emplace(def->name(), def);
        assert_unused(success || i->second == def);
    }
    bool is_external(const Def* def) const { return externals_.contains(def->name()); }
    const Def* lookup_external(Symbol s) const { return find(externals_, s); }
    auto external_lambdas() const { return map_range(range(externals_,
                [](auto p) { return p.second->template isa<Lambda>(); }),
                [](auto p) { return p.second->as_lambda(); });
    }
    //@}

    //@{ debugging infrastructure
#ifndef NDEBUG
    void breakpoint(size_t number);
    const Breakpoints& breakpoints() const;
    void swap_breakpoints(World& other);
    bool track_history() const;
    void enable_history(bool flag = true);
#endif
    void check(const Def* def) { type_check_.check(def); }
    bool expensive_checks_enabled() const { return expensive_checks_; }
    void enable_expensive_checks(bool on = true) { expensive_checks_ = on; }
    bool assignable(const Def* a, const Def* b) { return !expensive_checks_enabled() || a->assignable(b); }
    template<typename... Args>
    [[noreturn]] void errorf(const char* fmt, Args... args) {
        std::ostringstream oss;
        streamf(oss, fmt, std::forward<Args>(args)...);
        throw TypeError(oss.str());
    }
    //@}

    //@{ misc
    const DefSet& defs() const { return defs_; }
    auto lambdas() const { return map_range(range(defs_,
                [](auto def) { return def->isa_lambda(); }),
                [](auto def) { return def->as_lambda(); }); }
    const Def* types_from_tuple_type(const Def* type);
    //@}

    friend void swap(World& w1, World& w2) {
        using std::swap;
        swap(w1.debug_,            w2.debug_);
        swap(w1.defs_,             w2.defs_);
        swap(w1.externals_,        w2.externals_);
        swap(w1.axioms_,           w2.axioms_);
        swap(w1.universe_->world_, w2.universe_->world_);
#ifndef NDEBUG
        swap(w1.breakpoints_,      w2.breakpoints_);
        swap(w1.track_history_,    w2.track_history_);
#endif
    }

private:
    template<class T, bool infer_qualifier = true, class I>
    const Def* bound(const Def* q, Range<I> ops);
    template<class T, bool infer_qualifier = true>
    const Def* bound(const Def* q, Defs ops) {
        return bound<T, infer_qualifier>(q, range(ops));
    }
    template<class T, bool infer_qualifier = true, class I>
    const Def* type_bound(const Def* q, Range<I> ops) {
        return bound<T, infer_qualifier>(q, map_range(ops, [&] (auto def) {
                    assertf(!def->is_universe(), "{} has no type, can't be used as subexpression in types", def);
                    return def->type();
                }));
    }
    template<class T, bool infer_qualifier = true>
    const Def* type_bound(const Def* q, Defs ops) {
        return type_bound<T, infer_qualifier>(q, range(ops));
    }

protected:
    template<class T, class... Args>
    const T* unify(size_t num_ops, Args&&... args) {
        auto def = alloc<T>(num_ops, args...);
#ifndef NDEBUG
        if (breakpoints_.contains(def->gid())) THORIN_BREAK;
#endif
        assert(!def->is_nominal());
        auto [i, success] = defs_.emplace(def);
        if (success) {
            def->finalize();
            return def;
        }

        dealloc<T>(def);
        return static_cast<const T*>(*i);
    }

    template<class T, class... Args>
    T* insert(size_t num_ops, Args&&... args) {
        auto def = alloc<T>(num_ops, args...);
#ifndef NDEBUG
        if (breakpoints_.contains(def->gid())) THORIN_BREAK;
#endif
        auto p = defs_.emplace(def);
        assert_unused(p.second);
        return def;
    }

    struct Zone {
        static const size_t Size = 1024 * 1024 - sizeof(std::unique_ptr<int>); // 1MB - sizeof(next)
        std::unique_ptr<Zone> next;
        char buffer[Size];
    };

#ifndef NDEBUG
    struct Lock {
        Lock() {
            assert((alloc_guard_ = !alloc_guard_) && "you are not allowed to recursively invoke alloc");
        }
        ~Lock() { alloc_guard_ = !alloc_guard_; }
        static bool alloc_guard_;
    };
#else
    struct Lock { ~Lock() {} };
#endif
    template<class T> static size_t num_bytes_of(size_t num_ops) {
        size_t result = std::is_empty<typename T::Extra>() ? 0 : sizeof(typename T::Extra);
        result += sizeof(Def) + sizeof(const Def*)*num_ops;
        return (result + (sizeof(void*)-1)) & ~(sizeof(void*)-1); // align properly
    }
    template<class T, class... Args>
    T* alloc(size_t num_ops, Args&&... args) {
        static_assert(sizeof(Def) == sizeof(T), "you are not allowed to introduce any additional data in subclasses of Def - use 'Extra' struct");
        Lock lock;
        size_t num_bytes = num_bytes_of<T>(num_ops);
        num_bytes = (num_bytes + (sizeof(void*) - 1)) & ~(sizeof(void*)-1);
        assert(num_bytes < Zone::Size);

        if (buffer_index_ + num_bytes >= Zone::Size) {
            auto page = new Zone;
            cur_page_->next.reset(page);
            cur_page_ = page;
            buffer_index_ = 0;
        }

        auto result = new (cur_page_->buffer + buffer_index_) T(args...);
        buffer_index_ += num_bytes;
        assert(buffer_index_ % alignof(T) == 0);

        return result;
    }

    template<class T>
    void dealloc(const T* def) {
        size_t num_bytes = num_bytes_of<T>(def->num_ops());
        num_bytes = (num_bytes + (sizeof(void*) - 1)) & ~(sizeof(void*)-1);
        def->~T();
        if (ptrdiff_t(buffer_index_ - num_bytes) > 0) // don't care otherwise
            buffer_index_-= num_bytes;
        assert(buffer_index_ % alignof(T) == 0);
    }

    mutable Debug debug_;
    std::unique_ptr<Zone> root_page_;
    Zone* cur_page_;
    size_t buffer_index_ = 0;
    DefSet defs_;
    SymbolMap<const Axiom*> axioms_;
    SymbolMap<const Def*> externals_;
    const Universe* universe_;
    const Axiom* kind_qualifier_;
    const Axiom* type_qualifier_;
    const Axiom* arity_succ_;
    const Axiom* arity_eliminator_;
    const Axiom* arity_recursor_to_arity_;
    const Axiom* arity_recursor_to_multi_;
    const Axiom* arity_recursor_to_star_;
    const Axiom* index_zero_;
    const Axiom* index_succ_;
    const Axiom* index_eliminator_;
    const Axiom* multi_recursor_;
    const Def* rank_;
    std::array<const Lit*, 4> qualifier_;
    std::array<const Lit*, 4> unit_;
    std::array<const Def*, 4> unit_val_;
    std::array<const Def*, 4> unit_kind_;
    std::array<const Def*, 4> unit_kind_val_;
    std::array<const Kind*, 4> kind_arity_;
    std::array<const Kind*, 4> kind_multi_;
    std::array<const Kind*, 4> kind_star_;
    std::array<const Axiom*, Num<BOp>> BOp_;
    std::array<const Axiom*, Num<NOp>> NOp_;
    const Axiom* type_bool_;
    const Axiom* type_nat_;
    const Lit* lit_nat_0_;
    std::array<const Lit*, 2> lit_bool_;
    std::array<const Lit*, 7> lit_nat_;
    const Axiom* cn_br_;
    Lambda* cn_end_;
    TypeCheck type_check_;
#ifndef NDEBUG
    Breakpoints breakpoints_;
    bool track_history_ = false;
    bool expensive_checks_  = true;
#else
    bool expensive_checks_ = false;
#endif
};

inline const Def* app_callee(const Def* def) { return def->as<App>()->callee(); }
inline const Def* app_arg(const Def* def) { return def->as<App>()->arg(); }
inline const Def* app_arg(const Def* def, u64 i) { return def->world().extract(app_arg(def), i); }

}

#endif
