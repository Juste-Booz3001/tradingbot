#include "StrategyManager.hpp"

namespace tradingbot {

void StrategyManager::addStrategy(std::unique_ptr<IStrategy> strategy, std::string description, bool enabledByDefault) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = std::make_unique<Entry>();
    entry->name = strategy->name();
    entry->description = std::move(description);
    entry->strategy = std::move(strategy);
    entry->enabled = enabledByDefault;
    entries_.push_back(std::move(entry));
}

std::vector<Signal> StrategyManager::onTick(const Tick& tick, const std::vector<Tick>& history) {
    std::vector<Signal> signals;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : entries_) {
        if (!entry->enabled.load()) continue;
        auto signal = entry->strategy->onTick(tick, history);
        if (signal) signals.push_back(*signal);
    }
    return signals;
}

std::vector<StrategyManager::Info> StrategyManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Info> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back({entry->name, entry->description, entry->enabled.load()});
    }
    return result;
}

bool StrategyManager::setEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : entries_) {
        if (entry->name == name) {
            entry->enabled = enabled;
            return true;
        }
    }
    return false;
}

} // namespace tradingbot
