#include "map_renderer.h"
#include "geo.h"
#include <algorithm>
#include <set>

using namespace std::literals;
using namespace svg;
using geo::Coordinates;

namespace renderer {

class SphereProjector {
public:
    SphereProjector() = default;

    template <typename PointInputIt>
    SphereProjector(PointInputIt begin, PointInputIt end, double max_width,
                    double max_height, double padding)
        : padding_(padding) {
        if (begin == end) return;

        const auto [min_lon_it, max_lon_it] = std::minmax_element(begin, end,
            [](const auto& lhs, const auto& rhs) {
                return lhs.lng < rhs.lng;
            });

        min_lon_ = min_lon_it->lng;
        max_lon_ = max_lon_it->lng;

        const auto [min_lat_it, max_lat_it] = std::minmax_element(begin, end,
            [](const auto& lhs, const auto& rhs) {
                return lhs.lat < rhs.lat;
            });

        min_lat_ = min_lat_it->lat;
        max_lat_ = max_lat_it->lat;

        const double width_zoom = (max_lon_ - min_lon_ == 0) ? 0 :
            (max_width - 2 * padding) / (max_lon_ - min_lon_);
        const double height_zoom = (max_lat_ - min_lat_ == 0) ? 0 :
            (max_height - 2 * padding) / (max_lat_ - min_lat_);

        zoom_coeff_ = (width_zoom == 0 || height_zoom == 0)
            ? std::max(width_zoom, height_zoom)
            : std::min(width_zoom, height_zoom);
    }

    svg::Point operator()(Coordinates coords) const {
        return {
            (coords.lng - min_lon_) * zoom_coeff_ + padding_,
            (max_lat_ - coords.lat) * zoom_coeff_ + padding_
        };
    }

private:
    double padding_ = 0.0;
    double min_lon_ = 0.0, max_lon_ = 0.0;
    double min_lat_ = 0.0, max_lat_ = 0.0;
    double zoom_coeff_ = 0.0;
};

MapRenderer::MapRenderer(const RenderSettings& settings)
    : settings_(settings) {}

svg::Document MapRenderer::RenderMap(const TransportCatalogue& db) const {
    svg::Document doc;

    std::vector<Coordinates> geo_coords;
    std::set<std::string_view> seen_stops;

    for (const auto& bus : db.GetAllBuses()) {
        for (const auto& stop_name : bus.stops) {
            const Stop* stop = db.FindStop(stop_name);
            if (stop && seen_stops.insert(stop->name).second) {
                geo_coords.push_back({stop->lat, stop->lng});
            }
        }
    }

    SphereProjector projector(geo_coords.begin(), geo_coords.end(),
                            settings_.width, settings_.height, settings_.padding);

    auto buses = db.GetAllBuses();
    std::vector<const Bus*> sorted_buses;
    for (const auto& bus : buses) {
        sorted_buses.push_back(&bus);
    }
    std::sort(sorted_buses.begin(), sorted_buses.end(),
        [](const Bus* lhs, const Bus* rhs) {
            return lhs->name < rhs->name;
        }
    );

    DrawBusLines(doc, db, sorted_buses, projector);
    DrawBusLabels(doc, db, sorted_buses, projector);
    
    auto stops_to_render = CollectStopsToRender(db, buses);
    DrawStopCircles(doc, stops_to_render, projector);
    DrawStopLabels(doc, stops_to_render, projector);

    return doc;
}

void MapRenderer::DrawBusLines(svg::Document& doc, const TransportCatalogue& db,
                              const std::vector<const Bus*>& sorted_buses,
                              const SphereProjector& projector) const {
    size_t color_index = 0;
    
    for (const auto* bus : sorted_buses) {
        if (bus->stops.empty()) continue;

        svg::Polyline polyline;
        polyline.SetStrokeColor(settings_.color_palette[color_index % settings_.color_palette.size()]);
        polyline.SetFillColor("none");
        polyline.SetStrokeWidth(settings_.line_width);
        polyline.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
        polyline.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

        if (bus->is_roundtrip) {
            for (const auto& stop_name : bus->stops) {
                const Stop* stop = db.FindStop(stop_name);
                if (stop) {
                    polyline.AddPoint(projector({stop->lat, stop->lng}));
                }
            }
        } else {
            size_t n = bus->stops.size();
            for (size_t i = 0; i < n; ++i) {
                const Stop* stop = db.FindStop(bus->stops[i]);
                if (stop) {
                    polyline.AddPoint(projector({stop->lat, stop->lng}));
                }
            }
            for (size_t i = n - 2; i < n; --i) {
                const Stop* stop = db.FindStop(bus->stops[i]);
                if (stop) {
                    polyline.AddPoint(projector({stop->lat, stop->lng}));
                }
                if (i == 0) break;
            }
        }

        doc.Add(polyline);
        ++color_index;
    }
}

void MapRenderer::DrawBusLabels(svg::Document& doc, const TransportCatalogue& db,
                               const std::vector<const Bus*>& sorted_buses,
                               const SphereProjector& projector) const {
    size_t color_index = 0;
    
    for (const auto* bus : sorted_buses) {
        if (bus->stops.empty()) continue;
        svg::Color bus_color = settings_.color_palette[color_index % settings_.color_palette.size()];
        std::vector<const Stop*> terminal_stops;
        if (bus->is_roundtrip) {
            const Stop* first_stop = db.FindStop(bus->stops[0]);
            if (first_stop) terminal_stops.push_back(first_stop);
        } else {
            const Stop* first_stop = db.FindStop(bus->stops[0]);
            const Stop* last_stop = db.FindStop(bus->stops.back());
            if (first_stop) terminal_stops.push_back(first_stop);
            if (last_stop && last_stop != first_stop) terminal_stops.push_back(last_stop);
        }
        for (const Stop* terminal_stop : terminal_stops) {
            svg::Point pos = projector({terminal_stop->lat, terminal_stop->lng});
            svg::Text underlayer;
            underlayer.SetPosition(pos)
                      .SetOffset(settings_.bus_label_offset)
                      .SetFontSize(settings_.bus_label_font_size)
                      .SetFontFamily("Verdana")
                      .SetFontWeight("bold")
                      .SetData(bus->name)
                      .SetFillColor(settings_.underlayer_color)
                      .SetStrokeColor(settings_.underlayer_color)
                      .SetStrokeWidth(settings_.underlayer_width)
                      .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
                      .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
            doc.Add(underlayer);
            svg::Text text;
            text.SetPosition(pos)
                .SetOffset(settings_.bus_label_offset)
                .SetFontSize(settings_.bus_label_font_size)
                .SetFontFamily("Verdana")
                .SetFontWeight("bold")
                .SetData(bus->name)
                .SetFillColor(bus_color);
            doc.Add(text);
        }
        ++color_index;
    }
}

std::vector<const Stop*> MapRenderer::CollectStopsToRender(const TransportCatalogue& db, const std::deque<Bus>& buses) const {
    std::set<std::string_view> stops_with_buses;
    for (const auto& bus : buses) {
        for (const auto& stop_name : bus.stops) {
            stops_with_buses.insert(stop_name);
        }
    }
    std::vector<const Stop*> stops_to_render;
    for (std::string_view stop_name : stops_with_buses) {
        const Stop* stop = db.FindStop(stop_name);
        if (stop) stops_to_render.push_back(stop);
    }
    std::sort(stops_to_render.begin(), stops_to_render.end(),
        [](const Stop* lhs, const Stop* rhs) {
            return lhs->name < rhs->name;
        }
    );
    return stops_to_render;
}

void MapRenderer::DrawStopCircles(svg::Document& doc, const std::vector<const Stop*>& stops_to_render,
                                 const SphereProjector& projector) const {
    for (const Stop* stop : stops_to_render) {
        svg::Circle circle;
        circle.SetCenter(projector({stop->lat, stop->lng}))
              .SetRadius(settings_.stop_radius)
              .SetFillColor("white");
        doc.Add(circle);
    }
}

void MapRenderer::DrawStopLabels(svg::Document& doc, const std::vector<const Stop*>& stops_to_render,
                                const SphereProjector& projector) const {
    for (const Stop* stop : stops_to_render) {
        svg::Point pos = projector({stop->lat, stop->lng});
        svg::Text underlayer;
        underlayer.SetPosition(pos)
                  .SetOffset(settings_.stop_label_offset)
                  .SetFontSize(settings_.stop_label_font_size)
                  .SetFontFamily("Verdana")
                  .SetData(stop->name)
                  .SetFillColor(settings_.underlayer_color)
                  .SetStrokeColor(settings_.underlayer_color)
                  .SetStrokeWidth(settings_.underlayer_width)
                  .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
                  .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
        doc.Add(underlayer);
        svg::Text text;
        text.SetPosition(pos)
            .SetOffset(settings_.stop_label_offset)
            .SetFontSize(settings_.stop_label_font_size)
            .SetFontFamily("Verdana")
            .SetData(stop->name)
            .SetFillColor("black");
        doc.Add(text);
    }
}

}  // namespace renderer
