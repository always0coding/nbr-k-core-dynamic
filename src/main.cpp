#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Hypergraph.h"
#include "Algorithm/NbrCD.h"
#include "Algorithm_withInfo/NbrCD.h"
#include "graphTool/graphTool.h"
#include "Dynamic_Algorithm/DynamicModification.h"

struct CommandContext {
    std::string dataset;
    std::string command;
    std::vector<std::string> args;
    bool withInfo;
};

using CommandHandler = int (*)(const CommandContext&);
using Clock = std::chrono::steady_clock;

struct CommandSpec {
    const char* name;
    const char* usage;
    CommandHandler handler;
};

static std::vector<void (*)(Hypergraph&)> fun_list = {
    nbrCoreDecomposition,
};

static std::vector<void (*)(Hypergraph&)> info_fun_list = {
    withInfo::nbrCoreDecomposition,
};

static std::vector<std::string> alg_name = {
    "HyperCD++ (nbrCoreDecomposition)"
};

static double elapsed_seconds(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static std::filesystem::path experiment_output_dir() {
    const char* env_dir = std::getenv("DHYP_OUTPUT_DIR");
    std::filesystem::path dir = (env_dir != nullptr && env_dir[0] != '\0') ? env_dir : "outputs";

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "[Warning] Cannot create output directory " << dir << ": " << ec.message() << "\n";
    }
    return dir;
}

static std::string base_name_without_extension(const std::string& path) {
    std::filesystem::path p(path);
    std::string stem = p.stem().string();
    return stem.empty() ? p.filename().string() : stem;
}

static std::string sanitize_token(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char raw_ch : value) {
        unsigned char ch = static_cast<unsigned char>(raw_ch);
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) return "input";
    if (out.size() > 80) out.resize(80);
    return out;
}

static std::string trim_copy(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

static std::size_t parse_size_t_token(const std::string& value, const std::string& label) {
    std::size_t pos = 0;
    std::string trimmed = trim_copy(value);
    if (trimmed.empty() || !std::isdigit(static_cast<unsigned char>(trimmed.front()))) {
        throw std::invalid_argument("Invalid " + label + ": " + value);
    }
    unsigned long long parsed = std::stoull(trimmed, &pos);
    while (pos < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[pos]))) ++pos;
    if (pos != trimmed.size() || parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Invalid " + label + ": " + value);
    }
    return static_cast<std::size_t>(parsed);
}

static std::vector<std::size_t> parse_edge_text(const std::string& text) {
    std::vector<std::size_t> edge;
    std::size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && (text[pos] == ',' || std::isspace(static_cast<unsigned char>(text[pos])))) ++pos;
        if (pos >= text.size()) break;
        if (!std::isdigit(static_cast<unsigned char>(text[pos]))) {
            throw std::invalid_argument("Invalid edge node list: " + text);
        }
        std::size_t end_pos = pos;
        while (end_pos < text.size() && std::isdigit(static_cast<unsigned char>(text[end_pos]))) ++end_pos;
        edge.push_back(parse_size_t_token(text.substr(pos, end_pos - pos), "node id"));
        pos = end_pos;
    }
    std::sort(edge.begin(), edge.end());
    edge.erase(std::unique(edge.begin(), edge.end()), edge.end());
    return edge;
}

static std::vector<std::size_t> parse_edge_from_args(const std::vector<std::string>& args) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ",";
        oss << args[i];
    }
    return parse_edge_text(oss.str());
}

static std::vector<std::size_t> read_edge_id_file(const std::string& edge_file) {
    std::ifstream ifs(edge_file);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open edge id file: " + edge_file);
    }

    std::vector<std::size_t> eids;
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim_copy(line);
        if (line.empty()) continue;
        eids.push_back(parse_size_t_token(line, "edge id"));
    }
    std::sort(eids.begin(), eids.end());
    eids.erase(std::unique(eids.begin(), eids.end()), eids.end());
    return eids;
}

static std::vector<std::vector<std::size_t>> read_edge_file(const std::string& edge_file) {
    std::ifstream ifs(edge_file);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open edge file: " + edge_file);
    }

    std::vector<std::vector<std::size_t>> edges;
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim_copy(line);
        if (line.empty()) continue;
        std::vector<std::size_t> edge = parse_edge_text(line);
        if (edge.empty()) {
            throw std::invalid_argument("Each inserted hyperedge must contain at least one node: " + line);
        }
        edges.push_back(std::move(edge));
    }
    return edges;
}

static void validate_edge_id(const Hypergraph& hg, std::size_t edge_id) {
    if (edge_id == 0 || edge_id >= hg.getGraphEdgeSize()) {
        std::ostringstream oss;
        oss << "edge id must be in [1, " << (hg.getGraphEdgeSize() - 1) << "]: " << edge_id;
        throw std::out_of_range(oss.str());
    }
}

static void validate_edge_ids(const Hypergraph& hg, const std::vector<std::size_t>& edge_ids) {
    for (std::size_t edge_id : edge_ids) {
        validate_edge_id(hg, edge_id);
    }
}

static void validate_existing_nodes(const Hypergraph& hg, const std::vector<std::size_t>& edge) {
    std::vector<std::size_t> normalized = edge;
    std::sort(normalized.begin(), normalized.end());
    auto last = std::unique(normalized.begin(), normalized.end());
    normalized.erase(last, normalized.end());

    if (normalized.empty()) {
        throw std::invalid_argument("A new hyperedge must contain at least one existing node");
    }

    for (std::size_t node_id : normalized) {
        if (node_id == 0 || node_id >= hg.getGraphNodeSize()) {
            std::ostringstream oss;
            oss << "inserted node id must be in [1, " << (hg.getGraphNodeSize() - 1) << "]: " << node_id;
            throw std::out_of_range(oss.str());
        }
    }
}

static void validate_existing_nodes(const Hypergraph& hg, const std::vector<std::vector<std::size_t>>& edges) {
    for (const auto& edge : edges) {
        validate_existing_nodes(hg, edge);
    }
}

static void ensure_baseline_core(Hypergraph& hg, bool withInfoFlag) {
    if (withInfoFlag) withInfo::nbrCoreDecomposition(hg);
    else              nbrCoreDecomposition(hg);
}

static void write_core_values(const Hypergraph& hg, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open core output file: " + path.string());
    }
    for (std::size_t i = 1; i < hg.getGraphNodeSize(); ++i) {
        out << i << "," << hg.nodes_core.at(i) << "\n";
    }
}

static void write_time_value(double seconds, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open time output file: " + path.string());
    }
    out << std::fixed << std::setprecision(9) << seconds << "\n";
}

static void write_dynamic_outputs(
    const Hypergraph& hg,
    double seconds,
    const std::string& dataset,
    const std::string& command,
    const std::string& suffix
) {
    std::filesystem::path output_dir = experiment_output_dir();
    std::string dataset_name = sanitize_token(base_name_without_extension(dataset));
    std::string safe_suffix = sanitize_token(suffix);
    std::string prefix = dataset_name + "_" + command;
    if (!safe_suffix.empty()) {
        prefix += "_" + safe_suffix;
    }

    std::filesystem::path core_path = output_dir / (prefix + "_core.txt");
    std::filesystem::path time_path = output_dir / (prefix + "_time.txt");
    write_core_values(hg, core_path);
    write_time_value(seconds, time_path);
    std::cout << "Core values written to " << core_path.string() << "\n";
    std::cout << "Execution time written to " << time_path.string() << "\n";
}

static Hypergraph load_graph_with_core(const CommandContext& ctx) {
    Hypergraph hg;
    std::cout << "Loading dataset from " << ctx.dataset << "\n";
    hg.loadFromFiles(ctx.dataset);
    std::cout << "Running static core decomposition ...\n";
    ensure_baseline_core(hg, ctx.withInfo);
    return hg;
}

static int command_info(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> info");
    Hypergraph hg;
    std::cout << "get graph info, loading datasets from " << ctx.dataset << "\n";
    hg.loadFromFiles(ctx.dataset);
    printGraphInfo(hg);
    return 0;
}

static int command_detail(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> detail");
    Hypergraph hg;
    std::cout << "get graph detail, loading datasets from " << ctx.dataset << "\n";
    hg.loadFromFiles(ctx.dataset);
    getGraphDetiles(hg, ctx.dataset);
    return 0;
}

static int command_org(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> org");
    organize_nodes(ctx.dataset);
    return 0;
}

static int command_varyV(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> varyV");
    vary_nodes(ctx.dataset);
    return 0;
}

static int command_vary(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> vary");
    vary_nodes(ctx.dataset);
    return 0;
}

static int command_varyE(const CommandContext& ctx) {
    if (!ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> varyE");
    vary_edges(ctx.dataset);
    return 0;
}

static int command_sortN(const CommandContext& ctx) {
    if (ctx.args.size() != 2) throw std::invalid_argument("Usage: <dataset> sortN <degree|nbr> <asc|desc>");
    orderType order = cover_to_orderType(ctx.args[1].c_str());
    if (ctx.args[0] == "degree") sortNodes(ctx.dataset, DEGREE, order);
    else if (ctx.args[0] == "nbr") sortNodes(ctx.dataset, NBR, order);
    else throw std::invalid_argument("Usage: <dataset> sortN <degree|nbr> <asc|desc>");
    return 0;
}

static int command_sort(const CommandContext& ctx) {
    if (ctx.args.size() != 2) throw std::invalid_argument("Usage: <dataset> sort <degree|nbr> <asc|desc>");
    orderType order = cover_to_orderType(ctx.args[1].c_str());
    if (ctx.args[0] == "degree") sortNodes(ctx.dataset, DEGREE, order);
    else if (ctx.args[0] == "nbr") sortNodes(ctx.dataset, NBR, order);
    else throw std::invalid_argument("Usage: <dataset> sort <degree|nbr> <asc|desc>");
    return 0;
}

static int command_sortE(const CommandContext& ctx) {
    if (ctx.args.size() != 1) throw std::invalid_argument("Usage: <dataset> sortE <asc|desc>");
    sortEdges(ctx.dataset, cover_to_orderType(ctx.args[0].c_str()));
    return 0;
}

static int command_sortEV(const CommandContext& ctx) {
    if (ctx.args.size() != 2) throw std::invalid_argument("Usage: <dataset> sortEV <edge-order> <node-order>");
    orderType edge = cover_to_orderType(ctx.args[0].c_str());
    orderType node = cover_to_orderType(ctx.args[1].c_str());
    sortEdgeNodes(ctx.dataset, node, edge);
    return 0;
}

static int command_gen(const CommandContext& ctx) {
    if (ctx.args.size() != 4) throw std::invalid_argument("Usage: <dataset> gen P <nbr|deg> <numNodes> <numEdges>");
    if (ctx.args[0] != "P") {
        throw std::invalid_argument("Only PowerLaw generation is supported. Use P as the distribution.");
    }
    if (ctx.args[1] != "nbr" && ctx.args[1] != "deg") {
        throw std::invalid_argument("Generation target must be nbr or deg");
    }

    bool isNbr = ctx.args[1] == "nbr";
    std::size_t numNodes = parse_size_t_token(ctx.args[2], "numNodes");
    std::size_t numEdges = parse_size_t_token(ctx.args[3], "numEdges");
    if (numNodes == 0) throw std::invalid_argument("numNodes must be greater than 0");
    if (numEdges < 2) throw std::invalid_argument("numEdges must be at least 2 for PowerLaw generation");

    if (isNbr) {
        generateHypergraph_nbr_PowerLaw(numNodes, numEdges, ctx.dataset);
    } else {
        generateHypergraph_deg_PowerLaw(numNodes, numEdges, ctx.dataset);
    }
    return 0;
}

static int command_genR(const CommandContext& ctx) {
    if (ctx.args.size() != 3) throw std::invalid_argument("Usage: <dataset> genR <numNodes> <numEdges> <maxEdgeSize>");
    std::size_t numNodes = parse_size_t_token(ctx.args[0], "numNodes");
    std::size_t numEdges = parse_size_t_token(ctx.args[1], "numEdges");
    std::size_t maxEdgeSize = parse_size_t_token(ctx.args[2], "maxEdgeSize");
    if (numNodes == 0) throw std::invalid_argument("numNodes must be greater than 0");
    if (numEdges == 0) throw std::invalid_argument("numEdges must be greater than 0");
    if (maxEdgeSize == 0) throw std::invalid_argument("maxEdgeSize must be greater than 0");
    generateHypergraph_random(
        numNodes,
        numEdges,
        maxEdgeSize,
        ctx.dataset
    );
    return 0;
}

static int command_run(const CommandContext& ctx) {
    if (ctx.args.empty() || ctx.args.size() > 2) throw std::invalid_argument("Usage: <dataset> run <alg> [iters] [--info]");
    std::size_t alg = parse_size_t_token(ctx.args[0], "algorithm id");
    std::size_t iters = ctx.args.size() == 2 ? parse_size_t_token(ctx.args[1], "iterations") : 1;
    if (alg >= fun_list.size()) throw std::invalid_argument("Invalid algorithm id");
    if (iters == 0) throw std::invalid_argument("Iterations must be greater than 0");

    Hypergraph hg;
    std::cout << "Loading datasets from " << ctx.dataset << "\n";
    hg.loadFromFiles(ctx.dataset);

    double static_time = 0.0;
    for (std::size_t i = 0; i < iters; ++i) {
        auto static_start = Clock::now();
        if (ctx.withInfo) info_fun_list[alg](hg);
        else              fun_list[alg](hg);
        static_time += elapsed_seconds(static_start);
        if (i + 1 < iters) hg.resetCore();
    }

    std::cout << "Static decomposition time: " << std::fixed << std::setprecision(6) << static_time << " s.\n";
    std::filesystem::path output_dir = experiment_output_dir();
    std::string dataset_name = sanitize_token(base_name_without_extension(ctx.dataset));
    write_time_value(static_time, output_dir / (dataset_name + "_static_time_" + std::to_string(alg) + ".txt"));

    if (ctx.withInfo) {
        write_core_values(hg, output_dir / (dataset_name + "_static_core_" + std::to_string(alg) + ".txt"));
    }

    return 0;
}

static int command_delE_single(const CommandContext& ctx) {
    if (ctx.args.size() != 1) throw std::invalid_argument("Usage: <dataset> delE_single <edge_id>");
    std::size_t edge_id = parse_size_t_token(ctx.args[0], "edge id");
    Hypergraph hg = load_graph_with_core(ctx);
    validate_edge_id(hg, edge_id);

    auto start = Clock::now();
    dynamic_modification::delE_single(hg, edge_id);
    double elapsed = elapsed_seconds(start);

    std::cout << "Single-edge deletion finished in " << std::fixed << std::setprecision(9) << elapsed << " s.\n";
    write_dynamic_outputs(hg, elapsed, ctx.dataset, "delE_single", "edge_" + std::to_string(edge_id));
    return 0;
}

static int command_addE_single(const CommandContext& ctx) {
    if (ctx.args.empty()) throw std::invalid_argument("Usage: <dataset> addE_single <node1,node2,...>");
    std::vector<std::size_t> edge = parse_edge_from_args(ctx.args);
    if (edge.empty()) throw std::invalid_argument("A new hyperedge must contain at least one node");

    Hypergraph hg = load_graph_with_core(ctx);
    validate_existing_nodes(hg, edge);
    dynamic_modification::InsertionWorkspace workspace;
    workspace.prepare(hg.getGraphNodeSize(), hg.getGraphEdgeSize() + 1);
    auto start = Clock::now();
    dynamic_modification::addE_single_optimized(hg, edge, workspace);
    double elapsed = elapsed_seconds(start);

    std::ostringstream suffix;
    suffix << "edge";
    for (std::size_t node : edge) suffix << "_" << node;
    std::cout << "Single-edge insertion finished in " << std::fixed << std::setprecision(9) << elapsed << " s.\n";
    write_dynamic_outputs(hg, elapsed, ctx.dataset, "addE_single", suffix.str());
    return 0;
}

static int command_delE_batch(const CommandContext& ctx) {
    if (ctx.args.size() != 1) throw std::invalid_argument("Usage: <dataset> delE_batch <edge_id_file>");
    std::vector<std::size_t> eids = read_edge_id_file(ctx.args[0]);
    if (eids.empty()) throw std::invalid_argument("No edge ids found in " + ctx.args[0]);

    Hypergraph hg = load_graph_with_core(ctx);
    validate_edge_ids(hg, eids);
    auto start = Clock::now();
    dynamic_modification::delE_batch(hg, eids);
    double elapsed = elapsed_seconds(start);

    std::cout << "Batch deletion of " << eids.size() << " edges finished in "
              << std::fixed << std::setprecision(9) << elapsed << " s.\n";
    write_dynamic_outputs(hg, elapsed, ctx.dataset, "delE_batch", base_name_without_extension(ctx.args[0]));
    return 0;
}

static int command_addE_batch(const CommandContext& ctx) {
    if (ctx.args.size() != 1) throw std::invalid_argument("Usage: <dataset> addE_batch <edge_file>");
    std::vector<std::vector<std::size_t>> edges = read_edge_file(ctx.args[0]);
    if (edges.empty()) throw std::invalid_argument("No inserted edges found in " + ctx.args[0]);

    Hypergraph hg = load_graph_with_core(ctx);
    validate_existing_nodes(hg, edges);
    dynamic_modification::InsertionWorkspace workspace;
    workspace.prepare(hg.getGraphNodeSize(), hg.getGraphEdgeSize() + edges.size());
    auto start = Clock::now();
    dynamic_modification::addE_batch_optimized(hg, edges, workspace);
    double elapsed = elapsed_seconds(start);

    std::cout << "Batch insertion of " << edges.size() << " edges finished in "
              << std::fixed << std::setprecision(9) << elapsed << " s.\n";
    write_dynamic_outputs(hg, elapsed, ctx.dataset, "addE_batch", base_name_without_extension(ctx.args[0]));
    return 0;
}

static const std::vector<CommandSpec>& command_specs() {
    static const std::vector<CommandSpec> specs = {
        {"info", "<dataset> info", command_info},
        {"detail", "<dataset> detail", command_detail},
        {"org", "<dataset> org", command_org},
        {"vary", "<dataset> vary", command_vary},
        {"varyV", "<dataset> varyV", command_varyV},
        {"varyE", "<dataset> varyE", command_varyE},
        {"sort", "<dataset> sort <degree|nbr> <asc|desc>", command_sort},
        {"sortN", "<dataset> sortN <degree|nbr> <asc|desc>", command_sortN},
        {"sortE", "<dataset> sortE <asc|desc>", command_sortE},
        {"sortEV", "<dataset> sortEV <edge-order> <node-order>", command_sortEV},
        {"sortVE", "<dataset> sortVE <edge-order> <node-order>", command_sortEV},
        {"gen", "<dataset> gen P <nbr|deg> <numNodes> <numEdges>", command_gen},
        {"genR", "<dataset> genR <numNodes> <numEdges> <maxEdgeSize>", command_genR},
        {"run", "<dataset> run <alg> [iters] [--info]", command_run},
        {"delE_single", "<dataset> delE_single <edge_id>", command_delE_single},
        {"addE_single", "<dataset> addE_single <node1,node2,...>", command_addE_single},
        {"delE_batch", "<dataset> delE_batch <edge_id_file>", command_delE_batch},
        {"addE_batch", "<dataset> addE_batch <edge_file>", command_addE_batch},
    };
    return specs;
}

static void showHelpMessage() {
    std::cout << "Usage: ./DynamicHypergraphCore <dataset> <command> [arguments] [--info]\n\n";
    std::cout << "Commands:\n";
    for (const CommandSpec& spec : command_specs()) {
        std::cout << "  " << spec.usage << "\n";
    }
    std::cout << "\nDynamic update outputs:\n";
    std::cout << "  <output>/<dataset>_<command>_<input>_core.txt\n";
    std::cout << "  <output>/<dataset>_<command>_<input>_time.txt\n";
    std::cout << "\nSet DHYP_OUTPUT_DIR to change the output directory. Default: outputs\n";
    std::cout << "\nAlgorithms:\n";
    for (std::size_t i = 0; i < alg_name.size(); ++i) {
        std::cout << "  " << i << " - " << alg_name[i] << "\n";
    }
}

static CommandContext parse_context(int argc, char* argv[]) {
    CommandContext ctx;
    ctx.dataset = argv[1];
    ctx.command = argv[2];
    ctx.withInfo = false;

    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--info") == 0) {
            ctx.withInfo = true;
        } else {
            ctx.args.emplace_back(argv[i]);
        }
    }
    return ctx;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "help") == 0) {
        showHelpMessage();
        return 0;
    }

    if (argc < 3 || std::strcmp(argv[2], "--help") == 0 || std::strcmp(argv[2], "help") == 0) {
        showHelpMessage();
        return 0;
    }

    try {
        CommandContext ctx = parse_context(argc, argv);
        for (const CommandSpec& spec : command_specs()) {
            if (ctx.command == spec.name) {
                return spec.handler(ctx);
            }
        }
        std::cerr << "[Error] Unknown command: " << ctx.command << "\n\n";
        showHelpMessage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "[Error] " << ex.what() << "\n";
        return 1;
    }
}
