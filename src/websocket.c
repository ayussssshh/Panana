#define _GNU_SOURCE

#include "websocket.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>



#define BACKLOG 128
#define MAX_CLIENTS 512
#define OUT_MAX (1 << 20)
#define IDLE_SECONDS 60



static struct pollfd fds[MAX_CLIENTS + 1];
static client_t *clients[MAX_CLIENTS + 1];
static int nfds = 0;




static int listen_socket(const char *port) {

  struct addrinfo hint, *res, *p;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(NULL, port, &hint, &res);

  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }

  int lfd = -1;
  for (p = res; p; p = p->ai_next) {

    lfd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);

    if (lfd < 0)
      continue;

    int yes = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(lfd, p->ai_addr, p->ai_addrlen) == 0 && listen(lfd, BACKLOG) == 0)
      break;

    perror("bind/socket");
    close(lfd);
    lfd = -1;
  }

  freeaddrinfo(res);
  return lfd;
}




int client_send(client_t *c, const void *data, size_t len) {

  if (c->dead || c->close_after_flush)
    return -1;

  size_t pending = c->out_len - c->out_sent;
  if (pending + len > OUT_MAX) {
    c->dead = 1;
    return -1;
  }

  if (c->out_sent > 0) {
    memmove(c->out, c->out + c->out_sent, pending);
    c->out_len = pending;
    c->out_sent = 0;
  }

  if (c->out_len + len > c->out_cap) {
    size_t cap = c->out_cap ? c->out_cap : 4096;
    while (cap < c->out_len + len) {
      cap *= 2;
    }

    uint8_t *p = realloc(c->out, cap);
    if (!p) {
      c->dead = 1;
      return -1;
    }

    c->out = p;
    c->out_cap = cap;
  }

  memcpy(c->out + c->out_len, data, len);
  c->out_len += len;
  return 0;
}





void client_close(client_t *c) { c->close_after_flush = 1; }

static void remove_client(int i, const tcp_callbacks *cb) {

  client_t *c = clients[i];

  if (cb->on_close)
    cb->on_close(c);
  close(c->fd);
  free(c->out);
  free(c);
  fds[i] = fds[nfds - 1];
  clients[i] = clients[nfds - 1];
  nfds--;
}

static void accept_client(int lfd, const tcp_callbacks *cb) {

  for (;;) {

    int fd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK);

    if (fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      if (errno == EINTR || errno == ECONNABORTED)
        continue;
      perror("accept4");
      return;
    }

    if (nfds - 1 >= MAX_CLIENTS) {
      close(fd);
      continue;
    }

    client_t *c = calloc(1, sizeof(*c));
    if (!c) {
      close(fd);
      continue;
    }
    c->fd = fd;
    c->last_active = time(NULL);

    fds[nfds].fd = fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;

    clients[nfds] = c;
    nfds++;

    if (cb->on_connect)
      cb->on_connect(c);
  }
}




static int read_client(client_t *c, const tcp_callbacks *cb) {

  ssize_t n = read(c->fd, c->in + c->in_len, IN_CAP - c->in_len);
  if (n == 0)
    return -1; // client closed the connection
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
    return -1; // real error
  }

  c->in_len += n;
  c->last_active = time(NULL); 

  while (c->in_len > 0 && !c->dead && !c->close_after_flush) {

    size_t used = cb->on_data(c, c->in, c->in_len);
    if (used == 0) break;                       // incomplete message: wait for more
    if (used > c->in_len) used = c->in_len;
    memmove(c->in, c->in + used, c->in_len - used);
    c->in_len -= used;
  
  }

  if (c->in_len == IN_CAP) return -1;             // buffer full, no progress: abuse
  return 0;
}




static int flush_client(client_t *c) {
    while (c->out_sent < c->out_len) {
        ssize_t n = send(c->fd, c->out + c->out_sent, c->out_len - c->out_sent,
                         MSG_NOSIGNAL);             
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  
            if (errno == EINTR) continue;
            return -1;
        }
        c->out_sent += n;
        c->last_active = time(NULL);
    }
    c->out_len = c->out_sent = 0;                   
    return 0;
}



int tcp_run(const char *port, tcp_callbacks cb){

    signal(SIGPIPE, SIG_IGN);

    int lfd = listen_socket(port);
    if(lfd < 0) return -1;

    fds[0].fd = lfd;
    fds[0].events = POLLIN;
    nfds = 1;
    printf("listening on port %s\n", port);

    for(;;){

        for (int i = nfds - 1; i >= 1; i--) {
            
            client_t *c = clients[i];
            int pending = c->out_sent < c->out_len;

            if (c->dead || (c->close_after_flush && !pending)) {
                remove_client(i, &cb);
                continue;
            }

            fds[i].events = c->close_after_flush ? 0 : POLLIN;
            if(pending) fds[i].events |= POLLOUT;

        }


        int ready = poll(fds, nfds, 1000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        time_t now = time(NULL);


        for (int i = nfds - 1; i >= 0; i--) {
            if (i == 0) {                                        
                if (fds[0].revents & POLLIN) accept_client(lfd, &cb);
                continue;
            }

            client_t *c = clients[i];
            short re = fds[i].revents;
            int drop = 0;

            if (re & POLLIN)                    drop = read_client(c, &cb) < 0;
            if (!drop && (re & POLLOUT))        drop = flush_client(c) < 0;
            if (!drop && (re & (POLLERR | POLLNVAL))) drop = 1;
            if (!drop && (re & POLLHUP) && !(re & POLLIN)) drop = 1;
            if (!drop && now - c->last_active > IDLE_SECONDS)  drop = 1;

            if (drop) remove_client(i, &cb);
        }
    }

    close(lfd);
    return -1;
}