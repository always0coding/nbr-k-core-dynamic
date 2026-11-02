#include "util.h"
#include "../Hypergraph.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace hg {
namespace maint {

void updateEdgeCoreForEdge(Hypergraph& graph, std::size_t edgeId) {
    const auto& nodes = graph.getNodes(edgeId);
    std::size_t minCoreOnEdge = std::numeric_limits<std::size_t>::max();
    bool hasAnyNode = false;

    for (std::size_t nodeOnEdge : nodes) {
        std::size_t nodeCore = graph.nodes_core[nodeOnEdge];
        minCoreOnEdge = std::min(minCoreOnEdge, nodeCore);
        hasAnyNode = true;
    }

    graph.getEdgeCore(edgeId) = hasAnyNode ? minCoreOnEdge : 0;
}

void updateIncidentEdgesCoreForNode(Hypergraph& graph, std::size_t nodeId) {
    const auto& edges = graph.getEdges(nodeId);
    for (std::size_t edgeId : edges) {
        updateEdgeCoreForEdge(graph, edgeId);
    }
}

std::size_t addEdge(Hypergraph& graph, const std::vector<std::size_t>& nodes) {
    if (nodes.empty()) {
        return 0;
    }

    std::size_t new_edge_id = graph.edge_node_simplices.size();

    std::vector<std::size_t> sorted_nodes = nodes;
    std::sort(sorted_nodes.begin(), sorted_nodes.end());
    auto last = std::unique(sorted_nodes.begin(), sorted_nodes.end());
    sorted_nodes.erase(last, sorted_nodes.end());

    for (std::size_t node_id : sorted_nodes) {
        if (node_id == 0 || node_id >= graph.node_edge_simplices.size()) {
            throw std::out_of_range("inserted node id is out of range: " + std::to_string(node_id));
        }
    }

    graph.edge_node_simplices.push_back(std::move(sorted_nodes));

    for (std::size_t node_id : graph.edge_node_simplices.back()) {
        std::vector<std::size_t>& incident_edges = graph.node_edge_simplices[node_id];
        incident_edges.push_back(new_edge_id);
    }

    graph.edges_core.push_back(0);

    return new_edge_id;
}

}
}
