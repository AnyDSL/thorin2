#include "mim/world.h"

#include "mim/plug/direct/direct.h"

namespace mim::plug::direct {

/// `cps2ds` is directly converted to `op_cps2ds_dep f` in its normalizer.
Ref normalize_cps2ds(Ref, Ref, Ref fun) { return op_cps2ds_dep(fun); }

MIM_direct_NORMALIZER_IMPL

} // namespace mim::plug::direct
