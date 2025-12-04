/*
* Optimizaiton Modulo Theories (OMT) based Analyzer
* 
* The boxed (multiple independent objectives) OMT problem is to find the maximum values of f1, f2,...
* subject to a constraint P.
* (We do not need to find a single model, but instead we can find mulitple models that maximizes f1, f2, ..., respectively.)
*
*  For many template domains (interval, zeones, etc), we can reduce the symbolic abstraction problem to a boxed OMT problem.
*
*  TBD: how to support abstract domains that do not naturally fit into the template-based category, e.g., affine, constant propagation, ...?  
* 
*/


#include "Analysis/Sprattus/Analyzer.h"

#include "Analysis/Sprattus/Utils.h"
#include "Analysis/Sprattus/ValueMapping.h"
#include "Analysis/Sprattus/repr.h"
#include "Analysis/Sprattus/Config.h"

