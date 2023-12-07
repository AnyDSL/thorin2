#pragma once

#include <ostream>

namespace thorin {

class World;

namespace plug::ll {

void emit(World&, std::ostream&);

int compile(World&, std::string name);
int compile(World&, std::string ll, std::string out);
int compile_and_run(World&, std::string name, std::string args = {});

} // namespace plug::ll
} // namespace thorin
