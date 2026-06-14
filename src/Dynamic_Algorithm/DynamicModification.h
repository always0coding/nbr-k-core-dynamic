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

} // namespace dynamic_modification
