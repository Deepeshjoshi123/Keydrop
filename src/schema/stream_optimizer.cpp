#include "keydrop/schema/stream_optimizer.hpp"

namespace keydrop {

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
}

void StreamOptimizer::optimize_outgoing(
    const std::string& schema_name,
    const NamedPayload& payload,
    const Buffer& encoded_packet,
    StreamOptimizationOutput& out
)
{
    out = {};
    out.packet = encoded_packet;

    if (!config_.enabled)
    {
        last_payload_by_schema_[schema_name] = payload;
        last_packet_by_schema_[schema_name] = encoded_packet;
        sample_count_by_schema_[schema_name] += 1;
        return;
    }

    const auto last_payload_it = last_payload_by_schema_.find(schema_name);
    const bool has_previous = last_payload_it != last_payload_by_schema_.end();
    const usize sample_count = sample_count_by_schema_[schema_name];
    out.aggressive_mode = sample_count >= config_.aggressive_after_samples;

    if (has_previous && config_.enable_packet_reuse && payload_equals(last_payload_it->second, payload))
    {
        out.emit_now = true;
        out.used_packet_reuse = true;
        out.packet = last_packet_by_schema_[schema_name];
    }
    else if (has_previous && config_.enable_delta_updates && config_.enable_batching && out.aggressive_mode)
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

    last_payload_by_schema_[schema_name] = payload;
    last_packet_by_schema_[schema_name] = encoded_packet;
    sample_count_by_schema_[schema_name] = sample_count + 1;
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
        const auto prev = previous.find(it->first);
        if (prev == previous.end())
        {
            changed += 1;
            continue;
        }

        NamedPayload one_left;
        one_left[it->first] = it->second;
        NamedPayload one_right;
        one_right[it->first] = prev->second;
        if (!payload_equals(one_left, one_right))
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
