#pragma once

#include "llvm/ADT/StringRef.h"
#include <memory>

namespace lotus {

class KVStore {
  class KVImpl;
  std::unique_ptr<KVImpl> Impl;
public:
  KVStore();
  ~KVStore();
  void hIncrBy(llvm::StringRef Key, llvm::StringRef Field, int Incr);
  bool hGet(llvm::StringRef Key, llvm::StringRef Field, std::string &Value);
  void hSet(llvm::StringRef Key, llvm::StringRef Field, llvm::StringRef Value);
};

}
