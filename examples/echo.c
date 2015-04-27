#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "libeinbus.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: echo /path/to/echo.method\n");
        exit(1);
    }

    einbus_t *service = einbus_create(argv[1]);

    if (service == 0) {
        perror("einbus_create");
        exit(0);
    }

    fd_set rfds;

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
            char buff[200];
            int len = einbus_read(chan, &buff, 200);

            if (len < 1) {
                einbus_disconnect(chan);
            }
            else {
                einbus_write(chan, &buff, len);
            }
        }
    }

    einbus_destroy(service);
    return 0;
}
