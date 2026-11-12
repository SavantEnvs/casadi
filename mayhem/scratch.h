// Per-process scratch file for harnesses whose API only takes a file name. The file is created once
// under $TMPDIR and rewritten from the fuzzer input on every call.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unistd.h>

static inline std::string scratch_write(const uint8_t* data, size_t size) {
  static int fd = -1;
  static std::string path;
  if (fd < 0) {
    const char* dir = getenv("TMPDIR");
    path = std::string(dir && *dir ? dir : "/tmp") + "/casadi-fuzz-XXXXXX";
    fd = mkstemp(&path[0]);
    if (fd < 0) abort();
  }
  if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) != 0) abort();
  size_t off = 0;
  while (off < size) {
    ssize_t n = write(fd, data + off, size - off);
    if (n <= 0) abort();
    off += (size_t)n;
  }
  return path;
}
