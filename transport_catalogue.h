#pragma once
#include "domain.h"
#include <deque>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>

class TransportCatalogue {
public:
    void AddStop(const Stop& stop);
    void AddBus(const Bus& bus);
    void SetDistance(const Stop* from, const Stop* to, int distance);

    const Stop* FindStop(std::string_view name) const;
    const Bus* FindBus(std::string_view name) const;

    std::vector<std::string_view> GetBusesByStop(std::string_view stop_name) const;

    int GetRoadDistance(const Stop* from, const Stop* to) const;
    int GetRoadDistanceBidirectional(const Stop* from, const Stop* to) const;

    const std::deque<Stop>& GetAllStops() const { return stops_; }
    const std::deque<Bus>& GetAllBuses() const { return buses_; }

private:
    std::deque<Stop> stops_;
    std::deque<Bus> buses_;
    std::unordered_map<std::string_view, const Stop*> stopname_to_stop_;
    std::unordered_map<std::string_view, const Bus*> busname_to_bus_;
    std::unordered_map<const Stop*, std::unordered_map<const Stop*, int>> distances_;
    std::unordered_map<const Stop*, std::unordered_set<std::string_view>> stop_to_buses_;
};