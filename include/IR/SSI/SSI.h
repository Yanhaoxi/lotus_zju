/*
Static Single Information (SSI) IR

SSI additionally guarantees that
- Every definition of a variable dominates all its uses (SSA property)
- Every use of a variable post-dominates all its reaching definitions

Static Single Information (SSI) form = SSA + σ-functions
1. Start from SSA form.
2. Compute the iterated post-dominance frontier (analogous to dominance frontier) to decide where σ’s are needed.
3. Insert σ-functions at each control-flow split whose successors are not in the same post-dom tree region.
4. Rename variables again to give unique names to σ results (mirrors SSA renaming).

*/