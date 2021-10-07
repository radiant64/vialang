#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

struct via_value* via_except_type_error(struct via_vm* vm, const char* message);

#ifdef __cplusplus
}
#endif

