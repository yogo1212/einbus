#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "libeinbus.h"


#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

int main(int argc, char **argv)
{

    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        fprintf(stderr, "usage: clock /path/to/clock.signal\n");
        exit(1);
    }

    einbus_t *service = einbus_create(argv[1]);

    if (service == 0) {
        perror("einbus_create");
        exit(0);
    }

    fd_set rfds;

    char buff [2000];
    int len = 0;;

    for (;;) {
        struct timeval tv;
        bzero(&tv, sizeof(struct timeval));
        tv.tv_sec = 60;

        FD_ZERO(&rfds);
        int maxfd = einbus_select_all(service, &rfds);

        if (select(maxfd + 2, &rfds, NULL, NULL, &tv) < 0) {
            perror("select");
            exit(1);
        }

        einbus_activate_all(service, &rfds, EINBUS_IGNORE_INBOUND);

        einbus_chan_t *chan = 0;

        while ((chan = einbus_fresh_chan(service))) {
            einbus_write(chan, &buff, len);
        }

        FILE *f = popen("date +%H:%M", "r");
        char c = fgetc(f);

        while (c != EOF) {
            einbus_broadcast(service, &c, 1);
            c = fgetc(f);
        }

        pclose(f);
    }

    einbus_destroy(service);
    return 0;
}

