#include "transport_router.h"
#include <algorithm>
#include <stdexcept>

TransportRouter::TransportRouter(const TransportCatalogue& catalogue) 
    : catalogue_(catalogue) {
}

void TransportRouter::SetRoutingSettings(const RoutingSettings& settings) {
    settings_ = settings;
    graph_built_ = false;
}

void TransportRouter::BuildGraph() {
    if (graph_built_) return;
    
    const auto& stops = catalogue_.GetAllStops();
    size_t vertex_count = stops.size() * 2;
    
    graph_ = std::make_unique<graph::DirectedWeightedGraph<double>>(vertex_count);
    
    for (size_t i = 0; i < stops.size(); ++i) {
        graph::VertexId wait_vertex = static_cast<graph::VertexId>(i * 2);
        graph::VertexId bus_vertex = static_cast<graph::VertexId>(i * 2 + 1);
        
        stop_to_wait_vertex_[stops[i].name] = wait_vertex;
        stop_to_bus_vertex_[stops[i].name] = bus_vertex;
        vertex_to_stop_[wait_vertex] = stops[i].name;
        vertex_to_stop_[bus_vertex] = stops[i].name;
        
        graph::Edge<double> wait_edge{wait_vertex, bus_vertex, static_cast<double>(settings_.bus_wait_time)};
        graph_->AddEdge(wait_edge);
    }
    
    for (const auto& bus : catalogue_.GetAllBuses()) {
        AddEdgesForBus(bus);
    }

    router_ = std::make_unique<graph::Router<double>>(*graph_);
    
    graph_built_ = true;
}

void TransportRouter::AddEdgesForBus(const Bus& bus) {
    if (bus.stops.size() < 2) return;
    
    for (size_t i = 0; i < bus.stops.size(); ++i) {
        for (size_t j = i + 1; j < bus.stops.size(); ++j) {
            const std::string& from_stop = bus.stops[i];
            const std::string& to_stop = bus.stops[j];
            
            auto from_bus_vertex_it = stop_to_bus_vertex_.find(from_stop);
            auto to_wait_vertex_it = stop_to_wait_vertex_.find(to_stop);
            
            if (from_bus_vertex_it == stop_to_bus_vertex_.end() || 
                to_wait_vertex_it == stop_to_wait_vertex_.end()) {
                continue;
            }
            
            graph::VertexId from_vertex = from_bus_vertex_it->second;
            graph::VertexId to_vertex = to_wait_vertex_it->second;
            
            double total_time = 0.0;
            int total_distance = 0;
            
            for (size_t k = i; k < j; ++k) {
                const std::string& current_stop = bus.stops[k];
                const std::string& next_stop = bus.stops[k + 1];
                
                const Stop* current_stop_ptr = catalogue_.FindStop(current_stop);
                const Stop* next_stop_ptr = catalogue_.FindStop(next_stop);
                
                if (!current_stop_ptr || !next_stop_ptr) continue;
                
                int distance = catalogue_.GetRoadDistance(current_stop_ptr, next_stop_ptr);
                if (distance == 0) {
                    distance = catalogue_.GetRoadDistance(next_stop_ptr, current_stop_ptr);
                }
                
                if (distance == 0) continue;
                
                total_distance += distance;
            }
            
            if (total_distance == 0) continue;
            
            total_time = CalculateTime(total_distance);
            
            graph::Edge<double> edge{from_vertex, to_vertex, total_time};
            graph::EdgeId edge_id = graph_->AddEdge(edge);
            
            edge_to_bus_[edge_id] = bus.name;
            edge_to_span_count_[edge_id] = static_cast<int>(j - i);
        }
    }
    
    if (!bus.is_roundtrip) {
        for (size_t i = bus.stops.size() - 1; i > 0; --i) {
            for (size_t j = i - 1; j < bus.stops.size(); --j) {
                const std::string& from_stop = bus.stops[i];
                const std::string& to_stop = bus.stops[j];
                
                auto from_bus_vertex_it = stop_to_bus_vertex_.find(from_stop);
                auto to_wait_vertex_it = stop_to_wait_vertex_.find(to_stop);
                
                if (from_bus_vertex_it == stop_to_bus_vertex_.end() || 
                    to_wait_vertex_it == stop_to_wait_vertex_.end()) {
                    continue;
                }
                
                graph::VertexId from_vertex = from_bus_vertex_it->second;
                graph::VertexId to_vertex = to_wait_vertex_it->second;
                
                double total_time = 0.0;
                int total_distance = 0;
                
                for (size_t k = i; k > j; --k) {
                    const std::string& current_stop = bus.stops[k];
                    const std::string& next_stop = bus.stops[k - 1];
                    
                    const Stop* current_stop_ptr = catalogue_.FindStop(current_stop);
                    const Stop* next_stop_ptr = catalogue_.FindStop(next_stop);
                    
                    if (!current_stop_ptr || !next_stop_ptr) continue;
                    
                    int distance = catalogue_.GetRoadDistance(current_stop_ptr, next_stop_ptr);
                    if (distance == 0) {
                        distance = catalogue_.GetRoadDistance(next_stop_ptr, current_stop_ptr);
                    }
                    
                    if (distance == 0) continue;
                    
                    total_distance += distance;
                }
                
                if (total_distance == 0) continue;
                
                total_time = CalculateTime(total_distance);
                
                graph::Edge<double> edge{from_vertex, to_vertex, total_time};
                graph::EdgeId edge_id = graph_->AddEdge(edge);
                
                edge_to_bus_[edge_id] = bus.name;
                edge_to_span_count_[edge_id] = static_cast<int>(i - j);
            }
        }
    }
}

double TransportRouter::CalculateTime(int distance) const {
    if (settings_.bus_velocity <= 0) return 0.0;
    double velocity_m_per_min = settings_.bus_velocity * 1000.0 / 60.0;
    return distance / velocity_m_per_min;
}

std::optional<RouteInfo> TransportRouter::BuildRoute(const std::string& from, const std::string& to) const {
    const_cast<TransportRouter*>(this)->BuildGraph();
    
    auto from_wait_vertex_it = stop_to_wait_vertex_.find(from);
    auto to_wait_vertex_it = stop_to_wait_vertex_.find(to);
    
    if (from_wait_vertex_it == stop_to_wait_vertex_.end() || 
        to_wait_vertex_it == stop_to_wait_vertex_.end()) {
        return std::nullopt;
    }
    
    graph::VertexId from_vertex = from_wait_vertex_it->second;
    graph::VertexId to_vertex = to_wait_vertex_it->second;
    
    return ReconstructRoute(from_vertex, to_vertex, *router_);
}

std::optional<RouteInfo> TransportRouter::ReconstructRoute(graph::VertexId from, graph::VertexId to, 
                                                         const graph::Router<double>& router) const {
    auto route = router.BuildRoute(from, to);
    if (!route) {
        return std::nullopt;
    }
    
    RouteInfo result;
    result.total_time = route->weight;
    
    if (route->edges.empty()) {
        return result;
    }
    
    for (graph::EdgeId edge_id : route->edges) {
        const auto& edge = graph_->GetEdge(edge_id);
        
        if (edge.to == edge.from + 1 && edge.from % 2 == 0) {
            RouteItem wait_item;
            wait_item.type = RouteItem::Type::Wait;
            wait_item.stop_name = vertex_to_stop_.at(edge.from);
            wait_item.time = edge.weight;
            result.items.push_back(wait_item);
        } else {
            auto bus_it = edge_to_bus_.find(edge_id);
            auto span_count_it = edge_to_span_count_.find(edge_id);
            
            if (bus_it != edge_to_bus_.end() && span_count_it != edge_to_span_count_.end()) {
                RouteItem bus_item;
                bus_item.type = RouteItem::Type::Bus;
                bus_item.bus = bus_it->second;
                bus_item.span_count = span_count_it->second;
                bus_item.time = edge.weight;
                result.items.push_back(bus_item);
            }
        }
    }
    
    return result;
}