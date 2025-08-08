#pragma once

#include "svg.h"
#include "transport_catalogue.h"
#include <string>
#include <vector>
#include <variant>
#include <deque>

struct RenderSettings {
    double width = 0.0;
    double height = 0.0;
    double padding = 0.0;

    double line_width = 0.0;
    double stop_radius = 0.0;

    int bus_label_font_size = 0;
    svg::Point bus_label_offset;

    int stop_label_font_size = 0;
    svg::Point stop_label_offset;

    svg::Color underlayer_color;
    double underlayer_width = 0.0;

    std::vector<svg::Color> color_palette;
};

namespace renderer {

class SphereProjector;

class MapRenderer {
public:
    explicit MapRenderer(const RenderSettings& settings);

    svg::Document RenderMap(const TransportCatalogue& db) const;

private:
    void DrawBusLines(svg::Document& doc, const TransportCatalogue& db, 
                     const std::vector<const Bus*>& sorted_buses,
                     const SphereProjector& projector) const;
    void DrawBusLabels(svg::Document& doc, const TransportCatalogue& db, 
                      const std::vector<const Bus*>& sorted_buses,
                      const SphereProjector& projector) const;
    std::vector<const Stop*> CollectStopsToRender(const TransportCatalogue& db, const std::deque<Bus>& buses) const;
    void DrawStopCircles(svg::Document& doc, const std::vector<const Stop*>& stops_to_render,
                        const SphereProjector& projector) const;
    void DrawStopLabels(svg::Document& doc, const std::vector<const Stop*>& stops_to_render,
                       const SphereProjector& projector) const;
    
    RenderSettings settings_;
};

}  // namespace renderer