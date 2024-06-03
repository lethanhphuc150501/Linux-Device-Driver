#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include "dummy_ioctl.h"

#define CHAR_DEVICE     "/dev/dummy_char"
#define MAX_BUF_SZ      20

int main(int argc, char **argv) {
    int fd = 0;
    int size = 0;
    char data_rcv[MAX_BUF_SZ];
    char data_snd[] = "mailisa";
    ssize_t num_bytes_written;
    
    if ((fd = open(CHAR_DEVICE, O_RDWR)) < 0)
        return -1;
    num_bytes_written = write(fd, data_snd, strlen(data_snd));
    lseek(fd, 0, SEEK_SET);
    read(fd, data_rcv, strlen(data_snd));
    printf("Read data: %s\n", data_rcv);

    ioctl(fd, DUMMY_GETSIZE, &size);
    printf("size = %d\n", size);

    ioctl(fd, DUMMY_RESIZE, 20);
    char data_snd_nxt[] = "banviencorp";
    num_bytes_written = write(fd, data_snd_nxt, strlen(data_snd_nxt));

    lseek(fd, 0, SEEK_SET);
    read(fd, data_rcv, MAX_BUF_SZ);    
    printf("Read data next: %s\n", data_rcv);
    ioctl(fd, DUMMY_CLEAR);
    read(fd, data_rcv, MAX_BUF_SZ);
    printf("Read data next: %s\n", data_rcv);
    close(fd);
    return 0;
}