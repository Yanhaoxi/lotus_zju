#pragma once

#include "Alias/TPA/PointerAnalysis/FrontEnd/ConstPointerMap.h"

namespace llvm
{
	class Type;
}

namespace tpa
{

class ArrayLayout;

using ArrayLayoutMap = ConstPointerMap<llvm::Type, ArrayLayout>;

}
