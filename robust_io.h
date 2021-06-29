
#ifndef robust_io_h
#define robust_io_h
#include <stdio.h>

//open function with many error checks.
int
robust_open(const char* file, int flags);
//write function with many error checks.
ssize_t
robust_write (int fd, const void* buf, size_t size);
//close function with many error checks.
void
robust_close(int fd);

ssize_t
robust_read (int fd, void* buf, size_t size);
ssize_t
robust_pwrite (int fd, const void* buf, size_t size, off_t offset);

ssize_t
robust_pread (int fd, void* buf, size_t size, off_t offset); 

off_t
robust_seek(int fd, off_t o, int flags);

void
robust_exit(const char* message, int status);
#endif /* robust_io_h */


