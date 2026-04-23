# project-michclleg
Distributed Pi Calculator:

A distributed large scale calculator for π using the Chudnovsky algorithm and binary splitting across 5 FABRIC nodes.

Coordinator (Node 1)
- Splits term range across 10 worker slots
- Dispatches JSON tasks in parallel
- Tree-reduces returned (P, Q, T) tuples
- Computes final π via GMP floating-point                   
Node 1-5 ports :5000/:5001, 2 workers per node..
  
Workers: All five nodes, two processes each on ports 5000/5001, accept a JSON task containing a k-range [a, b) and the digit target, run binary splitting over that range, and return the three integers P, Q, T as hex-encoded strings in a JSON response.
Coordinator: Node 1 parses command-line arguments, divides the total term count across all ten worker slots, dispatches tasks in parallel, tree-reduces the returned partial (P,Q,T) tuples, and finally computes =42688010005 Q/T using GMP floating-point arithmetic at the requested precision

Notebook.ipynb: Orchestration notebook
Coordinator.c: Dispatches tasks to workers, tree reduces results and computes pi.
Worker.c: Runs binary splitting across an assigned k-range.
DistributedPiCalculator.pdf: Project Documentation

Link to video demo: https://youtu.be/AfcKvwYpENk 
