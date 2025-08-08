#include "json_reader.h"
#include "geo.h"
#include <iostream>
#include <unordered_set>
#include <sstream>
#include "json_builder.h"

JsonReader::JsonReader(TransportCatalogue& db) : db_(db) {}

void JsonReader::LoadData(const json::Document& doc) {
    const auto& root = doc.GetRoot().AsMap();
    const auto& base_requests = root.at("base_requests").AsArray();
    
    LoadStops(base_requests);
    LoadDistances(base_requests);
    LoadBuses(base_requests);
    
    if (root.count("routing_settings")) {
        RoutingSettings routing_settings = ParseRoutingSettings(root.at("routing_settings").AsMap());
        router_ = std::make_unique<TransportRouter>(db_);
        router_->SetRoutingSettings(routing_settings);
    }
}

void JsonReader::LoadStops(const json::Array& base_requests) {
    for (const auto& node : base_requests) {
        const auto& dict = node.AsMap();
        if (dict.at("type").AsString() == "Stop") {
            Stop stop;
            stop.name = dict.at("name").AsString();
            stop.lat = dict.at("latitude").AsDouble();
            stop.lng = dict.at("longitude").AsDouble();
            db_.AddStop(stop);
        }
    }
}

void JsonReader::LoadDistances(const json::Array& base_requests) {
    for (const auto& node : base_requests) {
        const auto& dict = node.AsMap();
        if (dict.at("type").AsString() == "Stop" && dict.count("road_distances")) {
            const std::string& from = dict.at("name").AsString();
            const Stop* from_stop = db_.FindStop(from);
            const auto& distances = dict.at("road_distances").AsMap();
            for (const auto& [to, dist] : distances) {
                const Stop* to_stop = db_.FindStop(to);
                if (from_stop && to_stop) {
                    db_.SetDistance(from_stop, to_stop, dist.AsInt());
                }
            }
        }
    }
}

void JsonReader::LoadBuses(const json::Array& base_requests) {
    for (const auto& node : base_requests) {
        const auto& dict = node.AsMap();
        if (dict.at("type").AsString() == "Bus") {
            Bus bus;
            bus.name = dict.at("name").AsString();
            for (const auto& stop_node : dict.at("stops").AsArray()) {
                bus.stops.push_back(stop_node.AsString());
            }
            bus.is_roundtrip = dict.at("is_roundtrip").AsBool();
            db_.AddBus(bus);
        }
    }
}

json::Array JsonReader::ProcessRequests(const json::Document& doc, const renderer::MapRenderer& map_renderer) {
    json::Array responses;
    const auto& stat_requests = doc.GetRoot().AsMap().at("stat_requests").AsArray();
    for (const auto& request : stat_requests) {
        const auto& dict = request.AsMap();
        const std::string& type = dict.at("type").AsString();
        
        if (type == "Stop") {
            responses.push_back(ProcessStopRequest(dict));
        } else if (type == "Bus") {
            responses.push_back(ProcessBusRequest(dict));
        } else if (type == "Map") {
            responses.push_back(ProcessMapRequest(dict, map_renderer));
        } else if (type == "Route") {
            responses.push_back(ProcessRouteRequest(dict));
        }
    }
    return responses;
}

json::Node JsonReader::ProcessStopRequest(const json::Dict& request) {
    int id = request.at("id").AsInt();
    const std::string& stop_name = request.at("name").AsString();
    const Stop* stop = db_.FindStop(stop_name);
    
    if (!stop) {
        return json::Builder{}
            .StartDict()
                .Key("request_id").Value(id)
                .Key("error_message").Value("not found")
            .EndDict()
            .Build();
    } else {
        auto buses_view = db_.GetBusesByStop(stop_name);
        auto builder = json::Builder{};
        builder.StartDict()
            .Key("buses").StartArray();
        for (const auto& bus : buses_view) {
            builder.Value(std::string(bus));
        }
        return builder.EndArray()
            .Key("request_id").Value(id)
            .EndDict()
            .Build();
    }
}

json::Node JsonReader::ProcessBusRequest(const json::Dict& request) {
    int id = request.at("id").AsInt();
    const std::string& bus_name = request.at("name").AsString();
    const Bus* bus = db_.FindBus(bus_name);
    
    if (!bus) {
        return json::Builder{}
            .StartDict()
                .Key("request_id").Value(id)
                .Key("error_message").Value("not found")
            .EndDict()
            .Build();
    } else {
        if (bus->stops.empty()) {
            return json::Builder{}
                .StartDict()
                    .Key("curvature").Value(0.0)
                    .Key("request_id").Value(id)
                    .Key("route_length").Value(0)
                    .Key("stop_count").Value(0)
                    .Key("unique_stop_count").Value(0)
                .EndDict()
                .Build();
        }
        std::unordered_set<std::string_view> unique_stops(bus->stops.begin(), bus->stops.end());
        int unique_stop_count = static_cast<int>(unique_stops.size());
        int stop_count = bus->is_roundtrip ? static_cast<int>(bus->stops.size()) : static_cast<int>(bus->stops.size()) * 2 - 1;
        int route_length = 0;
        double geo_length = 0.0;
        for (size_t i = 1; i < bus->stops.size(); ++i) {
            const Stop* prev = db_.FindStop(bus->stops[i-1]);
            const Stop* curr = db_.FindStop(bus->stops[i]);
            if (prev && curr) {
                int road_dist = db_.GetRoadDistanceBidirectional(prev, curr);
                double geo_dist = geo::ComputeDistance({prev->lat, prev->lng}, {curr->lat, curr->lng});
                route_length += road_dist;
                geo_length += geo_dist;
            }
        }
        if (!bus->is_roundtrip) {
            for (size_t i = bus->stops.size() - 1; i > 0; --i) {
                const Stop* prev = db_.FindStop(bus->stops[i]);
                const Stop* curr = db_.FindStop(bus->stops[i-1]);
                if (prev && curr) {
                    int road_dist = db_.GetRoadDistanceBidirectional(prev, curr);
                    double geo_dist = geo::ComputeDistance({prev->lat, prev->lng}, {curr->lat, curr->lng});
                    route_length += road_dist;
                    geo_length += geo_dist;
                }
            }
        }
        double curvature = geo_length > 0 ? static_cast<double>(route_length) / geo_length : 0.0;
        return json::Builder{}
            .StartDict()
                .Key("curvature").Value(curvature)
                .Key("request_id").Value(id)
                .Key("route_length").Value(route_length)
                .Key("stop_count").Value(stop_count)
                .Key("unique_stop_count").Value(unique_stop_count)
            .EndDict()
            .Build();
    }
}

json::Node JsonReader::ProcessMapRequest(const json::Dict& request, const renderer::MapRenderer& map_renderer) {
    int id = request.at("id").AsInt();
    std::ostringstream svg_stream;
    svg::Document svg_doc = map_renderer.RenderMap(db_);
    svg_doc.Render(svg_stream);
    return json::Builder{}
        .StartDict()
            .Key("map").Value(svg_stream.str())
            .Key("request_id").Value(id)
        .EndDict()
        .Build();
}

RenderSettings JsonReader::ParseRenderSettings(const json::Dict& settings) {
    RenderSettings result;
    
    result.width = settings.at("width").AsDouble();
    result.height = settings.at("height").AsDouble();
    result.padding = settings.at("padding").AsDouble();
    result.line_width = settings.at("line_width").AsDouble();
    result.stop_radius = settings.at("stop_radius").AsDouble();
    result.bus_label_font_size = settings.at("bus_label_font_size").AsInt();
    result.stop_label_font_size = settings.at("stop_label_font_size").AsInt();
    result.underlayer_width = settings.at("underlayer_width").AsDouble();
    
    const auto& bus_offset = settings.at("bus_label_offset").AsArray();
    result.bus_label_offset = svg::Point(bus_offset[0].AsDouble(), bus_offset[1].AsDouble());
    
    const auto& stop_offset = settings.at("stop_label_offset").AsArray();
    result.stop_label_offset = svg::Point(stop_offset[0].AsDouble(), stop_offset[1].AsDouble());
    
    const auto& underlayer_color = settings.at("underlayer_color");
    if (underlayer_color.IsArray()) {
        const auto& color_array = underlayer_color.AsArray();
        if (color_array.size() == 3) {
            result.underlayer_color = svg::Rgb(
                color_array[0].AsInt(),
                color_array[1].AsInt(),
                color_array[2].AsInt()
            );
        } else if (color_array.size() == 4) {
            result.underlayer_color = svg::Rgba(
                color_array[0].AsInt(),
                color_array[1].AsInt(),
                color_array[2].AsInt(),
                color_array[3].AsDouble()
            );
        }
    } else if (underlayer_color.IsString()) {
        result.underlayer_color = underlayer_color.AsString();
    }
    
    const auto& palette = settings.at("color_palette").AsArray();
    for (const auto& color_node : palette) {
        if (color_node.IsString()) {
            result.color_palette.push_back(color_node.AsString());
        } else if (color_node.IsArray()) {
            const auto& color_array = color_node.AsArray();
            if (color_array.size() == 3) {
                result.color_palette.push_back(svg::Rgb(
                    color_array[0].AsInt(),
                    color_array[1].AsInt(),
                    color_array[2].AsInt()
                ));
            } else if (color_array.size() == 4) {
                result.color_palette.push_back(svg::Rgba(
                    color_array[0].AsInt(),
                    color_array[1].AsInt(),
                    color_array[2].AsInt(),
                    color_array[3].AsDouble()
                ));
            }
        }
    }
    
    return result;
}

RoutingSettings JsonReader::ParseRoutingSettings(const json::Dict& settings) {
    RoutingSettings result;
    result.bus_wait_time = settings.at("bus_wait_time").AsInt();
    result.bus_velocity = settings.at("bus_velocity").AsDouble();
    return result;
}

json::Node JsonReader::ProcessRouteRequest(const json::Dict& request) {
    int id = request.at("id").AsInt();
    const std::string& from = request.at("from").AsString();
    const std::string& to = request.at("to").AsString();
    
    if (!router_) {
        return json::Builder{}
            .StartDict()
                .Key("request_id").Value(id)
                .Key("error_message").Value("not found")
            .EndDict()
            .Build();
    }
    
    auto route = router_->BuildRoute(from, to);
    if (!route) {
        return json::Builder{}
            .StartDict()
                .Key("request_id").Value(id)
                .Key("error_message").Value("not found")
            .EndDict()
            .Build();
    }
    
    auto builder = json::Builder{};
    builder.StartDict()
        .Key("request_id").Value(id)
        .Key("total_time").Value(route->total_time)
        .Key("items").StartArray();
    
    for (const auto& item : route->items) {
        builder.StartDict();
        if (item.type == RouteItem::Type::Wait) {
            builder.Key("type").Value("Wait")
                  .Key("stop_name").Value(item.stop_name)
                  .Key("time").Value(item.time);
        } else if (item.type == RouteItem::Type::Bus) {
            builder.Key("type").Value("Bus")
                  .Key("bus").Value(item.bus)
                  .Key("span_count").Value(item.span_count)
                  .Key("time").Value(item.time);
        }
        builder.EndDict();
    }
    
    return builder.EndArray().EndDict().Build();
}