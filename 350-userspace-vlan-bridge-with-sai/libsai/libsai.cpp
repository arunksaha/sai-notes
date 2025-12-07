#include <iostream>
#include <vector>
#include <string>

#include "sai_necessary.h"
#include "libsai_oid.h"
#include "state/switch_state.h"

static sai_fdb_event_notification_fn g_fdb_event_cb = nullptr;

// ============================================================================
// my_create_switch()
// Implements a simple one-shot switch creation.
//
// Behavior:
//   • On the first call:
//        - Generate a random 64-bit switch_id
//        - Log it
//        - Return SAI_STATUS_SUCCESS
//
//   • On subsequent calls:
//        - Log an error
//        - Return SAI_STATUS_FAILURE
//
// This function keeps internal state (“created” flag + stored switch_id).
// ============================================================================
static sai_status_t my_create_switch(
    sai_object_id_t *switch_id,
    uint32_t attr_count,
    sai_attribute_t const* attr_list)
{
    static bool created = false;
    static sai_object_id_t allocated_switch_id = 0;

    if (attr_list) {
        for (uint32_t i = 0; i < attr_count; ++i) {
            if (attr_list[i].id == SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY) {
                g_fdb_event_cb = reinterpret_cast<sai_fdb_event_notification_fn>(attr_list[i].value.ptr);
                break;
            }
        }
    }

    if (!created) {
        // First invocation → allocate random 64-bit switch ID
        uint64_t r = (static_cast<uint64_t>(std::rand()) << 32) ^ static_cast<uint64_t>(std::rand());
        allocated_switch_id = static_cast<sai_object_id_t>(r);

        created = true;
        *switch_id = allocated_switch_id;

        // std::cout << "[libsai] my_create_switch(): first invocation, "
        //           << "allocated switch_id = "
        //           << std::hex << std::showbase << allocated_switch_id
        //           << std::dec << " fdb_event_cb=" << reinterpret_cast<const void*>(g_fdb_event_cb)
        //           << "\n";

        return SAI_STATUS_SUCCESS;
    }

    // Already created → error
    std::cerr << "[libsai] my_create_switch(): switch already created, "
              << "previous switch_id = "
                  << std::hex << std::showbase << allocated_switch_id
                  << std::dec << "\n";

    return SAI_STATUS_FAILURE;
}

// ============================================================================
// Helper: Extract VLAN ID from sai_attribute_t list
// ============================================================================
static sai_status_t extract_vlan_id(
    uint32_t attr_count,
    sai_attribute_t const* attr_list,
    uint16_t &vlan_id)
{
    for (uint32_t i = 0; i < attr_count; i++) {
        if (attr_list[i].id == SAI_VLAN_ATTR_VLAN_ID) {
            vlan_id = attr_list[i].value.u16;
            return SAI_STATUS_SUCCESS;
        }
    }
    return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
}

// ============================================================================
// VLAN CREATE implementation (SAI signature)
// ============================================================================
static sai_status_t my_create_vlan(
    sai_object_id_t *vlan_oid,
    [[maybe_unused]] sai_object_id_t switch_id,
    uint32_t attr_count,
    sai_attribute_t const *attr_list)
{
    uint16_t vlan_id = 0;
    auto rc = extract_vlan_id(attr_count, attr_list, vlan_id);
    if (rc != SAI_STATUS_SUCCESS)
        return rc;

    g_switch_state.createVlan(vlan_id);

    *vlan_oid = libsai_encode(ResourceType::Vlan, vlan_id);
    return SAI_STATUS_SUCCESS;
}

// ============================================================================
// VLAN MEMBER CREATE implementation (minimal)
// ============================================================================
static sai_status_t my_create_vlan_member(
    sai_object_id_t *member_oid,
    [[maybe_unused]] sai_object_id_t switch_id,
    uint32_t attr_count,
    sai_attribute_t const *attr_list)
{
    uint16_t vlan_id = 0;
    uint16_t port_id = 0;
    bool tagged = false;

    for (uint32_t i = 0; i < attr_count; i++) {
        switch (attr_list[i].id) {

            case SAI_VLAN_MEMBER_ATTR_VLAN_ID:
                vlan_id = static_cast<uint16_t>(attr_list[i].value.oid & 0xFFFF);
                break;

            case SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID:
                port_id = static_cast<uint16_t>(attr_list[i].value.oid & 0xFFFF);
                break;

            case SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE:
                tagged = (attr_list[i].value.s32 == SAI_VLAN_TAGGING_MODE_TAGGED);
                break;
        }
    }

    g_switch_state.addVlanMember(vlan_id, port_id, tagged);

    *member_oid = libsai_encode(ResourceType::Port, port_id);
    return SAI_STATUS_SUCCESS;
}


// ============================================================================
// Switch API Table (minimal)
// Only initialize_switch() implemented now.
// ============================================================================
static sai_switch_api_t g_my_switch_api = {
    .create_switch              = my_create_switch,
    .remove_switch              = nullptr,
    .set_switch_attribute       = nullptr,
    .get_switch_attribute       = nullptr,
    .get_switch_stats           = nullptr,
    .get_switch_stats_ext       = nullptr,
    .clear_switch_stats         = nullptr,
    .switch_mdio_read           = nullptr,
    .switch_mdio_write          = nullptr,
    .create_switch_tunnel       = nullptr,
    .remove_switch_tunnel       = nullptr,
    .set_switch_tunnel_attribute= nullptr,
    .get_switch_tunnel_attribute= nullptr,
    .switch_mdio_cl22_read      = nullptr,
    .switch_mdio_cl22_write     = nullptr
};

static sai_vlan_api_t g_my_vlan_api = {
    .create_vlan                 = my_create_vlan,
    .remove_vlan                 = nullptr,
    .set_vlan_attribute          = nullptr,
    .get_vlan_attribute          = nullptr,
    .create_vlan_member          = my_create_vlan_member,
    .remove_vlan_member          = nullptr,
    .set_vlan_member_attribute   = nullptr,
    .get_vlan_member_attribute   = nullptr,
    .create_vlan_members         = nullptr,
    .remove_vlan_members         = nullptr,
    .get_vlan_stats              = nullptr,
    .get_vlan_stats_ext          = nullptr,
    .clear_vlan_stats            = nullptr
};

// ============================================================================
// SAI API QUERY — authentic vendor-style implementation
// ============================================================================
extern "C"
sai_status_t sai_api_query(
    sai_api_t api_id,
    void **api_method_table)
{
    switch (api_id) {

    case SAI_API_SWITCH:
        *api_method_table = &g_my_switch_api;
        return SAI_STATUS_SUCCESS;

    case SAI_API_VLAN:
        *api_method_table = &g_my_vlan_api;
        return SAI_STATUS_SUCCESS;

    default:
        return SAI_STATUS_NOT_SUPPORTED;
    }
}

static inline void
copy_mac_bytes(uint8_t (&dst)[6], uint64_t mac)
{
    dst[0] = static_cast<uint8_t>(mac >> 40);
    dst[1] = static_cast<uint8_t>(mac >> 32);
    dst[2] = static_cast<uint8_t>(mac >> 24);
    dst[3] = static_cast<uint8_t>(mac >> 16);
    dst[4] = static_cast<uint8_t>(mac >> 8);
    dst[5] = static_cast<uint8_t>(mac);
}

void
sai_inform_mac_learn(
    uint16_t vlan,
    uint64_t mac,
    uint16_t port
)
{
    if (!g_fdb_event_cb) {
        return;
    }

    // // Debug message
    // std::cout << "[libsai] inform_mac_learn vlan = " << vlan
    //           << " mac = " << std::hex << mac << std::dec
    //           << " port = " << port << "\n";

    sai_attribute_t attrs[2]{};
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = libsai_encode(ResourceType::Port, port);

    sai_fdb_event_notification_data_t event{};
    event.event_type = SAI_FDB_EVENT_LEARNED;
    event.attr_count = 2;
    event.attr = attrs;
    event.fdb_entry.switch_id = SAI_NULL_OBJECT_ID;
    event.fdb_entry.bv_id = libsai_encode(ResourceType::Vlan, vlan);

    copy_mac_bytes(event.fdb_entry.mac_address, mac);

    g_fdb_event_cb(1, &event);
}
