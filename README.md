# project-michclleg
Distributed Pi Calculator:
A distributed large scale calculator for π using the Chudnovsky algorithm and binary splitting across 5 FABRIC nodes.

##Architecture
```
┌──────────────────────────────────────────────────────────────┐
│                     Coordinator (Node 1)                     │
│  - Splits term range across 10 worker slots                  │
│  - Dispatches JSON tasks in parallel                         │
│  - Tree-reduces returned (P, Q, T) tuples                    │
│  - Computes final π via GMP floating-point                   │
└──────────────────┬───────────────────────────────────────────┘
                   │ HTTP/JSON
       ┌───────────┼───────────┐
       ▼           ▼           ▼  ...
  Node 1        Node 2       Node 3-5
 :5000/:5001  :5000/:5001  :5000/:5001
  (2 workers   (2 workers   (2 workers
   per node)    per node)    per node)
```

##Worker & Coordinator
- Workers: All five nodes, two processes each on ports 5000/5001, accept a JSON task containing a k-range [a, b) and the digit target, run binary splitting over that range, and return the three integers P, Q, T as hex-encoded strings in a JSON response.
- Coordinator: Node 1 parses command-line arguments, divides the total term count across all ten worker slots, dispatches tasks in parallel, tree-reduces the returned partial (P,Q,T) tuples, and finally computes pi using GMP floating-point arithmetic at the requested precision

##Files
- Notebook.ipynb: Orchestration notebook
- Coordinator.c: Dispatches tasks to workers, tree reduces results and computes pi.
- Worker.c: Runs binary splitting across an assigned k-range.
- DistributedPiCalculator.pdf: Project Documentation

##Prerequisites
- A FABRIC account with an active project
- SSH credentials configured for FABRIC
- coordinator.c and worker.c in the working directory
- Update project_id in the notebook before running

Run the project end-to-end using Notebook.ipynb. The coordinator prints a progress table as workers return, then writes the final value to 'pi_(digits).txt'. By default the coordinator enforces a hard limit of 10,000,000 digits. To raise it, patch the constant in coordinator.c and recompile as shown in cell 10 of the notebook.

Link to video demo: https://youtu.be/AfcKvwYpENk 
