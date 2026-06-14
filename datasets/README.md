# Datasets

Each dataset is a plain-text hypergraph. Every line is one hyperedge, and vertex
identifiers are comma-separated integers.

Example:

```text
1,2,3
1,4,5,6
2,4,7,8
```

Included sample files:

- `coauth-MAG-History-full.hyp`: the bundled sample hypergraph.
- `coauth-MAG-History-delete-edges.txt`: 1-based edge IDs for batch deletion examples.
- `coauth-MAG-History-insert-edges.hyp`: hyperedges for batch insertion examples.

Large benchmark datasets should not be committed directly. Put local copies under
this directory or pass absolute paths to the executable. The `.gitignore` keeps
large local dataset dumps out of version control by default.
