#pragma once

#include "transport_catalogue.h"
#include "graph.h"
#include "router.h"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>

struct RoutingSettings {
    int bus_wait_time = 0;
    double bus_velocity = 0.0;
};

struct RouteItem {
    enum class Type {
        Wait,
        Bus
    };
    
    Type type;
    std::string stop_name;
    std::string bus;
    int span_count;
    double time;
};

struct RouteInfo {
    double total_time;
    std::vector<RouteItem> items;
};

class TransportRouter {
public:
    explicit TransportRouter(const TransportCatalogue& catalogue);
    
    void SetRoutingSettings(const RoutingSettings& settings);
    std::optional<RouteInfo> BuildRoute(const std::string& from, const std::string& to) const;

private:
    void BuildGraph();
    void AddEdgesForBus(const Bus& bus);
    double CalculateTime(int distance) const;
    std::optional<RouteInfo> ReconstructRoute(graph::VertexId from, graph::VertexId to, 
                                            const graph::Router<double>& router) const;
    
    const TransportCatalogue& catalogue_;
    RoutingSettings settings_;
    std::unique_ptr<graph::DirectedWeightedGraph<double>> graph_;
    std::unique_ptr<graph::Router<double>> router_;
    
    // Маппинги для работы с графом
    // Для каждой остановки у нас есть две вершины:
    // - wait_vertex: вершина "ожидание на остановке"
    // - bus_vertex: вершина "посадка в автобус на остановке"
    std::unordered_map<std::string, graph::VertexId> stop_to_wait_vertex_;
    std::unordered_map<std::string, graph::VertexId> stop_to_bus_vertex_;
    std::unordered_map<graph::VertexId, std::string> vertex_to_stop_;
    
    // Информация о ребрах
    std::unordered_map<graph::EdgeId, std::string> edge_to_bus_;
    std::unordered_map<graph::EdgeId, int> edge_to_span_count_;
    
    bool graph_built_ = false;
};