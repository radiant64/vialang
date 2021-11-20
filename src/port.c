#include <via/port.h>

#include "exception-strings.h"

#include <via/alloc.h>
#include <via/exceptions.h>
#include <via/type-utils.h>
#include <via/value.h>
#include <via/vm.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

void via_p_file_stdin(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_get(vm, "file-stdin-object");

    if (vm->ret->type == VIA_V_UNDEFINED) {
        vm->ret = via_make_handle(vm, stdin);
        via_env_set(vm, via_sym(vm, "file-stdin-object"), vm->ret);
    }
}

void via_p_file_stdout(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_get(vm, "file-stdin-object");

    if (vm->ret->type == VIA_V_UNDEFINED) {
        vm->ret = via_make_handle(vm, stdout);
        via_env_set(vm, via_sym(vm, "file-stdout-object"), vm->ret);
    }
}

void via_p_file_stderr(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_get(vm, "file-stderr-object");

    if (vm->ret->type == VIA_V_UNDEFINED) {
        vm->ret = via_make_handle(vm, stderr);
        via_env_set(vm, via_sym(vm, "file-stderr-object"), vm->ret);
    }
}

void via_p_file_open(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* filename = via_pop_arg(vm);
    const struct via_value* mode = via_pop_arg(vm);

    const char* mode_str = "";
    if (mode == via_sym(vm, INPUT_MODE)) {
        mode_str = "rb";
    } else if (mode == via_sym(vm, OUTPUT_MODE)) {
        mode_str = "wb";
    } else if (mode == via_sym(vm, INPUT_OUTPUT_APPEND_MODE)) {
        mode_str = "rb+";
    } else if (mode == via_sym(vm, INPUT_OUTPUT_CREATE_MODE)) {
        mode_str = "wb+";
    } else {
        via_throw(vm, via_except_invalid_type(vm, INVALID_MODE));
        return;
    }

    FILE* f = fopen(filename->v_string, mode_str);
    if (!f) {
        via_throw(vm, via_except_io_error(vm, FILE_OPEN_FAILED));
        return;
    }

    vm->ret = via_make_handle(vm, f);
}

void via_p_file_read(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }
    const struct via_value* char_count = via_pop_arg(vm);
    if (char_count->type != VIA_V_INT) {
        via_throw(vm, via_except_invalid_type(vm, INT_REQUIRED));
        return;
    }

    if (feof(handle->v_handle)) {
        return;
    }

    char* dest = via_calloc(1, char_count->v_int + 1);
    if (!dest) {
        via_throw(vm, via_except_out_of_memory(vm, ALLOC_FAIL));
        return;
    }

    size_t count = fread(dest, 1, char_count->v_int + 1, handle->v_handle);
    if (count != char_count->v_int) {
        if (feof(handle->v_handle)) {
            via_throw(vm, via_except_end_of_file(vm, END_OF_FILE));
        } else {
            via_throw(vm, via_except_io_error(vm, READ_ERROR));
        }
        goto cleanup;
    }

    dest[char_count->v_int] = '\0';

    vm->ret = via_make_string(vm, dest);

cleanup:
    via_free(dest);
}

void via_p_file_readline(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }

    char* dest = via_malloc(4096);
    if (!dest) {
        via_throw(vm, via_except_out_of_memory(vm, ALLOC_FAIL));
        return;
    }

    const char* buf = fgets(dest, 4096, handle->v_handle);
    if (!buf) {
        if (feof(handle->v_handle)) {
            via_throw(vm, via_except_end_of_file(vm, END_OF_FILE));
        } else {
            via_throw(vm, via_except_io_error(vm, READ_ERROR));
        }
        goto cleanup;
    }

    vm->ret = via_make_string(vm, buf);

cleanup:
    via_free(dest);
}

void via_p_file_write(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }
    const struct via_value* char_seq = via_pop_arg(vm);
    if (char_seq->type != VIA_V_STRING && char_seq->type != VIA_V_STRINGVIEW) {
        via_throw(vm, via_except_invalid_type(vm, STRING_REQUIRED));
        return;
    }

    fwrite(char_seq->v_string, 1, strlen(char_seq->v_string), handle->v_handle);
    if (ferror(handle->v_handle)) {
        via_throw(vm, via_except_io_error(vm, WRITE_ERROR));
    }
}

void via_p_file_seek(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }
    const struct via_value* offset = via_pop_arg(vm);
    if (offset->type != VIA_V_INT) {
        via_throw(vm, via_except_invalid_type(vm, INT_REQUIRED));
        return;
    }
    const struct via_value* whence_val = via_pop_arg(vm);

    int whence = SEEK_SET;
    if (whence_val) {
        if (whence_val->type != VIA_V_SYMBOL) {
            via_throw(vm, via_except_invalid_type(vm, SYMBOL_REQUIRED));
            return;
        }

        if (whence_val == via_sym(vm, "set")) {
            whence = SEEK_SET;
        } else if (whence_val == via_sym(vm, "current")) {
            whence = SEEK_CUR;
        } else if (whence_val == via_sym(vm, "end")) {
            whence = SEEK_END;
        } else {
            via_throw(vm, via_except_argument_error(vm, INVALID_SYMBOL));
            return;
        }
    }

    int result = fseek(handle->v_handle, offset->v_int, whence);
    if (result != 0) {
        if (errno == ESPIPE) {
            via_throw(vm, via_except_no_capability(vm, NOT_SEEKABLE));
        } if (errno == EINVAL) {
            via_throw(vm, via_except_out_of_bounds(vm, SEEK_BOUNDS));
        }else {
            via_throw(vm, via_except_io_error(vm, FILE_ACCESS));
        }
    }
}

void via_p_file_tell(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }

    int offset = ftell(handle->v_handle);
    if (offset == -1) {
        if (errno == ESPIPE) {
            via_throw(vm, via_except_no_capability(vm, NOT_SEEKABLE));
        } else {
            via_throw(vm, via_except_io_error(vm, FILE_ACCESS));
        }
        return;
    }

    vm->ret = via_make_int(vm, offset);
}

void via_p_file_eofp(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }

    vm->ret = via_make_bool(vm, feof(handle->v_handle));
}

void via_p_file_close(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* handle = via_pop_arg(vm);
    if (handle->type != VIA_V_HANDLE) {
        via_throw(vm, via_except_invalid_type(vm, EXPECTED_HANDLE));
        return;
    }

    via_file_close(handle);
}

void via_add_port_procedures(struct via_vm* vm) {
    via_register_proc(
        vm,
        "file-stdin",
        "file-stdin-proc",
        NULL,
        (via_bindable) via_p_file_stdin
    );
    via_register_proc(
        vm,
        "file-stdout",
        "file-stdout-proc",
        NULL,
        (via_bindable) via_p_file_stdout
    );
    via_register_proc(
        vm,
        "file-stderr",
        "file-stderr-proc",
        NULL,
        (via_bindable) via_p_file_stderr
    );
    via_register_proc(
        vm,
        "file-open",
        "file-open-proc",
        NULL,
        (via_bindable) via_p_file_open
    );
    via_register_proc(
        vm,
        "file-read",
        "file-read-proc",
        NULL,
        (via_bindable) via_p_file_read
    );
    via_register_proc(
        vm,
        "file-read-line",
        "file-read-line-proc",
        NULL,
        (via_bindable) via_p_file_readline
    );
    via_register_proc(
        vm,
        "file-write",
        "file-write-proc",
        NULL,
        (via_bindable) via_p_file_write
    );
    via_register_proc(
        vm,
        "file-seek",
        "file-seek-proc",
        NULL,
        (via_bindable) via_p_file_seek
    );
    via_register_proc(
        vm,
        "file-tell",
        "file-tell-proc",
        NULL,
        (via_bindable) via_p_file_tell
    );
    via_register_proc(
        vm,
        "file-eof?",
        "file-eofp-proc",
        NULL,
        (via_bindable) via_p_file_eofp
    );
    via_register_proc(
        vm,
        "file-close",
        "file-close-proc",
        NULL,
        (via_bindable) via_p_file_close
    );
}

void via_file_close(const struct via_value* handle) {
    FILE* f = handle->v_handle;
    if (f == stdout || f == stdin || f == stderr) {
        return;
    }

    fclose(handle->v_handle);
}

