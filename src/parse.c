#include <via/parse.h>

#include <via/alloc.h>
#include <via/type-utils.h>
#include <via/vm.h>

#include <stdlib.h>
#include <string.h>

const struct via_value* via_parse_expr(
    struct via_vm* vm,
    const struct via_value* context
);

const char* via_parse_ctx_cursor(const struct via_value* ctx) {
    return ctx->v_car->v_string;
}

const char* via_parse_ctx_source(const struct via_value* ctx) {
    return ctx->v_cdr->v_car->v_string;
}

const struct via_value* via_parse_ctx_program(const struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_car;
}

via_bool via_parse_ctx_matched(const struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_cdr->v_car->v_bool;
}

via_bool via_parse_ctx_expr_open(const struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_cdr->v_cdr->v_car->v_bool;
}

const char* via_parse_ctx_file_path(const struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_cdr->v_cdr->v_cdr->v_car->v_string;
}

via_bool via_parse_success(const struct via_value* ctx) {
    return via_parse_ctx_matched(ctx) && !via_parse_ctx_expr_open(ctx);
}

const struct via_value* via_parse_ctx_parent(const struct via_value* ctx) {
    return ctx->v_cdr->v_cdr->v_cdr->v_cdr->v_cdr->v_cdr->v_car;
}

const struct via_value* via_create_parse_ctx(
    struct via_vm* vm,
    const char* cursor,
    const struct via_value* program,
    const struct via_value* parent,
    via_bool matched,
    via_bool expr_open,
    const char* file_path
) {
    return via_make_pair(
        vm,
        via_make_stringview(vm, cursor),
        via_make_pair(
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
                        via_make_bool(vm, expr_open),
                        via_make_pair(
                            vm,
                            via_make_stringview(vm, file_path),
                            via_make_pair(
                                vm,
                                parent,
                                NULL
                            )
                        )
                    )
                )
            )
        )
    );
}

const struct via_value* via_parse_ctx_make_unmatched(
    struct via_vm* vm,
    const struct via_value* ctx,
    via_bool expr_open
) {
    return via_create_parse_ctx(
        vm,
        via_parse_ctx_cursor(ctx),
        via_parse_ctx_program(ctx),
        via_parse_ctx_parent(ctx),
        false,
        expr_open,
        via_parse_ctx_file_path(ctx)
    );
}

const struct via_value* via_parse_ctx_program_add(
    struct via_vm* vm,
    const struct via_value* ctx,
    const char* cursor,
    const struct via_value* val
) {
    const struct via_value* program = via_parse_ctx_program(ctx);
    if (!program) {
        return via_create_parse_ctx(
            vm,
            cursor,
            via_make_pair(vm, val, NULL),
            via_parse_ctx_parent(ctx),
            true,
            false,
            via_parse_ctx_file_path(ctx)
        );
    }

    while (program->v_cdr) {
        program = program->v_cdr;
    }

    ((struct via_value*) program)->v_cdr = via_make_pair(vm, val, NULL);

    return via_create_parse_ctx(
        vm,
        cursor,
        via_parse_ctx_program(ctx),
        via_parse_ctx_parent(ctx),
        true,
        false,
        via_parse_ctx_file_path(ctx)
    );
}

static via_bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

const struct via_value* via_parse_whitespace(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);

    while (is_whitespace(*c)) {
        c++;
    }

    // Comments are valid whitespace. If first char in source is '#' and
    // parsing from a file, also treat line as a comment (to support hashbang
    // notation).
    if (
        *c == ';'
        || (
            c == via_parse_ctx_source(context) 
            && via_parse_ctx_file_path(context)
            && *c == '#'
        )
    ) {
        while (*c && *c != '\r' && *c != '\n') {
            c++;
        }
        if (*c) {
            // Repeat until all consecutive comments and whitespace have been
            // parsed.
            return via_parse_whitespace(
                vm,
                via_create_parse_ctx(
                    vm,
                    c,
                    via_parse_ctx_program(context),
                    via_parse_ctx_parent(context),
                    true,
                    true,
                    via_parse_ctx_file_path(context)
                )
            );
        }
    }

    return via_create_parse_ctx(
        vm,
        c,
        via_parse_ctx_program(context),
        via_parse_ctx_parent(context),
        true,
        true,
        via_parse_ctx_file_path(context)
    ); 
}

static via_bool terminates_value(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ')'
        || c == ';' || c == '\0';
}

const struct via_value* via_parse_int(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    char* end;
    via_int value = strtol(c, &end, 0);
    if (c == end || !terminates_value(*end)) {
        return via_parse_ctx_make_unmatched(vm, context, false);
    }

    return via_parse_ctx_program_add(vm, context, end, via_make_int(vm, value));
}

const struct via_value* via_parse_float(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    char* end;
    via_float value = strtod(c, &end);
    if (c == end || !terminates_value(*end)) {
        return via_parse_ctx_make_unmatched(vm, context, false);
    }

    return via_parse_ctx_program_add(
        vm,
        context,
        end,
        via_make_float(vm, value)
    );
}

const struct via_value* via_parse_bool(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    if (c[0] == '#' && (c[1] == 't' || c[1] == 'f') && terminates_value(c[2])) {
        return via_parse_ctx_program_add(
            vm,
            context,
            &c[2],
            via_make_bool(vm, c[1] == 't')
        );
    }
    return via_parse_ctx_make_unmatched(vm, context, false);
}

static via_int via_copy_string(const char* c, char* dest) {
    if (*(c++) != '"') {
        return -2;
    }

    const char* start = c;

    bool escaped = false;
    via_int len = 0;
    while (*c != '"') {
        if (escaped) {
            char ec = 0;
            switch (*c) {
            case 'n':
                ec = '\n';
                break;
            case 'r':
                ec = '\r';
                break;
            case 't':
                ec = '\t';
                break;
            }
            if (ec) {
                if (dest) {
                    dest[len] = ec;
                }
                len++;
            }
            escaped = false;
        } else {
            if (*c == '\0') {
                return -1;
            }
            if (*c == '\n' || *c == '\r') {
                return -2;
            }
            if (*c == '\\') {
                escaped = true;
            } else {
                if (dest) {
                    dest[len] = *c;
                }
                len++;
            }
        }
        c++;
    }
    if (dest) {
        dest[len] = '\0';
        // If writing to buffer, return number of characters read rather than
        // written, to advance cursor properly!
        return ((intptr_t) c) - ((intptr_t) start);
    }

    return len;
}

const struct via_value* via_parse_string(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    via_int len = via_copy_string(c, NULL);
    if (len < 0) {
        // A length of -1 indicates end of program with an open string, so
        // indicate the expression is still open (to allow streaming to
        // continue.
        return via_parse_ctx_make_unmatched(vm, context, len == -1);
    }

    char* buffer = via_malloc(len + 1);
    c += via_copy_string(c, buffer) + 2;
    struct via_value* val = via_make_value(vm);
    val->type = VIA_V_STRING;
    val->v_string = buffer;

    return via_parse_ctx_program_add(vm, context, c, val);
}

const struct via_value* via_parse_symbol(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* start = via_parse_ctx_cursor(context);
    const char* c = start;
    if (
        terminates_value(*c) || *c == '"' || *c == '\'' || *c == '#'
            || (*c >= '0' && *c <= '9') || *c == '('
    ) {
        return via_parse_ctx_make_unmatched(vm, context, false);
    }

    for (c += 1; !terminates_value(*c); ++c) {
        if (*c == '"' || *c == '\'' || *c == '#' || *c == '(') {
            return via_parse_ctx_make_unmatched(vm, context, false);
        }
    }
    char* buffer = via_malloc(c - start + 1);
    memcpy(buffer, start, c - start);
    buffer[c - start] = '\0';

    const struct via_value* symbol = via_sym(vm, buffer);
    via_free(buffer);

    return via_parse_ctx_program_add(vm, context, c, symbol);
}

typedef const struct via_value*(*parse_func)(
    struct via_vm*,
    const struct via_value*
);

const struct via_value* via_parse_value(
    struct via_vm* vm,
    const struct via_value* context
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

    return via_parse_ctx_make_unmatched(
        vm,
        context,
        via_parse_ctx_expr_open(context)
    );
}

const struct via_value* via_parse_oparen(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    if (*c != '(') {
        return via_parse_ctx_make_unmatched(vm, context, false);
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
        true,
        true,
        via_parse_ctx_file_path(context)
    );
}

const struct via_value* via_parse_cparen(
    struct via_vm* vm,
    const struct via_value* context
) {
    const char* c = via_parse_ctx_cursor(context);
    if (*c != ')') {
        return via_parse_ctx_make_unmatched(vm, context, true);
    }
        
    const struct via_value* parent = via_create_parse_ctx(
        vm,
        c + 1,
        via_parse_ctx_parent(context)->v_car,
        via_parse_ctx_parent(context)->v_cdr,
        true,
        false,
        via_parse_ctx_file_path(context)
    );
    
    return via_parse_ctx_program_add(
        vm,
        parent,
        c + 1,
        via_parse_ctx_program(context)
    );
}

const struct via_value* via_parse_sexpr(
    struct via_vm* vm,
    const struct via_value* context
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

const struct via_value* via_parse_expr(
    struct via_vm* vm,
    const struct via_value* context
) {
    context = via_parse_sexpr(vm, via_parse_whitespace(vm, context));
    if (!via_parse_ctx_matched(context)) {
        return via_parse_ctx_make_unmatched(
            vm,
            context,
            via_parse_ctx_expr_open(context)
        );
    }
    return context;
}

const struct via_value* via_parse(
    struct via_vm* vm,
    const char* source,
    const char* file_path
) {
    const struct via_value* context = via_create_parse_ctx(
        vm,
        source,
        NULL,
        NULL,
        true,
        true,
        file_path
    );

    return via_parse_expr(vm, context);
}

