#ifndef ANYDSL_AIR_TYPE_H
#define ANYDSL_AIR_TYPE_H

#include "anydsl/air/def.h"

#include "anydsl/support/hash.h" // TODO move to cpp

namespace anydsl {

class PrimLit;
class World;

//------------------------------------------------------------------------------

class Type : public AIRNode {
protected:

    Type(World& world, IndexKind index)
        : AIRNode(index)
        , world_(world)
    {}
    virtual ~Type() {}

public:

    World& world() const { return world_; }
    virtual uint64_t hash() const = 0;

private:

    World& world_;
};

typedef std::vector<const Type*> Types;

//------------------------------------------------------------------------------

/// Primitive types -- also known as atomic or scalar types.
class PrimType : public Type {
private:

    PrimType(World& world, PrimTypeKind kind);

public:

    PrimTypeKind kind() const { return (PrimTypeKind) index(); }

    virtual uint64_t hash() const { return hash(kind()); }
    static  uint64_t hash(PrimTypeKind kind);

    friend class World;
};

//------------------------------------------------------------------------------

class CompoundType : public Type {
protected:

    /// Creates an empty \p CompoundType.
    CompoundType(World& world, IndexKind index)
        : Type(world, index)
    {}

    /// Copies over the range specified by \p begin and \p end to \p types_.
    template<class T>
    CompoundType(World& world, IndexKind index, T begin, T end)
        : Type(world, index)
    {
        types_.insert(types_.begin(), begin, end);
    }

public:

    /// Get element type via index.
    const Type* get(size_t i) const { 
        anydsl_assert(i < types_.size(), "index out of range"); 
        return types_[i]; 
    }

    /// Get element type via anydsl::PrimLit which serves as index.
    const Type* get(PrimLit* c) const;

protected:

    Types types_;
};

//------------------------------------------------------------------------------

/// A tuple type.
class Sigma : public CompoundType {
private:

    Sigma(World& world, bool named)
        : CompoundType(world, Index_Sigma)
        , named_(named)
    {}

    /// Creates an unamed Sigma from the given range.
    template<class T>
    Sigma(World& world, T begin, T end, bool named)
        : CompoundType(world, Index_Sigma, begin, end)
        , named_(named)
    {}

public:

    bool named() const { return named_; }

    template<class T>
    void set(T begin, T end) {
        anydsl_assert(named_, "only allowed on named Sigmas");
        anydsl_assert(types_.empty(), "members already set");
        types_.insert(types_.begin(), begin, end);
    }

    virtual uint64_t hash() const { return hash1(Index_Sigma); }
    //static  uint64_t hash() { return hash1(Index_Sigma); }

private:

    bool named_;

    friend class World;
};

//------------------------------------------------------------------------------

/// A function type.
class Pi : public CompoundType {
private:

    Pi(World& world)
        : CompoundType(world, Index_Pi)
    {}

    template<class T>
    Pi(World& world, T begin, T end, const std::string& name)
        : CompoundType(world, Index_Sigma, begin, end, name)
    {}

    virtual uint64_t hash() const { return hash1(Index_Pi); }
    //static  uint64_t hash() { return hash1(Index_Pi); }

    friend class World;
};

//------------------------------------------------------------------------------

} // namespace anydsl

#endif // ANYDSL_AIR_TYPE_H
