#!/bin/bash

# Define the path to the git blame ignore file
IGNORE_REVS_FILE=".git-blame-ignore-revs"

# make sure the script is run from the root of the repository
if [ ! -d ".git" ]; then
  echo "This script must be run from the root of the repository."
  exit 1
fi

# Ensure the file exists
if [ ! -f "$IGNORE_REVS_FILE" ]; then
  touch "$IGNORE_REVS_FILE"
  echo "Created .git-blame-ignore-revs file."
fi

# Add any commit with "chore: auto-format code" in the commit message to .git-blame-ignore-revs
# We will get the commit hashes of all commits with the message containing "chore: auto-format code"
COMMIT_HASHES=$(git log --grep="chore: auto-format code" --format="%H")

# If there are any matching commits, append their hashes to .git-blame-ignore-revs
if [ ! -z "$COMMIT_HASHES" ]; then
  for commit_hash in $COMMIT_HASHES; do
    # Check if the commit hash is already in the ignore file
    if ! grep -q "$commit_hash" "$IGNORE_REVS_FILE"; then
      echo "$commit_hash" >> "$IGNORE_REVS_FILE"
      echo "Added commit $commit_hash to $IGNORE_REVS_FILE"
    else
      echo "Commit $commit_hash already exists in $IGNORE_REVS_FILE"
    fi
  done
else
  echo "No 'chore: auto-format code' commits found to add."
fi

# Set global git config to use the ignore file for blame
git config --global blame.ignoreRevsFile .git-blame-ignore-revs
echo "Configured git to ignore reformatting commits for blame."

echo "Formatting and commit update complete."