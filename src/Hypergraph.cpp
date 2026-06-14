//
// Created by _ on 21/7/2023.
//

#include <fstream>
#include <algorithm>
#include <sstream>
#include <queue>
#include "Hypergraph.h"

// Update: Return const reference directly
const std::vector<std::size_t> &Hypergraph::getEdges(std::size_t node_id) const {
    return node_edge_simplices[node_id];
}

std::size_t &Hypergraph::getNodeCore(std::size_t node_id) {
    return nodes_core[node_id];
}

// Update: Return const reference directly
const std::vector<std::size_t> &Hypergraph::getNodes(std::size_t edge_id) const {
    return edge_node_simplices[edge_id];
}

std::size_t &Hypergraph::getEdgeCore(std::size_t edge_id) {
    return edges_core[edge_id];
}

std::size_t Hypergraph::getNbrCount(std::size_t node_id) const {
    const auto &deg_edges = getEdges(node_id);

    if (deg_edges.empty()) return 0;

    if (deg_edges.size() == 1) {
        const auto &edge_nodes = getNodes(deg_edges[0]);
        return edge_nodes.size() - 1;
    }

    if (deg_edges.size() == 2) {
        const auto &edge_nodes1 = getNodes(deg_edges[0]);
        const auto &edge_nodes2 = getNodes(deg_edges[1]);
        Counter c;
        std::set_union(edge_nodes1.begin(), edge_nodes1.end(),
                       edge_nodes2.begin(), edge_nodes2.end(),
                       std::back_inserter(c));
        return c.count - 1;
    }

    std::unordered_set<std::size_t> nbrs;
    // Reserve reasonable size to avoid frequent reallocations
    nbrs.reserve(deg_edges.size() * 2); 
    
    for (std::size_t edge_id : deg_edges) {
        const auto &edge_nodes = getNodes(edge_id);
        for (std::size_t node : edge_nodes) {
            if (node != node_id) nbrs.insert(node);
        }
    }
    return nbrs.size();
}

void Hypergraph::loadFromFiles(const std::string &nverts, const std::string &simplices) {

    std::ifstream nvertFile(nverts);
    std::ifstream simplexFile(simplices);

    if (!nvertFile.is_open()) {
        throw std::runtime_error("Error to open file " + nverts);
    }
    if (!simplexFile.is_open()) {
        throw std::runtime_error("Error to open file " + simplices);
    }

    // Clear and prepare 1-based indexing (index 0 is dummy)
    edge_node_simplices.clear();
    edge_node_simplices.emplace_back(); 

    std::size_t numNodes = 0;
    std::vector<size_t> edge_sizes;

    // Read edge sizes (degree of each hyperedge)
    while (!nvertFile.eof()) {
        std::size_t temp;
        if (nvertFile >> temp) {
            edge_sizes.push_back(temp);
        }
    }

    // Read nodes for each edge
    for (size_t size : edge_sizes) {
        std::vector<size_t> edge;
        edge.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            std::size_t node;
            if (simplexFile >> node) {
                edge.push_back(node);
                numNodes = std::max(numNodes, node);
            }
        }
        std::sort(edge.begin(), edge.end()); // Ensure nodes in edge are sorted
        edge_node_simplices.push_back(std::move(edge));
    }

    loadFromFiles_helper(numNodes);

    std::cout << "loaded" << std::endl;
}

void Hypergraph::loadFromFiles(const std::string &hyp) {
    std::ifstream hypFile(hyp);
    if (!hypFile.is_open()) {
        throw std::runtime_error("Error to open file " + hyp);
    }

    // Clear and prepare 1-based indexing
    edge_node_simplices.clear();
    edge_node_simplices.emplace_back();

    std::size_t numNodes = 0;

    std::string line;
    while (std::getline(hypFile, line)) {
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::stringstream ss(line);
        std::size_t temp;
        std::vector<std::size_t> edge;
        while (ss >> temp) {
            edge.push_back(temp);
            numNodes = std::max(numNodes, temp);
        }
        std::sort(edge.begin(), edge.end());
        edge_node_simplices.push_back(std::move(edge));
    }

    loadFromFiles_helper(numNodes);

    std::cout << "loaded" << std::endl;
}

void Hypergraph::loadFromFiles_helper(std::size_t numNodes) {
    // 1-based indexing for nodes
    node_edge_simplices.assign(numNodes + 1, {});

    // Build the inverted index (node -> edges)
    // Since we iterate edges 1..M in order, the pushed edge_ids will be sorted naturally.
    for (size_t edge_id = 1; edge_id < edge_node_simplices.size(); ++edge_id) {
        for (size_t node_id : edge_node_simplices[edge_id]) {
            node_edge_simplices[node_id].push_back(edge_id);
        }
    }

    // Resize core arrays
    // Use .size() which is numNodes + 1
    nodes_core.assign(node_edge_simplices.size(), std::numeric_limits<std::size_t>::max());
    edges_core.assign(edge_node_simplices.size(), std::numeric_limits<std::size_t>::max());
}

std::size_t Hypergraph::getGraphEdgeSize() const {
    return edges_core.size();
}

std::size_t Hypergraph::getGraphNodeSize() const {
    return nodes_core.size();
}

// Updated signature: node_a, nbr_index, node_b
std::vector<std::size_t>
Hypergraph::findIntersectionEdge(std::size_t node_a, std::size_t nbr_index, std::size_t node_b) {
    // Lazy check: if cache is empty, compute it
    if (nbr_intersection[node_a][nbr_index].empty()) {
        const auto &edge_a = getEdges(node_a);
        const auto &edge_b = getEdges(node_b);
        std::set_intersection(edge_a.begin(), edge_a.end(),
                              edge_b.begin(), edge_b.end(),
                              std::back_inserter(nbr_intersection[node_a][nbr_index]));
    }
    return nbr_intersection[node_a][nbr_index];
}

// Overload: find index first
std::vector<std::size_t> Hypergraph::findIntersectionEdge(std::size_t node_a, std::size_t node_b) {
    const auto &nbrs = nbr_simplices[node_a];
    auto it = std::lower_bound(nbrs.begin(), nbrs.end(), node_b);
    if (it == nbrs.end() || *it != node_b) {
        return {}; // Not a neighbor
    }
    size_t nbr_index = static_cast<size_t>(std::distance(nbrs.begin(), it));
    return findIntersectionEdge(node_a, nbr_index, node_b);
}

std::vector<std::size_t> Hypergraph::computeIntersectionEdge(std::size_t node_a, std::size_t node_b) {
    std::vector<std::size_t> result;
    const auto &edge_a = getEdges(node_a);
    const auto &edge_b = getEdges(node_b);
    std::set_intersection(edge_a.begin(), edge_a.end(),
                          edge_b.begin(), edge_b.end(),
                          std::back_inserter(result));
    return result;
}

void Hypergraph::computeNbrs() {
    size_t num_nodes = getGraphNodeSize() - 1; 
    
    // Resize outer vectors
    nbr_simplices.assign(num_nodes + 1, {});
    nbr_intersection.assign(num_nodes + 1, {});

    // Use a temporary vector to collect neighbors to avoid repeated set allocations if possible,
    // but unordered_set is needed for deduplication.
    
    for (std::size_t i = 1; i <= num_nodes; ++i) {
        std::unordered_set<std::size_t> distinct_nbrs;
        const auto &incident_edges = getEdges(i);
        
        for (std::size_t edge_id : incident_edges) {
            const auto &nodes_in_edge = getNodes(edge_id);
            for (std::size_t neighbor : nodes_in_edge) {
                if (neighbor != i) {
                    distinct_nbrs.insert(neighbor);
                }
            }
        }
        
        // Store in sorted vector
        auto &nbr_vec = nbr_simplices[i];
        nbr_vec.assign(distinct_nbrs.begin(), distinct_nbrs.end());
        std::sort(nbr_vec.begin(), nbr_vec.end());

        // Resize the intersection cache for this node
        // nbr_intersection[i][j] caches intersection with nbr_simplices[i][j]
        nbr_intersection[i].resize(nbr_vec.size());
    }
}

const std::vector<std::size_t> &Hypergraph::getNbr(std::size_t node_id) const {
    return nbr_simplices[node_id];
}

void Hypergraph::printNodesCore() const {
    std::cout << "node ";
    for (std::size_t i = 1; i < nodes_core.size(); ++i) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
    std::cout << "core ";
    for (std::size_t i = 1; i < nodes_core.size(); ++i) {
        std::cout << nodes_core[i] << " & ";
    }
    std::cout << std::endl;
}

void Hypergraph::printEdgesCore() const {
    std::cout << "edge ";
    for (std::size_t i = 1; i < edges_core.size(); ++i) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
    std::cout << "core ";
    for (std::size_t i = 1; i < edges_core.size(); ++i) {
        std::cout << edges_core[i] << " & ";
    }
    std::cout << std::endl;
}

std::size_t Hypergraph::getAvgDegree() const {
    std::size_t sum = 0;
    std::size_t count = 0;
    for (std::size_t node_id = 1; node_id < getGraphNodeSize(); ++node_id) {
        sum += getEdges(node_id).size();
        count++;
    }
    return count > 0 ? sum / count : 0;
}

std::size_t Hypergraph::getAvgEdgeSize() const {
    std::size_t sum = 0;
    std::size_t count = 0;
    for (std::size_t edge_id = 1; edge_id < getGraphEdgeSize(); ++edge_id) {
        sum += getNodes(edge_id).size();
        count++;
    }
    return count > 0 ? sum / count : 0;
}

void Hypergraph::resetCore() {
    std::fill(nodes_core.begin(), nodes_core.end(), std::numeric_limits<std::size_t>::max());
    std::fill(edges_core.begin(), edges_core.end(), std::numeric_limits<std::size_t>::max());
}

std::size_t Hypergraph::getEdgeSize(std::size_t edge_id) const {
    return getNodes(edge_id).size();
}