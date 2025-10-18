#include <stdbool.h>
#include <sys/types.h>

#ifndef API_H
#define API_H 1

#define SOCKBUFALLOCSIZ 65536

#define io_new(init) _io_new(init, false)
#define sock_io_new() _io_new(SOCKBUFALLOCSIZ, true)

struct io_data *_io_new(size_t initial, bool socket_buf);
void io_reinit(struct io_data *io_data);

struct io_data
{
  size_t siz;
  char *ptr;
  char *cur;
  bool sock;
  bool close;
};

void api(int api_thr_id);

#endif // API_H
