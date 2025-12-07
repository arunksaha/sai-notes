#pragma once

#include "sai_necessary.h"

// Expose sai_api_query without requiring callers to declare extern "C".
// The implementation lives in libsai.cpp.
extern "C" sai_status_t sai_api_query(sai_api_t api_id, void **api_method_table);

void
sai_inform_mac_learn(
    uint16_t vlan,
    uint64_t mac,
    uint16_t port
);