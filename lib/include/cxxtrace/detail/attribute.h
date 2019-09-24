#ifndef CXXTRACE_DETAIL_ATTRIBUTE_H
#define CXXTRACE_DETAIL_ATTRIBUTE_H

#include <cxxtrace/detail/have.h>

#if CXXTRACE_HAVE_NO_UNIQUE_ADDRESS
#define CXXTRACE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define CXXTRACE_NO_UNIQUE_ADDRESS
#endif

#endif
