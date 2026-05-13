#ifndef CLUSTER_H
#define CLUSTER_H

#define CLUSTER_SLOTS 16384
#define NODE_ID_LEN   40

typedef struct {
    char  host[256];
    int   port;
} ClusterNode;

typedef struct {
    char         node_id[NODE_ID_LEN + 1];
    ClusterNode  self;
    int          cluster_enabled;
} ClusterState;

void         cluster_init(ClusterState *cs, const char *host, int port);
unsigned int cluster_keyslot(const char *key);
void         cluster_info(const ClusterState *cs, char *buf, size_t bufsize);
void         cluster_slots_str(const ClusterState *cs, char *buf, size_t bufsize);

#endif
