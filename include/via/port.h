#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_MODE "input"
#define OUTPUT_MODE "output"
#define INPUT_OUTPUT_APPEND_MODE "input/output-append"
#define INPUT_OUTPUT_CREATE_MODE "input/output-create"

struct via_value;
struct via_vm;

void via_p_file_stdin(struct via_vm* vm);

void via_p_file_stdout(struct via_vm* vm);

void via_p_file_open(struct via_vm* vm);

void via_p_file_read(struct via_vm* vm);

void via_p_file_readline(struct via_vm* vm);

void via_p_file_write(struct via_vm* vm);

void via_p_file_seek(struct via_vm* vm);

void via_p_file_tell(struct via_vm* vm);

void via_p_file_eofp(struct via_vm* vm);

void via_p_file_close(struct via_vm* vm);

void via_add_port_procedures(struct via_vm* vm);

void via_file_close(const struct via_value* handle);

#ifdef __cplusplus
}
#endif

