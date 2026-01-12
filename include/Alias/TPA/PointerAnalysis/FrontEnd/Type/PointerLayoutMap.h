#pragma once

#include "Alias/TPA/PointerAnalysis/FrontEnd/ConstPointerMap.h"

namespace llvm
{
	class Type;
}

namespace tpa
{

class PointerLayout;

using PointerLayoutMap = ConstPointerMap<llvm::Type, PointerLayout>;

}
