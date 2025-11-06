#ifndef PCOMB_MAIN_HEADER_H
#define PCOMB_MAIN_HEADER_H

// This is a header that pulls in all the headers for parsers and combinators
#include "Utils/General/pcomb/Parser/PredicateCharParser.h"
#include "Utils/General/pcomb/Parser/RegexParser.h"
#include "Utils/General/pcomb/Parser/StringParser.h"

#include "Utils/General/pcomb/Combinator/AltParser.h"
#include "Utils/General/pcomb/Combinator/SeqParser.h"
#include "Utils/General/pcomb/Combinator/ManyParser.h"
#include "Utils/General/pcomb/Combinator/TokenParser.h"
#include "Utils/General/pcomb/Combinator/ParserAdapter.h"
#include "Utils/General/pcomb/Combinator/LazyParser.h"
#include "Utils/General/pcomb/Combinator/LexemeParser.h"

#endif
