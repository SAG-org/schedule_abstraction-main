#!/bin/bash

# make sure the script is run from the root of the repository
if [ ! -d ".git" ]; then
  echo "This script must be run from the root of the repository."
  exit 1
fi

FILES=$(find . -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.c" -o -name "*.h" \) -not -path "./lib/*" -not -path "./build/*")
echo "+ Running clang-format on the following files:"
echo $FILES
clang-format -i $FILES