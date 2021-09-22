#include <via/parse.h>

#include <via/alloc.h>
#include <via/vm.h>

#include <stdlib.h>
#include <string.h>

struct via_value* via_parse_expr(struct via_vm* vm, struct via_value* context);

const char* via_parse_ctx_cursor(struct via_value* ctx) {
    return ctx->v_car->v_string;
}

struct via_value* via_parse_ctx_program(struct via_value* ctx) {
    return ctx->v_cdr->v_car;
}

via_bool via_parse_ctx_matched(struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_car->v_bool;
}

struct via_value* via_parse_ctx_parent(struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_cdr->v_car;
}

struct via_value* via_create_parse_ctx(
    struct via_vm* vm,
    const char* cursor,
    struct via_value* program,
    struct via_value* parent,
    via_bool matched
) {
    return via_make_pair(
        vm,
        via_make_stringview(vm, cursor),
        via_make_pair(
            vm,
            program,
            via_make_pair(
                vm,
                via_make_bool(vm, matched),
                via_make_pair(
                    vm,
                    parent,
                    NULL
                )
            )
        )
    );
}

struct via_value* via_parse_ctx_make_unmatched(
    struct via_vm* vm,
    struct via_value* ctx
) {
    return via_create_parse_ctx(
        vm,
        via_parse_ctx_cursor(ctx),
        via_parse_ctx_program(ctx),
        via_parse_ctx_parent(ctx),
        false
    );
}

struct via_value* via_parse_ctx_program_add(
    struct via_vm* vm,
    struct via_value* ctx,
    const char* cursor,
    struct via_value* val
) {
    struct via_value* program = via_parse_ctx_program(ctx);
    if (!program) {
        return via_create_parse_ctx(
            vm,
            cursor,
            via_make_pair(vm, val, NULL),
            via_parse_ctx_parent(ctx),
            true
        );
    }

    while (program->v_cdr) {
        program = program->v_cdr;
    }

    program->v_cdr = via_make_pair(vm, val, NULL);

    return via_create_parse_ctx(
        vm,
        cursor,
        via_parse_ctx_program(ctx),
        via_parse_ctx_parent(ctx),
        true
    );
}

static via_bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

struct via_value* via_parse_whitespace(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);

    while (is_whitespace(*c)) {
        c++;
    }

    // Comments are valid whitespace.
    if (*c == ';') {
        while (*c && *c != '\r' && *c != '\n') {
            c++;
        }
        // Repeat until all consecutive comments and whitespace have been
        // parsed.
        return via_parse_whitespace(vm, context);
    }

    return via_create_parse_ctx(
        vm,
        c,
        via_parse_ctx_program(context),
        via_parse_ctx_parent(context),
        true
    ); 
}

static via_bool terminates_value(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ')'
        || c == ';' || c == '\0';
}

struct via_value* via_parse_int(struct via_vm* vm, struct via_value* context) {
    const char* c = via_parse_ctx_cursor(context);
    char* end;
    via_int value = strtol(c, &end, 10);
    if (c == end || !terminates_value(*end)) {
        return via_parse_ctx_make_unmatched(vm, context);
    }

    return via_parse_ctx_program_add(vm, context, end, via_make_int(vm, value));
}

struct via_value* via_parse_float(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    char* end;
    via_float value = strtod(c, &end);
    if (c == end || !terminates_value(*end)) {
        return via_parse_ctx_make_unmatched(vm, context);
    }

    return via_parse_ctx_program_add(
        vm,
        context,
        end,
        via_make_float(vm, value)
    );
}

struct via_value* via_parse_bool(struct via_vm* vm, struct via_value* context) {
    const char* c = via_parse_ctx_cursor(context);
    if (c[0] == '#' && (c[1] == 't' || c[1] == 'f') && terminates_value(c[2])) {
        return via_parse_ctx_program_add(
            vm,
            context,
            &c[2],
            via_make_bool(vm, c[1] == 't')
        );
    }
    return via_parse_ctx_make_unmatched(vm, context);
}

static via_int via_copy_string(const char* c, char* dest) {
    if (*(c++) != '"') {
        return -1;
    }

    bool escaped = false;
    via_int len = 0;
    while (*c != '"') {
        if (!escaped && (*c == '\0' || *c == '\n' || *c == '\r')) {
            return -1;
        }
        if (!escaped && *c == '\\') {
            escaped = true;
        } else {
            escaped = false;
            if (dest) {
                dest[len] = *c;
            }
            len++;
        }
        c++;
    }

    return len;
}

struct via_value* via_parse_string(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    via_int len = via_copy_string(c, NULL);
    if (len < 0) {
        return via_parse_ctx_make_unmatched(vm, context);
    }

    char* buffer = via_malloc(len + 1);
    c += via_copy_string(c, buffer) + 2;
    struct via_value* val = via_make_value(vm);
    val->type = VIA_V_STRING;
    val->v_string = buffer;

    return via_parse_ctx_program_add(vm, context, c, val);
}

struct via_value* via_parse_symbol(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* start = via_parse_ctx_cursor(context);
    const char* c = start;
    if (
        terminates_value(*c) || *c == '"' || *c == '\'' || *c == '#'
            || (*c >= '0' && *c <= '9') || *c == '('
    ) {
        return via_parse_ctx_make_unmatched(vm, context);
    }

    for (c += 1; !terminates_value(*c); ++c) {
        if (*c == '"' || *c == '\'' || *c == '#' || *c == '(') {
            return via_parse_ctx_make_unmatched(vm, context);
        }
    }
    char* buffer = via_malloc(c - start + 1);
    memcpy(buffer, start, c - start);
    buffer[c - start] = '\0';

    struct via_value* symbol = via_symbol(vm, buffer);

    return via_parse_ctx_program_add(vm, context, c, symbol);
}

typedef struct via_value*(*parse_func)(struct via_vm*, struct via_value*);

struct via_value* via_parse_value(
    struct via_vm* vm,
    struct via_value* context
) {
    static const parse_func parsers[] = {
        via_parse_int,
        via_parse_float,
        via_parse_bool,
        via_parse_string,
        via_parse_symbol
    };
    static const size_t parsers_count = sizeof(parsers) / sizeof(parse_func);

    for (size_t i = 0; i < parsers_count; ++i) {
        context = parsers[i](vm, context);
        if (via_parse_ctx_matched(context)) {
            return context;
        }
    }

    return via_parse_ctx_make_unmatched(vm, context);
}

struct via_value* via_parse_oparen(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    if (*c != '(') {
        return via_parse_ctx_make_unmatched(vm, context);
    }

    return via_create_parse_ctx(
        vm,
        c + 1,
        NULL,
        via_make_pair(
            vm,
            via_parse_ctx_program(context),
            via_parse_ctx_parent(context)
        ),
        true
    );
}

struct via_value* via_parse_cparen(
    struct via_vm* vm,
    struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    if (*c != ')') {
        return via_parse_ctx_make_unmatched(vm, context);
    }
        
    struct via_value* parent = via_create_parse_ctx(
        vm,
        c + 1,
        via_parse_ctx_parent(context)->v_car,
        via_parse_ctx_parent(context)->v_cdr,
        true
    );
    
    return via_parse_ctx_program_add(
        vm,
        parent,
        c + 1,
        via_parse_ctx_program(context)
    );
}

struct via_value* via_parse_sexpr(
    struct via_vm* vm,
    struct via_value* context
) {
    context = via_parse_value(vm, context);

    if (!via_parse_ctx_matched(context)) {
        context = via_parse_oparen(vm, context);
        if (!via_parse_ctx_matched(context)) {
            return context;
        }

        while (via_parse_ctx_matched(context)) {

            context = via_parse_expr(vm, context);
        }
        context = via_parse_cparen(vm, context);

        if (!via_parse_ctx_matched(context)) {
            return context;
        }
    }

    return context;
}

struct via_value* via_parse_expr(struct via_vm* vm, struct via_value* context) {
    context = via_parse_sexpr(vm, via_parse_whitespace(vm, context));
    if (!via_parse_ctx_matched(context)) {
        return via_parse_ctx_make_unmatched(vm, context);
    }
    return via_parse_whitespace(vm, context);
}

struct via_value* via_parse(struct via_vm* vm, const char* source) {
    struct via_value* context = via_create_parse_ctx(
        vm,
        source,
        NULL,
        NULL,
        true
    );

    return via_parse_expr(vm, context);
}

