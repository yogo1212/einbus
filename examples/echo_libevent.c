#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event.h>

#include "libeinbus.h"
#include "libeinbus-event.h"

typedef struct {
    struct event *evt;
    einbus_chan_t *chan;
} channel_event_data_t;

void read_event(evutil_socket_t fd, short events, void *userdata)
{
    channel_event_data_t *data = userdata;

    char buff[200];
    int len = read(fd, &buff, 200);


    if (len <= 0) {
        printf("finished on %d (%d)", fd, len);
        if (len == -1)
            printf("%s", strerror(errno));
        putchar('\n');
        einbus_disconnect(data->chan);
        printf("yooooooo\n");
        event_free(data->evt);
        free(userdata);
    }
    else {
        printf("got stuff to read and write on %d\n", fd);
        /* TODO this is still blocking */
        write(fd, &buff, len);
    }
}

void handle_connection(einbus_chan_t *chan, void *userdata)
{
    printf("Got new channel: %d %p\n", einbus_fd(chan), (void *) chan);

    channel_event_data_t *ding = malloc(sizeof(channel_event_data_t));
    ding->evt = event_new(userdata, einbus_fd(chan), EV_READ | EV_PERSIST, read_event, ding);
    ding->chan = chan;
    event_add(ding->evt, NULL);
}

int main(int argc, char *argv[])
{
    struct event_base *base = event_base_new();

    if (argc < 2) {
        fprintf(stderr, "usage: echo /path/to/echo.method\n");
        exit(1);
    }

    /* this is not async */
    einbus_t *service = einbus_create(argv[1]);

    if (service == 0) {
        perror("einbus_create");
        exit(0);
    }

    einbus_add_accept_event(service, base, handle_connection, base);

    event_base_loop(base, 0);
    printf("past loop\n");

    einbus_destroy(service);

    event_base_free(base);
    return 0;
}
