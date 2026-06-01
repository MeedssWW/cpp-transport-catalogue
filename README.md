# C++ Transport Catalogue

Transport Catalogue is a command-line C++ application for storing public transport data and answering route/statistics requests in JSON format.

The project focuses on data modeling, JSON processing, graph-based route calculation, and SVG map rendering. It was built as a practical modern C++ project with separated modules for the domain model, request handling, routing, and rendering.

## Features

- Stores stops, bus routes, and road distances.
- Parses input and stat requests from JSON.
- Calculates route statistics for bus lines.
- Finds optimal routes between stops using a weighted directed graph.
- Renders a transport map as SVG.
- Produces structured JSON responses.

## Tech Stack

- C++17 / C++20
- STL containers and algorithms
- Custom JSON parser
- Graph routing algorithms
- SVG generation
- Modular C++ architecture

## Project Structure

```text
.
|-- main.cpp                  # Application entry point
|-- domain.*                  # Transport domain entities
|-- transport_catalogue.*     # Main storage and lookup logic
|-- json.*                    # JSON parser and printer
|-- json_reader.*             # Input parsing and request processing
|-- request_handler.*         # Query interface over the catalogue
|-- transport_router.*        # Route graph construction and routing
|-- graph.h / router.h        # Generic graph and router implementation
|-- map_renderer.*            # SVG map rendering
`-- svg.*                     # SVG primitives and output helpers
```

## Build

The repository currently contains the source files directly. One simple way to build it with a local compiler is:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  main.cpp domain.cpp geo.cpp json.cpp json_builder.cpp json_reader.cpp \
  map_renderer.cpp request_handler.cpp svg.cpp transport_catalogue.cpp \
  transport_router.cpp \
  -o transport_catalogue
```

If you use CMake, create a build target that includes all `.cpp` files listed above.

## Usage

```bash
./transport_catalogue < input.json > output.json
```

The input JSON contains:

- base requests with stops, bus routes, coordinates, and distances;
- render settings for SVG map output;
- routing settings such as bus speed and waiting time;
- stat requests for buses, stops, maps, and routes.

## Example

```json
{
  "base_requests": [
    {
      "type": "Stop",
      "name": "A",
      "latitude": 55.611087,
      "longitude": 37.20829,
      "road_distances": { "B": 1200 }
    },
    {
      "type": "Stop",
      "name": "B",
      "latitude": 55.595884,
      "longitude": 37.209755,
      "road_distances": {}
    },
    {
      "type": "Bus",
      "name": "256",
      "stops": ["A", "B"],
      "is_roundtrip": false
    }
  ],
  "stat_requests": [
    { "id": 1, "type": "Bus", "name": "256" }
  ],
  "render_settings": {},
  "routing_settings": {
    "bus_wait_time": 2,
    "bus_velocity": 40
  }
}
```

## What This Project Demonstrates

- Designing a non-trivial C++ domain model.
- Working with references and stable storage for domain entities.
- Building graph abstractions for route calculation.
- Separating parsing, business logic, rendering, and output formatting.
- Handling structured input/output without external JSON dependencies.

## Possible Improvements

- Add a root `CMakeLists.txt`.
- Add GitHub Actions for Linux builds.
- Add unit tests for the catalogue, JSON parser, and router.
- Add example input/output files.
- Add AddressSanitizer and UndefinedBehaviorSanitizer checks in CI.
