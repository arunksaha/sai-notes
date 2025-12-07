#pragma once

#include <stdint.h>
#include <saitypes.h>

// 63.......48 | 47..................................0
//    type     |               resource_id

enum class ResourceType : uint16_t {
    Switch      = 1,
    Port        = 2,
    Vlan        = 3,
    VlanMember  = 4,
    BridgePort  = 5,
    // Extend freely
};

using ResourceId = uint64_t; // lower 48 bits

enum {
    ResourceIdBitCount = 48
};

inline sai_object_id_t libsai_encode(ResourceType type, ResourceId id)
{
    return ((static_cast<uint64_t>(type) & 0xFFFFULL) << ResourceIdBitCount) |
           (id & 0xFFFFFFFFFFFFULL);
}

inline ResourceType libsai_decode_type(sai_object_id_t oid)
{
    return static_cast<ResourceType>((oid >> ResourceIdBitCount) & 0xFFFFULL);
}

inline ResourceId libsai_decode_id(sai_object_id_t oid)
{
    return oid & 0xFFFFFFFFFFFFULL;
}
