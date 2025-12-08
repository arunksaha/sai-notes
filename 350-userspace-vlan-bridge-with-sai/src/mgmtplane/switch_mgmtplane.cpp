#include <condition_variable>
#include <iostream>
#include <mutex>
#include <cstdio>
#include <cassert>

#include "libsai.h"
#include "libsai_oid.h"

sai_switch_api_t* g_switch_api = nullptr;
sai_vlan_api_t*   g_vlan_api   = nullptr;
sai_object_id_t   g_switch_id  = SAI_NULL_OBJECT_ID;

constexpr std::size_t kMacStringLen = 18;
constexpr uint16_t kVlan73 = 73;

static inline void
mac_to_string(const sai_mac_t mac, char* buf)
{
    std::snprintf(buf, kMacStringLen, "%02x:%02x:%02x:%02x:%02x:%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline char const *
event_to_string(sai_fdb_event_t const event)
{
    switch (event) {
        case SAI_FDB_EVENT_LEARNED: return "LEARNED";
        case SAI_FDB_EVENT_AGED: return "AGED";
        case SAI_FDB_EVENT_MOVE: return "MOVE";
        case SAI_FDB_EVENT_FLUSHED: return "FLUSHED";
        default: return "UNKNOWN";
    }
}

static void
on_fdb_event(uint32_t count, sai_fdb_event_notification_data_t const * const data)
{
    std::cout << "[MGMT] FDB event callback, count=" << count << "\n";
    if (!count || data == nullptr) {
        return;
    }

    char mac_buf[kMacStringLen];
    for (uint32_t i = 0; i < count; ++i) {
        const auto& entry = data[i].fdb_entry;
        mac_to_string(entry.mac_address, mac_buf);

        std::cout << "  [" << i << "] event=" << event_to_string(data[i].event_type)
                  << " mac=" << mac_buf
                  << " bv_id=" << std::hex << entry.bv_id
                  << " switch=" << entry.switch_id
                  << std::dec << " attrs=" << data[i].attr_count << "\n";

        
        // Reduce console messages
        break;

        if (data[i].attr_count == 0 || data[i].attr == nullptr) {
            continue;
        }

        for (uint32_t j = 0; j < data[i].attr_count; ++j) {
            const sai_attribute_t& attr = data[i].attr[j];
            std::cout << "    attr[" << j << "] id=" << attr.id;
            switch (attr.id) {
            case SAI_FDB_ENTRY_ATTR_TYPE:
                std::cout << " type=" << attr.value.s32;
                break;
            case SAI_FDB_ENTRY_ATTR_PACKET_ACTION:
                std::cout << " packet_action=" << attr.value.s32;
                break;
            case SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID:
                std::cout << " bridge_port=0x" << std::hex << attr.value.oid << std::dec;
                break;
            case SAI_FDB_ENTRY_ATTR_USER_TRAP_ID:
                std::cout << " user_trap=0x" << std::hex << attr.value.oid << std::dec;
                break;
            case SAI_FDB_ENTRY_ATTR_META_DATA:
                std::cout << " meta=" << attr.value.u32;
                break;
            case SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE:
                std::cout << " allow_move=" << (attr.value.booldata ? "true" : "false");
                break;
            default:
                break;
            }
            std::cout << "\n";
        }
    }
}

static void
init_api_pointers()
{
    std::cout << "[MGMT] Initializing SAI...\n";

    if (sai_api_query(SAI_API_SWITCH, reinterpret_cast<void**>(&g_switch_api)) == SAI_STATUS_SUCCESS) {
        std::cout << "[MGMT] SWITCH API ready\n";
    } else {
        std::cerr << "[MGMT] Failed to query SWITCH API\n";

    }

    if (sai_api_query(SAI_API_VLAN, reinterpret_cast<void**>(&g_vlan_api)) == SAI_STATUS_SUCCESS) {
        std::cout << "[MGMT] VLAN API ready\n";
    } else {
        std::cerr << "[MGMT] Failed to query VLAN API\n";
    }
}

static void
init_switch()
{
    assert(g_switch_api);
    assert(g_switch_api->create_switch);

    sai_attribute_t attr{};
    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = reinterpret_cast<void*>(on_fdb_event);

    sai_status_t rc = g_switch_api->create_switch(&g_switch_id, 1, &attr);
    if (rc == SAI_STATUS_SUCCESS) {
        std::cout << "[MGMT] Switch created, switch_id = " << std::hex << g_switch_id << std::dec << "\n";
    } else {
        std::cerr << "[MGMT] Switch create failed, status = " << rc << "\n";
    }
}

static sai_object_id_t
create_vlan(uint16_t const vlan_id)
{
    sai_attribute_t vlan_attr{};
    vlan_attr.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan_attr.value.u16 = vlan_id;

    sai_object_id_t vlan_object_id = SAI_NULL_OBJECT_ID;
    sai_status_t vlan_rc = g_vlan_api->create_vlan(&vlan_object_id, g_switch_id, 1, &vlan_attr);
    if (vlan_rc == SAI_STATUS_SUCCESS) {
        std::cout << "[MGMT] VLAN " << vlan_id << " created, vlan_object_id = "
                  << std::hex << vlan_object_id << std::dec << "\n";
    } else {
        std::cerr << "[MGMT] VLAN " << vlan_id << " create failed, status = " << vlan_rc << "\n";
    }

    return vlan_object_id;
}

static void
create_vlan_member(
    sai_object_id_t const vlan_object_id,
    uint16_t const port_id
)
{
    sai_attribute_t attrs[3]{};

    attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attrs[0].value.oid = vlan_object_id;

    attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = libsai_encode(ResourceType::BridgePort, port_id);

    attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
    attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;

    sai_object_id_t member_oid = SAI_NULL_OBJECT_ID;
    sai_status_t rc = g_vlan_api->create_vlan_member(&member_oid, g_switch_id, 3, attrs);
    if (rc == SAI_STATUS_SUCCESS) {
        std::cout << "[MGMT] VLAN member added: port " << port_id
                  << " -> vlan " << kVlan73
                  << ", member_oid = " << std::hex << member_oid << std::dec << "\n";
    } else {
        std::cerr << "[MGMT] Failed to add port " << port_id
                  << " to vlan " << kVlan73
                  << ", status = " << rc << "\n";
    }
}

static void
init_mgmtplane()
{
    init_api_pointers();

    init_switch();

    sai_object_id_t const vlan73_object_id = create_vlan(kVlan73);

    create_vlan_member(vlan73_object_id, 0);
    create_vlan_member(vlan73_object_id, 1);
    create_vlan_member(vlan73_object_id, 3);
}

void
run_mgmtplane()
{
    init_mgmtplane();

    std::cout << "[MGMT] Initialization complete\n";

    // Block the thread here.
    std::mutex block_mutex;
    std::unique_lock<std::mutex> lock(block_mutex);
    std::condition_variable blocker;
    blocker.wait(lock, [] { return false; });

    // Should not reach here.
    assert(false);
}
