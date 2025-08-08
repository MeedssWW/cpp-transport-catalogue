#pragma once

#include "transport_catalogue.h"
#include "transport_router.h"
#include "json.h"
#include "map_renderer.h"

class JsonReader {
public:
    explicit JsonReader(TransportCatalogue& db);
    
    void LoadData(const json::Document& doc);
    json::Array ProcessRequests(const json::Document& doc, const renderer::MapRenderer& map_renderer);
    RenderSettings ParseRenderSettings(const json::Dict& settings);
    RoutingSettings ParseRoutingSettings(const json::Dict& settings);

private:
    void LoadStops(const json::Array& base_requests);
    void LoadDistances(const json::Array& base_requests);
    void LoadBuses(const json::Array& base_requests);
    
    json::Node ProcessStopRequest(const json::Dict& request);
    json::Node ProcessBusRequest(const json::Dict& request);
    json::Node ProcessMapRequest(const json::Dict& request, const renderer::MapRenderer& map_renderer);
    json::Node ProcessRouteRequest(const json::Dict& request);
    
    TransportCatalogue& db_;
    std::unique_ptr<TransportRouter> router_;
};