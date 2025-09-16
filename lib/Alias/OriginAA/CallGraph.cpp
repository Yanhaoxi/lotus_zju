#include "Alias/OriginAA/CallGraph.h"
#include <fstream>

using namespace llvm;

CGNode CallGraph::getOrCreateNode(llvm::Function *func, Context ctx)
{
    auto key = std::make_pair(func, ctx);
    auto it = ValueContextToNodeMap.find(key);
    if (it != ValueContextToNodeMap.end())
    {
        return it->second;
    }
    CGNode node = CGNode(nextNodeId++, func, ctx);

    // errs() << "Creating CGNode: id=" << node.id
    //        << ", function=" << (func ? func->getName() : "null")
    //        << ", context=" << ctx << "\n";

    auto found = idToNodeMap.find(node.id);
    assert(found == idToNodeMap.end() &&
           ("Node ID already exists in idToNodeMap: id=" + std::to_string(node.id) +
            ", existing function=" + (found != idToNodeMap.end() && found->second.function ? found->second.function->getName().str() : "null"))
               .c_str());

    idToNodeMap[node.id] = node;
    ValueContextToNodeMap[key] = node;
    return node;
}

void CallGraph::printCG(std::ofstream &outFile) const
{
    outFile << "\n\n\n\nCall Graph:\n";
    for (const auto &entry : graph_)
    {
        // entry.first is now a CGNode, entry.second is a set of CGNode
        const auto caller = getNode(entry.first);
        std::string callerStr;
        llvm::raw_string_ostream callerStream(callerStr);
        caller.print(callerStream);
        callerStream.flush();
        outFile << "Caller: " << callerStr << "\n";

        for (const auto &callee_id : entry.second)
        {
            const auto callee = getNode(callee_id);
            std::string calleeStr;
            llvm::raw_string_ostream rso(calleeStr);
            rso << callee;
            rso.flush();
            outFile << "  -> Callee: " << calleeStr << "\n";
        }
    }
}
