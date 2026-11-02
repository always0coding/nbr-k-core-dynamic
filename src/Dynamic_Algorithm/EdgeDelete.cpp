#include "DynamicModification.h"
#include "../Hypergraph.h"
#include <algorithm>
#include <queue>
#include <vector>

namespace dynamic_modification {

static inline bool check_support_survives(
    Hypergraph& graph,
    std::size_t u,
    std::size_t targetK,
    std::vector<char>& visited_marker,
    std::vector<std::size_t>& visited_history
) {
    const auto& incident_edges = graph.getEdges(u);

    std::vector<std::size_t> valid_edges;
    valid_edges.reserve(incident_edges.size());

    for (std::size_t eid : incident_edges) {
        if (graph.getEdgeCore(eid) >= targetK) {
            if (graph.getEdgeSize(eid) - 1 >= targetK) {
                return true;
            }
            valid_edges.push_back(eid);
        }
    }

    if (valid_edges.empty()) return false;

    std::size_t current_support = 0;
    visited_history.clear();
    bool survives = false;

    for (std::size_t eid : valid_edges) {
        const auto& nodes_in_edge = graph.getNodes(eid);

        for (std::size_t v : nodes_in_edge) {
            if (v == u) continue;

            if (!visited_marker[v]) {
                visited_marker[v] = 1;
                visited_history.push_back(v);
                current_support++;

                if (current_support >= targetK) {
                    survives = true;
                    break;
                }
            }
        }
        if (survives) break;
    }

    for (std::size_t v : visited_history) {
        visited_marker[v] = 0;
    }

    return survives;
}

void delE_single(Hypergraph &graph, std::size_t edgeId) {
    if (edgeId == 0 || edgeId >= graph.getGraphEdgeSize()) return;

    std::size_t old_e_core = graph.getEdgeCore(edgeId);
    if (graph.getNodes(edgeId).empty()) return;

    std::size_t numNodes = graph.getGraphNodeSize();
    std::vector<char> inBucket(numNodes, 0);
    std::vector<std::size_t> ruc_seeds;

    std::vector<std::size_t> edgeNodes = graph.getNodes(edgeId);

    graph.getEdgeCore(edgeId) = 0;
    graph.edge_node_simplices[edgeId].clear();

    for (std::size_t nodeId : edgeNodes) {
        std::vector<std::size_t>& incident_edges = graph.node_edge_simplices[nodeId];
        incident_edges.erase(
            std::remove(incident_edges.begin(), incident_edges.end(), edgeId),
            incident_edges.end()
        );

        if (graph.nodes_core[nodeId] == old_e_core) {
            if (!inBucket[nodeId]) {
                inBucket[nodeId] = 1;
                ruc_seeds.push_back(nodeId);
            }
        }
    }

    if (ruc_seeds.empty()) return;

    std::vector<char> visited_marker(numNodes, 0);
    std::vector<std::size_t> visited_history;
    visited_history.reserve(4096);

    std::vector<std::queue<std::size_t>> buckets(old_e_core + 2);

    for (std::size_t u : ruc_seeds) {
        buckets[old_e_core].push(u);
    }

    for (std::size_t K = old_e_core; K > 0; --K) {
        while (!buckets[K].empty()) {
            std::size_t u = buckets[K].front();
            buckets[K].pop();
            inBucket[u] = 0;

            if (graph.nodes_core[u] < K) continue;

            bool survives = check_support_survives(
                graph, u, K, visited_marker, visited_history
            );

            if (!survives) {
                graph.nodes_core[u] = K - 1;

                if (K - 1 > 0 && !inBucket[u]) {
                    buckets[K - 1].push(u);
                    inBucket[u] = 1;
                }

                const auto& u_edges = graph.getEdges(u);
                for (std::size_t eid : u_edges) {
                    if (graph.getEdgeCore(eid) >= K) {
                        graph.getEdgeCore(eid) = K - 1;

                        const auto& nbrs = graph.getNodes(eid);
                        for (std::size_t v : nbrs) {
                            if (v != u && graph.nodes_core[v] == K && !inBucket[v]) {
                                buckets[K].push(v);
                                inBucket[v] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

void delE_batch(Hypergraph &graph, const std::vector<std::size_t>& batchEdgeIds) {
    if (batchEdgeIds.empty()) return;

    std::size_t numNodes = graph.getGraphNodeSize();
    std::vector<char> cleanup_flags(numNodes, 0);
    std::vector<std::size_t> cleanup_nodes;
    std::vector<char> inBucket(numNodes, 0);
    std::vector<std::size_t> ruc_seeds;

    std::size_t max_affected_K = 0;

    for (std::size_t edgeId : batchEdgeIds) {
        if (edgeId == 0 || edgeId >= graph.getGraphEdgeSize()) continue;

        std::size_t old_e_core = graph.getEdgeCore(edgeId);
        if (graph.getNodes(edgeId).empty()) continue;

        std::vector<std::size_t> edgeNodes = graph.getNodes(edgeId);

        graph.getEdgeCore(edgeId) = 0;
        graph.edge_node_simplices[edgeId].clear();

        for (std::size_t nodeId : edgeNodes) {
            if (!cleanup_flags[nodeId]) {
                cleanup_flags[nodeId] = 1;
                cleanup_nodes.push_back(nodeId);
            }

            if (graph.nodes_core[nodeId] == old_e_core) {
                if (!inBucket[nodeId]) {
                    inBucket[nodeId] = 1;
                    ruc_seeds.push_back(nodeId);
                    max_affected_K = std::max(max_affected_K, old_e_core);
                }
            }
        }
    }

    for (std::size_t nodeId : cleanup_nodes) {
        std::vector<std::size_t>& incident_edges = graph.node_edge_simplices[nodeId];
        auto new_end = std::remove_if(incident_edges.begin(), incident_edges.end(),
            [&graph](std::size_t eid) {
                return graph.getNodes(eid).empty();
            });
        incident_edges.erase(new_end, incident_edges.end());
    }

    std::vector<char> visited_marker(numNodes, 0);
    std::vector<std::size_t> visited_history;
    visited_history.reserve(4096);

    std::vector<std::queue<std::size_t>> buckets(max_affected_K + 2);

    for (std::size_t u : ruc_seeds) {
        std::size_t k = graph.nodes_core[u];
        if (k > 0) {
            buckets[k].push(u);
        }
    }

    for (std::size_t K = max_affected_K; K > 0; --K) {
        while (!buckets[K].empty()) {
            std::size_t u = buckets[K].front();
            buckets[K].pop();
            inBucket[u] = 0;

            if (graph.nodes_core[u] < K) continue;

            bool survives = check_support_survives(
                graph, u, K, visited_marker, visited_history
            );

            if (!survives) {
                graph.nodes_core[u] = K - 1;

                if (K - 1 > 0 && !inBucket[u]) {
                    buckets[K - 1].push(u);
                    inBucket[u] = 1;
                }

                const auto& u_edges = graph.getEdges(u);
                for (std::size_t eid : u_edges) {
                    if (graph.getEdgeCore(eid) >= K) {
                        graph.getEdgeCore(eid) = K - 1;

                        const auto& nbrs = graph.getNodes(eid);
                        for (std::size_t v : nbrs) {
                            if (v != u && graph.nodes_core[v] == K && !inBucket[v]) {
                                buckets[K].push(v);
                                inBucket[v] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

}
