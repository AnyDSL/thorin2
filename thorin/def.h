#pragma once

#include <optional>
#include <vector>

#include "thorin/config.h"

#include "thorin/util/array.h"
#include "thorin/util/cast.h"
#include "thorin/util/hash.h"
#include "thorin/util/loc.h"
#include "thorin/util/print.h"
#include "thorin/util/util.h"

// clang-format off
#define THORIN_NODE(m)                                                        \
    m(Type, type)       m(Univ, univ)   m(UMax, umax)       m(UInc, uinc)     \
    m(Pi, pi)           m(Lam, lam)     m(App, app)                           \
    m(Sigma, sigma)     m(Tuple, tuple) m(Extract, extract) m(Insert, insert) \
    m(Arr, arr)         m(Pack, pack)                                         \
    m(Join, join)       m(Vel, vel)     m(Test, test)       m(Top, top)       \
    m(Meet, meet)       m(Ac,  ac )     m(Pick, pick)       m(Bot, bot)       \
    m(Proxy, proxy)                                                           \
    m(Axiom, axiom)                                                           \
    m(Lit, lit)                                                               \
    m(Nat, nat)         m(Idx, idx)                                           \
    m(Var, var)                                                               \
    m(Infer, infer)                                                           \
    m(Global, global)                                                         \
    m(Singleton, singleton)
// clang-format on

namespace thorin {

namespace Node {
#define CODE(node, name) node,
enum : node_t { THORIN_NODE(CODE) };
#undef CODE
} // namespace Node

class App;
class Axiom;
class Var;
class Def;
class World;

using Defs     = Span<const Def*>;
using DefArray = Array<const Def*>;

//------------------------------------------------------------------------------

/// Helper class to retrieve Infer::arg if present.
class Ref {
public:
    Ref() = default;
    Ref(const Def* def)
        : def_(def) {}

    const Def* operator*() const { return refer(def_); }
    const Def* operator->() const { return refer(def_); }
    operator const Def*() const { return refer(def_); }
    explicit operator bool() const { return def_; }
    static const Def* refer(const Def* def); ///< Retrieves Infer::arg from @p def.

    friend std::ostream& operator<<(std::ostream&, Ref);

private:
    const Def* def_ = nullptr;
};

using NormalizeFn = Ref (*)(Ref, Ref, Ref);

//------------------------------------------------------------------------------

/// References a user.
/// A Def `u` which uses Def `d` as `i^th` operand is a Use with Use::index `i` of Def `d`.
class Use {
public:
    static constexpr size_t Type = -1_s;

    Use() {}
    Use(const Def* def, size_t index)
        : def_(def)
        , index_(index) {}

    size_t index() const { return index_; }
    const Def* def() const { return def_; }
    operator const Def*() const { return def_; }
    const Def* operator->() const { return def_; }
    bool operator==(Use other) const { return this->def_ == other.def_ && this->index_ == other.index_; }

private:
    const Def* def_;
    size_t index_;
};

struct UseHash {
    inline hash_t operator()(Use) const;
};

struct UseEq {
    bool operator()(Use u1, Use u2) const { return u1 == u2; }
};

using Uses = absl::flat_hash_set<Use, UseHash, UseEq>;

// TODO remove or fix this
enum class Sort { Term, Type, Kind, Space, Univ, Level };

//------------------------------------------------------------------------------

/// @name Dep
///@{
enum class Dep : unsigned {
    None  = 0,
    Axiom = 1 << 0,
    Infer = 1 << 1,
    Mut   = 1 << 2,
    Proxy = 1 << 3,
    Var   = 1 << 4,
};

inline unsigned operator&(Dep d1, Dep d2) { return unsigned(d1) & unsigned(d2); }
inline unsigned operator|(Dep d1, Dep d2) { return unsigned(d1) | unsigned(d2); }
inline unsigned operator&(unsigned d1, Dep d2) { return d1 & unsigned(d2); }
inline unsigned operator|(unsigned d1, Dep d2) { return d1 | unsigned(d2); }
inline unsigned operator&(Dep d1, unsigned d2) { return unsigned(d1) & d2; }
inline unsigned operator|(Dep d1, unsigned d2) { return unsigned(d1) | d2; }
inline unsigned operator==(unsigned d1, Dep d2) { return d1 == unsigned(d2); }
inline unsigned operator!=(unsigned d1, Dep d2) { return d1 != unsigned(d2); }
inline unsigned operator==(Dep d1, unsigned d2) { return unsigned(d1) == d2; }
inline unsigned operator!=(Dep d1, unsigned d2) { return unsigned(d1) != d2; }
///@}

/// Use as mixin to wrap all kind of Def::proj and Def::projs variants.
#define THORIN_PROJ(NAME, CONST)                                                  \
    nat_t num_##NAME##s() CONST { return ((const Def*)NAME())->num_projs(); }     \
    Ref NAME(nat_t a, nat_t i) CONST { return ((const Def*)NAME())->proj(a, i); } \
    Ref NAME(nat_t i) CONST { return ((const Def*)NAME())->proj(i); }             \
    template<nat_t A = -1_s, class F>                                             \
    auto NAME##s(F f) CONST {                                                     \
        return ((const Def*)NAME())->projs<A, F>(f);                              \
    }                                                                             \
    template<nat_t A = -1_s>                                                      \
    auto NAME##s() CONST {                                                        \
        return ((const Def*)NAME())->projs<A>();                                  \
    }                                                                             \
    template<class F>                                                             \
    auto NAME##s(nat_t a, F f) CONST {                                            \
        return ((const Def*)NAME())->projs<F>(a, f);                              \
    }                                                                             \
    auto NAME##s(nat_t a) CONST { return ((const Def*)NAME())->projs(a); }

// clang-format off
/// Use as mixin to declare setters for Def::loc \& Def::name using a *covariant* return type.
#define THORIN_SETTERS_(T)                                                                                                  \
public:                                                                                                                     \
    template<bool Ow = false> const T* set(Loc l               ) const { if (Ow || !dbg_.loc) dbg_.loc = l;  return this; } \
    template<bool Ow = false>       T* set(Loc l               )       { if (Ow || !dbg_.loc) dbg_.loc = l;  return this; } \
    template<bool Ow = false> const T* set(       Sym s        ) const { if (Ow || !dbg_.sym) dbg_.sym = s;  return this; } \
    template<bool Ow = false>       T* set(       Sym s        )       { if (Ow || !dbg_.sym) dbg_.sym = s;  return this; } \
    template<bool Ow = false> const T* set(       std::string s) const {         set(get_sym(std::move(s))); return this; } \
    template<bool Ow = false>       T* set(       std::string s)       {         set(get_sym(std::move(s))); return this; } \
    template<bool Ow = false> const T* set(Loc l, Sym s        ) const { set(l); set(s);                     return this; } \
    template<bool Ow = false>       T* set(Loc l, Sym s        )       { set(l); set(s);                     return this; } \
    template<bool Ow = false> const T* set(Loc l, std::string s) const { set(l); set(get_sym(std::move(s))); return this; } \
    template<bool Ow = false>       T* set(Loc l, std::string s)       { set(l); set(get_sym(std::move(s))); return this; } \
    template<bool Ow = false> const T* set(Dbg d) const { set(d.loc, d.sym); return this; }                                 \
    template<bool Ow = false>       T* set(Dbg d)       { set(d.loc, d.sym); return this; }
// clang-format on

#ifdef DOXYGEN
#define THORIN_SETTERS(T) public: // Don't spam each and every sub class of Def with basically the same docs.
#else
#define THORIN_SETTERS(T) THORIN_SETTERS_(T)
#endif

#define THORIN_DEF_MIXIN(T)                                                            \
public:                                                                                \
    THORIN_SETTERS(T)                                                                  \
    T* stub(World& w, const Def* type) { return stub_(w, type)->set(dbg())->as<T>(); } \
    static constexpr auto Node = Node::T;                                              \
                                                                                       \
private:                                                                               \
    Ref rebuild_(World&, Ref, Defs) const override;                                    \
    friend class World;

/// Base class for all Def%s.
/// The data layout (see World::alloc and Def::partial_ops) looks like this:
/// ```
/// Def| type | op(0) ... op(num_ops-1) |
///    |---------partial_ops------------|
///           |-------extended_ops------|
/// ```
/// @attention This means that any subclass of Def **must not** introduce additional members.
/// @see @ref mut
class Def : public RuntimeCast<Def> {
private:
    Def& operator=(const Def&) = delete;
    Def(const Def&)            = delete;

protected:
    Def(World*, node_t, const Def* type, Defs ops, flags_t flags); ///< Constructor for an *immutable* Def.
    Def(node_t n, const Def* type, Defs ops, flags_t flags);
    Def(node_t, const Def* type, size_t num_ops, flags_t flags); ///< Constructor for a *mutable* Def.
    virtual ~Def() = default;

public:
    /// @name Getters
    ///@{
    World& world() const;
    flags_t flags() const { return flags_; }
    u32 gid() const { return gid_; }
    hash_t hash() const { return hash_; }
    node_t node() const { return node_; }
    std::string_view node_name() const;
    ///@}

    /// @name type
    ///@{

    /// Yields the **raw** type of this Def, i.e. maybe `nullptr`. @see Def::unfold_type.
    const Def* type() const { return type_; }
    /// Yields the type of this Def and unfolds it if necessary. @see Def::type, Def::reduce_rec.
    const Def* unfold_type() const;
    /// Yields `true` if `this:T` and `T:(.Type 0)`.
    bool is_term() const;
    ///@}

    /// @name arity
    ///@{
    const Def* arity() const;
    std::optional<nat_t> isa_lit_arity() const;
    nat_t as_lit_arity() const {
        auto a = isa_lit_arity();
        assert(a.has_value());
        return *a;
    }
    ///@}

    /// @name ops
    ///@{
    template<size_t N = -1_s>
    auto ops() const {
        if constexpr (N == -1_s)
            return Defs(num_ops_, ops_ptr());
        else
            return Span<const Def*>(N, ops_ptr()).template to_array<N>();
    }
    const Def* op(size_t i) const { return ops()[i]; }
    size_t num_ops() const { return num_ops_; }
    ///@}

    /// @name set/unset ops (mutables only)
    ///@{
    /// You are supposed to set operands from left to right.
    /// You can change operands later on or even Def::unset them.
    /// @warning But Thorin assumes that a mutable is "final" if its last operands is set.
    /// So, don't set the last operand and leave the first one unset.
    Def* set(size_t i, const Def* def);
    Def* set(Defs ops) {
        for (size_t i = 0, e = num_ops(); i != e; ++i) set(i, ops[i]);
        return this;
    }

    void unset(size_t i);
    void unset() {
        for (size_t i = 0, e = num_ops(); i != e; ++i) unset(i);
    }
    Def* set_type(const Def*);
    void unset_type();

    /// Resolves Infer%s of this Def's type.
    void update() {
        if (auto r = Ref::refer(type()); r && r != type()) set_type(r);
    }

    /// Yields `true` if empty or the last op is set.
    bool is_set() const;
    ///@}

    /// @name extended_ops
    ///@{
    /// Includes Def::type() (if not `nullptr`) and then the other Def::ops() in this order.
    /// Def::ops() is only included, if Def::is_set.
    Defs extended_ops() const;
    const Def* extended_op(size_t i) const { return extended_ops()[i]; }
    size_t num_extended_ops() const { return extended_ops().size(); }
    ///@}

    /// @name partial_ops
    ///@{
    /// Includes Def::type() and then the other Def::ops() in this order.
    /// Also works with partially set Def%s and doesn't assert.
    /// Unset operands are `nullptr`.
    Defs partial_ops() const { return Defs(num_ops_ + 1, ops_ptr() - 1); }
    const Def* partial_op(size_t i) const { return partial_ops()[i]; }
    size_t num_partial_ops() const { return partial_ops().size(); }
    ///@}

    /// @name uses
    ///@{
    const Uses& uses() const { return uses_; }
    size_t num_uses() const { return uses().size(); }
    ///@}

    /// @name dep
    ///@{
    /// @see Dep.
    unsigned dep() const { return dep_; }
    bool has_dep(Dep d) const { return has_dep(unsigned(d)); }
    bool has_dep(unsigned u) const { return dep() & u; }
    bool dep_const() const { return !has_dep(Dep::Mut | Dep::Var); }
    ///@}

    /// @name proj
    /// @anchor proj
    ///@{
    /// Splits this Def via Extract%s or directly accessing the Def::ops in the case of Sigma%s or Arr%ays.

    /// Yields Def::arity as_lit, if it is in fact a Lit, or `1` otherwise.
    nat_t num_projs() const {
        if (auto a = isa_lit_arity()) return *a;
        return 1;
    }
    /// Similar to World::extract while assuming an arity of @p a but also works on Sigma%s, and Arr%ays.
    const Def* proj(nat_t a, nat_t i) const;

    /// Same as above but takes Def::num_projs as arity.
    const Def* proj(nat_t i) const { return proj(num_projs(), i); }

    /// Splits this Def via Def::proj%ections into an Arr%ay (if `A == -1_s`) or `std::array` (otherwise).
    /// Applies @p f to each element.
    /// ```
    /// std::array<const Def*, 2> ab = def->projs<2>();
    /// std::array<u64, 2>        xy = def->projs<2>(as_lit<nat_t>);
    /// auto [a, b] = def->projs<2>();
    /// auto [x, y] = def->projs<2>(as_lit<nat_t>);
    /// Array<const Def*> projs = def->projs();               // projs has def->num_projs() many elements
    /// Array<const Def*> projs = def->projs(n);              // projs has n elements - asserts if incorrect
    /// Array<const Lit*> lits = def->projs(as_lit<nat_t>);   // same as above but applies as_lit<nat_t> to each element
    /// Array<const Lit*> lits = def->projs(n, as_lit<nat_t>);// same as above but applies as_lit<nat_t> to each element
    /// ```
    template<nat_t A = -1_s, class F>
    auto projs(F f) const {
        using R = std::decay_t<decltype(f(this))>;
        if constexpr (A == -1_s) {
            return projs(num_projs(), f);
        } else {
            assert(A == as_lit_arity());
            std::array<R, A> array;
            for (nat_t i = 0; i != A; ++i) array[i] = f(proj(A, i));
            return array;
        }
    }

    template<class F>
    auto projs(nat_t a, F f) const {
        using R = std::decay_t<decltype(f(this))>;
        return Array<R>(a, [&](nat_t i) { return f(proj(a, i)); });
    }
    template<nat_t A = -1_s>
    auto projs() const {
        return projs<A>([](const Def* def) { return def; });
    }
    auto projs(nat_t a) const {
        return projs(a, [](const Def* def) { return def; });
    }
    ///@}

    /// @name var
    ///@{
    /// Retrieve Var for *mut*ables.
    /// @see @ref proj
    const Var* var();
    THORIN_PROJ(var, )
    ///@}

    /// @name external
    ///@{
    bool is_external() const { return external_; }
    bool is_internal() const { return !is_external(); } ///< **Not** Def::is_external.
    void make_external();
    void make_internal();
    ///@}

    /// @name Casts
    ///@{
    /// @see @ref cast_builtin
    // clang-format off
    template<class T = Def> const T* isa_imm() const { return isa_mut<T, true>(); }
    template<class T = Def> const T*  as_imm() const { return  as_mut<T, true>(); }
    template<class T = Def, class R> const T* isa_imm(R (T::*f)() const) const { return isa_mut<T, R, true>(f); }
    // clang-format on

    /// If `this` is *mut*able, it will cast `const`ness away and perform a `dynamic_cast` to @p T.
    template<class T = Def, bool invert = false>
    T* isa_mut() const {
        if constexpr (std::is_same<T, Def>::value)
            return mut_ ^ invert ? const_cast<Def*>(this) : nullptr;
        else
            return mut_ ^ invert ? const_cast<Def*>(this)->template isa<T>() : nullptr;
    }

    /// Asserts that `this` is a *mutable*, casts `const`ness away and performs a `static_cast` to @p T.
    template<class T = Def, bool invert = false>
    T* as_mut() const {
        assert(mut_ ^ invert);
        if constexpr (std::is_same<T, Def>::value)
            return const_cast<Def*>(this);
        else
            return const_cast<Def*>(this)->template as<T>();
    }

    template<class T = Def, class R>
    T* isa_mut(R (T::*f)() const) const {
        if (auto t = isa_mut<T>(); t && (t->*f)()) return t;
        return nullptr;
    }
    ///@}

    /// @name Dbg Getters
    ///@{
    Dbg dbg() const { return dbg_; }
    Loc loc() const { return dbg_.loc; }
    Sym sym() const { return dbg_.sym; }
    std::string unique_name() const; ///< name + "_" + Def::gid
    ///@}

    /// @name Dbg Setters
    ///@{
    /// Every subclass `S` of Def has the same setters that return `S*`/`const S*` but will not show up in Doxygen.
    THORIN_SETTERS_(Def)
    ///@}

    /// @name debug_prefix/suffix
    ///@{
    /// Prepends/Appends a prefix/suffix to Def::name - but only in `Debug` build.
#ifndef NDEBUG
    const Def* debug_prefix(std::string) const;
    const Def* debug_suffix(std::string) const;
#else
    const Def* debug_prefix(std::string) const { return this; }
    const Def* debug_suffix(std::string) const { return this; }
#endif
    ///@}

    /// @name reduce
    ///@{
    /// Rewrites Def::ops by substituting `this` mutable's Var with @p arg.
    DefArray reduce(const Def* arg) const;
    DefArray reduce(const Def* arg);
    /// Transitively Def::reduce Lam%s, if `this` is an App.
    /// @returns the reduced body.
    const Def* reduce_rec() const;
    ///@}

    /// @name stub/rebuild
    ///@{
    Def* stub(World& w, Ref type) { return stub_(w, type)->set(dbg()); }
    /// Def::rebuild%s this Def while using @p new_op as substitute for its @p i'th Def::op
    Ref rebuild(World& w, Ref type, Defs ops) const { return rebuild_(w, type, ops)->set(dbg()); }
    ///@}

    /// @name Virtual Methods
    ///@{
    virtual void check() {}
    virtual size_t first_dependend_op() { return 0; }
    const Def* refine(size_t i, const Def* new_op) const;
    virtual const Def* restructure() { return nullptr; }
    ///@}

    /// @name dump
    ///@{
    void dump() const;
    void dump(int max) const;
    void write(int max) const;
    void write(int max, const char* file) const;
    std::ostream& stream(std::ostream&, int max) const;
    ///@}

protected:
    virtual Ref rebuild_(World&, Ref, Defs) const = 0;
    virtual Def* stub_(World&, Ref) { unreachable(); }

    const Def** ops_ptr() const {
        return reinterpret_cast<const Def**>(reinterpret_cast<char*>(const_cast<Def*>(this + 1)));
    }
    void finalize();
    bool equal(const Def* other) const;

    union {
        NormalizeFn normalizer_; ///< Axiom%s use this member to store their normalizer.
        const Axiom* axiom_;     /// Curried App%s of Axiom%s use this member to propagate the Axiom.
        mutable World* world_;
    };

    /// @name Wrappers for World::sym
    ///@{
    /// These are here to have Def::set%ters inline without including `thorin/world.h`.
    Sym get_sym(const char*) const;
    Sym get_sym(std::string_view) const;
    Sym get_sym(std::string) const;
    ///@}

    flags_t flags_;
    uint8_t node_;
    bool mut_      : 1;
    bool external_ : 1;
    unsigned dep_  : 5;
    bool padding_  : 1;
    u8 curry_;
    u8 trip_;
    hash_t hash_;
    u32 gid_;
    u32 num_ops_;
    mutable Uses uses_;
    mutable Dbg dbg_;
    const Def* type_;

    friend class World;
    friend void swap(World&, World&);
    friend std::ostream& operator<<(std::ostream&, const Def*);
};

/// @name Formatted Output
///@{
/// Uses @p def->loc() as Loc%ation.
template<class T = std::logic_error, class... Args>
[[noreturn]] void error(const Def* def, const char* fmt, Args&&... args) {
    error(def->loc(), fmt, std::forward<Args&&>(args)...);
}
///@}

//------------------------------------------------------------------------------

template<class To>
using DefMap  = GIDMap<const Def*, To>;
using DefSet  = GIDSet<const Def*>;
using Def2Def = DefMap<const Def*>;
using DefDef  = std::tuple<const Def*, const Def*>;
using DefVec  = std::vector<const Def*>;

struct DefDefHash {
    hash_t operator()(DefDef pair) const {
        hash_t hash = std::get<0>(pair)->gid();
        hash        = murmur3(hash, std::get<1>(pair)->gid());
        hash        = murmur3_finalize(hash, 8);
        return hash;
    }
};

struct DefDefEq {
    bool operator()(DefDef p1, DefDef p2) const { return p1 == p2; }
};

template<class To>
using DefDefMap = absl::flat_hash_map<DefDef, To, DefDefHash, DefDefEq>;
using DefDefSet = absl::flat_hash_set<DefDef, DefDefHash, DefDefEq>;

template<class To>
using MutMap  = GIDMap<Def*, To>;
using MutSet  = GIDSet<Def*>;
using Mut2Mut = MutMap<Def*>;

//------------------------------------------------------------------------------

class Var : public Def {
private:
    Var(const Def* type, Def* mut)
        : Def(Node, type, Defs{mut}, 0) {}

public:
    /// @name ops
    ///@{
    Def* mut() const { return op(0)->as_mut(); }
    ///@}

    THORIN_DEF_MIXIN(Var)
};

template<class To>
using VarMap  = GIDMap<const Var*, To>;
using VarSet  = GIDSet<const Var*>;
using Var2Var = VarMap<const Var*>;

class Univ : public Def {
private:
    Univ(World& world)
        : Def(&world, Node, nullptr, Defs{}, 0) {}

    THORIN_DEF_MIXIN(Univ)
};

class UMax : public Def {
private:
    UMax(World&, Defs ops);

    THORIN_DEF_MIXIN(UMax)
};

using level_t = u64;

class UInc : public Def {
private:
    UInc(const Def* op, level_t offset)
        : Def(Node, op->type()->as<Univ>(), {op}, offset) {}

public:
    /// @name ops
    ///@{
    const Def* op() const { return Def::op(0); }
    level_t offset() const { return flags(); }
    ///@}

    THORIN_DEF_MIXIN(UInc)
};

class Type : public Def {
private:
    Type(const Def* level)
        : Def(Node, nullptr, {level}, 0) {}

public:
    const Def* level() const { return op(0); }

    THORIN_DEF_MIXIN(Type)
};

class Lit : public Def {
private:
    Lit(const Def* type, flags_t val)
        : Def(Node, type, Defs{}, val) {}

public:
    using Def::as;
    using Def::isa;

    /// @name Casts
    ///@{
    /// @see @ref cast_lit
    template<class T = nat_t>
    static std::optional<T> isa(Ref def) {
        if (!def) return {};
        if (auto lit = def->isa<Lit>()) return lit->get<T>();
        return {};
    }
    template<class T = nat_t>
    static T as(Ref def) {
        return def->as<Lit>()->get<T>();
    }
    ///@}

    template<class T = flags_t>
    T get() const {
        static_assert(sizeof(T) <= 8);
        return bitcast<T>(flags_);
    }

    THORIN_DEF_MIXIN(Lit)
};

class Nat : public Def {
private:
    Nat(World& world);

    THORIN_DEF_MIXIN(Nat)
};

/// A built-in constant of type `.Nat -> *`.
class Idx : public Def {
private:
    Idx(const Def* type)
        : Def(Node, type, Defs{}, 0) {}

public:
    /// Checks if @p def isa `.Idx s` and returns s or `nullptr` otherwise.
    static Ref size(Ref def);

    /// @name Convert between Idx::size and bitwidth and vice versa
    ///@{
    // clang-format off
    static constexpr nat_t bitwidth2size(nat_t n) { assert(n != 0); return n == 64 ? 0 : (1_n << n); }
    static constexpr nat_t size2bitwidth(nat_t n) { return n == 0 ? 64 : std::bit_width(n - 1_n); }
    // clang-format on
    static std::optional<nat_t> size2bitwidth(const Def* size);
    ///@}

    THORIN_DEF_MIXIN(Idx)
};

class Proxy : public Def {
private:
    Proxy(const Def* type, Defs ops, u32 pass, u32 tag)
        : Def(Node, type, ops, (u64(pass) << 32_u64) | u64(tag)) {}

public:
    /// @name Getters
    ///@{
    u32 pass() const { return u32(flags() >> 32_u64); } ///< IPass::index within PassMan.
    u32 tag() const { return u32(flags()); }
    ///@}

    THORIN_DEF_MIXIN(Proxy)
};

/// @deprecated A global variable in the data segment.
/// A Global may be mutable or immutable.
/// @attention WILL BE REMOVED.
class Global : public Def {
private:
    Global(const Def* type, bool is_mutable)
        : Def(Node, type, 1, is_mutable) {}

public:
    /// @name ops
    ///@{
    /// This thing's sole purpose is to differentiate on global from another.
    const Def* init() const { return op(0); }
    void set(const Def* init) { Def::set(0, init); }
    ///@}

    /// @name type
    ///@{
    const App* type() const;
    const Def* alloced_type() const;
    ///@}

    /// @name Getters
    ///@{
    bool is_mutable() const { return flags(); }
    ///@}

    THORIN_DEF_MIXIN(Global)
    Global* stub_(World&, Ref) override;
};

hash_t UseHash::operator()(Use use) const { return hash_combine(hash_begin(u16(use.index())), hash_t(use->gid())); }

} // namespace thorin
