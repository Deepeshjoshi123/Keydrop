#include "keydrop/schema/packet_layout.hpp"

namespace keydrop {

PacketLayout build_packet_layout(const SchemaDef& schema)
{
    PacketLayout layout;
    layout.message_id = schema.message_id;
    layout.fields.reserve(schema.fields.size());

    usize cursor = 2;
    bool dynamic_offset = false;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];

        FieldLayout field_layout;
        field_layout.type = field.type;
        field_layout.schema_index = i;
        field_layout.byte_offset = cursor;
        field_layout.dynamic_offset = dynamic_offset;
        field_layout.has_max_length = field.constraints.has_max_length;
        field_layout.max_length = field.constraints.max_length;

        usize fixed_size = 0;
        if (try_field_type_fixed_size(field.type, fixed_size))
        {
            field_layout.fixed_size = fixed_size;
            layout.fixed_payload_bytes += fixed_size;
            layout.minimum_packet_size += fixed_size;
            cursor += fixed_size;
        }
        else
        {
            field_layout.variable_length = true;
            layout.variable_field_count += 1;
            layout.minimum_packet_size += 2;
            layout.fixed_size_only = false;
            dynamic_offset = true;
            cursor += 2;
        }

        layout.fields.push_back(field_layout);
    }

    if (layout.fixed_size_only)
    {
        layout.fixed_packet_size = layout.minimum_packet_size;
    }

    return layout;
}

}
