#pragma once

#include <ostream>

#include "thorin/phase/phase.h"

namespace thorin {

class World;

namespace haskell {

void emit(World&, std::ostream&);

class OCamlEmitter : public Phase {
public:
// OCamlEmitter(World& world, std::ostream& os)
OCamlEmitter(World& world)
        : Phase(world, "ocaml_emitter", false)
        , os_(std::move(std::cout))
        {
            // os_ = std::cout;
         }

    void start() override { emit(world(), os_); }

private:

    std::ostream&& os_;

};

} // namespace haskell
} // namespace thorin
