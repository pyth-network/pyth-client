#pragma once

#define PC_PACKED        __attribute__((__packed__))
#define PC_UNLIKELY(ARG) __builtin_expect((ARG),1)
