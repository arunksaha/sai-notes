#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <utility>
#include <shared_mutex>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
constexpr uint32_t MAC_ADDRESS_BITS = 48;
constexpr uint32_t VLAN_ID_BITS     = 16;

constexpr uint64_t MAC_ADDRESS_MASK =
    (1ULL << MAC_ADDRESS_BITS) - 1;     // Mask for 48-bit MAC

constexpr uint64_t VLAN_SHIFT = MAC_ADDRESS_BITS;  // Shift VLAN above MAC

// -----------------------------------------------------------------------------
// Basic type aliases
// -----------------------------------------------------------------------------
typedef uint16_t VlanId;       // VLAN identifier
typedef uint32_t PortId;       // Logical port identifier
typedef uint64_t MacAddress;   // Packed 48-bit MAC

// -----------------------------------------------------------------------------
// Compound types
// -----------------------------------------------------------------------------

// VLAN member list
typedef std::vector<PortId> VlanMemberList;

// VLAN table: VLAN → list of ports
typedef std::map<VlanId, VlanMemberList> VlanTable;

// Port → PVID
typedef std::map<PortId, VlanId> PortPvidTable;

// -----------------------------------------------------------------------------
// FdbKey: packed (VLAN << 48) | MAC
// -----------------------------------------------------------------------------
class FdbKey {
public:
    FdbKey(VlanId vlan, MacAddress mac)        // Construct from VLAN & MAC
    {
        key_ = (static_cast<uint64_t>(vlan) << VLAN_SHIFT) |
               (mac & MAC_ADDRESS_MASK);
    }

    bool operator<(const FdbKey& other) const  // Needed for std::map
    {
        return key_ < other.key_;
    }

    VlanId vlan() const                        // Extract VLAN
    {
        return static_cast<VlanId>(key_ >> VLAN_SHIFT);
    }

    MacAddress mac() const                     // Extract MAC
    {
        return key_ & MAC_ADDRESS_MASK;
    }

private:
    uint64_t key_;                             // Packed VLAN+MAC key
};


// Entire FDB map
typedef std::map<FdbKey, PortId> FdbTable;

// -----------------------------------------------------------------------------
// SwitchState: central in-memory model for VLAN, FDB, port state
// -----------------------------------------------------------------------------
class SwitchState {
public:
    // Constructor ensures object is fully initialized
    SwitchState();

    // Return the number of ports of this switch.
    int numPorts() const;

    // Create VLAN (if not exist)
    void createVlan(VlanId vlan);

    // Add port to VLAN
    void addVlanMember(VlanId vlan, PortId port, bool tagged);

    // Get VLAN members; return true if VLAN exists
    bool getVlanMembers(VlanId vlan, VlanMemberList& outMembers) const;

    // Learn or update FDB entry
    std::pair<bool, bool> learnMac(VlanId vlan, MacAddress mac, PortId port);

    // Lookup FDB entry; return true if found
    bool lookupFdb(VlanId vlan, MacAddress mac, PortId& outPort) const;

    // Dump FDB table
    void dumpFdb(FdbTable& outTable) const;

    // Set port PVID
    void setPortPvid(PortId port, VlanId pvid);

    // Get port PVID; return true if available
    bool getPortPvid(PortId port, VlanId& outPvid) const;

private:
    // Clear state.
    void reset();

private:
    mutable std::shared_mutex mtx_;  // Read/write lock

    int const      numPorts_;        // Number of ports
    VlanTable      vlanMembers_;     // VLAN → ports
    FdbTable       fdb_;             // (VLAN,MAC) → port
    PortPvidTable  portPvid_;        // Port → PVID
};


// Global switch-state instance shared by dataplane & mgmt-plane
extern SwitchState g_switch_state;
