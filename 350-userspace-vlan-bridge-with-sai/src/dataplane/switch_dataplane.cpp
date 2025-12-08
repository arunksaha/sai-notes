#include "switch_state.h"
#include "switch_config.h"

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include <cstring>
#include <iostream>
#include <vector>

static uint16_t
extract_ethertype(const uint8_t* p) {
    return p[0] << 8 | p[1];
}

/*
 * ------------------------- TAP MANAGEMENT -----------------------------
 * openTap() attaches this userspace process to an existing TAP device.
 * This uses ioctl(TUNSETIFF) and struct ifreq.
 *
 * struct ifreq is a kernel-defined structure used for configuring
 * network interfaces. Here we fill:
 *
 *   ifr_name  = name of TAP device
 *   ifr_flags = IFF_TAP | IFF_NO_PI
 *
 * IFF_TAP  -> give us Ethernet frames
 * IFF_NO_PI -> do not prepend packet-info header
 *
 * On success, we get a file descriptor. Reading from this FD gives
 * us raw L2 frames. Writing to it injects frames into the interface.
 * 
 * Notes:
 *  - open() gets a generic TUN/TAP FD. It is not associated to any specific tap yet.
 *  - ioctl() associates the fd to tapa0, tap1 etc.
 */
void initialize_fds(int* fds, struct pollfd* pfd) {
    // ------------------------------------------------------------------
    // Open AF_PACKET sockets for veth0…vethN
    // ------------------------------------------------------------------
    for (int port = 0; port < NumSwitchPorts; port++) {

        fds[port] = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (fds[port] < 0) {
            perror("socket");
            exit(1);
        }

        std::string ifname = "veth" + std::to_string(port);

        int ifindex = if_nametoindex(ifname.c_str());
        if (ifindex == 0) {
            perror("if_nametoindex");
            exit(1);
        }

        struct sockaddr_ll sll = {};
        sll.sll_family   = AF_PACKET;
        sll.sll_protocol = htons(ETH_P_ALL);
        sll.sll_ifindex  = ifindex;

        if (bind(fds[port], reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll)) < 0) {
            perror("bind");
            exit(1);
        }

        pfd[port].fd     = fds[port];
        pfd[port].events = POLLIN;

        std::cout << "[DP] port=" << port
                  << " bound to " << ifname << "\n";
    }
}

void logPacket(
    char const * const indent,
    char const * const type,
    PortId const port,
    MacAddress const dmac,
    MacAddress const smac,
    uint16_t const ethtype) {

    bool const ignore = ethtype == ETH_P_IPV6;
    if (ignore) {
        return;
    }

    MacString const dmacStr = macToString(dmac);
    MacString const smacStr = macToString(smac);
    ::printf("%s[%s] port = %d, dmac = %s, smac = %s, ethtype = %#06x\n",
        indent, type, port, dmacStr.data(), smacStr.data(), ethtype);
}

void logLearn(
    VlanId const vlan,
    MacAddress const smac,
    PortId const port) {

    MacString const smacStr = macToString(smac);
    ::printf(" +LEARN vlan = %d, mac = %s at port = %d\n",
        vlan, smacStr.data(), port);
}

void sendPacket(
    int fd,
    uint8_t const * const pktBuffer,
    size_t const nbytes,
    PortId const port,
    MacAddress const dmac,
    MacAddress const smac,
    uint16_t const ethtype) {

    send(fd, pktBuffer, nbytes, 0);

    logPacket("  ", "Tx", port, dmac, smac, ethtype);
}

// Dataplane main loop
void run_dataplane()
{
    int fds[NumSwitchPorts];
    struct pollfd pfd[NumSwitchPorts];

    initialize_fds(fds, pfd);

    // Frame buffer, reused.
    uint8_t buf[MaxFrameByteLen];

    ssize_t const minFrameLen = 2 * MacAddressByteLen + 2;

    // ------------------------------------------------------------------
    // Dataplane Loop
    // ------------------------------------------------------------------
    for (;;) {

        int ret = poll(pfd, NumSwitchPorts, 1000);
        if (ret < 0) {
            perror("poll");
            continue;
        }
        if (ret == 0)
            continue;

        for (PortId port = 0; port < NumSwitchPorts; port++) {
            if (!(pfd[port].revents & POLLIN))
                continue;

            ssize_t const n = recv(fds[port], buf, sizeof(buf), 0);
            if (n <= 0)
                continue;

            if (n < minFrameLen)
                continue;

            MacAddress const dmac = extract_mac(buf);
            MacAddress const smac = extract_mac(buf + MacAddressByteLen);
            uint16_t const ethtype = extract_ethertype(buf + 2 * MacAddressByteLen);

            logPacket("\n", "Rx", port, dmac, smac, ethtype);

            // For this project, skip IPv6 packets for now.
            if (ethtype == ETH_P_IPV6) {
                continue;
            }

            // Determine VLAN via PVID
            VlanId vlan;
            bool const portVlanConfigured = g_switch_state.getPortPvid(port, vlan);
            // If the port has no VLAN configured, then user default VLAN.
            if (!portVlanConfigured) {
                vlan = DefaultVlanId;
            }

            // Learn source MAC
            auto const [learned, moved] = g_switch_state.learnMac(vlan, smac, port);
            bool const learned_or_moved = learned || moved;
            if (learned_or_moved) {
                logLearn(vlan, smac, port);
            }

            // Lookup destination MAC
            PortId out;
            bool const found = g_switch_state.lookupFdb(vlan, dmac, out);

            if (found && out != port) {
                // Unicast
                sendPacket(fds[out], buf, n, out, dmac, smac, ethtype);
            } else {
                // Flood inside VLAN
                VlanMemberList members;
                if (!g_switch_state.getVlanMembers(vlan, members)) {
                    // No VLAN config, flood to ALL ports except ingress port
                    for (PortId p = 0; p < NumSwitchPorts; p++) {
                        if (p != port) {
                            sendPacket(fds[p], buf, n, p, dmac, smac, ethtype);
                        }
                    }
                } else {
                    // Flood to remaining ports of this vlan
                    for (PortId p : members) {
                        if (p != port) {
                            sendPacket(fds[p], buf, n, p, dmac, smac, ethtype);
                        }
                    }
                }
            }

            if (learned_or_moved) {
                auto const fdbString = g_switch_state.tostringFdb();
                std::cout << "== Current FDB ==\n";
                std::cout << fdbString << std::endl;
            }
        }
    }
}
