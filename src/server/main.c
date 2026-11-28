#include "websocket.h"
#include <stdio.h>

static void on_connect(client_t *c) { printf("[fd %d] connected\n", c->fd); }

static size_t on_data(client_t *c, const uint8_t *data, size_t len) {
    printf("[fd %d] got %zu bytes\n", c->fd, len);
    client_send(c, data, len);          // send the same bytes back
    return len;                         // we used all of them
}

static void on_close(client_t *c) { printf("[fd %d] closed\n", c->fd); }

int main(void) {
    tcp_callbacks cb = { on_connect, on_data, on_close };
    return tcp_run("8080", cb) < 0 ? 1 : 0;
}