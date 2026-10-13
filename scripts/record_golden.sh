#!/bin/sh
# Records the current tree lengths of all instances in data/instances as the expected values.
# Usage: scripts/record_golden.sh <flute executable> <lookup table> > data/golden_lengths.csv
set -eu
flute="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
lookup="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
root="$(cd "$(dirname "$0")/.." && pwd)"
echo "# instance (relative to data/), expected tree length with a full 8-pin lookup table"
cd "$root/data"
find instances -name '*.txt' | LC_ALL=C sort | while read -r instance; do
  length="$("$flute" solve "$instance" --lookup "$lookup" --quiet)"
  echo "$instance,$length"
done
