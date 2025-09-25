#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>

int open_pty(void);

static struct termios orig;

static void open_keyboard(void)
{
    struct termios new;

    tcgetattr(0, &orig);
    new = orig;
    new.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    new.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | BRKINT);
    new.c_cflag &= ~(CSIZE | PARENB);
    new.c_cflag |= CS8;
    new.c_cc[VMIN] = 1;     /* =1 required for lone ESC key processing */
    new.c_cc[VTIME] = 0;
    tcsetattr(0, TCSAFLUSH, &new);
}

static void close_keyboard(void)
{
    tcsetattr(0, TCSANOW, &orig);
}

int main(int ac, char **av)
{
    int term_fd = open_pty();
    if (term_fd < 0) {
        printf("Can't create PTYs\n");
        return 1;
    }

    open_keyboard();
    printf("START\n");
    for (;;) {
        fd_set fdset;
        int n, ret;
        char buf[256];

        FD_ZERO(&fdset);
        FD_SET(0, &fdset);
        FD_SET(term_fd, &fdset);
        ret = select(term_fd + 1, &fdset, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (FD_ISSET(0, &fdset)) {
            n = read(0, buf, sizeof(buf));
            if (n > 0)
                write(term_fd, buf, n);
            else break;
        }
        if (FD_ISSET(term_fd, &fdset)) {
            n = read(term_fd, buf, sizeof(buf));
            if (n > 0)
                write(1, buf, n);
            else break;
        }
    }
    printf("END\n");
    close_keyboard();
    return 0;
}
