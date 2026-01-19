# Cypher Query Language Examples for PDG

This directory contains examples demonstrating how to query Program Dependence Graphs (PDG) using Cypher, a graph query language that allows you to analyze program dependencies and security properties.

## Files

- `test-program.c` - A C program with various patterns for analysis
- `example-queries.txt` - Example Cypher queries and policies
- `README.md` - This file

## Building and Running

1. Compile the test program to LLVM bitcode:
   ```bash
   clang -emit-llvm -S -g test-program.c -o test-program.bc
   ```

2. Build the PDG query tool:
   ```bash
   cd /path/to/lotus
   mkdir build && cd build
   cmake ..
   make pdg-query
   ```

3. Run queries:
   ```bash
   # Interactive mode
   ./build/bin/pdg-query -i test-program.bc
   
   # Single query
   ./build/bin/pdg-query -q "MATCH (n) RETURN n" test-program.bc
   
   # Batch queries from file
   ./build/bin/pdg-query -f example-queries.txt test-program.bc
   ```

## Cypher Query Language Syntax

The PDG query tool uses Cypher, a declarative graph query language. Here are the basic patterns:

### Basic Node Queries

- `MATCH (n) RETURN n` - Get all nodes in the PDG
- `MATCH (n:INST_FUNCALL) RETURN n` - Get all function call nodes
- `MATCH (n:FUNC_ENTRY) WHERE n.name = 'main' RETURN n` - Get nodes matching a condition

### Edge Queries

- `MATCH ()-[r:DATA_DEF_USE]->() RETURN r` - Get all data definition-use edges
- `MATCH (a)-[r]->(b) RETURN a, b` - Get all connected node pairs
- `MATCH (a)-[r:CONTROLDEP_BR]->(b) RETURN a, b` - Get control dependency edges

### Path Queries

- `MATCH path = (start)-[*]->(end) RETURN path` - Find all paths between nodes
- `MATCH path = shortestPath((start)-[*]->(end)) RETURN path` - Find shortest path
- `MATCH (n)-[*]->(m) RETURN DISTINCT m` - Forward slice (all reachable nodes)
- `MATCH (m)-[*]->(n) RETURN DISTINCT m` - Backward slice (all nodes that can reach)

### Function-Specific Queries

- **Return values of a function:**
  ```cypher
  MATCH (n:FUNC_ENTRY)-[:PARAMETER_OUT]->(ret:INST_RET)
  WHERE n.name = 'main'
  RETURN ret
  ```

- **Formal parameters of a function:**
  ```cypher
  MATCH (n:FUNC_ENTRY)-[:PARAMETER_IN]->(param:PARAM_FORMALIN)
  WHERE n.name = 'main'
  RETURN param
  ```

- **Function entry points:**
  ```cypher
  MATCH (n:FUNC_ENTRY)
  WHERE n.name = 'main'
  RETURN n
  ```

### Set Operations

- **Union:** Use `UNION` keyword
  ```cypher
  MATCH (a) RETURN a
  UNION
  MATCH (b) RETURN b
  ```

- **Intersection:** Use `WHERE EXISTS` subquery
  ```cypher
  MATCH (n)
  WHERE EXISTS {
    MATCH (m)
    WHERE n.id = m.id AND m:INST_BR
  }
  RETURN n
  ```

- **Difference:** Use `WHERE NOT`
  ```cypher
  MATCH (calls:INST_FUNCALL)
  WHERE NOT (calls:INST_BR)
  RETURN calls
  ```

## Example Queries

### Basic Analysis

```cypher
# Get all nodes in the PDG
MATCH (n) RETURN n

# Get all function call nodes
MATCH (n:INST_FUNCALL) RETURN n

# Get all data dependency edges
MATCH ()-[r:DATA_DEF_USE]->() RETURN r

# Get return values of main function
MATCH (n:FUNC_ENTRY)-[:PARAMETER_OUT]->(ret:INST_RET)
WHERE n.name = 'main'
RETURN ret
```

### Information Flow Analysis

```cypher
# Check for direct flows from input to output
MATCH path = (input:FUNC_ENTRY)-[:PARAMETER_OUT]->(inputRet:INST_RET)-[*]->(output:FUNC_ENTRY)-[:PARAMETER_IN]->(outputParam:PARAM_FORMALIN)
WHERE input.name = 'getInput' AND output.name = 'printOutput'
RETURN path

# Check for flows from secret to network
MATCH path = (secret:FUNC_ENTRY)-[:PARAMETER_OUT]->(secretRet:INST_RET)-[*]->(network:FUNC_ENTRY)-[:PARAMETER_IN]->(networkParam:PARAM_FORMALIN)
WHERE secret.name = 'getSecret' AND network.name = 'networkSend'
RETURN path

# Forward slice from user input
MATCH (input:FUNC_ENTRY)-[:PARAMETER_OUT]->(inputRet:INST_RET)
WHERE input.name = 'getInput'
MATCH (inputRet)-[*]->(n)
RETURN n

# Backward slice to output
MATCH (output:FUNC_ENTRY)-[:PARAMETER_IN]->(outputParam:PARAM_FORMALIN)
WHERE output.name = 'printOutput'
MATCH (n)-[*]->(outputParam)
RETURN n
```

### Security Policies

```cypher
# No explicit flows from secret to output
MATCH path = (secret:FUNC_ENTRY)-[:PARAMETER_OUT]->(secretRet:INST_RET)-[*]->(output:FUNC_ENTRY)-[:PARAMETER_IN]->(outputParam:PARAM_FORMALIN)
WHERE secret.name = 'getSecret' AND output.name = 'printOutput'
RETURN path

# No flows from input to network without sanitization
MATCH (sources:FUNC_ENTRY)-[:PARAMETER_OUT]->(sourceRet:INST_RET)
WHERE sources.name = 'getInput'
MATCH (sinks:FUNC_ENTRY)-[:PARAMETER_IN]->(sinkParam:PARAM_FORMALIN)
WHERE sinks.name = 'networkSend'
MATCH (sanitizers:FUNC_ENTRY)-[:PARAMETER_OUT]->(sanitizerRet:INST_RET)
WHERE sanitizers.name = 'sanitize'
MATCH path = (sourceRet)-[*]->(sinkParam)
WHERE NOT EXISTS {
  MATCH (sourceRet)-[*]->(sanitizerRet)-[*]->(sinkParam)
}
RETURN path

# Access control: sensitive operations only when authorized
MATCH (auth:FUNC_ENTRY)-[:PARAMETER_OUT]->(authRet:INST_RET)
WHERE auth.name = 'isAuthorized'
MATCH (authRet)-[:CONTROLDEP_BR]->(check)
MATCH (sensitiveOps:INST_FUNCALL)
WHERE NOT EXISTS {
  MATCH (check)-[:CONTROLDEP_BR]->(sensitiveOps)
}
RETURN sensitiveOps
```

### Complex Queries

```cypher
# Find all paths from input to output through sanitization
MATCH (input:FUNC_ENTRY)-[:PARAMETER_OUT]->(inputRet:INST_RET)
WHERE input.name = 'getInput'
MATCH (output:FUNC_ENTRY)-[:PARAMETER_IN]->(outputParam:PARAM_FORMALIN)
WHERE output.name = 'printOutput'
MATCH (sanitized:FUNC_ENTRY)-[:PARAMETER_OUT]->(sanitizedRet:INST_RET)
WHERE sanitized.name = 'sanitize'
MATCH path1 = (inputRet)-[*]->(sanitizedRet)
MATCH path2 = (sanitizedRet)-[*]->(outputParam)
RETURN path1, path2

# Find nodes that are both data and control dependent
MATCH (dataNodes:INST_FUNCALL)
WHERE EXISTS {
  MATCH (dataNodes)-[:CONTROLDEP_BR]->()
}
RETURN dataNodes
```

## Node Types (Labels)

- `INST_FUNCALL` - Function call instructions
- `INST_RET` - Return instructions
- `INST_BR` - Branch instructions
- `INST_OTHER` - Other instructions
- `FUNC_ENTRY` - Function entry points
- `PARAM_FORMALIN` - Formal input parameters
- `PARAM_FORMALOUT` - Formal output parameters
- `PARAM_ACTUALIN` - Actual input parameters
- `PARAM_ACTUALOUT` - Actual output parameters
- `FUNC` - Function nodes
- `CLASS` - Class nodes

## Edge Types (Relationship Types)

- `DATA_DEF_USE` - Data definition-use edges
- `DATA_RAW` - Read-after-write edges
- `DATA_READ` - Read edges
- `DATA_ALIAS` - Alias edges
- `CONTROLDEP_BR` - Control dependency from branches
- `CONTROLDEP_ENTRY` - Control dependency from entry
- `PARAMETER_IN` - Parameter input edges
- `PARAMETER_OUT` - Parameter output edges
- `GLOBAL_DEP` - Global dependency edges

## Security Analysis Examples

The test program contains several security-relevant patterns:

1. **Direct Information Flow**: Input flows directly to output
2. **Controlled Access**: Secret only accessed when authorized
3. **Sanitized Flow**: Input sanitized before network transmission
4. **Uncontrolled Secret Flow**: Secret leaked based on input
5. **Sensitive Data Logging**: Secret always logged
6. **Complex Control Flow**: Authorization checks with data processing

Use Cypher queries to detect these patterns and verify security properties.
