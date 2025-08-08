#include "json.h"
#include "json_reader.h"
#include "map_renderer.h"


int main() {
    const json::Document input = json::Load(std::cin);
    const auto& root = input.GetRoot().AsMap();

    TransportCatalogue catalogue;
    JsonReader reader(catalogue);
    reader.LoadData(input);

    RenderSettings render_settings = reader.ParseRenderSettings(root.at("render_settings").AsMap());
    renderer::MapRenderer map_renderer(render_settings);

    json::Array responses = reader.ProcessRequests(input, map_renderer);
    json::Document output{responses};
    json::Print(output, std::cout);

    return 0;
}
