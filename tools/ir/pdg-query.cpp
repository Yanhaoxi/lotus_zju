/**
 * @file pdg-query.cpp
 * @brief Command-line tool for querying Program Dependence Graphs using Cypher
 *
 * This tool provides a command-line interface for executing Cypher queries
 * against Program Dependence Graphs. It supports both interactive and batch
 * modes for query execution.
 *
 * Cypher Query Examples:
 *   MATCH (n) RETURN n                          - Get all nodes
 *   MATCH (n:INST_FUNCALL) RETURN n             - Get all function call nodes
 *   MATCH (a)-[r]->(b) RETURN a, b              - Get nodes connected by edges
 *   MATCH (n:FUNC_ENTRY) WHERE n.name = 'main'  - Filter by properties
 */

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "IR/PDG/CypherQuery.h"
#include "IR/PDG/ProgramDependencyGraph.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

using namespace llvm;
using namespace pdg;

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input bitcode file>"),
                                          cl::init("-"),
                                          cl::value_desc("filename"));

static cl::opt<std::string>
    QueryString("query", "q", cl::desc("Execute a single Cypher query"),
                cl::value_desc("cypher_query"));

static cl::opt<std::string>
    QueryFile("query-file", "f", cl::desc("Execute Cypher queries from file"),
              cl::value_desc("filename"));

static cl::opt<bool> Interactive("interactive", "i",
                                 cl::desc("Run in interactive mode"));

static cl::opt<bool> Verbose("verbose", "v", cl::desc("Enable verbose output"));

static cl::opt<bool> Explain("explain", "e",
                             cl::desc("Show query execution plan"));

static cl::opt<int> Timeout("timeout", "t",
                            cl::desc("Query timeout in seconds (default: 30)"),
                            cl::init(30));

static cl::opt<std::string>
    TargetFunction("function", cl::desc("Target function for analysis"),
                   cl::value_desc("function_name"));

static cl::opt<int>
    ResultLimit("limit",
                cl::desc("Maximum number of results to return (default: 100)"),
                cl::init(100));

static cl::opt<std::string>
    OutputFormat("output-format",
                 cl::desc("Output format: text, json (default: text)"),
                 cl::init("text"));

static cl::opt<bool> ShowVersion("show-version",
                                 cl::desc("Show version information"));

void printVersion() {
  outs() << "PDG Cypher Query Tool v1.0\n";
  outs() << "Part of the Lotus Program Analysis Framework\n";
}

void printUsage(const char *programName) {
  printVersion();
  errs() << "\nUsage: " << programName << " [options] <input bitcode file>\n";
  errs() << "\nOptions:\n";
  errs() << "  -q, --query <query>       Execute a single Cypher query\n";
  errs() << "  -f, --query-file <file>   Execute queries from file\n";
  errs() << "  -i, --interactive         Run in interactive mode\n";
  errs() << "  -v, --verbose             Enable verbose output\n";
  errs() << "  -e, --explain             Show query execution plan\n";
  errs() << "  -t, --timeout <seconds>   Query timeout (default: 30)\n";
  errs() << "  --function <name>         Target function for analysis\n";
  errs() << "  --limit <num>             Maximum results (default: 100)\n";
  errs() << "  --output-format <format>  Output format: text, json\n";
  errs() << "  --version                 Show version\n";
  errs() << "\nCypher Query Examples:\n";
  errs() << "  MATCH (n) RETURN n                              # All nodes\n";
  errs()
      << "  MATCH (n:INST_FUNCALL) RETURN n                 # Function calls\n";
  errs() << "  MATCH (a)-[r]->(b) RETURN a, b                  # Connected "
            "nodes\n";
  errs()
      << "  MATCH (n:FUNC_ENTRY) WHERE n.name = 'main'      # Filtered nodes\n";
  errs() << "  MATCH (a)-[*]->(b) RETURN a, b                  # "
            "Variable-length paths\n";
}

void printPDGInfo(ProgramGraph &pdg) {
  outs() << "PDG Information:\n";
  outs() << "  Total nodes: " << pdg.numNode() << "\n";
  outs() << "  Total edges: " << pdg.numEdge() << "\n";
  outs() << "  Functions: " << pdg.getFuncWrapperMap().size() << "\n";
}

void printQueryStats(const CypherQuery &query, const CypherResult &result,
                     std::chrono::microseconds duration) {
  outs() << "Query completed in " << duration.count() << "µs\n";
  outs() << "Result: " << result.toString() << "\n";

  if (Explain) {
    outs() << "Query plan hints:\n";
    outs() << "  - Patterns: " << query.getPatterns().size() << "\n";
    outs() << "  - Has WHERE: " << (query.getWhereClause() ? "yes" : "no")
           << "\n";
    outs() << "  - Return items: " << query.getReturnItems().size() << "\n";
    if (query.getOrderBy()) {
      outs() << "  - Ordered by: " << query.getOrderBy()->getVariable() << "\n";
    }
    if (query.getLimit() > 0) {
      outs() << "  - Limited to: " << query.getLimit() << " results\n";
    }
  }
}

bool executeQuery(CypherQueryExecutor &executor, const std::string &queryStr) {
  if (Verbose) {
    outs() << "Executing query: " << queryStr << "\n";
  }

  auto start = std::chrono::high_resolution_clock::now();

  CypherParser parser;
  auto query = parser.parse(queryStr);

  if (!query) {
    const auto &error = parser.getLastError();
    errs() << "Parse error: " << error.message << "\n";
    if (error.line > 0 || error.column > 0) {
      errs() << "  at line " << error.line;
      if (error.column > 0) {
        errs() << ", column " << error.column;
      }
      errs() << "\n";
    }
    if (!error.suggestion.empty()) {
      errs() << "  Suggestion: " << error.suggestion << "\n";
    }
    return false;
  }

  if (Explain) {
    outs() << "Parsed query:\n";
    outs() << "  - Patterns: " << query->getPatterns().size() << "\n";
    outs() << "  - Return items: " << query->getReturnItems().size() << "\n";
    if (query->hasWhere()) {
      outs() << "  - Has WHERE clause\n";
    }
    if (query->hasLimit()) {
      outs() << "  - Limit: " << query->getLimit() << "\n";
    }
  }

  // Apply limit from command line if query doesn't have one
  if (!query->hasLimit() && ResultLimit > 0) {
    const_cast<CypherQuery *>(query.get())->setLimit(ResultLimit);
  }

  auto result = executor.execute(*query);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  if (result) {
    const auto &stats = executor.getLastStats();
    outs() << "Query completed in " << duration.count() << "µs\n";
    outs() << "Result: " << result->toString() << "\n";

    if (Verbose) {
      outs() << "Stats:\n";
      outs() << "  - Nodes visited: " << stats.nodesVisited << "\n";
      outs() << "  - Edges visited: " << stats.edgesVisited << "\n";
      outs() << "  - Results returned: " << stats.resultsReturned << "\n";
    }
    return true;
  } else {
    errs() << "Execution error: " << executor.getLastError() << "\n";
    return false;
  }
}

void runInteractiveMode(CypherQueryExecutor &executor) {
  outs() << "PDG Cypher Query Interactive Mode\n";
  outs() << "Type 'help' for commands, 'quit' to exit\n";
  outs() << "Cypher syntax: MATCH (n:Label) WHERE n.prop = 'value' RETURN n\n";
  outs() << "> ";

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      outs() << "> ";
      continue;
    }

    if (line == "quit" || line == "exit") {
      break;
    }

    if (line == "help") {
      outs() << "Commands:\n";
      outs() << "  help       Show this help message\n";
      outs() << "  quit       Exit interactive mode\n";
      outs() << "  info       Show PDG information\n";
      outs() << "  stats      Show query execution statistics\n";
      outs() << "  clear      Clear screen\n";
      outs() << "  <query>    Execute Cypher query\n";
      outs() << "\nNode Labels:\n";
      outs() << "  :INST_FUNCALL, :INST_RET, :INST_BR, :FUNC_ENTRY\n";
      outs() << "  :PARAM_FORMALIN, :PARAM_FORMALOUT, :FUNC, :CLASS\n";
      outs() << "\nRelationship Types:\n";
      outs() << "  :DATA_DEP, :CONTROL_DEP, :CALL_INV, :CALL_RET\n";
      outs() << "  :PARAM_IN, :PARAM_OUT\n";
    } else if (line == "info") {
      printPDGInfo(executor.getPDG());
    } else if (line == "stats") {
      outs() << "Use -v (verbose) flag for detailed statistics\n";
    } else if (line == "clear") {
      // Simple clear - just print newlines
      for (int i = 0; i < 50; ++i) {
        outs() << "\n";
      }
    } else {
      executeQuery(executor, line);
    }

    outs() << "> ";
  }
}

void runBatchMode(CypherQueryExecutor &executor, const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    errs() << "Error: Could not open file " << filename << "\n";
    return;
  }

  outs() << "Executing queries from: " << filename << "\n";

  std::string line;
  int queryCount = 0;
  int successCount = 0;

  while (std::getline(file, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Trim whitespace
    size_t start = line.find_first_not_of(" \t");
    size_t end = line.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
      continue;
    }
    line = line.substr(start, end - start + 1);

    outs() << "\nQuery " << (++queryCount) << ": " << line << "\n";
    outs() << "-----\n";

    if (executeQuery(executor, line)) {
      successCount++;
    }
  }

  outs() << "\nBatch execution complete: " << successCount << "/" << queryCount
         << " queries succeeded\n";
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "PDG Cypher Query Tool\n");

  if (ShowVersion) {
    printVersion();
    return 0;
  }

  if (InputFilename.empty()) {
    printUsage(argv[0]);
    return 1;
  }

  // Create LLVM context and load module
  LLVMContext Context;
  SMDiagnostic Err;

  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  outs() << "Loaded module: " << InputFilename << "\n";

  // Build PDG
  ProgramGraph &pdg = ProgramGraph::getInstance();
  pdg.build(*M);
  pdg.bindDITypeToNodes(*M);

  if (Verbose) {
    printPDGInfo(pdg);
  }

  // Create query executor
  CypherQueryExecutor executor(pdg);

  // Execute queries based on mode
  if (Interactive) {
    runInteractiveMode(executor);
  } else if (!QueryString.empty()) {
    executeQuery(executor, QueryString);
  } else if (!QueryFile.empty()) {
    runBatchMode(executor, QueryFile);
  } else {
    outs() << "No query specified. Use -q for a single query, -i for "
              "interactive mode, or -f for batch file.\n";
    outs() << "Example: " << argv[0] << " -q \"MATCH (n) RETURN n\" "
           << InputFilename << "\n";
    outs() << "\nAvailable node labels:\n";
    outs() << "  :INST_FUNCALL - Function call instructions\n";
    outs() << "  :INST_RET     - Return instructions\n";
    outs() << "  :INST_BR      - Branch instructions\n";
    outs() << "  :FUNC_ENTRY   - Function entry points\n";
    outs() << "  :PARAM_FORMALIN - Formal input parameters\n";
  }

  return 0;
}