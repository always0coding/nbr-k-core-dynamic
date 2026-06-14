#ifndef HYPERGRAPHKCORE_HYPERGRAPH_H
#define HYPERGRAPHKCORE_HYPERGRAPH_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <limits>
#include <algorithm>
#include <set>
#include <chrono>
#include <cmath>
#include <queue>

struct Counter {
    struct value_type {
        template<typename T>
        value_type(const T &) {}
    };

    void push_back(const value_type &) { ++count; }

    size_t count = 0;
};


class Hypergraph {
public:

    Hypergraph() = default;

    Hypergraph(const Hypergraph &hg) = default;

    Hypergraph(Hypergraph &&hg) = default;

    Hypergraph &operator=(const Hypergraph &hg) = default;

    Hypergraph &operator=(Hypergraph &&hg) = default;

    ~Hypergraph() = default;

    [[nodiscard]] const std::vector<std::size_t> &getNbr(std::size_t node_id) const;

    [[nodiscard]] const std::vector<std::size_t> &getEdges(std::size_t node_id) const;

    [[nodiscard]] std::size_t &getNodeCore(std::size_t node_id);

    [[nodiscard]] const std::vector<std::size_t> &getNodes(std::size_t edge_id) const;

    [[nodiscard]] std::size_t getNbrCount(std::size_t node_id) const;

    [[nodiscard]] std::size_t &getEdgeCore(std::size_t edge_id);

    [[nodiscard]] std::size_t getGraphEdgeSize() const;

    [[nodiscard]] std::size_t getGraphNodeSize() const;

    [[nodiscard]] std::size_t getEdgeSize(std::size_t edge_id) const;

    [[nodiscard]] std::size_t getAvgDegree() const;

    [[nodiscard]] std::size_t getAvgEdgeSize() const;

    // Updated: nbr_index logic changes with vector of vectors
    std::vector<std::size_t> findIntersectionEdge(std::size_t node_a, std::size_t nbr_index, std::size_t node_b);

    std::vector<std::size_t> findIntersectionEdge(std::size_t node_a, std::size_t node_b);

    std::vector<std::size_t> computeIntersectionEdge(std::size_t node_a, std::size_t node_b);

    void loadFromFiles(const std::string &nverts, const std::string &simplices);

    void loadFromFiles(const std::string &hyp);

    void computeNbrs();

    void printNodesCore() const;

    void printEdgesCore() const;

    static void bin_sort_hyperedge(Hypergraph &graph, const std::size_t node_id) {
        // Access directly as reference
        std::vector<size_t> &edges = graph.node_edge_simplices[node_id];
        if (edges.empty()) return;

        std::size_t node_core = graph.getNodeCore(node_id);
        std::vector<size_t> bin_count(node_core + 1, 0);
        std::vector<size_t> bin(edges.size(), 0);

        for (std::size_t edge_id : edges) {
            std::size_t edge_core = graph.getEdgeCore(edge_id);
            bin_count[std::min(node_core, edge_core)]++;
        }
        for (std::size_t i = node_core - 1; i > 0; --i) {
            bin_count[i] += bin_count[i + 1];
        }

        for (std::size_t edge_id : edges) {
            std::size_t edge_core = graph.getEdgeCore(edge_id);
            bin[--bin_count[std::min(node_core, edge_core)]] = edge_id;
        }
        
        edges = bin;
    }

//    Alg 6 with implement optimization, using built-in sort
    static std::size_t CoreEvaluationFinal(Hypergraph &graph, const std::size_t node_id) {
        std::unordered_set<std::size_t> core_weight_set(graph.getNodeCore(node_id) + 1);
        const std::vector<std::size_t> &incident_edges_ref = graph.getEdges(node_id);

        std::vector<std::size_t> incident_edges = incident_edges_ref; // Copy

        std::sort(incident_edges.begin(),
                  incident_edges.end(),
                  [&graph](std::size_t a, std::size_t b) {
                      return graph.edges_core[a] > graph.edges_core[b];
                  });


        if (incident_edges.empty()) {
            return 0;
        }

        if (incident_edges.size() == 1) {
            std::size_t edge_id = incident_edges[0];
            return std::min(graph.getEdgeCore(edge_id), graph.getEdgeSize(edge_id) - 1);
        }

        if (incident_edges.size() >= 2) {
            std::size_t first_edge_id = incident_edges[0];
            std::size_t second_edge_id = incident_edges[1];
            std::size_t first_edge_core = graph.getEdgeCore(first_edge_id);
            std::size_t second_edge_core = graph.getEdgeCore(second_edge_id);
            std::size_t first_edge_size = graph.getEdgeSize(first_edge_id) - 1;
//            if first satisfy the condition, just return the core value
            if (first_edge_size >= first_edge_core) {
                return first_edge_core;
            }
            if (first_edge_size >= second_edge_core) {
                return first_edge_size;
            }
        }

        for (std::size_t edge_id : incident_edges) {
            std::size_t edge_core = graph.getEdgeCore(edge_id);
            if (core_weight_set.size() >= edge_core) return core_weight_set.size();
            const std::vector<std::size_t> &edge_nodes = graph.getNodes(edge_id);
            
            for (std::size_t current_node : edge_nodes) {
                if (current_node == node_id) continue;
                core_weight_set.insert(current_node);
                if (core_weight_set.size() >= edge_core) return edge_core;
            }
        }

        return std::min(core_weight_set.size(), graph.getNodeCore(node_id));
    }

//    exact same as Alg 5 in the paper
    static std::size_t CoreEvaluation(Hypergraph &graph, std::size_t node_id) {
        std::size_t currentNodeCore = graph.nodes_core[node_id];
        std::size_t count_core_nbr = 0;
        std::vector<std::size_t> count(currentNodeCore + 1, 0);
        
        const std::vector<std::size_t> &nbrs = graph.getNbr(node_id);
        
        for (std::size_t i = 0; i < nbrs.size(); ++i) {
            std::size_t nbr = nbrs[i];
            // Access intersection cache using [node_id][i]
            std::vector<size_t> IntersectionEdges = graph.findIntersectionEdge(node_id, i, nbr);
            std::size_t max_core = 0;
            for (auto edge_id: IntersectionEdges) {
                max_core = std::max(max_core, graph.getEdgeCore(edge_id));
                if (max_core >= currentNodeCore) {
                    ++count_core_nbr;
                    if (count_core_nbr >= currentNodeCore) {
                        return currentNodeCore;
                    }
                    break;
                }
            }
            ++count[std::min(currentNodeCore, max_core)];
        }
        std::size_t sum = 0;
        for (auto k = currentNodeCore; k > 0; --k) {
            sum += count[k];
            if (sum >= k) { // check if there are k edges with core value >= k
                currentNodeCore = k;
                break; // break the inner loop if the core value is updated
            }
        }
        return currentNodeCore;
    }

    std::vector<std::size_t> nodes_core; // for node

    std::vector<std::size_t> nodes_support; // maximum support neighbours for nodes

//  DATA STRUCTURE CHANGED TO VECTOR OF VECTORS
    std::vector<std::vector<std::size_t>> node_edge_simplices;

//    edge_simplices for each edge
    std::vector<std::vector<std::size_t>> edge_node_simplices;

//    edges_core[e] is edge data of edge e
    std::vector<std::size_t> edges_core;

//    neighbors of node v
    std::vector<std::vector<std::size_t>> nbr_simplices;

    // Cache for intersections: nbr_intersection[node_id][nbr_index]
    std::vector<std::vector<std::vector<size_t>>> nbr_intersection;

    void resetCore();

private:
    void loadFromFiles_helper(std::size_t numNodes);

};


#endif //HYPERGRAPHKCORE_HYPERGRAPH_H
