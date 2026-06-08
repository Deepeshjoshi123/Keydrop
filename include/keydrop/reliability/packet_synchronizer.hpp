#pragma once

#include <string>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"
#include "keydrop/schema/schema_registry.hpp"

namespace keydrop {

enum class PacketSyncStatusCode {
    ok,
    no_packet_found
};

struct PacketSyncResult {
    PacketSyncStatusCode code = PacketSyncStatusCode::no_packet_found;
    std::string message;
    usize offset = 0;
    usize skipped_bytes = 0;
    u16 message_id = 0;
    std::string schema_name;
    Buffer packet;

    bool ok() const
    {
        return code == PacketSyncStatusCode::ok;
    }
};

class PacketSynchronizer {
public:
    static PacketSyncResult recover_next_packet(
        const Buffer& stream,
        const SchemaRegistry& registry
    );

    static bool recover_all_packets(
        const Buffer& stream,
        const SchemaRegistry& registry,
        std::vector<PacketSyncResult>& out_packets
    );
};

}
