#pragma once

#ifdef _MSC_VER
#define UNREACHABLE() __assume(false)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif // _MSC_VER
