/*
 * Serial port implementation for Linux
 */

#include "sat_serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    default:      return B115200;
    }
}

int serial_open(const char *device, int baud_rate)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "serial: open %s: %s\n", device, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "serial: tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    speed_t speed = baud_to_speed(baud_rate);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    /* 8N1, no flow control */
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= CLOCAL | CREAD;

    /* Raw mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                      INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    /* Non-blocking reads */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "serial: tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    /* Clear O_NONBLOCK after configuration */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return fd;
}

int serial_write(int fd, const uint8_t *data, size_t len)
{
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "serial: write: %s\n", strerror(errno));
            return -1;
        }
        written += (size_t)n;
    }
    return (int)written;
}

void serial_close(int fd)
{
    if (fd >= 0)
        close(fd);
}
