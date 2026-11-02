#include "DynamicModification.h"
#include "util.h"
#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <unordered_map>

using namespace hg::maint;

namespace dynamic_modification {

void InsertionWorkspace::prepare(std::size_t numNodes, std::size_t numEdges) {
    if (inCandidate.size() < numNodes) {
        inCandidate.resize(numNodes, 0);
        visited_marker.resize(numNodes, 0);
        filter_state.resize(numNodes, 0);
        inBucket.resize(numNodes, 0);
    }
    if (edge_state.size() < numEdges) {
        edge_state.resize(numEdges, 0);
    }
}

static void validate_inserted_edge(const Hypergraph& graph, const std::vector<std::size_t>& nodes) {
    if (nodes.empty()) throw std::invalid_argument("Inserted hyperedge must not be empty");
    for (std::size_t u : nodes) {
        if (u == 0 || u >= graph.getGraphNodeSize()) {
            throw std::invalid_argument("Inserted vertex is outside the fixed vertex set");
        }
    }
}

static std::size_t compute_support(
    const Hypergraph& graph, std::size_t u, std::size_t currentK,
    InsertionWorkspace& workspace, bool candidateView
) {
    std::size_t support = 0;
    std::size_t targetK = currentK + 1;

    for (std::size_t eid : graph.getEdges(u)) {
        // The upper bound uses every edge at level currentK; exact support
        // uses only the edges still valid in the candidate view.
        if (candidateView) {
            if (workspace.edge_state[eid] != 1) continue;
        } else {
            if (graph.edges_core[eid] < currentK) continue;
        }

        for (std::size_t v : graph.getNodes(eid)) {
            if (v != u && !workspace.visited_marker[v]) {
                workspace.visited_marker[v] = 1;
                workspace.visited_history.push_back(v);
                if (++support >= targetK) break;
            }
        }
        if (support >= targetK) break;
    }

    for (std::size_t v : workspace.visited_history) {
        workspace.visited_marker[v] = 0;
    }
    workspace.visited_history.clear();
    return support;
}

static void expand_candidates(Hypergraph& graph, std::size_t currentK, InsertionWorkspace& workspace) {
    for (std::size_t u : workspace.current_seeds) {
        if (graph.nodes_core[u] == currentK && !workspace.inCandidate[u]) {
            workspace.inCandidate[u] = 1;
            workspace.candidates.push_back(u);
        }
    }

    for (std::size_t i = 0; i < workspace.candidates.size(); ++i) {
        std::size_t u = workspace.candidates[i];
        for (std::size_t eid : graph.getEdges(u)) {
            if (graph.edges_core[eid] < currentK || workspace.edge_state[eid]) continue;
            workspace.edge_state[eid] = 1;
            workspace.touched_edges.push_back(eid);

            // EdgeFilter: a failed level-currentK endpoint invalidates the edge.
            // States are 0 = unchecked, 1 = valid, 2 = rejected.
            for (std::size_t v : graph.getNodes(eid)) {
                if (graph.nodes_core[v] != currentK) continue;
                if (!workspace.filter_state[v]) {
                    bool passes = compute_support(graph, v, currentK, workspace, false) > currentK;
                    workspace.filter_state[v] = passes ? 1 : 2;
                    workspace.filtered_vertices.push_back(v);
                }
                if (workspace.filter_state[v] == 2) {
                    workspace.edge_state[eid] = 2;
                    break;
                }
            }
            if (workspace.edge_state[eid] == 2) continue;

            for (std::size_t v : graph.getNodes(eid)) {
                if (graph.nodes_core[v] == currentK && !workspace.inCandidate[v]) {
                    workspace.inCandidate[v] = 1;
                    workspace.candidates.push_back(v);
                }
            }
        }
    }
}

static void promote_candidates(
    Hypergraph& graph, std::size_t currentK, InsertionWorkspace& workspace,
    const std::vector<std::size_t>& insertedNodes
) {
    expand_candidates(graph, currentK, workspace);
    bool stopped = false;

    while (true) {
        workspace.peeled.clear();
        for (std::size_t u : workspace.candidates) {
            if (!workspace.inCandidate[u]) continue;
            if (compute_support(graph, u, currentK, workspace, true) > currentK) continue;

            // EarlyStop applies only to single insertion (batch passes no endpoints).
            if (std::binary_search(insertedNodes.begin(), insertedNodes.end(), u)) {
                stopped = true;
                break;
            }
            workspace.peeled.push_back(u);
        }
        if (stopped || workspace.peeled.empty()) break;

        // Evaluate the whole round before removing any candidate or edge.
        for (std::size_t u : workspace.peeled) workspace.inCandidate[u] = 0;
        for (std::size_t u : workspace.peeled) {
            for (std::size_t eid : graph.getEdges(u)) {
                if (workspace.edge_state[eid] == 1) workspace.edge_state[eid] = 2;
            }
        }
    }

    workspace.next_seeds.clear();
    if (!stopped) {
        for (std::size_t u : workspace.candidates) {
            if (workspace.inCandidate[u]) {
                graph.nodes_core[u] = currentK + 1;
                workspace.next_seeds.push_back(u);
            }
        }
        // Every surviving edge has all its level-currentK endpoints promoted.
        for (std::size_t eid : workspace.touched_edges) {
            if (workspace.edge_state[eid] == 1) graph.edges_core[eid] = currentK + 1;
        }
    }

    for (std::size_t u : workspace.candidates) workspace.inCandidate[u] = 0;
    for (std::size_t eid : workspace.touched_edges) workspace.edge_state[eid] = 0;
    for (std::size_t u : workspace.filtered_vertices) workspace.filter_state[u] = 0;
    workspace.candidates.clear();
    workspace.touched_edges.clear();
    workspace.filtered_vertices.clear();
    workspace.peeled.clear();
}

void addE_single_optimized(
    Hypergraph &graph, const std::vector<std::size_t>& nodes, InsertionWorkspace& workspace
) {
    if (nodes.empty()) return;
    validate_inserted_edge(graph, nodes);
    workspace.prepare(graph.getGraphNodeSize(), graph.getGraphEdgeSize() + 1);

    std::size_t newEdgeId = addEdge(graph, nodes);
    updateEdgeCoreForEdge(graph, newEdgeId);
    const auto& insertedNodes = graph.getNodes(newEdgeId);
    std::size_t currentK = graph.edges_core[newEdgeId];
    for (std::size_t u : insertedNodes) {
        if (graph.nodes_core[u] == currentK) workspace.current_seeds.push_back(u);
    }

    while (!workspace.current_seeds.empty()) {
        bool stopped = false;
        // EarlyStop: the inserted edge must belong to the next core.
        for (std::size_t u : insertedNodes) {
            if (graph.nodes_core[u] != currentK) continue;
            bool passes = compute_support(graph, u, currentK, workspace, false) > currentK;
            workspace.filter_state[u] = passes ? 1 : 2;
            workspace.filtered_vertices.push_back(u);
            if (!passes) {
                stopped = true;
                break;
            }
        }
        if (stopped) break;

        promote_candidates(graph, currentK, workspace, insertedNodes);
        workspace.current_seeds.swap(workspace.next_seeds);
        ++currentK;
    }
    // Precheck can stop before promote_candidates clears the per-level cache.
    for (std::size_t u : workspace.filtered_vertices) workspace.filter_state[u] = 0;
    workspace.filtered_vertices.clear();
    workspace.current_seeds.clear();
    workspace.next_seeds.clear();
}

static void add_seed_to_bucket(
    const Hypergraph& graph, std::size_t u, InsertionWorkspace& workspace,
    std::map<std::size_t, std::vector<std::size_t>>& seeds_by_level
) {
    if (!workspace.inBucket[u]) {
        workspace.inBucket[u] = 1;
        seeds_by_level[graph.nodes_core[u]].push_back(u);
    }
}

void addE_batch_optimized(
    Hypergraph &graph, const std::vector<std::vector<std::size_t>>& batchEdges,
    InsertionWorkspace& workspace
) {
    if (batchEdges.empty()) return;
    for (const auto& nodes : batchEdges) validate_inserted_edge(graph, nodes);

    std::size_t firstNewEdge = graph.getGraphEdgeSize();
    workspace.prepare(graph.getGraphNodeSize(), firstNewEdge + batchEdges.size());
    for (const auto& nodes : batchEdges) {
        std::size_t newEdgeId = addEdge(graph, nodes);
        updateEdgeCoreForEdge(graph, newEdgeId);
    }
    std::size_t afterNewEdge = graph.getGraphEdgeSize();

    // Jump: combine overlapping size bounds before updating edge minima.
    std::unordered_map<std::size_t, std::size_t> old_core;
    for (std::size_t eid = firstNewEdge; eid < afterNewEdge; ++eid) {
        std::size_t lowerBound = graph.getNodes(eid).size() - 1;
        for (std::size_t u : graph.getNodes(eid)) {
            if (graph.nodes_core[u] < lowerBound) {
                old_core.try_emplace(u, graph.nodes_core[u]);
                graph.nodes_core[u] = lowerBound;
            }
        }
    }
    for (const auto& entry : old_core) {
        for (std::size_t eid : graph.getEdges(entry.first)) {
            if (!workspace.edge_state[eid]) {
                workspace.edge_state[eid] = 1;
                workspace.touched_edges.push_back(eid);
            }
        }
    }

    std::map<std::size_t, std::vector<std::size_t>> seeds_by_level;
    for (std::size_t eid : workspace.touched_edges) {
        std::size_t minNodeCore = std::numeric_limits<std::size_t>::max();
        std::size_t oldMinJumpCore = std::numeric_limits<std::size_t>::max();
        for (std::size_t u : graph.getNodes(eid)) {
            minNodeCore = std::min(minNodeCore, graph.nodes_core[u]);
            auto previous = old_core.find(u);
            if (previous != old_core.end()) oldMinJumpCore = std::min(oldMinJumpCore, previous->second);
        }
        graph.edges_core[eid] = minNodeCore;

        // An old edge can supply new support even when its minimum is unchanged.
        if (oldMinJumpCore <= minNodeCore) {
            for (std::size_t u : graph.getNodes(eid)) {
                if (graph.nodes_core[u] == minNodeCore) add_seed_to_bucket(graph, u, workspace, seeds_by_level);
            }
        }
        workspace.edge_state[eid] = 0;
    }
    workspace.touched_edges.clear();

    // Include inserted edges whose endpoints did not jump.
    for (std::size_t eid = firstNewEdge; eid < afterNewEdge; ++eid) {
        for (std::size_t u : graph.getNodes(eid)) {
            if (graph.nodes_core[u] == graph.edges_core[eid]) add_seed_to_bucket(graph, u, workspace, seeds_by_level);
        }
    }

    const std::vector<std::size_t> noSingleEdge;
    while (!seeds_by_level.empty()) {
        auto current = seeds_by_level.begin();
        std::size_t currentK = current->first;
        workspace.current_seeds.swap(current->second);
        seeds_by_level.erase(current);
        for (std::size_t u : workspace.current_seeds) workspace.inBucket[u] = 0;

        promote_candidates(graph, currentK, workspace, noSingleEdge);
        workspace.current_seeds.clear();
        for (std::size_t u : workspace.next_seeds) add_seed_to_bucket(graph, u, workspace, seeds_by_level);
        workspace.next_seeds.clear();
    }
}

} // namespace dynamic_modification
