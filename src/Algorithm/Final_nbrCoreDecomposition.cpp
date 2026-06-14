#include "../Hypergraph.h"
#include <chrono>
#include <map>
#include "NbrCD.h"

void nbrCoreDecomposition(Hypergraph &graph) {
    std::cout << "final " << std::endl;

    // init core
    auto nodes_lower_bound = std::vector<std::size_t>(graph.getGraphNodeSize() + 1, 0);

    for (std::size_t node = 1; node < graph.getGraphNodeSize(); ++node) {
        const auto &deg_edges = graph.getEdges(node); // Changed to vector reference

        if (deg_edges.size() == 1) {
            // if only one edge, the core value is the number of nodes in the edge
            const auto &edge_nodes = graph.getNodes(deg_edges[0]); // Changed
            graph.nodes_core[node] = edge_nodes.size() - 1;
            nodes_lower_bound[node] = graph.nodes_core[node];
            continue;
        }

        // if only two edges, use union to get the number of nodes in the two edges
        if (deg_edges.size() == 2) {
            const auto &edge_nodes1 = graph.getNodes(deg_edges[0]); // Changed
            const auto &edge_nodes2 = graph.getNodes(deg_edges[1]); // Changed
            Counter c;
            std::set_union(edge_nodes1.begin(), edge_nodes1.end(),
                           edge_nodes2.begin(), edge_nodes2.end(),
                           std::back_inserter(c));
            graph.nodes_core[node] = c.count - 1;
            
            // Calculate lower bound
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
    for (std::size_t edge_id = 1; edge_id < graph.getGraphEdgeSize(); ++edge_id) { // Use getGraphEdgeSize()
        const auto &edge_nodes = graph.getNodes(edge_id); // Changed
        for (std::size_t node_in_edge : edge_nodes) {
            graph.edges_core[edge_id] = std::min(graph.nodes_core[node_in_edge],
                                                 graph.edges_core[edge_id]);
        }
    }

    std::vector<bool> changed(graph.getGraphNodeSize(), true);

    bool updated = true;
    while (updated) {
        updated = false;
        for (std::size_t currentNodesId = 1; currentNodesId < graph.getGraphNodeSize(); ++currentNodesId) {
            if (!changed[currentNodesId]) {
                continue;
            }
            changed[currentNodesId] = false;
            std::size_t oldCoreV = graph.nodes_core[currentNodesId];
            if (oldCoreV == nodes_lower_bound[currentNodesId]) {
                continue;
            }
            std::size_t & currentNodeCore = graph.nodes_core[currentNodesId];
            currentNodeCore = Hypergraph::CoreEvaluationFinal(graph, currentNodesId);
            
            if (oldCoreV > currentNodeCore) { // if the core value is updated
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
            }
        }
    }

    const auto n = graph.getGraphNodeSize();
    if (graph.nodes_support.size() != n) graph.nodes_support.assign(n, 0);

    std::vector<char> seen(n, 0);
    std::vector<std::size_t> touched;
    
    for (std::size_t u = 1; u < n; ++u) {
        const std::size_t u_core = graph.nodes_core[u];
        std::size_t support = 0;

        const auto &u_edges = graph.getEdges(u); // Changed
        for (std::size_t e : u_edges) { // Changed loop

            if (graph.edges_core[e] < u_core) continue;
            
            const auto &e_nodes = graph.getNodes(e); // Changed
            for (std::size_t v : e_nodes) {
                if (v == u) continue;
                if (!seen[v]) {
                    seen[v] = 1;
                    touched.push_back(v);
                    if (graph.nodes_core[v] >= u_core) ++support;
                }
            }
        }

        graph.nodes_support[u] = support;

        for (auto v : touched) seen[v] = 0;
        touched.clear();
    }

}
