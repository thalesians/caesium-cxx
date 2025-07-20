#!/usr/bin/env python3
import matplotlib.pyplot as plt
import os
import sys

# Locate batch_sizes.txt in ../build relative to this script
data_path = os.path.join(os.path.dirname(__file__), '..', 'build', 'batch_sizes.txt')

if not os.path.isfile(data_path):
    print(f"Error: cannot find {data_path}", file=sys.stderr)
    sys.exit(1)

# Load batch sizes
sizes = []
with open(data_path) as f:
    for line in f:
        line = line.strip()
        if line.isdigit():
            sizes.append(int(line))

if not sizes:
    print(f"Error: no numeric data found in {data_path}", file=sys.stderr)
    sys.exit(1)

# Plot histogram
plt.figure()
plt.hist(sizes, bins=range(1, max(sizes) + 2))
plt.title("Distribution of Entries per Transaction")
plt.xlabel("Number of Entries")
plt.ylabel("Frequency")
plt.show()

# Summary statistics
total = len(sizes)
_min, _max = min(sizes), max(sizes)
_mean = sum(sizes) / total
print(f"Total transactions: {total}")
print(f"Min entries: {_min}, Max entries: {_max}, Mean entries: {_mean:.2f}")
