

#include "Alias/AllocAA/MemorySite.h"



AllocAAResult MemorySiteInfo::doesAlias(Value *V1, Value *V2) {

  /*
   * One or both values is not understood
   */
  auto ref1 = referenceSites.find(V1);
  if (ref1 == referenceSites.end())
    return AllocAAResult::May;
  auto ref2 = referenceSites.find(V2);
  if (ref2 == referenceSites.end())
    return AllocAAResult::May;

  /*
   * Both values escape
   */
  auto* site1 = ref1->second;
  auto* site2 = ref2->second;
  if (site1 == site2)
    return AllocAAResult::Must;
  if (site1->escapingValues.size() > 0 && site2->escapingValues.size() > 0)
    return AllocAAResult::May;

  /*
   * The values are not known to reference each other,
   * and at least one site is fully understood. That
   * ensures the two values do not alias
   */
  return AllocAAResult::No;
}

