#ifndef PUBSUB_H
#define PUBSUB_H

#include <pthread.h>

/*
 * Pub/Sub Messaging
 * -----------------
 * Clients subscribe to named channels.
 * When a message is published to a channel, all subscribers receive it.
 *
 * Commands:
 *   SUBSCRIBE channel [channel ...]
 *   UNSUBSCRIBE [channel ...]
 *   PUBLISH channel message
 */

/* Subscriber linked-list node */
typedef struct Subscriber {
    int                 fd;
    struct Subscriber  *next;
} Subscriber;

/* Channel: a named list of subscriber fds */
typedef struct Channel {
    char              *name;
    Subscriber        *subscribers;
    int                sub_count;
    struct Channel    *next;
} Channel;

/* Global Pub/Sub state */
typedef struct {
    Channel        *channels;      /* linked list of all channels */
    pthread_mutex_t lock;
} PubSubRegistry;

/* Initialise the global registry (call once at startup) */
void pubsub_init(PubSubRegistry *reg);

/* Subscribe fd to a channel.  Returns the total number of channels
   fd is now subscribed to (across all channels). */
int  pubsub_subscribe(PubSubRegistry *reg, int fd, const char *channel);

/* Unsubscribe fd from a channel.  Returns remaining subscription count.
   If channel is NULL, unsubscribe from ALL channels. */
int  pubsub_unsubscribe(PubSubRegistry *reg, int fd, const char *channel);

/* Unsubscribe fd from every channel (called on client disconnect). */
void pubsub_unsubscribe_all(PubSubRegistry *reg, int fd);

/* Publish a message to a channel.
   Returns the number of subscribers who received it. */
int  pubsub_publish(PubSubRegistry *reg, const char *channel, const char *message);

/* Free all channels and subscribers. */
void pubsub_free(PubSubRegistry *reg);

#endif
