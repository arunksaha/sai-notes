#include "switch_state.h"
#include "switch_config.h"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <string>

void sai_inform_mac_learn( uint16_t vlan, uint64_t mac, uint16_t port);

MacString macToString(MacAddress const mac) {
    MacString result;

    // Use snprintf for safe, formatted string conversion into the buffer.
    // The format specifier "%02X" ensures two hexadecimal digits and uppercase.
    // The MAC octets are extracted using bitwise shifts (MSB first).
    // The individual bytes (8 bits) are cast to unsigned int for snprintf compatibility.
    
    // snprintf returns the number of characters written (excluding the null terminator).
    // The size argument (result.size()) guarantees we never write past the buffer boundary.
    std::snprintf(
        result.data(),       // The underlying char* buffer
        result.size(),       // The size of the buffer (18)
        "%02x:%02x:%02x:%02x:%02x:%02x",
        static_cast<unsigned int>((mac >> 40) & 0xFF), // Byte 1 (most significant)
        static_cast<unsigned int>((mac >> 32) & 0xFF), // Byte 2
        static_cast<unsigned int>((mac >> 24) & 0xFF), // Byte 3
        static_cast<unsigned int>((mac >> 16) & 0xFF), // Byte 4
        static_cast<unsigned int>((mac >> 8) & 0xFF),  // Byte 5
        static_cast<unsigned int>(mac & 0xFF)          // Byte 6 (least significant)
    );

    // The snprintf function automatically null-terminates the string.
    return result;
}

// -----------------------------------------------------------------------------
// Global singleton instance
// -----------------------------------------------------------------------------
SwitchState g_switch_state;

MacAddress extract_mac(uint8_t const * const p)
{
    uint64_t m = 0;
    for (int i = 0; i < MacAddressByteLen; i++)
        m = (m << 8) | p[i];
    return m;
}
// -----------------------------------------------------------------------------
SwitchState::SwitchState() : numPorts_{NumSwitchPorts}
{
    reset();
}


// -----------------------------------------------------------------------------
void SwitchState::reset()
{
    std::unique_lock lock(mtx_);
    vlanMembers_.clear();
    fdb_.clear();
    portPvid_.clear();
}


// -----------------------------------------------------------------------------
// VLAN APIs
// -----------------------------------------------------------------------------
void SwitchState::createVlan(VlanId vlan)
{
    std::unique_lock lock(mtx_);

    // If the vlan does not exist, then create it with zero members.
    if (vlanMembers_.count(vlan) == 0) {
        vlanMembers_[vlan] = VlanMemberList{};
    }
}

void SwitchState::addVlanMember(VlanId vlan, PortId port, bool /*tagged*/)
{
    assert(vlan <= MaxVlanId);
    assert(static_cast<int>(port) < numPorts_);

    std::unique_lock lock(mtx_);

    auto it = vlanMembers_.find(vlan);
    if (it != vlanMembers_.end()) {
        it->second.push_back(port);
        portPvid_[port] = vlan;
    }
}

bool SwitchState::getVlanMembers(VlanId vlan, VlanMemberList& outMembers) const
{
    assert(vlan <= MaxVlanId);

    std::shared_lock lock(mtx_);

    auto it = vlanMembers_.find(vlan);
    if (it == vlanMembers_.end()) {
        outMembers.clear();
        return false;
    }

    outMembers = it->second;
    return true;
}


// -----------------------------------------------------------------------------
// FDB APIs
// -----------------------------------------------------------------------------
// Return (learned, moved)
std::pair<bool, bool> SwitchState::learnMac(VlanId vlan, MacAddress mac, PortId port)
{
    assert(vlan <= MaxVlanId);
    assert(static_cast<int>(port) < numPorts_);

    std::unique_lock lock(mtx_);

    FdbKey const key(vlan, mac);
    auto [it, inserted] = fdb_.emplace(key, port);
    if (inserted) {
        return {true, false};
    }

    if (it->second != port) {
        it->second = port;
        return {false, true};
    }

    sai_inform_mac_learn(
        static_cast<uint16_t>(vlan),
        static_cast<uint64_t>(mac),
        static_cast<uint16_t>(port)
    );

    return {false, false};
}

bool SwitchState::lookupFdb(VlanId vlan, MacAddress mac, PortId& outPort) const
{
    assert(vlan <= MaxVlanId);

    std::shared_lock lock(mtx_);

    FdbKey key(vlan, mac);
    auto it = fdb_.find(key);
    if (it == fdb_.end())
        return false;

    outPort = it->second;
    return true;
}

void SwitchState::dumpFdb(FdbTable& outTable) const
{
    std::shared_lock lock(mtx_);
    outTable = fdb_;
}

std::string SwitchState::tostringFdb() const
{
    std::shared_lock lock(mtx_);

    if (fdb_.empty()) {
        return {};
    }

    std::string out;
    out.reserve(fdb_.size() * 100); // Rough preallocation for speed

    char lineBuf[80];

    for (auto const& [key, port] : fdb_) {
        MacAddress const mac = key.mac();
        MacString const macstr = macToString(mac);

        int const n = std::snprintf(
            lineBuf,
            sizeof(lineBuf),
            "vlan=%u mac=%s port=%u\n",
            static_cast<unsigned>(key.vlan()),
            macstr.data(),
            static_cast<unsigned>(port));

        if (n > 0) {
            out.append(lineBuf, static_cast<size_t>(n));
        }
    }

    return out;
}

// -----------------------------------------------------------------------------
// Port PVID APIs
// -----------------------------------------------------------------------------

bool SwitchState::getPortPvid(PortId port, VlanId& outPvid) const
{
    assert(static_cast<int>(port) < numPorts_);

    std::shared_lock lock(mtx_);

    auto it = portPvid_.find(port);
    if (it == portPvid_.end())
        return false;

    outPvid = it->second;
    return true;
}
