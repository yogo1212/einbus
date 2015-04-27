#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libeinbus.h"

#include "tools.h"


int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: einbus-signal /path/to/echo.method\n");
        exit(1);
    }

    einbus_t *service = einbus_create(resolved_bus_path(argv[1]));

    if (service == 0) {
        perror("einbus_create");
        exit(errno);
    }

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    fd_set rfds;

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        int maxfd = einbus_select_all(service, &rfds);

        if (select(maxfd + 2, &rfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        einbus_activate_all(service, &rfds, 0);
        einbus_chan_t *chan = 0;

        while ((chan = einbus_ready_chan(service))) {
            char buff [200];
            int len = einbus_read(chan, &buff, 200);

            if (len < 1) {
                einbus_disconnect(chan);
            }
            else {
                write(1, &buff, len);
            }
        }

        if (FD_ISSET(0, &rfds)) {
            char buff [100];
            int n = read(0, &buff, 100);

            if (n == 0) {
                einbus_destroy(service);
                exit(0);
            }
            else if (n < 1) {
                perror("read");
                einbus_destroy(service);
                exit(errno);
            }

            if (einbus_broadcast(service, &buff, n) < 1) {
                perror("send");
                exit(errno);
            }
        }
    }

    einbus_destroy(service);
    return 0;
}


