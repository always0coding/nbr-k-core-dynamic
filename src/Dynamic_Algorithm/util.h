#pragma once

#include <cstddef>
#include <vector>

class Hypergraph;

namespace hg {
namespace maint {

void updateEdgeCoreForEdge(Hypergraph& graph, std::size_t edgeId);

void updateIncidentEdgesCoreForNode(Hypergraph& graph, std::size_t nodeId);

std::size_t addEdge(Hypergraph& graph, const std::vector<std::size_t>& nodes);

}
}
