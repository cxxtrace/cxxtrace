#ifndef CXXTRACE_DETAIL_ATTRIBUTE_H
#define CXXTRACE_DETAIL_ATTRIBUTE_H

#if defined(__clang__) && __clang_major__ <= 8
// Avoid the following diagnostic from Clang 8.0.0:
//
// > unknown attribute 'no_unique_address' ignored [-Wunknown-attributes]
#define CXXTRACE_NO_UNIQUE_ADDRESS
#else
#define CXXTRACE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#endif
