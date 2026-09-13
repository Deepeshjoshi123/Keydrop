#include <cassert>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/reliability/packet_synchronizer.hpp"
#include "keydrop/schema/schema_registry.hpp"

using namespace keydrop;

static void write_u16(Buffer& packet, u16 value)
{
    packet.write(static_cast<byte>(value & 0xFF));
    packet.write(static_cast<byte>((value >> 8) & 0xFF));
}

static void write_string(Buffer& packet, const char* value, u16 size)
{
    write_u16(packet, size);
    for (u16 i = 0; i < size; ++i)
    {
        packet.write(static_cast<byte>(value[i]));
    }
}

static void append_packet(Buffer& stream, const Buffer& packet)
{
    if (!packet.empty())
    {
        stream.append(packet.data().data(), packet.size());
    }
}

static Buffer make_sensor_packet(u8 temperature, const char* label, u16 label_size)
{
    Buffer packet;
    write_u16(packet, 21);
    packet.write(temperature);
    write_string(packet, label, label_size);
    return packet;
}

int main()
{
    SchemaRegistry registry;
    const SchemaDef schema {
        "SyncSensor",
        21,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"label", FieldType::string, 1, FieldConstraints {true, 8}},
        }
    };
    assert(registry.register_schema(schema).ok());

    const Buffer packet_a = make_sensor_packet(32, "alpha", 5);
    const Buffer packet_b = make_sensor_packet(33, "beta", 4);

    {
        Buffer stream;
        stream.write(0x99);
        stream.write(0x88);
        stream.write(0x77);
        append_packet(stream, packet_a);

        const PacketSyncResult recovered =
            PacketSynchronizer::recover_next_packet(stream, registry);
        assert(recovered.ok());
        assert(recovered.skipped_bytes == 3);
        assert(recovered.offset == 3);
        assert(recovered.message_id == 21);
        assert(recovered.schema_name == "SyncSensor");
        assert(recovered.packet.size() == packet_a.size());
    }

    {
        Buffer stream;
        append_packet(stream, packet_a);
        stream.write(0x21);
        stream.write(0x00);
        stream.write(0xFF);
        append_packet(stream, packet_b);

        std::vector<PacketSyncResult> recovered_packets;
        assert(PacketSynchronizer::recover_all_packets(stream, registry, recovered_packets));
        assert(recovered_packets.size() == 2);
        assert(recovered_packets[0].offset == 0);
        assert(recovered_packets[0].skipped_bytes == 0);
        assert(recovered_packets[1].offset == packet_a.size() + 3);
        assert(recovered_packets[1].skipped_bytes == 3);
    }

    {
        Buffer stream;
        stream.write(0xAA);
        stream.write(0xBB);
        stream.write(0xCC);

        const PacketSyncResult recovered =
            PacketSynchronizer::recover_next_packet(stream, registry);
        assert(!recovered.ok());
        assert(recovered.code == PacketSyncStatusCode::no_packet_found);
    }

    return 0;
}
