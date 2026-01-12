#!/bin/bash
cd "$(dirname "$0")/build/tests" && ctest -R "_test$" --output-on-failure
