add_library(CanaryReport STATIC
    SARIF.cpp
    BugReport.cpp
    BugReportMgr.cpp
    BugTypes.cpp
    ReportOptions.cpp
)
target_link_libraries(CanaryReport CanaryLLVMUtils CanaryDebugInfo)