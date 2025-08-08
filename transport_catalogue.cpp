#include "transport_catalogue.h"
#include <algorithm>

void TransportCatalogue::AddStop(const Stop& stop) {
    stops_.push_back(stop);
    const Stop* stop_ptr = &stops_.back();
    stopname_to_stop_[stop_ptr->name] = stop_ptr;
}

void TransportCatalogue::AddBus(const Bus& bus) {
    buses_.push_back(bus);
    const Bus* bus_ptr = &buses_.back();
    busname_to_bus_[bus_ptr->name] = bus_ptr;
    for (const auto& stop_name : bus_ptr->stops) {
        if (const Stop* stop = FindStop(stop_name)) {
            stop_to_buses_[stop].insert(bus_ptr->name);
        }
    }
}

void TransportCatalogue::SetDistance(const Stop* from, const Stop* to, int distance) {
    if (from && to) {
        distances_[from][to] = distance;
    }
}

const Stop* TransportCatalogue::FindStop(std::string_view name) const {
    if (auto it = stopname_to_stop_.find(name); it != stopname_to_stop_.end()) {
        return it->second;
    }
    return nullptr;
}

const Bus* TransportCatalogue::FindBus(std::string_view name) const {
    if (auto it = busname_to_bus_.find(name); it != busname_to_bus_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string_view> TransportCatalogue::GetBusesByStop(std::string_view stop_name) const {
    std::vector<std::string_view> result;
    if (const Stop* stop = FindStop(stop_name)) {
        if (auto it = stop_to_buses_.find(stop); it != stop_to_buses_.end()) {
            result.assign(it->second.begin(), it->second.end());
            std::sort(result.begin(), result.end());
        }
    }
    return result;
}

int TransportCatalogue::GetRoadDistance(const Stop* from, const Stop* to) const {
    if (!from || !to) return 0;
    if (auto it = distances_.find(from); it != distances_.end()) {
        if (auto jt = it->second.find(to); jt != it->second.end()) {
            return jt->second;
        }
    }
    return 0;
}

int TransportCatalogue::GetRoadDistanceBidirectional(const Stop* from, const Stop* to) const {
    if (!from || !to) return 0;
    
    int distance = GetRoadDistance(from, to);
    if (distance > 0) {
        return distance;
    }

    distance = GetRoadDistance(to, from);
    if (distance > 0) {
        return distance;
    }
    
    return 0;
}