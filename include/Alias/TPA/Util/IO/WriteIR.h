#pragma once

namespace llvm
{
	class Module;
} // namespace llvm

namespace util
{
namespace io
{

void writeModuleToText(const llvm::Module& module, const char* fileName);
void writeModuleToBitCode(const llvm::Module& module, const char* fileName);
void writeModuleToFile(const llvm::Module& module, const char* fileName, bool isText);

} // namespace io
} // namespace util