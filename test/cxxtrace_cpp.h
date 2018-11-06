#ifndef CXXTRACE_CPP_H
#define CXXTRACE_CPP_H

#define CXXTRACE_CPP_CALL(macro, ...) macro(__VA_ARGS__)

// Expand to the number of variadic arguments given.
//
// For example, CXXTRACE_CPP_COUNT_ARGS(x, y(z, w, q)) expands to 2.
#define CXXTRACE_CPP_COUNT_ARGS(...)                                           \
  CXXTRACE_CPP_COUNT_ARGS_INDEX(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define CXXTRACE_CPP_COUNT_ARGS_INDEX(                                         \
  a9, a8, a7, a6, a5, a4, a3, a2, a1, count, ...)                              \
  count
static_assert(CXXTRACE_CPP_COUNT_ARGS(x, y(z, w, q)) == 2);

#define CXXTRACE_CPP_PASTE(x, y) CXXTRACE_CPP_PASTE_INNER(x, y)
#define CXXTRACE_CPP_PASTE_INNER(x, y) x##y

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
  CXXTRACE_CPP_CALL(CXXTRACE_CPP_PASTE(CXXTRACE_CPP_STRING_LITERALS_,          \
                                       CXXTRACE_CPP_COUNT_ARGS(__VA_ARGS__)),  \
                    __VA_ARGS__)
#define CXXTRACE_CPP_STRING_LITERALS_1(a0) #a0
#define CXXTRACE_CPP_STRING_LITERALS_2(a0, a1) #a0, #a1
#define CXXTRACE_CPP_STRING_LITERALS_3(a0, a1, a2) #a0, #a1, #a2
#define CXXTRACE_CPP_STRING_LITERALS_4(a0, a1, a2, a3) #a0, #a1, #a2, #a3

#endif
