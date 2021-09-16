#include <via/alloc.h>

#include <stdlib.h>

void*(*via_malloc)(size_t size) = malloc;
void(*via_free)(void* ptr) = free;
void*(*via_calloc)(size_t nmemb, size_t size) = calloc;
void*(*via_realloc)(void* ptr, size_t size) = realloc;

