#pragma once


#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define  IN_CAP 65536

typedef struct client {
    int fd;                     // this client's socket
    uint8_t in[IN_CAP];             // bytes received but not yet consumed by your parser
    size_t in_len;                 // how many valid bytes are in `in`
    uint8_t *out;                    // bytes waiting to be sent
    size_t out_len;                // how many bytes are in `out`
    size_t out_cap;                // how much space `out` has
    size_t out_sent;               // how many bytes of `out` are already sent
    int close_after_flush;      // 1 = close once everything is sent
    int dead;                   // 1 = close at the next chance (error)
    time_t last_active;            // last time this client did anything
    void *user;                   // free slot for YOUR data (HTTP state, etc.)
} client_t;


typedef struct {

    void (*on_connect)(client_t *c);
    size_t (*on_data)(client_t *c, const uint8_t *data , size_t len);
    void (*on_close)(client_t *c);

} tcp_callbacks;



int  tcp_run(const char *port, tcp_callbacks cb);      // runs forever
int  client_send(client_t *c, const void *data, size_t len);
void client_close(client_t *c);                        // closes after pending data is sent


