# Analysis of Concurrent Programs

## Utilities

- ThreadAPI
- ThreadFlowGraph
- FBVClock
- BVVClock

## Analyses 
- LockSetAnalysis: 
- HappensBeforeAnalysis: 
- MHPAnalysis: 
- StaticVectorClockMHP: 
- StaticThreadSharingAnalysis: 
- MemUseDefAnalysis: 
- EscapeAnalysis:


## Limitations

- Missing: Support for std::memory_order_relaxed, acquire, release, etc. The ThreadAPI maps atoms roughly to locks/barriers but doesn't handle the fine-grained "synchronizes-with" relationships of weak atomics
