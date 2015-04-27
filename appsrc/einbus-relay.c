#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "libeinbus.h"


int main(int argc, char **argv)
{

    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        fprintf(stderr, "usage: echo /path/to/echo.method");
        exit(1);
    }

    einbus_t *service = einbus_create(argv[1]);

    if (service == 0) {
        perror("einbus_create");
        exit(0);
    }

    fd_set rfds;
    char buff [2048];

    for (;;) {
        FD_ZERO(&rfds);
        int maxfd = einbus_select_all(service, &rfds);

        if (select(maxfd + 2, &rfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        einbus_activate_all(service, &rfds, 0);
        einbus_chan_t *chan = 0;

        while ((chan = einbus_ready_chan(service))) {
            int len = einbus_read(chan, &buff, 2048);

            if (len < 1) {
                einbus_disconnect(chan);
            }
            else {
                einbus_broadcast_without(service, &buff, len, chan);
            }
        }
    }

    einbus_destroy(service);
    return 0;
}


