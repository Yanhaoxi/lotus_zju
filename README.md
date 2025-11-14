<p align="center">
  <img src="docs/logo.jpg" alt="Lotus Logo" width="100"/>
</p>

# Lotus 

Lotus is a program analysis, verification, and optimization framework. It provides several toolkits that can be used individually or in combination to perform different tasks.
The current version has been tested on x86 Linux and ARM Mac using LLVM-12 and LLVM-14 with Z3-4.11.

## Publications

If you use Lotus in your research or work, please cite the following:

```bibtex
@misc{lotus2025,
  title = {Lotus: A Versatile and Industrial-Scale Program Analysis Framework},
  author = {ZJU Programming Languages and Automated Reasoning Group},
  year = {2025},
  url = {https://github.com/ZJU-Automated-Reasoning-Group/lotus},
  note = {Program analysis framework built on LLVM}
}
```

## Docs

~~~~
https://zju-automated-reasoning-group.github.io/lotus
~~~~

- **ISSTA 2025**: Program Analysis Combining Generalized Bit-Level and Word-Level Abstractions. Guangsheng Fan, Liqian Chen, Banghu Yin, Wenyu Zhang, Peisen Yao, and Ji Wang.  
- **S&P 2024**: Titan: Efficient Multi-target Directed Greybox Fuzzing.
Heqing Huang, Peisen Yao, Hung-Chun Chiu, Yiyuan Guo, and Charles Zhang.
- **USENIX Security 2024**: Unleashing the Power of Type-Based Call Graph Construction by Using Regional Pointer Information. Yuandao Cai, Yibo Jin, Charles Zhang. 
- **TSE 2024**: Fast and Precise Static Null Exception Analysis with Synergistic Preprocessing. Yi Sun, Chengpeng Wang, Gang Fan, Qingkai Shi, Xiangyu Zhang.  
- **OOPSLA 2022**: Indexing the Extended Dyck-CFL Reachability for Context-Sensitive Program Analysis. Qingkai Shi, Yongchao Wang, Peisen Yao, and Charles Zhang.

## Components

For a detailed list of subsystems (alias analyses, intermediate representations,
solvers, abstract interpreters, and utilities), see the
[Major Components Overview](https://zju-automated-reasoning-group.github.io/lotus/major_components.html)
section of the documentation. The docs provide links to the relevant libraries,
command-line tools, and example usage for each component.

## Binary Tools

For detailed documentation on using the binary tools, see [TOOLS.md](TOOLS.md).

## Installation

### Prerequisites

- LLVM 12.0.0 or 14.0.0
- Z3 4.11
- CMake 3.10+
- C++14 compatible compiler

### Build LLVM

```bash
# Clone LLVM repository
git clone https://github.com/llvm/llvm-project.git
cd llvm-project

# Checkout desired version
git checkout llvmorg-14.0.0  # or llvmorg-12.0.0

# Build LLVM
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ../llvm
make -j$(nproc)  # Uses all available CPU cores
```

### Build Lotus

```bash
git clone https://github.com/ZJU-Automated-Reasoning-Group/lotus
cd lotus
mkdir build && cd build
cmake ../ -DLLVM_BUILD_PATH=/path/to/llvm/build
make -j$(nproc)
```

**Note**: The build system currently assumes that the system has the correct version of Z3 installed.

The build system will automatically download and build Boost if it's not found on your system. You can specify a custom Boost installation path with `-DCUSTOM_BOOST_ROOT=/path/to/boost`.

> **TODO**: Implement automatic download of LLVM and Z3 dependencies

