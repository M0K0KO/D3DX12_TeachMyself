#pragma once

#include <cassert>
#include "MokoLogger.h"

#define MOKO_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            MOKOLOG_ERROR("ASSERT FAILED: {} ({}:{})", #expr, __FILE__, __LINE__); \
            assert(expr); \
        } \
    } while (0)
