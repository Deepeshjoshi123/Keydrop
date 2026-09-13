#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include "keydrop/core/types.hpp"

namespace keydrop {

struct AdaptiveDictionaryConfig {
    bool enabled = false;
    bool enable_string_values = true;
    usize max_entries = 256;
};

enum class AdaptiveDictionaryCode {
    ok,
    miss,
    full
};

struct AdaptiveDictionaryResult {
    AdaptiveDictionaryCode code = AdaptiveDictionaryCode::ok;
    u16 id = 0;
    std::string value;

    bool ok() const
    {
        return code == AdaptiveDictionaryCode::ok;
    }
};

class AdaptiveDictionary {
public:
    AdaptiveDictionary();

    void configure(const AdaptiveDictionaryConfig& config);
    const AdaptiveDictionaryConfig& config() const;

    AdaptiveDictionaryResult create_or_get(const std::string& value);
    AdaptiveDictionaryResult lookup_id(const std::string& value) const;
    AdaptiveDictionaryResult lookup_value(u16 id) const;
    bool update(u16 id, const std::string& value);
    bool evict(u16 id);
    void reset();

    usize size() const;

private:
    void evict_oldest_if_needed();

    AdaptiveDictionaryConfig config_;
    u16 next_id_ = 1;
    std::unordered_map<std::string, u16> id_by_value_;
    std::unordered_map<u16, std::string> value_by_id_;
    std::deque<u16> insertion_order_;
};

}
