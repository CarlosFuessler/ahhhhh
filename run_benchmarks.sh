#!/bin/bash

# run_benchmarks.sh
# Compiles and executes Fibonacci(35) benchmark in C, Python, and ahhhhh,
# then presents a clean, formatted comparison table.

echo "==========================================="
echo "  Running Fibonacci(35) Benchmarks...      "
echo "==========================================="
echo ""

# 1. Compile C binary
echo "[1/4] Compiling C benchmark..."
clang -O3 fib.c -o fib_c

# 2. Verify ahhhhh binary exists, compile if missing
if [ ! -f "./zig-out/bin/ahhhhh" ]; then
    echo "[2/4] Building ahhhhh binary..."
    zig build -Doptimize=ReleaseFast
else
    echo "[2/4] ahhhhh binary found, using existing."
fi

# 3. Run benchmarks and capture execution times
echo "[3/4] Running C benchmark..."
c_out=$(./fib_c)
c_time=$(echo "$c_out" | grep "Time taken" | awk '{print $3}')

echo "[4/4] Running Python benchmark..."
py_out=$(python3 fib.py)
py_time=$(echo "$py_out" | grep "Time taken" | awk '{print $3}')

echo "[5/5] Running ahhhhh benchmark..."
ah_out=$(./zig-out/bin/ahhhhh fib.ahhhh)
ah_time=$(echo "$ah_out" | grep "Time taken" | awk '{print $3}')

# Calculate performance ratio relative to C using awk
py_ratio=$(awk -v t1="$py_time" -v t2="$c_time" 'BEGIN {printf "%.1fx", t1/t2}')
ah_ratio=$(awk -v t1="$ah_time" -v t2="$c_time" 'BEGIN {printf "%.1fx", t1/t2}')

# Clean up temporary compiled binary
rm -f fib_c

# 4. Print results table
echo ""
echo "========================================================="
echo "                   BENCHMARK RESULTS                     "
echo "========================================================="
printf "| %-12s | %-20s | %-12s |\n" "Language" "Time Taken (Seconds)" "Ratio"
echo "---------------------------------------------------------"
printf "| %-12s | %-20s | %-12s |\n" "C" "${c_time}s" "1.0x (Ref)"
printf "| %-12s | %-20s | %-12s |\n" "Python 3" "${py_time}s" "$py_ratio"
printf "| %-12s | %-20s | %-12s |\n" "ahhhhh" "${ah_time}s" "$ah_ratio"
echo "========================================================="
