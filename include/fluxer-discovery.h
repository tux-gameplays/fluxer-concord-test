#ifndef FLUXER_DISCOVERY_H
#define FLUXER_DISCOVERY_H

#include <stddef.h>
#include "concord-error.h"

struct fluxer_discovery {
    char *api_base;
    char *gateway_url;
    char *media_url;
};

CCORDcode fluxer_discovery_parse(const char json[], size_t size,
                                 struct fluxer_discovery *discovery);
CCORDcode fluxer_discovery_fetch(const char origin[],
                                 struct fluxer_discovery *discovery);
void fluxer_discovery_cleanup(struct fluxer_discovery *discovery);

#endif /* FLUXER_DISCOVERY_H */
