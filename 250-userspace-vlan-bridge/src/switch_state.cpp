#include "switch_state.h"
#include "switch_config.h"
#include <mutex>
#include <cassert>

// -----------------------------------------------------------------------------
// Global singleton instance
// -----------------------------------------------------------------------------
SwitchState g_switch_state;

enum {
    MaxVlanId = 4095
};

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

    if (vlanMembers_.count(vlan) == 0)
        vlanMembers_[vlan] = VlanMemberList{};
}

void SwitchState::addVlanMember(VlanId vlan, PortId port, bool /*tagged*/)
{
    assert(vlan <= MaxVlanId);
    assert(static_cast<int>(port) < numPorts_);

    std::unique_lock lock(mtx_);

    vlanMembers_[vlan].push_back(port);
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


// -----------------------------------------------------------------------------
// Port PVID APIs
// -----------------------------------------------------------------------------
void SwitchState::setPortPvid(PortId port, VlanId pvid)
{
    assert(static_cast<int>(port) < numPorts_);
    assert(pvid <= MaxVlanId);

    std::unique_lock lock(mtx_);
    portPvid_[port] = pvid;
}

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
