#include "Alias/OriginAA/ChannelSemantics.h"
#include "Alias/OriginAA/PointerAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "Alias/OriginAA/Util.h"
#include <regex>
#include "llvm/IR/Instructions.h"
#include "llvm/Demangle/Demangle.h"

using namespace llvm;

// not used for now
// these functions' callsite will be used to distinguish channel objects later
bool ChannelSemantics::isChannelCreateCall(std::string demangledName)
{
    // Check for exact mpsc channel creation functions
    return (demangledName == "std::sync::mpsc::channel" ||
            demangledName == "std::sync::mpsc::sync_channel" ||
            demangledName == "tokio::sync::mpsc::channel" ||
            demangledName == "tokio::sync::mpsc::unbounded_channel" ||
            // Also check for test/mock versions without full std path
            demangledName == "mpsc::channel");
}

bool ChannelSemantics::isSendCall(std::string demangledName)
{
    // Check for exact mpsc sender functions
    return (demangledName == "std::sync::mpsc::Sender::send" || demangledName == "std::sync::mpsc::Sender<T>::send" ||
            demangledName == "std::sync::mpsc::SyncSender::send" || demangledName == "std::sync::mpsc::SyncSender<T>::send" ||
            demangledName == "std::sync::mpsc::SyncSender::try_send" || demangledName == "std::sync::mpsc::SyncSender<T>::try_send" ||
            demangledName == "tokio::sync::mpsc::Sender::send" || demangledName == "tokio::sync::mpsc::Sender<T>::send" ||
            demangledName == "tokio::sync::mpsc::UnboundedSender::send" || demangledName == "tokio::sync::mpsc::UnboundedSender<T>::send" ||
            // Also check for test/mock versions without full std path
            demangledName == "mpsc::Sender::send");
}

bool ChannelSemantics::isRecvCall(std::string demangledName)
{
    // Check for exact mpsc receiver functions
    return (demangledName == "std::sync::mpsc::Receiver::recv" || demangledName == "std::sync::mpsc::Receiver<T>::recv" ||
            demangledName == "std::sync::mpsc::Receiver::try_recv" || demangledName == "std::sync::mpsc::Receiver<T>::try_recv" ||
            demangledName == "std::sync::mpsc::Receiver::recv_timeout" || demangledName == "std::sync::mpsc::Receiver<T>::recv_timeout" ||
            demangledName == "tokio::sync::mpsc::Receiver::recv" || demangledName == "tokio::sync::mpsc::Receiver<T>::recv" ||
            demangledName == "tokio::sync::mpsc::UnboundedReceiver::recv" || demangledName == "tokio::sync::mpsc::UnboundedReceiver<T>::recv" ||
            // Also check for test/mock versions without full std path
            demangledName == "mpsc::Receiver::recv");
}

// check if AI to string is equivalent to %_4 = alloca %"std::sync::mpmc::list::Channel<i32>", align 128,
// and AI is from function with demangled name std::sync::mpmc::channel
bool ChannelSemantics::isChannelAlloc(llvm::AllocaInst &AI)
{
    if (AI.getType()->isPointerTy() && AI.getAllocatedType()->isStructTy())
    {
        llvm::StringRef typeName = AI.getAllocatedType()->getStructName();
        std::string demangledName = getDemangledName(AI.getFunction()->getName().str());

        if (analysis->DebugMode)
            errs() << "Checking if alloc is a channel: " << AI << "\n\t" << typeName << "\n\t" << demangledName << "\n";

        if (typeName.startswith("std::sync::mpmc::list::Channel") && demangledName.find("std::sync::mpmc::channel") != std::string::npos)
        {
            return true; // This is a channel allocation
        }
    }
    return false;
}

ChannelInfo *ChannelSemantics::createChannelInfo(llvm::AllocaInst *channel_create, llvm::Node *channel_alloc)
{
    if (analysis->DebugMode)
        errs() << "Detected channel alloc: " << *channel_create << "\n";

    // check if this channel already exists
    auto it = channel2info.find(channel_alloc);
    if (it != channel2info.end())
    {
        if (analysis->DebugMode)
            errs() << "Channel already exists for: " << channel_alloc << "\n";

        // Channel already exists, return the existing info
        return it->second;
    }

    // Create a new ChannelInfo instance
    ChannelInfo *channel_info = new ChannelInfo(channel_create, channel_alloc);
    channel2info[channel_alloc] = channel_info;

    if (analysis->DebugMode)
    {
        errs() << "Creating new channel info for: " << channel_alloc->id << "\n";
        channel_info->print(errs());
        errs() << "\n";
    }

    return channel_info;
}

void ChannelSemantics::handleChannelOperation(llvm::CallBase &call, Context context)
{
    Function *func = call.getCalledFunction();
    if (!func)
        return;

    std::string demangledName = getDemangledName(func->getName().str());
    if (isSendCall(demangledName))
    {
        // Handle channel send operation
        handleChannelSend(call, context);
    }
    else if (isRecvCall(demangledName))
    {
        // Handle channel receive operation
        handleChannelRecv(call, context);
    }
    else if (isChannelCreateCall(demangledName))
    {
        // use this as the context to distinguish channel objects
        if (analysis->DebugMode)
            errs() << "Detected channel creation call: " << call << "\n";
    }
}

llvm::Node *ChannelSemantics::getChannelNode(llvm::Value *value, Context context)
{
    // Iterate all users of value and print GEPs that use it
    for (auto user : value->users())
    {
        if (auto *gep = dyn_cast<GetElementPtrInst>(user))
        {
            if (analysis->DebugMode)
            {
                errs() << "Found GEP using value: " << *gep << "\n";
            }
            // find the gep with indices [0,1] which is the channel pointer
            if (gep->getNumIndices() == 2)
            {
                auto idx_iter = gep->idx_begin();
                auto idx_end = gep->idx_end();
                // Ensure there are at least two indices
                if (std::distance(idx_iter, idx_end) == 2)
                {
                    auto idx1_val = dyn_cast<ConstantInt>(gep->getOperand(gep->getNumOperands() - 2));
                    auto idx2_val = dyn_cast<ConstantInt>(gep->getOperand(gep->getNumOperands() - 1));
                    if (idx1_val && idx2_val && idx1_val->getZExtValue() == 0 && idx2_val->getZExtValue() == 1)
                    {
                        // This is the channel pointer GEP, we can use it to find the channel node
                        if (analysis->DebugMode)
                        {
                            errs() << "Found channel pointer GEP: " << *gep << "\n";
                        }
                        // Get the channel node from the GEP
                        return analysis->getOrCreateNode(gep, context);
                    }
                }
            }
        }
    }
    return nullptr; // No channel node found
}

void ChannelSemantics::handleChannelSend(llvm::CallBase &call, Context context)
{
    if (analysis->DebugMode)
    {
        errs() << "Detected channel send: " << call << "\n";
    }

    // Handle send operation: tx.send(value).await
    // First operand is typically the sender (self)
    // Second operand is the data being sent, e.g.,
    //   %_18 = load i32, ptr %data, align 4, !dbg !29702, !noundef !28
    // ; invoke std::sync::mpsc::Sender<T>::send
    //   %14 = invoke { i32, i32 } @"_ZN3std4sync4mpsc15Sender$LT$T$GT$4send17h83acc09daea78d9fE"(ptr align 8 %_1, i32 %_18)
    assert(call.arg_size() == 2 && "Channel send must have exactly 2 arguments");
    Value *sender = call.getArgOperand(0); // self (sender)
    Value *data = call.getArgOperand(1);   // data being sent
    llvm::Node *channel_node = getChannelNode(sender, context);
    if (!channel_node)
    {
        if (analysis->DebugMode)
        {
            errs() << "No channel pointer GEP found for sender: " << *sender << "\n";
        }
        return;
    }

    Node *data_node = analysis->getOrCreateNode(data, context);
    ChannelOperation *send = new ChannelOperation(CHANNEL_SEND, &call, nullptr, channel_node, nullptr, data_node);
    send->data_type = data_node->value->getType(); // Set the data type for the send operation

    // store this send operation as a dangling operation and match its channel info later, since now pts is propably empty
    channel2DanglingOperations[channel_node] = send;

    if (analysis->DebugMode)
    {
        errs() << "Handling channel send: ";
        send->print(errs());
        errs() << "\n";
    }
}

void ChannelSemantics::handleChannelRecv(llvm::CallBase &call, Context context)
{
    if (analysis->DebugMode)
    {
        errs() << "Detected channel receive: " << call << "\n";
    }

    // Handle receive operation: rx.recv().await
    // First operand is typically the receiver (self), e.g.,
    // ; invoke std::sync::mpsc::Receiver<T>::recv
    // % 5 = invoke { i32, i32 } @"_ZN3std4sync4mpsc17Receiver$LT$T$GT$4recv17hf9501adc9acff3d3E"(ptr align 8 % _1)
    //   to label %bb1 unwind label %cleanup, !dbg !29713
    assert(call.arg_size() == 1 && "Channel receive must have exactly 1 argument");
    Value *receiver = call.getArgOperand(0); // self (receiver)
    llvm::Node *channel_node = getChannelNode(receiver, context);
    if (!channel_node)
    {
        if (analysis->DebugMode)
        {
            errs() << "No channel pointer GEP found for receiver: " << *receiver << "\n";
        }
        return;
    }

    ChannelOperation *recv = new ChannelOperation(CHANNEL_RECV, nullptr, &call, nullptr, channel_node, nullptr);

    // store this send operation as a dangling operation and match its channel info later, since now pts is propably empty
    channel2DanglingOperations[channel_node] = recv;

    if (analysis->DebugMode)
    {
        errs() << "Handling channel receive: ";
        recv->print(errs());
        errs() << "\n";
    }
}

bool ChannelSemantics::matchOperation(llvm::Node *channel_node, ChannelOperation *op)
{
    // match the channel node with the channel info
    for (auto &pair : channel2info)
    {
        llvm::Node *channel = pair.first;

        if (DebugMode)
            errs() << "Checking channel: " << channel->id << " against " << *channel_node << "\n";

        if (channel_node->pts.count(channel->id) > 0)
        {
            // If the op node's points-to set contains this channel, add the op operation
            // to the channel's op operation
            ChannelInfo *channel_info = pair.second;
            if (op->isSend())
            {
                if (!channel_info->send_op)
                {
                    if (analysis->DebugMode)
                    {
                        errs() << "Adding send operation to channel: " << channel->id << "\n";
                    }
                    channel_info->send_op = op; // Set the send operation for this channel
                    return true;
                }
                else
                {
                    // If a send operation already exists, we can choose to either ignore or log it
                    // For now, we will just log it
                    errs() << "Warning: Multiple send operations detected for channel: " << channel->id << "\n";
                }
            }
            else if (op->isRecv())
            {
                if (!channel_info->recv_op)
                {
                    if (analysis->DebugMode)
                    {
                        errs() << "Adding recv operation to channel: " << channel->id << "\n";
                    }
                    channel_info->recv_op = op; // Set the recv operation for this channel
                    return true;
                }
                else
                {
                    // If a recv operation already exists, we can choose to either ignore or log it
                    // For now, we will just log it
                    errs() << "Warning: Multiple recv operations detected for channel: " << channel->id << "\n";
                }
            }
        }
        else
        {
            if (analysis->DebugMode)
                errs() << "\t Channel node " << channel_node->id << " does not match channel " << channel->id << "\n";
        }
    }
    return false; // No matching channel info found for the operation
}

void ChannelSemantics::matchDanglingOperations(llvm::Node *channel_node)
{
    auto it = channel2DanglingOperations.find(channel_node);
    if (it != channel2DanglingOperations.end())
    {
        // We have dangling operations for this channel node, try to match them
        ChannelOperation *dangling_op = it->second;
        if (analysis->DebugMode)
        {
            errs() << "Matching dangling operation for channel node: " << *channel_node << "\n";
        }
        if (matchOperation(channel_node, dangling_op))
        {
            // Remove the dangling operation after matching
            if (analysis->DebugMode)
            {
                errs() << "Successfully matched dangling operation: ";
                dangling_op->print(errs());
                errs() << "\n";
            }
            channel2DanglingOperations.erase(it);
        }
    }
}

// no need to do so, result passing chain is good
bool ChannelSemantics::applyChannelConstraints()
{
    // // Apply channel-specific constraints to the pointer analysis
    // for (auto &pair : channel2info)
    // {
    //     ChannelInfo *channel_info = pair.second;
    //     ChannelOperation *send_op = channel_info->send_op;
    //     ChannelOperation *recv_op = channel_info->recv_op;
    //     if (send_op && recv_op)
    //     {
    //         // Create a constraint that data flows from sender to receiver
    //         // Add to worklist as an assignment constraint
    //         analysis->Worklist.push_back({Assign, send_op->sender_node->id, recv_op->receiver_node->id});

    //         if (analysis->DebugMode)
    //         {
    //             errs() << "Adding channel constraint: " << send_op->sender_node->id << " -> " << recv_op->receiver_node->id << "\n";
    //         }
    //     }
    // }
    return false;
}

void ChannelSemantics::printChannelInfo(llvm::raw_ostream &os)
{
    if (channel2info.empty())
    {
        os << "=== No Channel Found ===\n";
        return;
    }

    // Print channel semantics analysis summary
    os << "=== Channel Semantics Analysis ===\n";
    os << "# = " << channel2info.size() << "\n";
    for (auto &pair : channel2info)
    {
        ChannelInfo *channel = pair.second;
        channel->print(os);
    }
    os << "\n";

    // Print dangling operations
    if (channel2DanglingOperations.empty())
    {
        os << "=== No dangling operations found ===\n";
    }
    else
    {
        os << "=== Dangling Operations ===\n # =" << channel2DanglingOperations.size() << "\n";
        for (auto &pair : channel2DanglingOperations)
        {
            llvm::Node *channel_node = pair.first;
            os << "Channel Node: " << channel_node->id << "\n";
            ChannelOperation *op = pair.second;
            os << "\tDangling Operation: \n\t";
            op->print(os);
        }
    }
}

void llvm::ChannelOperation::print(llvm::raw_ostream &os) const
{
    os << "ChannelOperation: " << (operation == CHANNEL_SEND ? "Send: " : "Recv: ");
    if (operation == CHANNEL_SEND && sender_value)
    {
        sender_value->print(os);
        os << "\n\t -> data: ";
        data_node->print(os);
        os << "\n\t -> data type: ";
        if (data_type)
            data_type->print(os);
    }
    else
        os << "null";
    if (operation == CHANNEL_RECV && receiver_value)
        receiver_value->print(os);
    else
        os << "null";
}

void llvm::ChannelInfo::print(llvm::raw_ostream &os) const
{
    os << "ChannelInfo:\n";
    os << "  Creation Call: ";
    if (creation_call)
        creation_call->print(os);
    else
        os << "null";
    os << "\n  Channel Node: ";
    if (channel)
        channel->print(os); // Now legal, full definition is available
    else
        os << "null";
    os << "\n  Send Operation: ";
    if (send_op)
        send_op->print(os);
    else
        os << "null";
    os << "\n  Receive Operation: ";
    if (recv_op)
        recv_op->print(os);
    else
        os << "null";
}

// Overload operator<< for ChannelInfo as a free function
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::ChannelInfo &node)
{
    node.print(os);
    return os;
}

// Overload operator<< for ChannelOperation as a free function
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::ChannelOperation &node)
{
    node.print(os);
    return os;
}