#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libeinbus.h"

#include "tools.h"


int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: einbus-connect /path/to/echo.method\n");
        exit(1);
    }

    char *path = resolved_bus_path(argv[1]);
    einbus_chan_t *chan = einbus_connect(path);

    if (chan == 0) {
        perror("einbus_connect");
        exit(0);
    }

    if (einbus_activate(chan) == EINBUS_ERROR) {
        perror("einbus_activate");
        exit(0);
    }

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    fd_set rfds;
    int stdindead = 0;

    for (;;) {
        FD_ZERO(&rfds);

        if (!stdindead) {
            FD_SET(0, &rfds);
        }

        int cfd = einbus_chan_fd(chan);
        FD_SET(cfd, &rfds);

        if (select(cfd + 2, &rfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(errno);
        }

        if (FD_ISSET(0, &rfds)) {
            char buff [100];
            int n = read(0, &buff, 100);

            if (n == 0) {
                stdindead = 1;
                einbus_close(chan);
                continue;
            }
            else if (n < 1) {
                perror("read");
                exit(errno);
            }

            if (einbus_write(chan, &buff, n) < 1) {
                perror("send");
                exit(errno);
            }
        }

        if (FD_ISSET(cfd, &rfds)) {
            EINBUS_STATUS st = einbus_activate(chan);

            if (st == EINBUS_READY) {
                char buff [1000];
                int n = einbus_read(chan, &buff, 1000);

                if (n < 1) {
                    exit(0);
                }

                write(1, &buff, n);
            }
            else if (st == EINBUS_ERROR) {
                perror("einbus_activate");
                exit(errno);
            }
        }
    }

    einbus_disconnect(chan);
    return 0;
}
