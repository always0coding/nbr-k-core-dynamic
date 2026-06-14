#include "DynamicModification.h"
#include "../Hypergraph.h"
#include "util.h"
#include <vector>
#include <queue>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cstddef>
#include <iomanip>
#include <iostream>

using namespace hg::maint;

namespace dynamic_modification {
static inline std::size_t compute_support_in_candidate_view(
    const Hypergraph& graph, std::size_t u, std::size_t targetK,
    const std::vector<char>& inCandidate,
    std::vector<char>& local_visited_marker, 
    std::vector<std::size_t>& local_visited_history
) {
    std::size_t support = 0;
    local_visited_history.clear();
    
    for (std::size_t eid : graph.getEdges(u)) {
        bool edge_valid = true;
        for (std::size_t w : graph.getNodes(eid)) {
            if (w == u) continue;
            if (!inCandidate[w] && graph.nodes_core[w] < targetK) {
                edge_valid = false; 
                break;
            }
        }
        
        if (edge_valid) {
            for (std::size_t w : graph.getNodes(eid)) {
                if (w != u && !local_visited_marker[w]) {
                    local_visited_marker[w] = 1;
                    local_visited_history.push_back(w);
                    support++;
                    if (support >= targetK) break; 
                }
            }
        }
        if (support >= targetK) break; 
    }
    
    for (std::size_t v : local_visited_history) local_visited_marker[v] = 0;
    return support;
}

void addE_single(Hypergraph &graph, const std::vector<std::size_t>& nodes) {
    if (nodes.empty()) return;

    std::size_t numNodes = graph.getGraphNodeSize();

    std::size_t newEdgeId = addEdge(graph, nodes); 
    
    std::size_t minNodeCore = std::numeric_limits<std::size_t>::max();
    for (std::size_t u : nodes) {
        minNodeCore = std::min(minNodeCore, graph.nodes_core[u]);
    }
    
    graph.edges_core[newEdgeId] = minNodeCore;
    
    std::vector<std::size_t> current_seeds;
    for (std::size_t u : nodes) {
        if (graph.nodes_core[u] == minNodeCore) {
            current_seeds.push_back(u);
        }
    }

    if (current_seeds.empty()) return;

    std::vector<char> inCandidate(numNodes, 0);
    std::vector<std::size_t> candidates;
    
    std::vector<char> visited_marker(numNodes, 0);
    std::vector<std::size_t> visited_history;

    std::size_t currentK = minNodeCore;
    
    while (!current_seeds.empty()) {
        std::size_t targetK = currentK + 1;
        candidates.clear();
        std::queue<std::size_t> q;

        for (std::size_t u : current_seeds) {
            if (graph.nodes_core[u] == currentK && !inCandidate[u]) {
                inCandidate[u] = 1;
                q.push(u);
            }
        }

        if (q.empty()) break;

        while(!q.empty()){
            std::size_t u = q.front(); q.pop();
            candidates.push_back(u);

            const auto& incidentEdges = graph.getEdges(u);
            for(std::size_t eid : incidentEdges) {
                if (graph.getEdgeCore(eid) >= currentK) {
                    const auto& neighbors = graph.getNodes(eid);
                    for(std::size_t v : neighbors) {
                        if (v != u && graph.nodes_core[v] == currentK && !inCandidate[v]) {
                            inCandidate[v] = 1;
                            q.push(v);
                        }
                    }
                }
            }
        }

        std::vector<std::size_t> peeled;
        peeled.reserve(candidates.size());

        bool changed = true;
        while (changed) {
            changed = false;
            peeled.clear();

            for (std::size_t u : candidates) {
                if (!inCandidate[u]) continue;

                std::size_t support = compute_support_in_candidate_view(
                    graph, u, targetK, inCandidate, visited_marker, visited_history
                );
                if (support < targetK) {
                    peeled.push_back(u);
                }
            }

            if (!peeled.empty()) {
                changed = true;
                for (std::size_t u : peeled) {
                    inCandidate[u] = 0;
                }
            }
        }

        std::vector<std::size_t> next_seeds;
        
        for (std::size_t u : candidates) {
            if (inCandidate[u]) {
                graph.nodes_core[u] = targetK;
                next_seeds.push_back(u);
                updateIncidentEdgesCoreForNode(graph, u);
            }
            
            inCandidate[u] = 0; 
        }
        
        current_seeds = std::move(next_seeds);
        currentK++;
    }
}

void addE_batch(Hypergraph &graph, const std::vector<std::vector<std::size_t>>& batchEdges) {
    if (batchEdges.empty()) return;

    std::size_t numNodes = graph.getGraphNodeSize();

    std::size_t max_active_K = 0;
    std::vector<std::vector<std::size_t>> seeds_by_level(numNodes + 2);

    for (const auto& nodes : batchEdges) {
        std::size_t newEdgeId = addEdge(graph, nodes);
        
        std::size_t minNodeCore = std::numeric_limits<std::size_t>::max();
        for(std::size_t u : nodes) {
            minNodeCore = std::min(minNodeCore, graph.nodes_core[u]);
        }
        graph.edges_core[newEdgeId] = minNodeCore;
        
        for(std::size_t u : nodes) {
            if (graph.nodes_core[u] == minNodeCore) {
                seeds_by_level[minNodeCore].push_back(u);
                max_active_K = std::max(max_active_K, minNodeCore);
            }
        }
    }

    std::vector<char> inCandidate(numNodes, 0);
    std::vector<std::size_t> candidates;
    candidates.reserve(numNodes / 10); 
    
    std::vector<char> visited_marker(numNodes, 0);
    std::vector<std::size_t> visited_history;
    visited_history.reserve(4096);

    for (std::size_t currentK = 0; currentK <= max_active_K; ++currentK) {
        if (seeds_by_level[currentK].empty()) continue; 

        std::size_t targetK = currentK + 1;
        candidates.clear();
        std::queue<std::size_t> q;

        for (std::size_t u : seeds_by_level[currentK]) {
            if (graph.nodes_core[u] == currentK && !inCandidate[u]) {
                inCandidate[u] = 1;
                q.push(u);
            }
        }

        if (q.empty()) continue;

        while(!q.empty()){
            std::size_t u = q.front(); q.pop();
            candidates.push_back(u);

            const auto& incidentEdges = graph.getEdges(u);
            for(std::size_t eid : incidentEdges) {
                if (graph.getEdgeCore(eid) >= currentK) {
                    const auto& neighbors = graph.getNodes(eid);
                    for(std::size_t v : neighbors) {
                        if (v != u && graph.nodes_core[v] == currentK && !inCandidate[v]) {
                            inCandidate[v] = 1;
                            q.push(v);
                        }
                    }
                }
            }
        }

        std::vector<std::size_t> peeled;
        peeled.reserve(candidates.size());

        bool changed = true;
        while (changed) {
            changed = false;
            peeled.clear();

            for (std::size_t u : candidates) {
                if (!inCandidate[u]) continue;

                std::size_t support = compute_support_in_candidate_view(
                    graph, u, targetK, inCandidate, visited_marker, visited_history
                );
                if (support < targetK) {
                    peeled.push_back(u);
                }
            }

            if (!peeled.empty()) {
                changed = true;
                for (std::size_t u : peeled) {
                    inCandidate[u] = 0;
                }
            }
        }

        for (std::size_t u : candidates) {
            if (inCandidate[u]) {
                graph.nodes_core[u] = targetK;
                seeds_by_level[targetK].push_back(u);
                max_active_K = std::max(max_active_K, targetK); 
                
                updateIncidentEdgesCoreForNode(graph, u);
            }
            inCandidate[u] = 0; 
        }
    }
}
}
