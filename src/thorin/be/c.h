#ifndef THORIN_BE_C_H
#define THORIN_BE_C_H

#include <cstdint>
#include <iostream>

#include "thorin/continuation.h"

namespace thorin {

class World;
typedef ContinuationMap<std::tuple<int, int, int>> Cont2Config;

enum class Lang : uint8_t {
    C99,    ///< Flag for C99
    CUDA,   ///< Flag for CUDA
    OPENCL  ///< Flag for OpenCL
};

void emit_c(World&, const Cont2Config& kernel_config, std::ostream& stream, Lang lang, bool debug);

}

#endif
