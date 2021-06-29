//  robust_io.c
//  Lagrange
//
//  Created by Yusuf Patoglu on 7.04.2021.
//

#include "robust_io.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fcntl.h>           /* For O_* constants */



ssize_t
robust_write (int fd, const void* buf, size_t size)
{
    ssize_t ret;
    
    do
    {
        ret = write(fd, buf, size);
    } while ((ret < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN));
    if (ret < 0)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
    return ret;
}
ssize_t
robust_pwrite (int fd, const void* buf, size_t size, off_t offset)
{
    ssize_t ret;
    
    do
    {   
        ret = pwrite(fd, buf, size, offset);
    } while ((ret < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN));
    if (ret < 0)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
    return ret;
}

ssize_t
robust_pread (int fd, void* buf, size_t size, off_t offset)
{
    ssize_t ret;
    
    do
    {   
        ret = pread(fd, buf, size, offset);
    } while ((ret < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN));
    if (ret < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    return ret;
}

 void 
 robust_ftruncate(int fd, off_t length)
 {
     if(ftruncate(fd, length) == -1)
     {
         perror("ftruncate fails: ");
         exit(EXIT_FAILURE);
     }
 }


ssize_t
robust_read (int fd, void* buf, size_t size)
{
    ssize_t ret;
    
    do
    {
        ret = read(fd, buf, size);
    } while ((ret < 0) && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN));
    if (ret < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    return ret;
}
int
robust_open(const char* file, int flags){
    int fd;
    if ((fd = open(file, flags, 0666)) < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    return fd;
}

off_t
robust_seek(int fd, off_t o, int flags){
    off_t offset = 0;
    if ((offset = lseek(fd, o, flags)) == -1)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    return offset;
}

void
robust_close(int fd)
{
    if(close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

void
robust_exit(const char* message, int status)
{
    fprintf(stderr, "%s", message);
    exit(status);
}

