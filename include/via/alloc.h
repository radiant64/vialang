#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void*(*via_malloc)(size_t size);
extern void(*via_free)(void* ptr);
extern void*(*via_calloc)(size_t nmemb, size_t size);
extern void*(*via_realloc)(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif


