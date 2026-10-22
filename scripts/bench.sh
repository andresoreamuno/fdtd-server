#!/bin/bash
# fdtd_bench script
# Usage: scripts/bench.sh       
# Result: evidencia/bench_<maquina>.csv

REPS=5
THREADS="1 2 4 8"
OUT="evidencia/bench_$(hostname).csv"

mkdir -p evidencia
echo "scenario,nx,ny,steps,threads,k,init,compute,barrier,io,total,sum_abs_ez" > "$OUT"

run() {
    scenario=$1; nx=$2; ny=$3; steps=$4; k=$5
    for t in $THREADS; do
        for r in $(seq 1 $REPS); do
            line=$(./bin/fdtd_bench $nx $ny $steps $t $k | tail -n 1)
            echo "$scenario,$line" >> "$OUT"
            echo "$scenario T=$t rep=$r: $line"
        done
    done
}

run E1 2048 2048 1000 0
run E3 256 256 1000 0

echo "DONE: $OUT"