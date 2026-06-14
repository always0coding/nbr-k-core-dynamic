# Dynamic Hypergraph Core Maintenance Source Code

This repository provides the C++ implementation for dynamic neighbor-based core
maintenance in hypergraphs. It supports static neighbor-core decomposition,
single-edge insertion, single-edge deletion, batch edge insertion, batch edge
deletion, and a small set of graph preprocessing and generation utilities.

The project is organized as a standalone CMake package and can be built
independently from the paper source tree.

## Getting Started

These instructions will get a copy of the project running on your local machine
for development, testing, and artifact evaluation.

### Prerequisites

Before building the `DynamicHypergraphCore` tool, ensure that the following
dependencies are available:

- CMake 3.20 or newer
- A C++20-compatible compiler

On Ubuntu-like systems, install the usual build dependencies with:

```bash
sudo apt-get install build-essential cmake
```

On Windows, we recommend using Ninja with a C++20 compiler available in `PATH`.

### Suggested IDE

We recommend using CLion or Visual Studio Code for development. The project uses
CMake directly, so any IDE with CMake support should work.

## Building the Project

To build the project, follow these steps:

1. Clone or download the repository to your local machine.

2. Navigate to the root directory of the project.

```bash
cd dynamic_hypergraph
```

3. Configure the project with CMake.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

If CMake selects an unavailable generator on Windows, use Ninja explicitly:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

4. Compile the project.

```bash
cmake --build build --parallel
```

This creates the `DynamicHypergraphCore` executable in the `build` directory.

## Usage

To use the `DynamicHypergraphCore` tool, run the executable from the command line
by specifying a dataset and the desired operation. The basic syntax is:

```bash
./build/DynamicHypergraphCore <dataset> <command> [arguments] [--info]
```

On Windows, the executable may be:

```bash
./build/DynamicHypergraphCore.exe <dataset> <command> [arguments] [--info]
```

### Options

- `info`: Get basic hypergraph information.
- `detail`: Get detailed degree, neighbor, and edge-size information.
- `org`: Organize the hypergraph by renumbering vertices from 1 to N, removing
  duplicate vertices in the same hyperedge, and removing empty or single-vertex
  hyperedges. The output overwrites the input dataset.
- `gen P <nbr|deg> <numNodes> <numEdges>`: Generate an organized hypergraph
  with a PowerLaw distribution over neighbor count or degree.
- `run <alg> [iters] [--info]`: Run static neighbor-core decomposition.
- `delE_single <edge_id>`: Delete one hyperedge by its edge ID.
- `addE_single <node1,node2,...>`: Insert one hyperedge from command-line vertex
  IDs.
- `delE_batch <edge_id_file>`: Delete a batch of hyperedges listed in an
  external file.
- `addE_batch <edge_file>`: Insert a batch of hyperedges from an external file.

Currently, the supported static algorithm ID is:

- `0`: HyperCD++ (`nbrCoreDecomposition`)

Hyperedge IDs are 1-based. The first real hyperedge has ID `1`.

### Important

Before running decomposition or dynamic updates on a raw dataset, run:

```bash
./build/DynamicHypergraphCore <dataset> org
```

The `org` command rewrites the dataset in place. Keep a backup copy if the
original vertex IDs or input ordering must be preserved.

## Dynamic Update Outputs

All dynamic update commands write two output files:

```text
<output>/<dataset>_<command>_<input>_core.txt
<output>/<dataset>_<command>_<input>_time.txt
```

The first file stores the core value after the hypergraph update. The second
file stores the execution time of the dynamic update.

By default, outputs are written to `outputs/`. To change the output directory,
set `DHYP_OUTPUT_DIR`:

```bash
DHYP_OUTPUT_DIR=/tmp/dhyp_outputs ./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp delE_single 1
```

## Examples

To get the graph information of `datasets/coauth-MAG-History-full.hyp`:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp info
```

To run static neighbor-core decomposition:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp run 0 1 --info
```

To delete a single hyperedge whose edge ID is `1`:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp delE_single 1
```

To insert a single hyperedge containing vertices `10`, `11`, and `12`:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp addE_single 10,11,12
```

To delete a batch of hyperedges:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp delE_batch datasets/coauth-MAG-History-delete-edges.txt
```

To insert a batch of hyperedges:

```bash
./build/DynamicHypergraphCore datasets/coauth-MAG-History-full.hyp addE_batch datasets/coauth-MAG-History-insert-edges.hyp
```

For the full command summary, run:

```bash
./build/DynamicHypergraphCore --help
```

## Dataset File Format

The input dataset is a plain-text hypergraph file. Each line represents one
hyperedge, and vertices are separated by commas:

```text
1,2,3,4,5
2,4,6
3,5,7,9
```

This example represents three hyperedges. Vertex IDs are integer values.

The batch deletion file contains one 1-based edge ID per line:

```text
1
3
8
```

The batch insertion file uses the same format as the input hypergraph dataset:

```text
10,11,12
3,8,13,21
```

## Example Script

We provide a shell script `main_run.sh` to build the project and run a smoke test
on the bundled `coauth-MAG-History-full.hyp` dataset:

```bash
bash main_run.sh
```

The script configures CMake, builds the executable, runs basic dataset
information commands, runs static decomposition, runs the dynamic update
examples, and prints the help page. It is intended as a quick sanity check for a
fresh checkout.

## Code Attribution

The static neighbor-core decomposition implementation and graph utility code are
adapted from the source code associated with:

```bibtex
@article{10.1145/3709656,
  author = {Zhang, Wenqian and Yang, Zhengyi and Wen, Dong and Li, Wentao and Zhang, Wenjie and Lin, Xuemin},
  title = {Accelerating Core Decomposition in Billion-Scale Hypergraphs},
  year = {2025},
  issue_date = {February 2025},
  publisher = {Association for Computing Machinery},
  address = {New York, NY, USA},
  volume = {3},
  number = {1},
  url = {https://doi.org/10.1145/3709656},
  doi = {10.1145/3709656},
  journal = {Proc. ACM Manag. Data},
  month = feb,
  articleno = {6},
  numpages = {27},
  keywords = {core decomposition, hypergraph, k-core}
}
```

## Citation

If you use this code in your research, please cite the corresponding paper:

```bibtex
@misc{bai2026fastmaintenance,
  title = {Fast Maintenance of Neighborhood-Based k-Cores in Dynamic Hypergraphs},
  author = {Bai, Haotian and Zhang, Wenqian and Tang, Yifu and Luo, Qi and Yang, Zihan and Wen, Dong and Yang, Zhengyi},
  year = {2026},
  note = {Manuscript}
}
```

## License

This project is licensed under the MIT License. See the `LICENSE` file for
details.
