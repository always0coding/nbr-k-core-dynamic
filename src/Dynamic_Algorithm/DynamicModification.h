#pragma once

#include <cstddef>
#include <vector>
#include <queue>
#include "../Hypergraph.h"

namespace dynamic_modification {

    void delE_single(Hypergraph &graph, std::size_t edgeId);
    void delE_batch(Hypergraph &graph, const std::vector<std::size_t>& batchEdgeIds);

    void addE_single(Hypergraph &graph, const std::vector<std::size_t>& nodes);
    void addE_batch(Hypergraph &graph, const std::vector<std::vector<std::size_t>>& batchEdges);

    // Reuse these buffers across insertions; only touched entries are reset.
    struct InsertionWorkspace {
        std::vector<char> inCandidate, visited_marker, edge_state, filter_state, inBucket;
        std::vector<std::size_t> candidates, touched_edges, visited_history;
        std::vector<std::size_t> filtered_vertices, current_seeds, next_seeds, peeled;

        void prepare(std::size_t numNodes, std::size_t numEdges);
    };

    void addE_single_optimized(Hypergraph &graph, const std::vector<std::size_t>& nodes,
                               InsertionWorkspace& workspace);
    void addE_batch_optimized(Hypergraph &graph, const std::vector<std::vector<std::size_t>>& batchEdges,
                              InsertionWorkspace& workspace);

} // namespace dynamic_modification
