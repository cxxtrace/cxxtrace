#ifndef CXXTRACE_CPP_H
#define CXXTRACE_CPP_H

#define CXXTRACE_CPP_CALL(macro, ...) macro(__VA_ARGS__)

// Expand to the number of variadic arguments given.
//
// For example, CXXTRACE_CPP_COUNT_ARGS(x, y(z, w, q)) expands to 2.
#define CXXTRACE_CPP_COUNT_ARGS(...)                                           \
  CXXTRACE_CPP_COUNT_ARGS_INDEX(__VA_ARGS__,                                   \
                                19,                                            \
                                18,                                            \
                                17,                                            \
                                16,                                            \
                                15,                                            \
                                14,                                            \
                                13,                                            \
                                12,                                            \
                                11,                                            \
                                10,                                            \
                                9,                                             \
                                8,                                             \
                                7,                                             \
                                6,                                             \
                                5,                                             \
                                4,                                             \
                                3,                                             \
                                2,                                             \
                                1,                                             \
                                0)
#define CXXTRACE_CPP_COUNT_ARGS_INDEX(a19,                                     \
                                      a18,                                     \
                                      a17,                                     \
                                      a16,                                     \
                                      a15,                                     \
                                      a14,                                     \
                                      a13,                                     \
                                      a12,                                     \
                                      a11,                                     \
                                      a10,                                     \
                                      a9,                                      \
                                      a8,                                      \
                                      a7,                                      \
                                      a6,                                      \
                                      a5,                                      \
                                      a4,                                      \
                                      a3,                                      \
                                      a2,                                      \
                                      a1,                                      \
                                      count,                                   \
                                      ...)                                     \
  count
static_assert(CXXTRACE_CPP_COUNT_ARGS(x, y(z, w, q)) == 2);

#define CXXTRACE_CPP_PASTE(x, y) CXXTRACE_CPP_PASTE_INNER(x, y)
#define CXXTRACE_CPP_PASTE_INNER(x, y) x##y
#define CXXTRACE_CPP_STRINGIFY(x) #x

// Stringify each argument, expanding to a comma-separated list of string
// literals.
//
// For example, CXXTRACE_CPP_STRING_LITERALS(hello, world) expands to the
// following tokens:
//
//   "hello"
//   ,
//   "world"
#define CXXTRACE_CPP_STRING_LITERALS(...)                                      \
  CXXTRACE_CPP_MAP(CXXTRACE_CPP_STRINGIFY, __VA_ARGS__)

// Expand the given macro for each following argument, separating each expansion
// with a comma.
#define CXXTRACE_CPP_MAP(macro, ...)                                           \
  CXXTRACE_CPP_CALL(CXXTRACE_CPP_PASTE(CXXTRACE_CPP_MAP_,                      \
                                       CXXTRACE_CPP_COUNT_ARGS(__VA_ARGS__)),  \
                    macro,                                                     \
                    __VA_ARGS__)
#define CXXTRACE_CPP_MAP_1(macro, a0) macro(a0)
#define CXXTRACE_CPP_MAP_2(macro, a0, a1) macro(a0), macro(a1)
#define CXXTRACE_CPP_MAP_3(macro, a0, a1, a2) macro(a0), macro(a1), macro(a2)
#define CXXTRACE_CPP_MAP_4(macro, a0, a1, a2, a3)                              \
  macro(a0), macro(a1), macro(a2), macro(a3)
#define CXXTRACE_CPP_MAP_5(macro, a0, a1, a2, a3, a4)                          \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4)
#define CXXTRACE_CPP_MAP_6(macro, a0, a1, a2, a3, a4, a5)                      \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5)
#define CXXTRACE_CPP_MAP_7(macro, a0, a1, a2, a3, a4, a5, a6)                  \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6)
#define CXXTRACE_CPP_MAP_8(macro, a0, a1, a2, a3, a4, a5, a6, a7)              \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7)
#define CXXTRACE_CPP_MAP_9(macro, a0, a1, a2, a3, a4, a5, a6, a7, a8)          \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8)
#define CXXTRACE_CPP_MAP_10(macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)     \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9)
#define CXXTRACE_CPP_MAP_11(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)                          \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10)
#define CXXTRACE_CPP_MAP_12(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)                     \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11)
#define CXXTRACE_CPP_MAP_13(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)                \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12)
#define CXXTRACE_CPP_MAP_14(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)           \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13)
#define CXXTRACE_CPP_MAP_15(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)      \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13), macro(a14)
#define CXXTRACE_CPP_MAP_16(                                                   \
  macro, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13), macro(a14), macro(a15)
#define CXXTRACE_CPP_MAP_17(macro,                                             \
                            a0,                                                \
                            a1,                                                \
                            a2,                                                \
                            a3,                                                \
                            a4,                                                \
                            a5,                                                \
                            a6,                                                \
                            a7,                                                \
                            a8,                                                \
                            a9,                                                \
                            a10,                                               \
                            a11,                                               \
                            a12,                                               \
                            a13,                                               \
                            a14,                                               \
                            a15,                                               \
                            a16)                                               \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13), macro(a14), macro(a15), macro(a16)
#define CXXTRACE_CPP_MAP_18(macro,                                             \
                            a0,                                                \
                            a1,                                                \
                            a2,                                                \
                            a3,                                                \
                            a4,                                                \
                            a5,                                                \
                            a6,                                                \
                            a7,                                                \
                            a8,                                                \
                            a9,                                                \
                            a10,                                               \
                            a11,                                               \
                            a12,                                               \
                            a13,                                               \
                            a14,                                               \
                            a15,                                               \
                            a16,                                               \
                            a17)                                               \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13), macro(a14), macro(a15), macro(a16), macro(a17)
#define CXXTRACE_CPP_MAP_19(macro,                                             \
                            a0,                                                \
                            a1,                                                \
                            a2,                                                \
                            a3,                                                \
                            a4,                                                \
                            a5,                                                \
                            a6,                                                \
                            a7,                                                \
                            a8,                                                \
                            a9,                                                \
                            a10,                                               \
                            a11,                                               \
                            a12,                                               \
                            a13,                                               \
                            a14,                                               \
                            a15,                                               \
                            a16,                                               \
                            a17,                                               \
                            a18)                                               \
  macro(a0), macro(a1), macro(a2), macro(a3), macro(a4), macro(a5), macro(a6), \
    macro(a7), macro(a8), macro(a9), macro(a10), macro(a11), macro(a12),       \
    macro(a13), macro(a14), macro(a15), macro(a16), macro(a17), macro(a18)

#endif
