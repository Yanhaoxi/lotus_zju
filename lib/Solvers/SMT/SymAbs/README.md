
From the Section "Automatic Abstraction of Bit-Vector
Formulae"

## Abstract Domains

This module implements several abstract domains for symbolic abstraction:

### Zone Domain (Difference Bound Matrices)
- **File**: `Zone.cpp`, `Zone.h`
- **Constraints**: `x - y ≤ c` and `x ≤ c`
- **Description**: The zone domain (also known as Difference Bound Matrices or DBM) tracks difference constraints between variables. It is more restrictive than the octagon domain but computationally more efficient.
- **Use cases**: Timing analysis, scheduling, real-time systems verification

### Octagon Domain
- **File**: `Octagon.cpp`, `Octagon.h`
- **Constraints**: `±x ± y ≤ c`
- **Description**: The octagon domain is a relational numerical abstract domain that can express constraints involving linear combinations of at most two variables with coefficients in {-1, 0, 1}.
- **Use cases**: General numerical analysis, overflow detection

### Other Domains
- **Intervals**: Represented by unary constraints
- **Polyhedra**: General convex constraints
- **Affine Equalities**: Linear equality relations
- **Congruences**: Modular arithmetic constraints
- **Polynomials**: Non-linear polynomial constraints

