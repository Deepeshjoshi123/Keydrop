#include "keydrop/schema/fast_codec.hpp"

#include <array>
#include <cstring>
#include <stdexcept>

namespace keydrop {

namespace {

constexpr byte kOptimizedMarker = 0xFD;
constexpr byte kBatchMarker = 0xFC;
constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

void write_u16_le(Buffer& out, u16 value)
{
    out.write(static_cast<byte>(value & 0xFF));
    out.write(static_cast<byte>((value >> 8) & 0xFF));
}

void write_u32_le(Buffer& out, u32 value)
{
    out.write(static_cast<byte>(value & 0xFF));
    out.write(static_cast<byte>((value >> 8) & 0xFF));
    out.write(static_cast<byte>((value >> 16) & 0xFF));
    out.write(static_cast<byte>((value >> 24) & 0xFF));
}

void write_u64_le(Buffer& out, u64 value)
{
    for (usize i = 0; i < 8; ++i)
    {
        out.write(static_cast<byte>((value >> (8 * i)) & 0xFF));
    }
}

u16 read_u16_le(const byte* data)
{
    return static_cast<u16>(data[0])
        | static_cast<u16>(static_cast<u16>(data[1]) << 8);
}

bool is_bit_set(const std::vector<byte>& bitmap, usize bitmap_offset, usize index)
{
    const usize byte_index = bitmap_offset + (index / 8);
    const usize bit_index = index % 8;
    return (bitmap[byte_index] & static_cast<byte>(1u << bit_index)) != 0;
}

// ── Encode functions: direct writes, no dictionaries, no validation ──

void encode_u8(Buffer& out, const FieldValue& v) { out.write(v.as_u8); }
void encode_u16(Buffer& out, const FieldValue& v) { write_u16_le(out, v.as_u16); }
void encode_u32(Buffer& out, const FieldValue& v) { write_u32_le(out, v.as_u32); }
void encode_i8(Buffer& out, const FieldValue& v) { out.write(static_cast<byte>(v.as_i8)); }
void encode_i16(Buffer& out, const FieldValue& v) { write_u16_le(out, static_cast<u16>(v.as_i16)); }
void encode_i32(Buffer& out, const FieldValue& v) { write_u32_le(out, static_cast<u32>(v.as_i32)); }

void encode_f32(Buffer& out, const FieldValue& v)
{
    u32 bits = 0;
    std::memcpy(&bits, &v.as_f32, sizeof(bits));
    write_u32_le(out, bits);
}

void encode_f64(Buffer& out, const FieldValue& v)
{
    u64 bits = 0;
    std::memcpy(&bits, &v.as_f64, sizeof(bits));
    write_u64_le(out, bits);
}

void encode_string(Buffer& out, const FieldValue& v)
{
    write_u16_le(out, static_cast<u16>(v.as_string.size()));
    if (!v.as_string.empty())
    {
        out.append(reinterpret_cast<const byte*>(v.as_string.data()), v.as_string.size());
    }
}

void encode_bytes(Buffer& out, const FieldValue& v)
{
    write_u16_le(out, static_cast<u16>(v.as_bytes.size()));
    if (!v.as_bytes.empty())
    {
        out.append(v.as_bytes.data(), v.as_bytes.size());
    }
}

// ── Decode functions: zero-copy views, return false on dictionary miss ──

bool decode_u8(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::u8;
    f.as_integer = reader.read_u8();
    return true;
}

bool decode_u16(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::u16;
    f.as_integer = reader.read_u16();
    return true;
}

bool decode_u32(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::u32;
    f.as_integer = reader.read_u32();
    return true;
}

bool decode_i8(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::i8;
    f.as_integer = static_cast<u64>(static_cast<i64>(reader.read_i8()));
    return true;
}

bool decode_i16(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::i16;
    f.as_integer = static_cast<u64>(static_cast<i64>(reader.read_i16()));
    return true;
}

bool decode_i32(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::i32;
    f.as_integer = static_cast<u64>(static_cast<i64>(reader.read_i32()));
    return true;
}

bool decode_f32(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::f32;
    f.as_float = reader.read_f32();
    return true;
}

bool decode_f64(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::f64;
    f.as_float = reader.read_f64();
    return true;
}

bool decode_string(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary& dictionary)
{
    f.type = FieldType::string;
    f.view = BufferView();
    f.owned_string = false;
    const u16 marker_or_size = reader.read_u16();
    if (marker_or_size == kDictionaryStringReferenceMarker)
    {
        const u16 id = reader.read_u16();
        const AdaptiveDictionaryResult looked = dictionary.lookup_value(id);
        if (!looked.ok())
        {
            return false;
        }

        f.owned = looked.value;
        f.owned_string = true;
        return true;
    }

    const usize start = reader.position();
    reader.skip(marker_or_size);
    f.view = reader.buffer().slice(start, marker_or_size);
    return true;
}

bool decode_bytes(PacketReader& reader, FastDecodedField& f, AdaptiveDictionary&)
{
    f.type = FieldType::bytes;
    f.view = BufferView();
    f.owned_string = false;
    const u16 size = reader.read_u16();
    const usize start = reader.position();
    reader.skip(size);
    f.view = reader.buffer().slice(start, size);
    return true;
}

} // namespace

// The per-field function tables are indexed by FieldCodec, mirroring the
// static tables in schema_runtime.cpp. They are fixed at codec construction
// time so the hot loop is a pointer call per field with no switch.
namespace {

using FastEncodeFn = void (*)(Buffer&, const FieldValue&);
using FastDecodeFn = bool (*)(PacketReader&, FastDecodedField&, AdaptiveDictionary&);

const std::array<FastEncodeFn, static_cast<usize>(FieldCodec::count)> kFastEncode = {{
    encode_u8,
    encode_u16,
    encode_u32,
    encode_i8,
    encode_i16,
    encode_i32,
    encode_f32,
    encode_f64,
    encode_string,
    encode_bytes,
}};

const std::array<FastDecodeFn, static_cast<usize>(FieldCodec::count)> kFastDecode = {{
    decode_u8,
    decode_u16,
    decode_u32,
    decode_i8,
    decode_i16,
    decode_i32,
    decode_f32,
    decode_f64,
    decode_string,
    decode_bytes,
}};

} // namespace

FastCodec::FastCodec(const SchemaDef& schema)
    : schema_(schema)
    , layout_(build_packet_layout(schema))
{
    encode_fns_.reserve(layout_.fields.size());
    decode_fns_.reserve(layout_.fields.size());
    for (usize i = 0; i < layout_.fields.size(); ++i)
    {
        encode_fns_.push_back(kFastEncode[static_cast<usize>(layout_.fields[i].codec)]);
        decode_fns_.push_back(kFastDecode[static_cast<usize>(layout_.fields[i].codec)]);
    }
}

const SchemaDef& FastCodec::schema() const
{
    return schema_;
}

const PacketLayout& FastCodec::layout() const
{
    return layout_;
}

usize FastCodec::field_count() const
{
    return schema_.fields.size();
}

FastCodecResult FastCodec::encode(
    const FieldValue* values,
    usize count,
    Buffer& out
) const
{
    if (values == nullptr || count != schema_.fields.size())
    {
        return {
            FastCodecCode::field_count_mismatch,
            "Fast encode expects " + std::to_string(schema_.fields.size())
                + " fields, got " + std::to_string(count) + "."
        };
    }

    for (usize i = 0; i < schema_.fields.size(); ++i)
    {
        if (values[i].type != schema_.fields[i].type)
        {
            return {
                FastCodecCode::field_type_mismatch,
                "Field '" + schema_.fields[i].name + "' expects "
                    + field_type_to_string(schema_.fields[i].type) + ", got "
                    + field_type_to_string(values[i].type) + "."
            };
        }
    }

    out.clear();
    write_u16_le(out, schema_.message_id);
    for (usize i = 0; i < encode_fns_.size(); ++i)
    {
        encode_fns_[i](out, values[i]);
    }

    // Empty message on success: the result struct must not allocate in the
    // hot path. Error paths below still carry descriptive strings.
    return {FastCodecCode::ok, ""};
}

FastCodecResult FastCodec::decode(
    const Buffer& packet,
    FastDecodedField* fields,
    usize max_fields,
    usize& out_count,
    AdaptiveDictionary& dictionary
) const
{
    out_count = 0;
    if (fields == nullptr || max_fields < schema_.fields.size())
    {
        return {
            FastCodecCode::insufficient_storage,
            "Fast decode needs " + std::to_string(schema_.fields.size())
                + " field slots, got " + std::to_string(max_fields) + "."
        };
    }

    if (packet.size() < 2)
    {
        return {FastCodecCode::packet_too_small, "Packet too small to contain message_id."};
    }

    if (packet.data()[0] == kBatchMarker)
    {
        return {
            FastCodecCode::unsupported_envelope,
            "Fast path does not decode stream batch envelopes; use receive_stream()."
        };
    }

    const u16 message_id = read_u16_le(packet.data().data());
    if (message_id != schema_.message_id)
    {
        return {
            FastCodecCode::message_id_mismatch,
            "Packet message_id " + std::to_string(message_id)
                + " does not match schema message_id " + std::to_string(schema_.message_id) + "."
        };
    }

    if (packet.size() >= 3 && packet.data()[2] == kOptimizedMarker)
    {
        return decode_optimized(packet, fields, out_count, dictionary);
    }

    return decode_plain(packet, fields, out_count, dictionary);
}

FastCodecResult FastCodec::decode_plain(
    const Buffer& packet,
    FastDecodedField* fields,
    usize& out_count,
    AdaptiveDictionary& dictionary
) const
{
    try
    {
        PacketReader reader(packet);
        (void)reader.read_u16();
        for (usize i = 0; i < decode_fns_.size(); ++i)
        {
            if (!decode_fns_[i](reader, fields[i], dictionary))
            {
                return {
                    FastCodecCode::dictionary_reference_miss,
                    "Dictionary reference not found for field '" + schema_.fields[i].name + "'."
                };
            }
        }

        if (!reader.empty())
        {
            return {FastCodecCode::decode_truncated, "Packet has trailing unread bytes."};
        }

        out_count = schema_.fields.size();
        return {FastCodecCode::ok, ""};
    }
    catch (const std::out_of_range&)
    {
        return {FastCodecCode::decode_truncated, "Packet ended before fast decode completed."};
    }
}

FastCodecResult FastCodec::decode_optimized(
    const Buffer& packet,
    FastDecodedField* fields,
    usize& out_count,
    AdaptiveDictionary& dictionary
) const
{
    try
    {
        const std::vector<byte>& bytes = packet.data();
        if (bytes.size() < 4)
        {
            return {FastCodecCode::decode_truncated, "Optimized packet is too small."};
        }

        const usize bitmap_size = bytes[3];
        const usize expected_bitmap_size = (schema_.fields.size() + 7) / 8;
        if (bitmap_size != expected_bitmap_size || bytes.size() < 4 + bitmap_size)
        {
            return {FastCodecCode::decode_truncated, "Optimized packet bitmap is invalid."};
        }

        // Bits are read directly from the packet body; no bitmap copy.
        const usize bitmap_offset = 4;

        PacketReader reader(packet);
        (void)reader.read_u16();
        reader.skip(1); // optimized marker
        reader.skip(1); // bitmap size
        reader.skip(bitmap_size);

        for (usize i = 0; i < schema_.fields.size(); ++i)
        {
            const FieldLayout& field_layout = layout_.fields[i];

            if (!field_layout.variable_length && is_bit_set(bytes, bitmap_offset, i))
            {
                fields[i].type = field_layout.type;
                fields[i].as_integer = 0;
                fields[i].as_float = 0.0;
                continue;
            }

            if (!decode_fns_[i](reader, fields[i], dictionary))
            {
                return {
                    FastCodecCode::dictionary_reference_miss,
                    "Dictionary reference not found for field '" + schema_.fields[i].name + "'."
                };
            }
        }

        if (!reader.empty())
        {
            return {FastCodecCode::decode_truncated, "Packet has trailing unread bytes."};
        }

        out_count = schema_.fields.size();
        return {FastCodecCode::ok, ""};
    }
    catch (const std::out_of_range&)
    {
        return {FastCodecCode::decode_truncated, "Packet ended before fast decode completed."};
    }
}

}
