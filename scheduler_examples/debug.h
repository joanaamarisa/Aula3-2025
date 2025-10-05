#ifndef DEBUG_H
#define DEBUG_H

/*
 * This file implements a DBG macro, that works like printf, but
 * is only active if NDEBUG is not defined. In the case of CMake/CLion
 * NDEBUG is defined in Release mode, and not defined in Debug mode.
 */
#ifndef NDEBUG
  #define DBG(fmt, ...) \
  fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
  #define DBG(...) ((void)0)
#endif

#endif //DEBUG_H
