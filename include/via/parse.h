#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

const char* via_parse_ctx_cursor(const struct via_value* ctx);

const char* via_parse_ctx_source(const struct via_value* ctx);

const struct via_value* via_parse_ctx_program(const struct via_value* ctx);

via_bool via_parse_ctx_matched(const struct via_value* ctx);

via_bool via_parse_ctx_expr_open(const struct via_value* ctx);

via_bool via_parse_success(const struct via_value* ctx);

const struct via_value* via_parse(struct via_vm* vm, const char* source);

#ifdef __cplusplus
}
#endif

