#pragma once

#include <memory>

namespace llvm
{
	class Module;
} // namespace llvm

namespace util
{
namespace io
{

std::unique_ptr<llvm::Module> readModuleFromFile(const char* fileName);
std::unique_ptr<llvm::Module> readModuleFromString(const char *assembly);

} // namespace io
} // namespace util
