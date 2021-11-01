#pragma once

#include <via/defs.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum via_port_flags {
    VIA_PORT_INPUT = 0x01,
    VIA_PORT_OUTPUT = 0x02
};

enum via_port_seek {
    VIA_SEEK_CURRENT,
    VIA_SEEK_SET,
    VIA_SEEK_END
};

struct via_port_handle;

typedef via_int(*via_port_read)(
    const struct via_port_handle* port,
    char* buffer,
    via_int size
);
typedef char*(*via_port_readline)(
    const struct via_port_handle* port,
    char* buffer,
    via_int size
);
typedef via_int(*via_port_write)(
    const struct via_port_handle* port,
    const char* buffer,
    via_int size
);
typedef via_int(*via_port_seek)(
    const struct via_port_handle* port,
    via_int position,
    enum via_port_seek seek_flag
);
typedef via_int(*via_port_tell)(const struct via_port_handle* port);
typedef via_bool(*via_port_eof)(const struct via_port_handle* port);
typedef void(*via_port_close)(struct via_port_handle* port);
typedef void(*via_port_free)(struct via_port_handle* port);

struct via_port_handle {
    via_int flags;

    via_port_read read;
    via_port_readline readline;
    via_port_write write;
    via_port_seek seek;
    via_port_tell tell;
    via_port_eof eof;
    via_port_close close;
    via_port_free free_handle;
};

const struct via_port_handle* via_create_default_port(
    FILE* file,
    via_int flags
);

const struct via_port_handle* via_create_file_port(FILE* file, via_int flags); 

const struct via_port_handle* via_open_file_port(
    const char* filename,
    via_int flags
);

#ifdef __cplusplus
}
#endif

