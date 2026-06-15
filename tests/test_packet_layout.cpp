#include <cassert>

#include "keydrop/schema/packet_layout.hpp"
#include "keydrop/schema/schema_registry.hpp"

using namespace keydrop;

int main()
{
    const SchemaDef fixed_schema {
        "FixedData",
        10,
        {
            FieldDef {"a", FieldType::u8, 0, {}},
            FieldDef {"b", FieldType::u32, 1, {}},
            FieldDef {"c", FieldType::f64, 2, {}},
        }
    };

    const PacketLayout fixed = build_packet_layout(fixed_schema);
    assert(fixed.message_id == 10);
    assert(fixed.fields.size() == 3);
    assert(fixed.fixed_size_only);
    assert(fixed.variable_field_count == 0);
    assert(fixed.fixed_payload_bytes == 13);
    assert(fixed.minimum_packet_size == 15);
    assert(fixed.fixed_packet_size == 15);
    assert(fixed.fields[0].byte_offset == 2);
    assert(fixed.fields[1].byte_offset == 3);
    assert(fixed.fields[2].byte_offset == 7);
    assert(!fixed.fields[2].dynamic_offset);

    const SchemaDef mixed_schema {
        "MixedData",
        11,
        {
            FieldDef {"temperature", FieldType::u16, 0, {}},
            FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
            FieldDef {"status", FieldType::u8, 2, {}},
            FieldDef {"raw", FieldType::bytes, 3, FieldConstraints {true, 8}},
        }
    };

    const PacketLayout mixed = build_packet_layout(mixed_schema);
    assert(!mixed.fixed_size_only);
    assert(mixed.fixed_packet_size == 0);
    assert(mixed.variable_field_count == 2);
    assert(mixed.fixed_payload_bytes == 3);
    assert(mixed.minimum_packet_size == 9);
    assert(mixed.fields[0].byte_offset == 2);
    assert(!mixed.fields[0].dynamic_offset);
    assert(mixed.fields[1].variable_length);
    assert(mixed.fields[1].has_max_length);
    assert(mixed.fields[1].max_length == 32);
    assert(mixed.fields[2].dynamic_offset);
    assert(mixed.fields[3].dynamic_offset);

    SchemaRegistry registry;
    assert(registry.register_schema(mixed_schema).ok());
    const PacketLayout* by_name = registry.find_layout_by_name("MixedData");
    const PacketLayout* by_id = registry.find_layout_by_message_id(11);
    assert(by_name != nullptr);
    assert(by_id != nullptr);
    assert(by_name == by_id);
    assert(by_name->fields.size() == mixed_schema.fields.size());

    registry.clear();
    assert(registry.find_layout_by_name("MixedData") == nullptr);
    assert(registry.find_layout_by_message_id(11) == nullptr);

    return 0;
}
