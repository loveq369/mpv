#include <pthread.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>

#include <waio/waio.h>

#include "common/msg.h"
#include "osdep/io.h"
#include "input.h"

struct priv {
    struct mp_log *log;
    char *filename;
    struct mp_input_src *src;
    HANDLE terminate;
    pthread_t thread;
};

static void *reader_thread(void *ctx)
{
    struct priv *p = ctx;

    struct waio_cx_interface *waio = NULL;
    int mode = O_RDONLY;
    int fd = -1;
    bool close_fd = true;
    if (strcmp(p->filename, "/dev/stdin") == 0) { // for symmetry with unix
        fd = STDIN_FILENO;
        close_fd = false;
    }
    if (fd < 0)
        fd = open(p->filename, mode);
    if (fd < 0) {
        MP_ERR(p, "Can't open %s.\n", p->filename);
        goto done;
    }

    waio = waio_alloc((void *)_get_osfhandle(fd), 0, NULL, NULL);
    if (!waio) {
        MP_ERR(p, "Can't initialize win32 file reader.\n");
        goto done;
    }

    char buffer[128];
    struct waio_aiocb cb = {
        .aio_buf = buffer,
        .aio_nbytes = sizeof(buffer),
        .hsignal = p->terminate,
    };
    while (1) {
        if (waio_read(waio, &cb)) {
            MP_ERR(p, "Read operation failed.\n");
            break;
        }
        waio_suspend(waio, (const struct waio_aiocb *[]){&cb}, 1, NULL);
        if (WaitForSingleObject(p->terminate, 0) != WAIT_TIMEOUT)
            break;
        ssize_t r = waio_return(waio, &cb);
        if (r <= 0)
            break; // EOF or error
        mp_input_src_feed_cmd_text(p->src, buffer, r);
    }

done:
    MP_VERBOSE(p, "Exiting.\n");
    waio_free(waio);
    if (close_fd)
        close(fd);
    talloc_free(p);
    return NULL;
}

static void close_pipe(struct mp_input_src *src)
{
    struct priv *p = src->priv;
    // Cancel I/O and make the reader thread exit.
    SetEvent(p->terminate);
    pthread_join(p->thread, NULL);
    CloseHandle(p->terminate);
}

void mp_input_add_pipe(struct input_ctx *ictx, const char *filename)
{
    struct mp_input_src *src = mp_input_add_src(ictx);
    if (!src)
        return;

    struct priv *p = talloc_zero(NULL, struct priv);
    src->priv = p;
    p->filename = talloc_strdup(p, filename);
    p->src = src;
    p->log = mp_log_new(p, src->log, NULL);
    p->terminate = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!p->terminate || pthread_create(&p->thread, NULL, reader_thread, p)) {
        CloseHandle(p->terminate);
        talloc_free(p);
        mp_input_src_kill(src);
    } else {
        src->close = close_pipe;
    }
}
