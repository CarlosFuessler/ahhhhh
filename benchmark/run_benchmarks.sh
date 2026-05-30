#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==========================================="
echo "  Running Fibonacci(35) Benchmarks...      "
echo "==========================================="
echo ""


echo "[1/4] Compiling C benchmark..."
clang -O3 fib.c -o fib_c


echo "[2/4] Building optimized ahhhh binary..."
(cd .. && zig build -Doptimize=ReleaseFast)


echo "[3/4] Running C benchmark..."
c_out=$(./fib_c)
c_time=$(echo "$c_out" | grep "Time taken" | awk '{print $3}')


if [ ! -f "../zig-out/bin/ahhhh" ]; then
  echo "Error: ahhhh binary not found in ../zig-out/bin/ahhhh"
  rm -f fib_c
  exit 1
fi

echo "[4/4] Running Python benchmark..."
py_out=$(python3 fib.py)
py_time=$(echo "$py_out" | grep "Time taken" | awk '{print $3}')

echo "[5/5] Running ahhhh benchmark..."
ah_out=$(../zig-out/bin/ahhhh fib.ahhhh)
ah_time=$(echo "$ah_out" | grep "Time taken" | awk '{print $3}')


py_ratio=$(awk -v t1="$py_time" -v t2="$c_time" 'BEGIN {printf "%.1fx", t1/t2}')
ah_ratio=$(awk -v t1="$ah_time" -v t2="$c_time" 'BEGIN {printf "%.1fx", t1/t2}')


rm -f fib_c

echo ""
echo "========================================================="
echo "                   BENCHMARK RESULTS                     "
echo "========================================================="
printf "| %-12s | %-20s | %-12s |\n" "Language" "Time Taken (Seconds)" "Ratio"
echo "---------------------------------------------------------"
printf "| %-12s | %-20s | %-12s |\n" "C" "${c_time}s" "1.0x (Ref)"
printf "| %-12s | %-20s | %-12s |\n" "Python 3" "${py_time}s" "$py_ratio"
printf "| %-12s | %-20s | %-12s |\n" "ahhhh" "${ah_time}s" "$ah_ratio"
echo "========================================================="
