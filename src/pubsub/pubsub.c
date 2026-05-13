/*
 * pubsub.c — Pub/Sub messaging system
 *
 * Maintains a linked list of channels, each with a linked list of
 * subscriber fds.  PUBLISH sends a formatted message directly to
 * each subscriber's socket.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "pubsub.h"

void pubsub_init(PubSubRegistry *reg) {
    reg->channels = NULL;
    pthread_mutex_init(&reg->lock, NULL);
}

static Channel* find_channel(PubSubRegistry *reg, const char *name) {
    Channel *ch = reg->channels;
    while (ch) {
        if (strcmp(ch->name, name) == 0) return ch;
        ch = ch->next;
    }
    return NULL;
}

static Channel* find_or_create_channel(PubSubRegistry *reg, const char *name) {
    Channel *ch = find_channel(reg, name);
    if (ch) return ch;
    ch = (Channel*)malloc(sizeof(Channel));
    if (!ch) return NULL;
    ch->name = strdup(name);
    ch->subscribers = NULL;
    ch->sub_count = 0;
    ch->next = reg->channels;
    reg->channels = ch;
    return ch;
}

static int count_subscriptions(PubSubRegistry *reg, int fd) {
    int count = 0;
    Channel *ch = reg->channels;
    while (ch) {
        Subscriber *s = ch->subscribers;
        while (s) {
            if (s->fd == fd) { count++; break; }
            s = s->next;
        }
        ch = ch->next;
    }
    return count;
}

int pubsub_subscribe(PubSubRegistry *reg, int fd, const char *channel) {
    pthread_mutex_lock(&reg->lock);
    Channel *ch = find_or_create_channel(reg, channel);
    if (!ch) { pthread_mutex_unlock(&reg->lock); return 0; }

    /* Check if already subscribed */
    Subscriber *s = ch->subscribers;
    while (s) {
        if (s->fd == fd) {
            int total = count_subscriptions(reg, fd);
            pthread_mutex_unlock(&reg->lock);
            return total;
        }
        s = s->next;
    }

    /* Add subscriber */
    s = (Subscriber*)malloc(sizeof(Subscriber));
    if (!s) { pthread_mutex_unlock(&reg->lock); return 0; }
    s->fd = fd;
    s->next = ch->subscribers;
    ch->subscribers = s;
    ch->sub_count++;

    int total = count_subscriptions(reg, fd);
    pthread_mutex_unlock(&reg->lock);

    /* Send subscribe confirmation */
    char buf[512];
    snprintf(buf, sizeof(buf), "*subscribe*%s*%d\r\nEND\n", channel, total);
    send(fd, buf, strlen(buf), MSG_NOSIGNAL);

    return total;
}

int pubsub_unsubscribe(PubSubRegistry *reg, int fd, const char *channel) {
    pthread_mutex_lock(&reg->lock);
    Channel *ch = find_channel(reg, channel);
    if (!ch) {
        int total = count_subscriptions(reg, fd);
        pthread_mutex_unlock(&reg->lock);
        return total;
    }

    Subscriber *prev = NULL, *curr = ch->subscribers;
    while (curr) {
        if (curr->fd == fd) {
            if (prev) prev->next = curr->next;
            else      ch->subscribers = curr->next;
            free(curr);
            ch->sub_count--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    int total = count_subscriptions(reg, fd);
    pthread_mutex_unlock(&reg->lock);

    char buf[512];
    snprintf(buf, sizeof(buf), "*unsubscribe*%s*%d\r\nEND\n", channel, total);
    send(fd, buf, strlen(buf), MSG_NOSIGNAL);

    return total;
}

void pubsub_unsubscribe_all(PubSubRegistry *reg, int fd) {
    pthread_mutex_lock(&reg->lock);
    Channel *ch = reg->channels;
    while (ch) {
        Subscriber *prev = NULL, *curr = ch->subscribers;
        while (curr) {
            if (curr->fd == fd) {
                if (prev) prev->next = curr->next;
                else      ch->subscribers = curr->next;
                Subscriber *tmp = curr;
                curr = curr->next;
                free(tmp);
                ch->sub_count--;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
        ch = ch->next;
    }
    pthread_mutex_unlock(&reg->lock);
}

int pubsub_publish(PubSubRegistry *reg, const char *channel, const char *message) {
    pthread_mutex_lock(&reg->lock);
    Channel *ch = find_channel(reg, channel);
    if (!ch) { pthread_mutex_unlock(&reg->lock); return 0; }

    int delivered = 0;
    char buf[4096];
    snprintf(buf, sizeof(buf), "*message*%s*%s\r\nEND\n", channel, message);
    size_t len = strlen(buf);

    Subscriber *s = ch->subscribers;
    while (s) {
        if (send(s->fd, buf, len, MSG_NOSIGNAL) > 0)
            delivered++;
        s = s->next;
    }

    pthread_mutex_unlock(&reg->lock);
    return delivered;
}

void pubsub_free(PubSubRegistry *reg) {
    pthread_mutex_lock(&reg->lock);
    Channel *ch = reg->channels;
    while (ch) {
        Channel *next_ch = ch->next;
        Subscriber *s = ch->subscribers;
        while (s) {
            Subscriber *next_s = s->next;
            free(s);
            s = next_s;
        }
        free(ch->name);
        free(ch);
        ch = next_ch;
    }
    reg->channels = NULL;
    pthread_mutex_unlock(&reg->lock);
    pthread_mutex_destroy(&reg->lock);
}
