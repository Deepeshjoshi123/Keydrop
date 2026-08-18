#pragma once

#include <string>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/packet_reader.hpp"
#include "keydrop/core/types.hpp"
#include "keydrop/schema/adaptive_dictionary.hpp"
#include "keydrop/schema/field_mapper.hpp"
#include "keydrop/schema/packet_layout.hpp"
#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

enum class FastCodecCode {
    ok,
    field_count_mismatch,
    field_type_mismatch,
    packet_too_small,
    message_id_mismatch,
    unsupported_envelope,
    decode_truncated,
    dictionary_reference_miss,
    insufficient_storage
};

struct FastCodecResult {
    FastCodecCode code = FastCodecCode::ok;
    std::string message;

    bool ok() const
    {
        return code == FastCodecCode::ok;
    }
};

// One decoded field from the stateless fast path. Integer fields are
// sign-extended into as_integer; float fields are promoted to as_float.
// string/bytes fields are returned as a borrowed BufferView pointing into
// the packet that was decoded (zero-copy): the caller must keep the packet
// alive while using `view`. When a dictionary reference had to be resolved
// (there is nothing to borrow from the packet), the value is materialized
// into `owned` and `owned_string` is set. `view` and `owned_string` are
// only meaningful for string/bytes fields; for numeric fields only `type`,
// `as_integer`, and `as_float` are written.
struct FastDecodedField {
    FieldType type = FieldType::u8;
    u64 as_integer = 0;
    f64 as_float = 0.0;
    BufferView view;
    std::string owned;
    bool owned_string = false;
};

// Schema-specialized stateless codec, precomputed when the schema is
// registered. The hot encode/decode loops are direct per-field writes/reads:
// no map lookups, no temporary FieldValue copies, no generic validation
// walk. The general SchemaRuntime path remains available for dynamic or
// untrusted inputs.
//
// encode() writes into the caller's Buffer and only clears it first, so the
// caller's reserved capacity is reused across calls (encode_into semantics).
// encode() always produces the plain stateless packet format (message_id +
// schema-ordered fields); it never applies RuntimeOptimizer zero-value
// omission or stream batching. decode() accepts both the plain format and
// RuntimeOptimizer-optimized packets (0xFD marker), but rejects stream
// batch envelopes (0xFC): use SchemaRuntime::receive_stream() for those.
class FastCodec {
public:
    explicit FastCodec(const SchemaDef& schema);

    const SchemaDef& schema() const;
    const PacketLayout& layout() const;
    usize field_count() const;

    FastCodecResult encode(
        const FieldValue* values,
        usize count,
        Buffer& out
    ) const;

    FastCodecResult decode(
        const Buffer& packet,
        FastDecodedField* fields,
        usize max_fields,
        usize& out_count,
        AdaptiveDictionary& dictionary
    ) const;

private:
    using EncodeFn = void (*)(Buffer&, const FieldValue&);
    using DecodeFn = bool (*)(PacketReader&, FastDecodedField&, AdaptiveDictionary&);

    FastCodecResult decode_plain(
        const Buffer& packet,
        FastDecodedField* fields,
        usize& out_count,
        AdaptiveDictionary& dictionary
    ) const;

    FastCodecResult decode_optimized(
        const Buffer& packet,
        FastDecodedField* fields,
        usize& out_count,
        AdaptiveDictionary& dictionary
    ) const;

    SchemaDef schema_;
    PacketLayout layout_;
    std::vector<EncodeFn> encode_fns_;
    std::vector<DecodeFn> decode_fns_;
};

}
