#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>

struct Stop {
    std::string name;
    double lat = 0.0;
    double lng = 0.0;
    // Дорожные расстояния до других остановок
    std::unordered_map<std::string, int> road_distances;
};

struct Bus {
    std::string name;
    std::vector<std::string> stops;
    bool is_roundtrip = false;
};

/*
 * В этом файле вы можете разместить классы/структуры, которые являются частью предметной области (domain)
 * вашего приложения и не зависят от транспортного справочника. Например Автобусные маршруты и Остановки. 
 *
 * Их можно было бы разместить и в transport_catalogue.h, однако вынесение их в отдельный
 * заголовочный файл может оказаться полезным, когда дело дойдёт до визуализации карты маршрутов:
 * визуализатор карты (map_renderer) можно будет сделать независящим от транспортного справочника.
 *
 * Если структура вашего приложения не позволяет так сделать, просто оставьте этот файл пустым.
 *
 */