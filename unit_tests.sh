#!/bin/bash
cd "$(dirname "$0")/build" && ctest -R "Test$" --output-on-failure
