#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* TODO */
#include <libgen.h>

#include "libeinbus.h"
#include "libeinbus-util.h"


int mkpath(const char *s, mode_t mode)
{
    char *q = NULL, *r = NULL, *path = NULL, *up = NULL;
    int rv;

    rv = 1;

    /* is the first char a dot or a slash? */
    if (!strcmp(s, ".") || !strcmp(s, "/")) {
        return 0;
    }

    if ((path = strdup(s)) == NULL) {
        return 1;
    }

    if ((q = strdup(s)) == NULL) {
        goto out;
    }

    if ((r = dirname(q)) == NULL) {
        goto out;
    }

    if ((up = strdup(r)) == NULL) {
        goto out;
    }

    if ((mkpath(up, mode) == -1) && (errno != EEXIST)) {
        goto out;
    }

    if ((mkdir(path, mode) == -1) && (errno != EEXIST)) {
        goto out;
    }

    rv = 0;

out:

    if (up != NULL) {
        free(up);
    }

    if (q != NULL) {
        free(q);
    }

    if (path != NULL) {
        free(path);
    }

    return rv;
}

int mksocketpath(const char *s)
{
    char *tmp = strdup(s);
    char *tmpt = tmp;

    tmpt = dirname(tmpt);
    int r = mkpath(tmpt, S_IRWXU | S_IRWXG);

    free(tmp);

    return r;
}

// #define DEBUG_LINKED_LIST 1

einbus_channel *einbus_client_add(einbus_service *service, einbus_channel *c)
{

#ifdef EINBUS_DEBUG

    fprintf(stderr, "adding %p\n", (void *) c);

#endif
    einbus_channel *cur;

    c->next = NULL;
    c->prev = NULL;

    /* check, whether the service has got any channels yet */
    if (!service->chanlist) {
#ifdef EINBUS_DEBUG

        fprintf(stderr, "are the first to be added. next %p prev %p\n", (void *) c->next, (void *) c->prev);

#endif
        return service->chanlist = c;
    }

    /* advance to the last element */
    for (cur = service->chanlist; cur->next; cur = cur->next) ;

#ifdef EINBUS_DEBUG

    fprintf(stderr, "putting at end. Last element %p next %p prev %p\n", (void *) cur, (void *) cur->next, (void *) cur->prev);
    fprintf(stderr, "new element %p next %p prev %p\n", (void *) c, (void *) c->next, (void *) c->prev);

#endif
    cur->next = c;
    c->prev = cur;

    return c;
}

einbus_channel *einbus_client_del(einbus_service *service, einbus_channel *c)
{
    if (!c) {
        return NULL;
    }

    if (c->service != service) {
        fprintf(stderr, "can't remove channel from unrelated service\n");
        return NULL;
    }

    return einbus_client_del_raw(&service->chanlist, c);;
}

einbus_channel *einbus_client_del_raw(einbus_channel **head, einbus_channel *c)
{
    if (!c) {
        return NULL;
    }

#if EINBUS_DEBUG
    fprintf(stderr, "pre (deleting %p) \n", (void *) c);

    for (einbus_channel *e = *head; e; e = e->next) {
        fprintf(stderr, "  %p\n", (void *) e);
    }

#endif

    /* These are the new next and and prev pointers */
    einbus_channel *prev = c->prev;
    einbus_channel *next = c->next;

    if (prev) {
        prev->next = next;
    }

    if (next) {
        next->prev = prev;
    }

    /* Did we just delete an element with no precessor?? */
    if (!prev) {
        /* Was this element really the first of our service? */
        if (c != *head) {
            fprintf(stderr, "loose element in chanlist  %p\n", (void *) c);
            return NULL;
        }

        /* Now that we are sure, we can change the pointer to the list to be the next element */
        *head = next;
    }

    c->service = NULL;
    einbus_disconnect(c);

#if EINBUS_DEBUG
    fprintf(stderr, "post\n");

    for (einbus_channel *e = *head; e; e = e->next) {
        fprintf(stderr, "  %p\n", (void *) e);
    }

#endif

    return next;
}
