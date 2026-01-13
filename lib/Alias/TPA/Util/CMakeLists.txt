set (UtilCodes
	CommandLine/CommandLineParser.cpp
	CommandLine/TypedCommandLineParser.cpp
	IO/ReadIR.cpp
	IO/WriteIR.cpp
)

add_library (TPAUtil STATIC ${UtilCodes})