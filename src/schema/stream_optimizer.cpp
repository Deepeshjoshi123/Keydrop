#include "keydrop/schema/stream_optimizer.hpp"

#include <cstring>

#include "keydrop/core/encoder.hpp"
#include "keydrop/core/packet_reader.hpp"

namespace keydrop {

namespace {

// ── Delta packet format (marker 0xFB) ────────────────────────────
// [0xFB][message_id u16 LE][seq u16 LE][bitmap_size u8][bitmap...][fields]
// Fields appear in schema order only for bits set in the bitmap:
//   u8/i8            → 1 raw byte
//   u16/i16/u32/i32  → 1 tag byte + payload (tag 0=i8 delta, 1=i16 delta,
//                      2=i32 delta, 3=raw full-width value)
//   f32              → 4 raw bytes; f64 → 8 raw bytes (bitmap suppression
//                      only — float deltas never repay their tag)
//   string/bytes     → [u16 length][bytes] (full value, no dictionary
//                      references inside delta packets)

void write_u16_le(Buffer& out, u16 value)
{
    out.write(static_cast<byte>(value & 0xFF));
    out.write(static_cast<byte>((value >> 8) & 0xFF));
}

u16 read_u16_le(const std::vector<byte>& data, usize offset)
{
    return static_cast<u16>(data[offset])
        | static_cast<u16>(static_cast<u16>(data[offset + 1]) << 8);
}

bool is_bit_set(const std::vector<byte>& data, usize bitmap_offset, usize index)
{
    const usize byte_index = bitmap_offset + (index / 8);
    const usize bit_index = index % 8;
    return (data[byte_index] & static_cast<byte>(1u << bit_index)) != 0;
}

void set_bit(std::vector<byte>& bitmap, usize index)
{
    const usize byte_index = index / 8;
    const usize bit_index = index % 8;
    bitmap[byte_index] = static_cast<byte>(bitmap[byte_index] | static_cast<byte>(1u << bit_index));
}

void write_signed_le(Buffer& out, i64 value, usize size)
{
    for (usize i = 0; i < size; ++i)
    {
        out.write(static_cast<byte>((value >> (8 * i)) & 0xFF));
    }
}

bool fits_i8(i64 v) { return v >= -128 && v <= 127; }
bool fits_i16(i64 v) { return v >= -32768 && v <= 32767; }
bool fits_i32(i64 v) { return v >= -2147483647LL - 1 && v <= 2147483647LL; }

bool to_i64(const FieldValue& value, i64& out)
{
    switch (value.type)
    {
    case FieldType::u8: out = value.as_u8; return true;
    case FieldType::u16: out = value.as_u16; return true;
    case FieldType::u32: out = value.as_u32; return true;
    case FieldType::i8: out = value.as_i8; return true;
    case FieldType::i16: out = value.as_i16; return true;
    case FieldType::i32: out = value.as_i32; return true;
    default: return false;
    }
}

void write_numeric_delta(Buffer& out, const FieldValue& previous, const FieldValue& current)
{
    i64 prev_value = 0;
    i64 cur_value = 0;
    (void)to_i64(previous, prev_value);
    (void)to_i64(current, cur_value);
    const i64 delta = cur_value - prev_value;

    switch (current.type)
    {
    case FieldType::u8:
    case FieldType::i8:
        out.write(static_cast<byte>(cur_value & 0xFF));
        return;

    case FieldType::u16:
    case FieldType::i16:
        if (fits_i8(delta)) { out.write(0); write_signed_le(out, delta, 1); }
        else if (fits_i16(delta)) { out.write(1); write_signed_le(out, delta, 2); }
        else { out.write(3); write_signed_le(out, cur_value, 2); }
        return;

    case FieldType::u32:
    case FieldType::i32:
        if (fits_i8(delta)) { out.write(0); write_signed_le(out, delta, 1); }
        else if (fits_i16(delta)) { out.write(1); write_signed_le(out, delta, 2); }
        else if (fits_i32(delta)) { out.write(2); write_signed_le(out, delta, 4); }
        else { out.write(3); write_signed_le(out, cur_value, 4); }
        return;

    case FieldType::f32:
    {
        u32 bits = 0;
        std::memcpy(&bits, &current.as_f32, sizeof(bits));
        write_signed_le(out, static_cast<i64>(bits), 4);
        return;
    }

    case FieldType::f64:
    {
        u64 bits = 0;
        std::memcpy(&bits, &current.as_f64, sizeof(bits));
        for (usize i = 0; i < 8; ++i)
        {
            out.write(static_cast<byte>((bits >> (8 * i)) & 0xFF));
        }
        return;
    }

    default:
        return;
    }
}

i64 sign_extend(u64 raw, usize size)
{
    const usize bits = size * 8;
    const u64 mask = (static_cast<u64>(1) << bits) - 1;
    const u64 sign_bit = static_cast<u64>(1) << (bits - 1);
    raw &= mask;
    return (raw & sign_bit) != 0 ? static_cast<i64>(raw | ~mask) : static_cast<i64>(raw);
}

bool read_numeric_delta(PacketReader& reader, const FieldValue& previous, FieldValue& out)
{
    i64 prev_value = 0;
    (void)to_i64(previous, prev_value);

    switch (previous.type)
    {
    case FieldType::u8:
        out = FieldValue::from_u8(reader.read_u8());
        return true;

    case FieldType::i8:
        out = FieldValue::from_i8(reader.read_i8());
        return true;

    case FieldType::u16:
    case FieldType::i16:
    {
        const u8 tag = reader.read_u8();
        u64 raw = 0;
        bool absolute = false;
        switch (tag)
        {
        case 0: raw = reader.read_u8(); raw = static_cast<u64>(sign_extend(raw, 1)); break;
        case 1: raw = reader.read_u16(); raw = static_cast<u64>(sign_extend(raw, 2)); break;
        case 3: raw = reader.read_u16(); absolute = true; break;
        default: return false;
        }
        const i64 cur = absolute ? static_cast<i64>(raw) : prev_value + static_cast<i64>(raw);
        out = previous.type == FieldType::u16
            ? FieldValue::from_u16(static_cast<u16>(cur))
            : FieldValue::from_i16(static_cast<i16>(cur));
        return true;
    }

    case FieldType::u32:
    case FieldType::i32:
    {
        const u8 tag = reader.read_u8();
        u64 raw = 0;
        bool absolute = false;
        switch (tag)
        {
        case 0: raw = reader.read_u8(); raw = static_cast<u64>(sign_extend(raw, 1)); break;
        case 1: raw = reader.read_u16(); raw = static_cast<u64>(sign_extend(raw, 2)); break;
        case 2: raw = reader.read_u32(); raw = static_cast<u64>(sign_extend(raw, 4)); break;
        case 3: raw = reader.read_u32(); absolute = true; break;
        default: return false;
        }
        const i64 cur = absolute ? static_cast<i64>(raw) : prev_value + static_cast<i64>(raw);
        out = previous.type == FieldType::u32
            ? FieldValue::from_u32(static_cast<u32>(cur))
            : FieldValue::from_i32(static_cast<i32>(cur));
        return true;
    }

    case FieldType::f32:
        out = FieldValue::from_f32(reader.read_f32());
        return true;

    case FieldType::f64:
        out = FieldValue::from_f64(reader.read_f64());
        return true;

    default:
        return false;
    }
}

void write_full_field(Encoder& encoder, const FieldValue& value)
{
    switch (value.type)
    {
    case FieldType::u8: encoder.write_u8(value.as_u8); break;
    case FieldType::u16: encoder.write_u16(value.as_u16); break;
    case FieldType::u32: encoder.write_u32(value.as_u32); break;
    case FieldType::i8: encoder.write_i8(value.as_i8); break;
    case FieldType::i16: encoder.write_i16(value.as_i16); break;
    case FieldType::i32: encoder.write_i32(value.as_i32); break;
    case FieldType::f32: encoder.write_f32(value.as_f32); break;
    case FieldType::f64: encoder.write_f64(value.as_f64); break;
    case FieldType::string: encoder.write_string(value.as_string); break;
    case FieldType::bytes:
        encoder.write_u16(static_cast<u16>(value.as_bytes.size()));
        if (!value.as_bytes.empty())
        {
            encoder.write_bytes(value.as_bytes.data(), value.as_bytes.size());
        }
        break;
    }
}

void write_delta_field(Buffer& out, const FieldValue& value)
{
    switch (value.type)
    {
    case FieldType::string:
        write_u16_le(out, static_cast<u16>(value.as_string.size()));
        if (!value.as_string.empty())
        {
            out.append(reinterpret_cast<const byte*>(value.as_string.data()), value.as_string.size());
        }
        break;

    case FieldType::bytes:
        write_u16_le(out, static_cast<u16>(value.as_bytes.size()));
        if (!value.as_bytes.empty())
        {
            out.append(value.as_bytes.data(), value.as_bytes.size());
        }
        break;

    default:
        return;
    }
}

bool read_delta_field(PacketReader& reader, FieldType type, FieldValue& out)
{
    if (type == FieldType::string)
    {
        out = FieldValue::from_string(reader.read_string());
        return true;
    }

    if (type == FieldType::bytes)
    {
        const u16 size = reader.read_u16();
        out = FieldValue::from_bytes(reader.read_bytes(size));
        return true;
    }

    return false;
}

} // namespace

StreamOptimizer::StreamOptimizer()
    : config_()
{
}

void StreamOptimizer::configure(const StreamOptimizerConfig& config)
{
    config_ = config;
    if (config_.max_batch_packets == 0)
    {
        config_.max_batch_packets = 1;
    }
    if (config_.keyframe_interval == 0)
    {
        config_.keyframe_interval = 1;
    }
}

const StreamOptimizerConfig& StreamOptimizer::config() const
{
    return config_;
}

void StreamOptimizer::reset()
{
    last_payload_by_schema_.clear();
    last_packet_by_schema_.clear();
    sample_count_by_schema_.clear();
    batched_packets_.clear();
    delta_seq_by_schema_.clear();
    record_count_by_schema_.clear();
    last_decoded_by_schema_.clear();
    expected_delta_seq_by_schema_.clear();
}

bool StreamOptimizer::build_delta_packet(
    const SchemaDef& schema,
    const NamedPayload& previous,
    const NamedPayload& current,
    u16 sequence,
    Buffer& out
) const
{
    out.clear();
    out.write(kDeltaMarker);
    write_u16_le(out, schema.message_id);
    write_u16_le(out, sequence);

    const usize bitmap_size = (schema.fields.size() + 7) / 8;
    out.write(static_cast<byte>(bitmap_size));

    std::vector<byte> bitmap(bitmap_size, 0);
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        if (field_changed(previous, current, schema.fields[i].name))
        {
            set_bit(bitmap, i);
        }
    }
    out.append(bitmap.data(), bitmap.size());

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        if (!is_bit_set(bitmap, 0, i))
        {
            continue;
        }

        const FieldDef& field = schema.fields[i];
        const NamedPayload::const_iterator prev_it = previous.find(field.name);
        const NamedPayload::const_iterator cur_it = current.find(field.name);
        if (prev_it == previous.end() || cur_it == current.end())
        {
            return false;
        }

        if (field.type == FieldType::string || field.type == FieldType::bytes)
        {
            write_delta_field(out, cur_it->second);
        }
        else
        {
            write_numeric_delta(out, prev_it->second, cur_it->second);
        }
    }

    return true;
}

void StreamOptimizer::optimize_outgoing(
    const SchemaDef& schema,
    const std::string& schema_name,
    const NamedPayload& payload,
    const Buffer& encoded_packet,
    StreamOptimizationOutput& out
)
{
    out = {};
    out.packet = encoded_packet;

    const auto last_payload_it = last_payload_by_schema_.find(schema_name);
    const bool has_previous = last_payload_it != last_payload_by_schema_.end();
    const usize sample_count = sample_count_by_schema_[schema_name];
    out.aggressive_mode = sample_count >= config_.aggressive_after_samples;

    if (!config_.enabled)
    {
        last_payload_by_schema_[schema_name] = payload;
        last_packet_by_schema_[schema_name] = encoded_packet;
        sample_count_by_schema_[schema_name] = sample_count + 1;
        return;
    }

    const bool delta_mode = config_.enable_delta_packets;

    if (has_previous && config_.enable_packet_reuse && payload_equals(last_payload_it->second, payload))
    {
        // Identical payload. In delta mode an all-unchanged delta packet is
        // the smallest valid emission and keeps the sequence advancing; in
        // plain mode the previous packet is reused as before.
        if (delta_mode)
        {
            u16& seq = delta_seq_by_schema_[schema_name];
            Buffer delta;
            if (build_delta_packet(schema, last_payload_it->second, payload, seq, delta))
            {
                out.emit_now = true;
                out.used_delta_packet = true;
                out.packet = delta;
                seq = static_cast<u16>(seq + 1);
            }
        }
        else
        {
            out.emit_now = true;
            out.used_packet_reuse = true;
            out.packet = last_packet_by_schema_[schema_name];
        }
    }
    else if (delta_mode && (!has_previous || record_count_by_schema_[schema_name] >= config_.keyframe_interval))
    {
        // Initial record or periodic keyframe: full packet resynchronizes
        // the receiver and resets the delta sequence.
        out.emit_now = true;
        out.keyframe = true;
        delta_seq_by_schema_[schema_name] = 0;
        record_count_by_schema_[schema_name] = 0;
    }
    else if (delta_mode && has_previous && config_.enable_delta_updates && out.aggressive_mode)
    {
        const f32 changed = change_ratio(last_payload_it->second, payload);
        if (changed <= config_.low_change_ratio_threshold)
        {
            u16& seq = delta_seq_by_schema_[schema_name];
            Buffer delta;
            const bool built = build_delta_packet(schema, last_payload_it->second, payload, seq, delta);
            if (built && delta.size() < encoded_packet.size())
            {
                out.emit_now = true;
                out.used_delta_packet = true;
                out.packet = delta;
                seq = static_cast<u16>(seq + 1);
            }
            // Otherwise fall through to the full packet below (bitmap
            // overhead not repaid — the optimization disables itself).
        }
    }
    else if (!delta_mode && has_previous && config_.enable_delta_updates && config_.enable_batching && out.aggressive_mode)
    {
        const f32 changed = change_ratio(last_payload_it->second, payload);
        if (changed <= config_.low_change_ratio_threshold)
        {
            batched_packets_.push_back(encoded_packet);
            out.emit_now = false;
            out.queued_for_batch = true;

            if (batched_packets_.size() >= config_.max_batch_packets)
            {
                out.packet = build_batch_packet(batched_packets_);
                out.emit_now = true;
                out.queued_for_batch = false;
                batched_packets_.clear();
            }
        }
    }

    if (delta_mode && !out.used_delta_packet && !out.keyframe && out.emit_now)
    {
        // A full packet was emitted (delta not attempted or not repaid).
        // It resynchronizes the receiver, so the sender's sequence must
        // restart too — otherwise every following delta is rejected.
        delta_seq_by_schema_[schema_name] = 0;
    }

    last_payload_by_schema_[schema_name] = payload;
    last_packet_by_schema_[schema_name] = out.used_delta_packet ? out.packet : encoded_packet;
    sample_count_by_schema_[schema_name] = sample_count + 1;
    if (delta_mode)
    {
        record_count_by_schema_[schema_name] += 1;
    }
}

bool StreamOptimizer::flush_batched(Buffer& out_packet)
{
    if (batched_packets_.empty())
    {
        return false;
    }

    out_packet = build_batch_packet(batched_packets_);
    batched_packets_.clear();
    return true;
}

bool StreamOptimizer::expand_incoming(const Buffer& packet, std::deque<Buffer>& out_packets) const
{
    out_packets.clear();
    if (packet.size() < 1)
    {
        return false;
    }

    const std::vector<byte>& data = packet.data();
    if (data[0] != kBatchMarker)
    {
        out_packets.push_back(packet);
        return true;
    }

    if (packet.size() < 2)
    {
        return false;
    }

    const usize count = data[1];
    usize cursor = 2;
    for (usize i = 0; i < count; ++i)
    {
        if (cursor + 2 > data.size())
        {
            return false;
        }

        const u16 len = static_cast<u16>(data[cursor]) | (static_cast<u16>(data[cursor + 1]) << 8);
        cursor += 2;
        if (cursor + len > data.size())
        {
            return false;
        }

        Buffer part;
        part.append(&data[cursor], len);
        out_packets.push_back(part);
        cursor += len;
    }

    return cursor == data.size();
}

bool StreamOptimizer::expand_delta(
    const SchemaDef& schema,
    const Buffer& packet,
    Buffer& out_full_packet,
    NamedPayload& out_merged_payload
) const
{
    const std::vector<byte>& data = packet.data();
    if (packet.size() < 6 || data[0] != kDeltaMarker)
    {
        return false;
    }

    const u16 message_id = read_u16_le(data, 1);
    if (message_id != schema.message_id)
    {
        return false;
    }

    const u16 sequence = read_u16_le(data, 3);
    const usize bitmap_size = data[5];
    if (packet.size() < 6 + bitmap_size)
    {
        return false;
    }

    const auto last_it = last_decoded_by_schema_.find(schema.schema_name);
    if (last_it == last_decoded_by_schema_.end())
    {
        return false; // no keyframe state — caller must wait for a full packet
    }

    const auto expected_it = expected_delta_seq_by_schema_.find(schema.schema_name);
    const u16 expected_sequence = expected_it == expected_delta_seq_by_schema_.end()
        ? 0
        : expected_it->second;
    if (sequence != expected_sequence)
    {
        return false; // dropped packet or reordering — reject, never misdecode
    }

    const NamedPayload& previous = last_it->second;
    NamedPayload merged = previous;

    PacketReader reader(packet);
    reader.skip(6);
    reader.skip(bitmap_size);

    Encoder encoder;
    encoder.write_u16(schema.message_id);

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];
        const bool changed = is_bit_set(data, 6, i);

        if (!changed)
        {
            const NamedPayload::const_iterator prev_it = previous.find(field.name);
            if (prev_it == previous.end())
            {
                return false;
            }
            write_full_field(encoder, prev_it->second);
            continue;
        }

        const NamedPayload::const_iterator prev_it = previous.find(field.name);
        if (prev_it == previous.end())
        {
            return false;
        }

        FieldValue current;
        if (field.type == FieldType::string || field.type == FieldType::bytes)
        {
            if (!read_delta_field(reader, field.type, current))
            {
                return false;
            }
        }
        else if (!read_numeric_delta(reader, prev_it->second, current))
        {
            return false;
        }

        merged[field.name] = current;
        write_full_field(encoder, current);
    }

    if (!reader.empty())
    {
        return false; // trailing bytes — reject
    }

    out_merged_payload = std::move(merged);
    out_full_packet = encoder.take_buffer();
    return true;
}

void StreamOptimizer::record_decoded(const std::string& schema_name, const NamedPayload& payload)
{
    last_decoded_by_schema_[schema_name] = payload;
    expected_delta_seq_by_schema_[schema_name] = 0; // full packet resynchronizes
}

void StreamOptimizer::record_decoded_delta(const std::string& schema_name, const NamedPayload& payload)
{
    last_decoded_by_schema_[schema_name] = payload;
    expected_delta_seq_by_schema_[schema_name] =
        static_cast<u16>(expected_delta_seq_by_schema_[schema_name] + 1);
}

bool StreamOptimizer::payload_equals(const NamedPayload& a, const NamedPayload& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    for (NamedPayload::const_iterator it = a.begin(); it != a.end(); ++it)
    {
        const auto other = b.find(it->first);
        if (other == b.end() || other->second.type != it->second.type)
        {
            return false;
        }

        const FieldValue& left = it->second;
        const FieldValue& right = other->second;
        switch (left.type)
        {
        case FieldType::u8: if (left.as_u8 != right.as_u8) return false; break;
        case FieldType::u16: if (left.as_u16 != right.as_u16) return false; break;
        case FieldType::u32: if (left.as_u32 != right.as_u32) return false; break;
        case FieldType::i8: if (left.as_i8 != right.as_i8) return false; break;
        case FieldType::i16: if (left.as_i16 != right.as_i16) return false; break;
        case FieldType::i32: if (left.as_i32 != right.as_i32) return false; break;
        case FieldType::f32: if (left.as_f32 != right.as_f32) return false; break;
        case FieldType::f64: if (left.as_f64 != right.as_f64) return false; break;
        case FieldType::string: if (left.as_string != right.as_string) return false; break;
        case FieldType::bytes: if (left.as_bytes != right.as_bytes) return false; break;
        }
    }
    return true;
}

bool StreamOptimizer::field_changed(
    const NamedPayload& previous,
    const NamedPayload& current,
    const std::string& name
)
{
    const NamedPayload::const_iterator prev_it = previous.find(name);
    const NamedPayload::const_iterator cur_it = current.find(name);
    if (prev_it == previous.end() || cur_it == current.end())
    {
        return true; // absent before or now — treat as changed
    }

    NamedPayload one_left;
    one_left[name] = prev_it->second;
    NamedPayload one_right;
    one_right[name] = cur_it->second;
    return !payload_equals(one_left, one_right);
}

f32 StreamOptimizer::change_ratio(const NamedPayload& previous, const NamedPayload& current)
{
    if (current.empty())
    {
        return 0.0f;
    }

    usize changed = 0;
    usize total = 0;
    for (NamedPayload::const_iterator it = current.begin(); it != current.end(); ++it)
    {
        total += 1;
        if (field_changed(previous, current, it->first))
        {
            changed += 1;
        }
    }

    return static_cast<f32>(changed) / static_cast<f32>(total);
}

Buffer StreamOptimizer::build_batch_packet(const std::deque<Buffer>& packets)
{
    Buffer out;
    out.write(kBatchMarker);
    out.write(static_cast<byte>(packets.size()));
    for (usize i = 0; i < packets.size(); ++i)
    {
        const u16 len = static_cast<u16>(packets[i].size());
        out.write(static_cast<byte>(len & 0xFF));
        out.write(static_cast<byte>((len >> 8) & 0xFF));
        out.append(packets[i]);
    }
    return out;
}

}
