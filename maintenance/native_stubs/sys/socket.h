#pragma once
#include <cstddef>
#include <cerrno>
#define MSG_DONTWAIT 1
extern bool fakeSendBlocked;
inline int send(int fd, const void*, size_t n, int) {
  if (fd < 0) { errno = EBADF; return -1; }
  if (fakeSendBlocked) { errno = EAGAIN; return -1; }
  return (int)n;
}
