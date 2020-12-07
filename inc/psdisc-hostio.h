// Contents released under the The MIT License (MIT)

#pragma once

#include "psdisc-types.h"

#include <cstdint>
#include <functional>

static_assert(sizeof(psdisc_off_t) >= 8,      "PsDisc offset type must be at least 64-bits wide.");
static_assert(std::is_signed<psdisc_off_t>(), "PsDisc does not support unsigned file offset type.");
static_assert(std::is_signed<psdisc_sec_t>(), "PsDisc does not support unsigned sector offset type.");

using PsDiscFn_ioReadP               = std::function< intmax_t(void* dest, intmax_t count, intmax_t pos) >;
using PsDiscFn_ReadSectorData2048    = std::function<bool (uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length)>;
