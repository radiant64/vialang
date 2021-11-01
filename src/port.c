#include <via/port.h>

#include <via/alloc.h>

struct via_file_port {
    struct via_port_handle handle;

    FILE* file;
};

static via_int via_file_port_read(
    const struct via_port_handle* port,
    char* buffer,
    via_int size
) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    return fread(buffer, 1, size, file_port->file);
}

static char* via_file_port_readline(
    const struct via_port_handle* port,
    char* buffer,
    via_int size
) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    return fgets(buffer, size, file_port->file);
}

static via_int via_file_port_write(
    const struct via_port_handle* port,
    const char* buffer,
    via_int size
) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    return fwrite(buffer, 1, size, file_port->file);
}

static via_int via_file_port_seek(
    const struct via_port_handle* port,
    via_int position,
    enum via_port_seek seek_flag
) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    int whence = 0;
    switch (seek_flag) {
    case VIA_SEEK_CURRENT:
        whence = SEEK_CUR;
        break;
    case VIA_SEEK_SET:
        whence = SEEK_SET;
        break;
    case VIA_SEEK_END:
        whence = SEEK_END;
        break;
    }

    return fseek(file_port->file, position, whence);
}

static via_int via_file_port_tell(const struct via_port_handle* port) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    return ftell(file_port->file);
}

static via_bool via_file_port_eof(const struct via_port_handle* port) {
    const struct via_file_port* file_port = (const struct via_file_port*) port;

    return feof(file_port->file);
}

static void via_file_port_close(struct via_port_handle* port) {
    struct via_file_port* file_port = (struct via_file_port*) port;

    fclose(file_port->file);
    file_port->file = NULL;
}

static void via_file_port_free(struct via_port_handle* port) {
    struct via_file_port* file_port = (struct via_file_port*) port;

    if (file_port->file) {
        fclose(file_port->file);
    }
    via_free(file_port);
}

static void via_file_port_dummy_close(struct via_port_handle* port) {
}

static void via_file_port_dummy_free(struct via_port_handle* port) {
}

const struct via_port_handle* via_create_default_port(
    FILE* file,
    via_int flags
) {
    struct via_file_port* port = via_malloc(sizeof(struct via_file_port));
    if (!port) {
        return NULL;
    }

    port->file = file;

    port->handle = (struct via_port_handle) {
        flags,
        via_file_port_read,
        via_file_port_readline,
        via_file_port_write,
        via_file_port_seek,
        via_file_port_tell,
        via_file_port_eof,
        via_file_port_dummy_close,
        via_file_port_dummy_free
    };

    return (const struct via_port_handle*) port;
}

const struct via_port_handle* via_create_file_port(FILE* file, via_int flags) {
    struct via_file_port* port = via_malloc(sizeof(struct via_file_port));
    if (!port) {
        return NULL;
    }

    port->file = file;

    port->handle = (struct via_port_handle) {
        flags,
        via_file_port_read,
        via_file_port_readline,
        via_file_port_write,
        via_file_port_seek,
        via_file_port_tell,
        via_file_port_eof,
        via_file_port_close,
        via_file_port_free
    };

    return (const struct via_port_handle*) port;
}

const struct via_port_handle* via_open_file_port(
    const char* filename,
    via_int flags
) {
    FILE* file = NULL;

    switch (flags) {
    case VIA_PORT_INPUT:
        file = fopen(filename, "rb");
        break;
    case VIA_PORT_OUTPUT:
        file = fopen(filename, "wb");
        break;
    };
    
    if (!file) {
        return NULL;
    }

    const struct via_port_handle* port = via_create_file_port(file, flags);
    if (!port) {
        fclose(file);
        return NULL;
    }

    return port;
}

