#include "../Hypergraph.h"
#include <chrono>
#include <map>
#include "NbrCD.h"

// Note: Counter struct is assumed to be in Hypergraph.h or defined here if not.
// Since it was commented out in your provided snippet but present in Hypergraph.h, we use the one in Hypergraph.h

void withInfo::nbrCoreDecomposition(Hypergraph &graph) {
    std::cout << "final " << std::endl;

    auto init_start = std::chrono::high_resolution_clock::now();
    // init core
    auto nodes_lower_bound = std::vector<std::size_t>(graph.getGraphNodeSize() + 1, 0);

    for (std::size_t node = 1; node < graph.getGraphNodeSize(); ++node) {
        const auto &deg_edges = graph.getEdges(node); // Changed

        if (deg_edges.size() == 1) {
            const auto &edge_nodes = graph.getNodes(deg_edges[0]); // Changed
            graph.nodes_core[node] = edge_nodes.size() - 1;
            nodes_lower_bound[node] = graph.nodes_core[node];
            continue;
        }

        if (deg_edges.size() == 2) {
            const auto &edge_nodes1 = graph.getNodes(deg_edges[0]); // Changed
            const auto &edge_nodes2 = graph.getNodes(deg_edges[1]); // Changed
            Counter c;
            std::set_union(edge_nodes1.begin(), edge_nodes1.end(),
                           edge_nodes2.begin(), edge_nodes2.end(),
                           std::back_inserter(c));
            graph.nodes_core[node] = c.count - 1;
            
            size_t s1 = edge_nodes1.size() - 1;
            size_t s2 = edge_nodes2.size() - 1;
            nodes_lower_bound[node] = std::max(s1, s2);
            continue;
        }

        std::unordered_set<std::size_t> nbrs;
        nbrs.reserve(100);
        for (std::size_t edge_id : deg_edges) { // Changed loop
            const auto &edge_nodes = graph.getNodes(edge_id); // Changed
            for (std::size_t neighbor : edge_nodes) {
                if (neighbor != node) nbrs.insert(neighbor);
            }
            nodes_lower_bound[node] = std::max(nodes_lower_bound[node],
                                               edge_nodes.size() - 1);
        }
        graph.nodes_core[node] = nbrs.size();
    }

    // init core value for each edge
    for (std::size_t edge_id = 1; edge_id < graph.getGraphEdgeSize(); ++edge_id) { // Changed
        const auto &edge_nodes = graph.getNodes(edge_id); // Changed
        for (std::size_t node_in_edge : edge_nodes) {
            graph.edges_core[edge_id] = std::min(graph.nodes_core[node_in_edge],
                                                 graph.edges_core[edge_id]);
        }
    }

    std::vector<bool> changed(graph.getGraphNodeSize(), true);

    std::chrono::duration<double> diff = std::chrono::high_resolution_clock::now() - init_start;
    std::cout << "init_Time: " << diff.count() << " s" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    bool updated = true;
    auto decide_time = 0.0;
    auto compute_time = 0.0;
    std::size_t iter = 0;

    while (updated) {
        ++iter;
        updated = false;
        std::size_t loopedNode = 0;
        std::size_t changedNode = 0;
        for (std::size_t currentNodesId = 1; currentNodesId < graph.getGraphNodeSize(); ++currentNodesId) {
            if (!changed[currentNodesId]) {
                continue;
            }
            changed[currentNodesId] = false;
            std::size_t oldCoreV = graph.nodes_core[currentNodesId];
            if (oldCoreV == nodes_lower_bound[currentNodesId]) {
                continue;
            }
            ++loopedNode;
            std::size_t & currentNodeCore = graph.nodes_core[currentNodesId];
            auto time_start = std::chrono::high_resolution_clock::now();
            currentNodeCore = Hypergraph::CoreEvaluationFinal(graph, currentNodesId);
            compute_time += std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now() - time_start).count();
            
            if (oldCoreV > currentNodeCore) { // if the core value is updated
                ++changedNode;
                time_start = std::chrono::high_resolution_clock::now();
                const auto &incident_edges = graph.getEdges(currentNodesId); // Changed
                for (std::size_t edge_id : incident_edges) { // Changed loop
                    std::size_t & edge_core = graph.getEdgeCore(edge_id);
                    if (edge_core > currentNodeCore) {
                        const auto &nodes_in_edge = graph.getNodes(edge_id); // Changed
                        for (std::size_t node_in_edge : nodes_in_edge) {
                            if (graph.nodes_core[node_in_edge] == edge_core) {
                                changed[node_in_edge] = true;
                                updated = true;
                            }
                        }
                        edge_core = currentNodeCore;
                    }
                }
                decide_time += std::chrono::duration<double>(
                        std::chrono::high_resolution_clock::now() - time_start).count();
            }
        }
        std::cout << iter << "," << loopedNode << "," << changedNode << std::endl;
    }

    std::cout << "iter: " << iter << std::endl;
    diff = std::chrono::high_resolution_clock::now() - start;
    std::cout << "decide_time: " << decide_time << " s" << std::endl;
    std::cout << "compute_time: " << compute_time << " s" << std::endl;
    std::cout << "exc_Time: " << diff.count() << " s" << std::endl;
}