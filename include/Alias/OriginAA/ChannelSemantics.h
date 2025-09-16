#ifndef CHANNEL_SEMANTICS_H
#define CHANNEL_SEMANTICS_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "Alias/OriginAA/CallGraph.h"
#include "Alias/OriginAA/PointerAnalysis.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace llvm
{
    // Forward declare PointerAnalysis and Node to avoid circular dependencies
    class PointerAnalysis;
    struct Node;

    enum ChannelOpType
    {
        CHANNEL_CREATE,
        CHANNEL_SEND,
        CHANNEL_RECV,
        INVALID // Represents an unrecognized channel operation
    };

    // Represents a channel operation (send/recv/creation)
    struct ChannelOperation
    {
        ChannelOpType operation;
        llvm::CallBase *sender_value;   // The sender call
        llvm::CallBase *receiver_value; // The receiver call
        llvm::Node *sender_node;        // Node for sender endpoint, the pointer node to locate channel in pointer analysis
        llvm::Node *receiver_node;      // Node for receiver endpoint, the pointer node to locate channel in pointer analysis

        llvm::Type *data_type; // Type of data being transmitted, together with sender_value and receiver_value
        llvm::Node *data_node; // Node representing the data type in pointer analysis

        ChannelOperation(ChannelOpType op, llvm::CallBase *sender, llvm::CallBase *receiver,
                         llvm::Node *sender_node, llvm::Node *receiver_node, llvm::Node *data_node)
            : operation(op), sender_value(sender), receiver_value(receiver), sender_node(sender_node), receiver_node(receiver_node), data_node(data_node) {}

        bool operator==(const ChannelOperation &other) const
        {
            return operation == other.operation &&
                   sender_value == other.sender_value &&
                   receiver_value == other.receiver_value &&
                   sender_node == other.sender_node &&
                   receiver_node == other.receiver_node &&
                   data_node == other.data_node;
        }

        void print(llvm::raw_ostream &os) const;

        bool isSend() const
        {
            return operation == CHANNEL_SEND;
        }

        bool isRecv() const
        {
            return operation == CHANNEL_RECV;
        }
    };

    // Represents a complete channel instance including creation and both endpoints
    // TODO: multi receivers or senders, ROCs
    struct ChannelInfo
    {
        llvm::AllocaInst *creation_call; // The channel creation instruction,
        // e.g., we use %_4 = alloca %"std::sync::mpmc::list::Channel<i32>", align 128 from std::sync::mpsc::channel
        // %_6 = alloca %"std::sync::mpmc::zero::Channel<i32>", align 8 from std::sync::mpmc::sync_channel
        //      where %"std::sync::mpmc::list::Channel<i32>" = type { %"core::marker::PhantomData<i32>", %"std::sync::mpmc::utils::CachePadded<std::sync::mpmc::list::Position<i32>>", %"std::sync::mpmc::utils::CachePadded<std::sync::mpmc::list::Position<i32>>", %"std::sync::mpmc::waker::SyncWaker", [8 x i64] }
        llvm::Node *channel;       // Node representing the channel in pointer analysis
        ChannelOperation *send_op; // Send operation details
        ChannelOperation *recv_op; // Receive operation details

        llvm::Node *channelPtr; // Node representing the channel in pointer analysis
        ChannelInfo(llvm::AllocaInst *creation, llvm::Node *channel)
            : creation_call(creation), channel(channel), send_op(nullptr), recv_op(nullptr) {}

        bool operator==(const ChannelInfo &other) const
        { // Compare creation call and channel node, should be enough
            return creation_call == other.creation_call &&
                   channel == other.channel;
        }

        void print(llvm::raw_ostream &os) const;
    };

    // Channel semantics analyzer
    class ChannelSemantics
    {
    public:
        ChannelSemantics(PointerAnalysis *analysis = nullptr)
            : analysis(analysis) {}

        // Maps to track channel relationships
        std::unordered_map<llvm::Node *, ChannelInfo *> channel2info; // Maps sender/receiver objects to channel info
        std::unordered_map<llvm::Node *, ChannelOperation *> channel2DanglingOperations; // base node to unmatched operations, e.g., send/recv without info

        bool isChannelAlloc(llvm::AllocaInst &AI);
        ChannelInfo *createChannelInfo(llvm::AllocaInst *channel_create, llvm::Node *channel_alloc);
        void handleChannelOperation(llvm::CallBase &call, Context context = Everywhere());
        void handleChannelSend(llvm::CallBase &call, Context context = Everywhere());
        void handleChannelRecv(llvm::CallBase &call, Context context = Everywhere());

        bool matchOperation(llvm::Node *channel_node, ChannelOperation *op);
        void matchDanglingOperations(llvm::Node *channel_node);

        // Apply channel-specific constraints to pointer analysis
        bool applyChannelConstraints();

        // Debug printing
        void printChannelInfo(llvm::raw_ostream &os);

    private:
        PointerAnalysis *analysis = nullptr;                                             // Pointer analysis instance for this semantics

        // Helper functions to identify channel types and operations
        bool isChannelCreateCall(std::string demangledName);
        bool isSendCall(std::string demangledName);
        bool isRecvCall(std::string demangledName);

        // to get the channel node from the call to send/recv
        llvm::Node *getChannelNode(llvm::Value *value, Context context = Everywhere());
    };

} // namespace llvm

#endif // CHANNEL_SEMANTICS_H