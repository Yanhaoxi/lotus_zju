#include "Analysis/Sprattus/ResultStore.h"
#include "Analysis/Sprattus/AbstractValue.h"
#include "Analysis/Sprattus/repr.h"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include <sstream>

#ifdef ENABLE_DYNAMIC
using namespace cereal;

#define ID_WIDTH 32

namespace sprattus
{
void ResultStore::Key::prepareDBT()
{
    memset(&DBT_, 0, sizeof(DBT));
    DBT_.data = &ID_;
    DBT_.size = sizeof(ID_);
}

ResultStore::Key::Key(uint32_t id) : ID_(id)
{
    assert(!(id & (1 << (ID_WIDTH - 1))) && "ID range exceeded!");
    prepareDBT();
}

ResultStore::Key::Key(const llvm::Function& function,
                      llvm::BasicBlock* location, bool sound)
{
    const llvm::Module* module = function.getParent();

    ID_ = 0;
    for (auto& f : *module) {
        if (&f == &function)
            break;
        ID_ += f.size() + 1;
    }

    if (location != nullptr) {
        ID_ += 1; // slot 0 is reserved for Fragment::EXIT
        for (auto& bb : function) {
            if (&bb == location)
                break;
            ID_++;
        }
    }

    assert(!(ID_ & (1 << (ID_WIDTH - 1))) && "ID range exceeded!");

    ID_ |= (sound << (ID_WIDTH - 1));

    prepareDBT();
}

ResultStore::Key::Key(llvm::BasicBlock& bb, bool sound)
    : Key(*bb.getParent(), &bb, sound)
{
}

ResultStore::ResultStore(const std::string& filename) { initDB(filename); }

ResultStore::ResultStore(ResultStore&& other) { *this = std::move(other); }

ResultStore& ResultStore::operator=(ResultStore&& other)
{
    DBP_ = other.DBP_;
    Cache_ = std::move(other.Cache_);
    return *this;
}

void ResultStore::serialize(const AbstractValue& avalue, std::ostream& out)
{
    UserDataAdapter<ResultStore, BinaryOutputArchive> archive(*this, out);
    unique_ptr<AbstractValue> avalue_copy(avalue.clone());
    archive(avalue_copy);
}

unique_ptr<AbstractValue> ResultStore::deserialize(std::istream& in,
                                                   const FunctionContext& fctx)
{
    UserDataAdapter<FunctionContext, BinaryInputArchive> archive(
        const_cast<FunctionContext&>(fctx), in);

    unique_ptr<AbstractValue> result;
    archive(result);
    return result;
}

unique_ptr<AbstractValue> ResultStore::get(const Key& key_arg,
                                           const FunctionContext& fctx)
{
    Key key = key_arg; // copy since DBP_->get() needs a non-const pointer

    DBT value;
    memset(&value, 0, sizeof(DBT));
    value.flags = DB_DBT_MALLOC;
    int ret = DBP_->get(DBP_, nullptr, key, &value, 0);

    if (ret == DB_NOTFOUND)
        return nullptr;

    std::istringstream in_stream(std::string((char*)value.data, value.size));
    free(value.data);
    return deserialize(in_stream, fctx);
}

void ResultStore::put(const Key& key_arg, const AbstractValue& avalue)
{
    Key key = key_arg; // copy since DBP_->put needs a non-const pointer

    DBT value;
    std::ostringstream out;
    serialize(avalue, out);
    std::string str_value = out.str();
    memset(&value, 0, sizeof(DBT));
    value.data = (void*)str_value.c_str();
    value.size = str_value.length();
    int ret = DBP_->put(DBP_, nullptr, key, &value, 0);
    assert(ret == 0);
}

ResultStore::~ResultStore() { DBP_->close(DBP_, 0); }
} // namespace sprattus
#endif /* ENABLE_DYNAMIC */
