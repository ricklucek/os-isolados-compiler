#!/usr/bin/env python3
import argparse
import csv
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "macaronica"
TEMPLATES = ROOT / "benchmarks" / "templates"

def run(cmd, *, env=None, cwd=ROOT, capture=False):
    kwargs = {"cwd": cwd, "env": env, "text": True, "check": True}
    if capture:
        kwargs["stdout"] = subprocess.PIPE
        kwargs["stderr"] = subprocess.PIPE
    return subprocess.run(cmd, **kwargs)

def parse_csv_ints(value):
    return [int(x.strip()) for x in value.split(",") if x.strip()]

def parse_csv_strs(value):
    return [x.strip() for x in value.split(",") if x.strip()]

def render_source(kernel, n, dest):
    template = (TEMPLATES / f"{kernel}.mcn.in").read_text()
    dest.write_text(template.replace("@N@", str(n)))

def kernel_symbol(kernel_ll):
    text = kernel_ll.read_text()
    m = re.search(r"define\s+(?:void|float|double|i32)\s+@([^\s(]+)\(", text)
    if not m:
        raise RuntimeError(f"nao encontrou simbolo de kernel em {kernel_ll}")
    return m.group(1)

def harness_text(kernel, symbol, n, inner):
    common = f"""
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ns(void) {{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}}

static void *xmalloc(size_t bytes) {{
    void *p = malloc(bytes);
    if (!p) {{
        fputs("falha de memoria\\n", stderr);
        exit(2);
    }}
    return p;
}}

enum {{ N = {n}, INNER = {inner} }};
"""
    if kernel == "elementwise":
        return common + f"""
extern void {symbol}(
    float*, float*, int64_t, int64_t, int64_t,
    float*, float*, int64_t, int64_t, int64_t,
    float*, float*, int64_t, int64_t, int64_t);

int main(void) {{
    size_t i;
    float *a = xmalloc((size_t)N * sizeof(float));
    float *b = xmalloc((size_t)N * sizeof(float));
    float *c = xmalloc((size_t)N * sizeof(float));
    double begin, end;
    volatile float checksum;

    for (i = 0; i < (size_t)N; ++i) {{
        a[i] = (float)(i % 251U) * 0.001f;
        b[i] = (float)(i % 199U) * 0.002f;
        c[i] = 0.0f;
    }}

    {symbol}(a, a, 0, N, 1, b, b, 0, N, 1, c, c, 0, N, 1);

    begin = now_ns();
    for (i = 0; i < (size_t)INNER; ++i)
        {symbol}(a, a, 0, N, 1, b, b, 0, N, 1, c, c, 0, N, 1);
    end = now_ns();

    checksum = c[0] + c[N / 2] + c[N - 1];
    printf("ns=%.3f checksum=%.9g\\n", (end - begin) / (double)INNER,
           (double)checksum);

    free(a); free(b); free(c);
    return 0;
}}
"""
    if kernel == "prodesc":
        return common + f"""
extern float {symbol}(
    float*, float*, int64_t, int64_t, int64_t,
    float*, float*, int64_t, int64_t, int64_t);

int main(void) {{
    size_t i;
    float *a = xmalloc((size_t)N * sizeof(float));
    float *b = xmalloc((size_t)N * sizeof(float));
    double begin, end;
    volatile float result = 0.0f;

    for (i = 0; i < (size_t)N; ++i) {{
        a[i] = (i & 1U) ? 1.0f : -1.0f;
        b[i] = 0.25f;
    }}

    result = {symbol}(a, a, 0, N, 1, b, b, 0, N, 1);

    begin = now_ns();
    for (i = 0; i < (size_t)INNER; ++i)
        result = {symbol}(a, a, 0, N, 1, b, b, 0, N, 1);
    end = now_ns();

    printf("ns=%.3f checksum=%.9g\\n", (end - begin) / (double)INNER,
           (double)result);

    free(a); free(b);
    return 0;
}}
"""
    if kernel == "sumfloat":
        return common + f"""
extern float {symbol}(float*, float*, int64_t, int64_t, int64_t);

int main(void) {{
    size_t i;
    float *a = xmalloc((size_t)N * sizeof(float));
    double begin, end;
    volatile float result = 0.0f;

    for (i = 0; i < (size_t)N; ++i)
        a[i] = 1.0f / (float)(i + 1U);

    result = {symbol}(a, a, 0, N, 1);

    begin = now_ns();
    for (i = 0; i < (size_t)INNER; ++i)
        result = {symbol}(a, a, 0, N, 1);
    end = now_ns();

    printf("ns=%.3f checksum=%.9g\\n", (end - begin) / (double)INNER,
           (double)result);

    free(a);
    return 0;
}}
"""
    raise ValueError(kernel)

def compile_binary(config, kernel_ll, harness, exe, evidence_dir):
    clang = os.environ.get("CLANG_LLVM", "clang-18")
    kernel_obj = exe.with_suffix(".kernel.o")
    harness_obj = exe.with_suffix(".harness.o")
    rpass = evidence_dir / "rpass.txt"
    optimized = evidence_dir / "kernel_optimized.ll"

    kernel_flags = ["-O0"]
    link_flags = []
    if config in ("C1", "C1F"):
        kernel_flags = [
            "-O2", "-march=native",
            "-Rpass=loop-vectorize",
            "-Rpass-missed=loop-vectorize",
            "-Rpass-analysis=loop-vectorize",
        ]
    elif config == "C2":
        # A vetorizacao e decidida explicitamente no MLIR; -O2 fica
        # responsavel por canonicalizacao/seleção de instrucoes no LLVM,
        # deixando C1 x C2 comparaveis no mesmo nivel de otimizacao final.
        kernel_flags = ["-O2", "-march=native"]
    elif config == "C3":
        kernel_flags = ["-O2", "-march=native", "-fopenmp"]
        link_flags = ["-fopenmp"]

    start = time.perf_counter()
    with rpass.open("w") as log:
        subprocess.run(
            [clang, *kernel_flags, "-c", str(kernel_ll), "-o", str(kernel_obj)],
            cwd=ROOT, text=True, check=True, stderr=log)
    run([clang, "-O2", "-march=native", "-std=c11",
         "-c", str(harness), "-o", str(harness_obj)])
    run([clang, str(harness_obj), str(kernel_obj), *link_flags, "-lm",
         "-o", str(exe)])
    compile_ms = (time.perf_counter() - start) * 1000.0

    if config in ("C1", "C1F"):
        with (evidence_dir / "opt_ir_stderr.txt").open("w") as err:
            subprocess.run(
                [clang, "-O2", "-march=native", "-S", "-emit-llvm",
                 str(kernel_ll), "-o", str(optimized)],
                cwd=ROOT, text=True, check=True, stderr=err)

    return compile_ms

def run_binary(exe, threads):
    env = os.environ.copy()
    if threads:
        env["OMP_NUM_THREADS"] = str(threads)
    cp = run([str(exe)], env=env, capture=True)
    m = re.search(r"ns=([0-9.eE+-]+)\s+checksum=([0-9.eE+-]+)", cp.stdout)
    if not m:
        raise RuntimeError(f"saida de benchmark invalida: {cp.stdout!r}")
    return float(m.group(1)) / 1e6, float(m.group(2))

def write_summary(raw_rows, path):
    groups = {}
    for row in raw_rows:
        key = (row["kernel"], row["size"], row["config"], row["threads"])
        groups.setdefault(key, []).append(row)

    fields = ["kernel","size","config","threads",
              "compile_median_ms","run_median_ms","checksum_median","samples"]
    with path.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        w.writeheader()
        for key in sorted(groups):
            rows = groups[key]
            w.writerow({
                "kernel": key[0],
                "size": key[1],
                "config": key[2],
                "threads": key[3],
                "compile_median_ms": f"{statistics.median(r['compile_ms'] for r in rows):.6f}",
                "run_median_ms": f"{statistics.median(r['run_ms'] for r in rows):.6f}",
                "checksum_median": f"{statistics.median(r['checksum'] for r in rows):.9g}",
                "samples": len(rows),
            })

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sizes", default="1000000,10000000")
    p.add_argument("--reps", type=int, default=5)
    p.add_argument("--threads", default="1,2,4,8")
    p.add_argument("--configs", default="C0,C1,C2,C3")
    p.add_argument("--kernels", default="elementwise,prodesc")
    p.add_argument("--output", default="resultados/benchmarks")
    p.add_argument("--inner", type=int, default=0,
                   help="0 calcula repeticoes para ~50M elementos/amostra")
    p.add_argument("--fastmath-experiment", action="store_true")
    args = p.parse_args()

    if not BIN.exists():
        raise SystemExit("compile primeiro com make")

    sizes = parse_csv_ints(args.sizes)
    threads = parse_csv_ints(args.threads)
    configs = parse_csv_strs(args.configs)
    kernels = parse_csv_strs(args.kernels)
    outroot = ROOT / args.output
    outroot.mkdir(parents=True, exist_ok=True)

    with (outroot / "maquina.txt").open("w") as fh:
        subprocess.run(["sh", "scripts/system_info.sh"], cwd=ROOT,
                       text=True, check=True, stdout=fh)

    raw = []
    variant_script = ROOT / "scripts" / "benchmark_variant.sh"

    jobs = [(k, n, c) for k in kernels for n in sizes for c in configs]
    if args.fastmath_experiment:
        for n in sizes:
            jobs.extend([("sumfloat", n, "C1"), ("sumfloat", n, "C1F")])

    for kernel, n, config in jobs:
        case = outroot / kernel / str(n) / config
        case.mkdir(parents=True, exist_ok=True)
        source = case / "bench.mcn"
        input_mlir = case / "input.mlir"
        variant = case / "variant"
        inner = args.inner or max(1, 50_000_000 // n)

        render_source(kernel, n, source)
        run([str(BIN), str(source), "--emit-mlir", str(input_mlir)])

        # A transformação MLIR faz parte da compilação e é refeita em cada amostra.
        for rep in range(1, args.reps + 1):
            if variant.exists():
                shutil.rmtree(variant)
            variant.mkdir()
            start = time.perf_counter()
            run(["sh", str(variant_script), config, str(input_mlir), str(variant)])
            transform_ms = (time.perf_counter() - start) * 1000.0

            kernel_ll = variant / "kernel.ll"
            symbol = kernel_symbol(kernel_ll)
            harness = variant / "harness.c"
            harness.write_text(harness_text(kernel, symbol, n, inner))
            exe = variant / "programa"

            compile_ms = transform_ms + compile_binary(
                config, kernel_ll, harness, exe, variant)

            run_threads = threads if config == "C3" else [1]
            for t in run_threads:
                run_ms, checksum = run_binary(exe, t)
                if not math.isfinite(checksum):
                    raise RuntimeError("checksum nao finito")
                raw.append({
                    "kernel": kernel,
                    "size": n,
                    "config": config,
                    "threads": t,
                    "rep": rep,
                    "compile_ms": compile_ms,
                    "run_ms": run_ms,
                    "checksum": checksum,
                })
                print(f"{kernel} N={n} {config} T={t} "
                      f"rep={rep}: compile={compile_ms:.3f}ms "
                      f"run={run_ms:.3f}ms checksum={checksum:.9g}")

    raw_path = outroot / "raw.csv"
    fields = ["kernel","size","config","threads","rep",
              "compile_ms","run_ms","checksum"]
    with raw_path.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        w.writeheader()
        w.writerows(raw)

    summary = outroot / "summary.csv"
    write_summary(raw, summary)
    run([sys.executable, "scripts/plot_benchmarks.py",
         str(summary), str(outroot / "graficos")])

    print(f"Resultados: {raw_path}")
    print(f"Resumo:     {summary}")
    print(f"Graficos:   {outroot / 'graficos'}")

if __name__ == "__main__":
    main()
