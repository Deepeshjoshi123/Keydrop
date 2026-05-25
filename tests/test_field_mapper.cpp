#include <cassert>
#include <string>

#include "keydrop/core/packet_reader.hpp"
#include "keydrop/schema/field_mapper.hpp"

using namespace keydrop;

int main()
{
    const SchemaDef schema {
        "SensorData",
        1,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 16}},
        }
    };

    NamedPayload named;
    named["device_id"] = FieldValue::from_string("A1");
    named["humidity"] = FieldValue::from_u16(1000);
    named["temperature"] = FieldValue::from_u8(32);

    OrderedPayload ordered;
    FieldMapperResult map_ok = FieldMapper::map_named_to_ordered(schema, named, ordered);
    assert(map_ok.ok());
    assert(ordered.size() == 3);
    assert(ordered[0].type == FieldType::u8);
    assert(ordered[0].as_u8 == 32);
    assert(ordered[1].type == FieldType::u16);
    assert(ordered[1].as_u16 == 1000);
    assert(ordered[2].type == FieldType::string);
    assert(ordered[2].as_string == "A1");

    NamedPayload reverse_named;
    FieldMapperResult reverse_ok = FieldMapper::map_ordered_to_named(schema, ordered, reverse_named);
    assert(reverse_ok.ok());
    assert(reverse_named.size() == 3);
    assert(reverse_named["temperature"].as_u8 == 32);
    assert(reverse_named["humidity"].as_u16 == 1000);
    assert(reverse_named["device_id"].as_string == "A1");

    NamedPayload missing = named;
    missing.erase("humidity");
    assert(FieldMapper::map_named_to_ordered(schema, missing, ordered).code
           == FieldMapperCode::missing_required_field);

    NamedPayload extra = named;
    extra["unknown"] = FieldValue::from_u8(1);
    assert(FieldMapper::map_named_to_ordered(schema, extra, ordered).code
           == FieldMapperCode::unknown_extra_field);

    NamedPayload bad_type = named;
    bad_type["humidity"] = FieldValue::from_u8(10);
    assert(FieldMapper::map_named_to_ordered(schema, bad_type, ordered).code
           == FieldMapperCode::field_type_mismatch);

    OrderedPayload bad_reverse = ordered;
    bad_reverse[1] = FieldValue::from_u8(10);
    assert(FieldMapper::map_ordered_to_named(schema, bad_reverse, reverse_named).code
           == FieldMapperCode::field_type_mismatch);

    OrderedPayload bad_count = ordered;
    bad_count.pop_back();
    assert(FieldMapper::map_ordered_to_named(schema, bad_count, reverse_named).code
           == FieldMapperCode::ordered_value_count_mismatch);

    Encoder encoder;
    FieldMapperResult encode_ok = FieldMapper::encode_named_payload(schema, named, encoder);
    assert(encode_ok.ok());

    PacketReader reader(encoder.buffer());
    assert(reader.read_u8() == 32);
    assert(reader.read_u16() == 1000);
    const u16 str_len = reader.read_u16();
    assert(str_len == 2);
    assert(reader.read_u8() == static_cast<u8>('A'));
    assert(reader.read_u8() == static_cast<u8>('1'));
    assert(reader.empty());

    return 0;
}
