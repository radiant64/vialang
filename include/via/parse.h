#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

const char* via_parse_ctx_cursor(struct via_value* ctx);

struct via_value* via_parse_ctx_program(struct via_value* ctx);

via_bool via_parse_ctx_matched(struct via_value* ctx);

struct via_value* via_parse(struct via_vm* vm, const char* source);

#ifdef __cplusplus
}
#endif

