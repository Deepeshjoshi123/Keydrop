#include "keydrop/schema/adaptive_dictionary.hpp"

namespace keydrop {

AdaptiveDictionary::AdaptiveDictionary()
    : config_()
{
}

void AdaptiveDictionary::configure(const AdaptiveDictionaryConfig& config)
{
    config_ = config;
    if (config_.max_entries == 0)
    {
        config_.max_entries = 1;
    }

    while (size() > config_.max_entries)
    {
        evict_oldest_if_needed();
    }
}

const AdaptiveDictionaryConfig& AdaptiveDictionary::config() const
{
    return config_;
}

AdaptiveDictionaryResult AdaptiveDictionary::create_or_get(const std::string& value)
{
    const auto existing = id_by_value_.find(value);
    if (existing != id_by_value_.end())
    {
        return {AdaptiveDictionaryCode::ok, existing->second, value};
    }

    if (size() >= config_.max_entries)
    {
        evict_oldest_if_needed();
    }

    if (size() >= config_.max_entries)
    {
        return {AdaptiveDictionaryCode::full, 0, value};
    }

    const u16 id = next_id_++;
    id_by_value_[value] = id;
    value_by_id_[id] = value;
    insertion_order_.push_back(id);
    return {AdaptiveDictionaryCode::ok, id, value};
}

AdaptiveDictionaryResult AdaptiveDictionary::lookup_id(const std::string& value) const
{
    const auto it = id_by_value_.find(value);
    if (it == id_by_value_.end())
    {
        return {AdaptiveDictionaryCode::miss, 0, value};
    }
    return {AdaptiveDictionaryCode::ok, it->second, value};
}

AdaptiveDictionaryResult AdaptiveDictionary::lookup_value(u16 id) const
{
    const auto it = value_by_id_.find(id);
    if (it == value_by_id_.end())
    {
        return {AdaptiveDictionaryCode::miss, id, ""};
    }
    return {AdaptiveDictionaryCode::ok, id, it->second};
}

bool AdaptiveDictionary::update(u16 id, const std::string& value)
{
    const auto it = value_by_id_.find(id);
    if (it == value_by_id_.end())
    {
        return false;
    }

    id_by_value_.erase(it->second);
    it->second = value;
    id_by_value_[value] = id;
    return true;
}

bool AdaptiveDictionary::evict(u16 id)
{
    const auto it = value_by_id_.find(id);
    if (it == value_by_id_.end())
    {
        return false;
    }

    id_by_value_.erase(it->second);
    value_by_id_.erase(it);

    for (auto order_it = insertion_order_.begin(); order_it != insertion_order_.end(); ++order_it)
    {
        if (*order_it == id)
        {
            insertion_order_.erase(order_it);
            break;
        }
    }

    return true;
}

void AdaptiveDictionary::reset()
{
    id_by_value_.clear();
    value_by_id_.clear();
    insertion_order_.clear();
    next_id_ = 1;
}

usize AdaptiveDictionary::size() const
{
    return id_by_value_.size();
}

void AdaptiveDictionary::evict_oldest_if_needed()
{
    if (insertion_order_.empty())
    {
        return;
    }

    const u16 oldest = insertion_order_.front();
    insertion_order_.pop_front();
    const auto value_it = value_by_id_.find(oldest);
    if (value_it == value_by_id_.end())
    {
        return;
    }

    id_by_value_.erase(value_it->second);
    value_by_id_.erase(value_it);
}

}
