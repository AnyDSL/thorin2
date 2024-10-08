#pragma once

#include <automaton/dfa.h>

#include "mim/plug/regex/regex.h"

/// You can dl::get this function.
extern "C" THORIN_EXPORT const mim::Def* dfa2matcher(mim::World&, const automaton::DFA&, mim::Ref);
