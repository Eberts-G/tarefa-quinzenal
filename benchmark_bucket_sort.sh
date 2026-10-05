#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

REPETICOES="${REPETICOES:-3}"       # mínimo exigido: 3
CFLAGS="-Wall -Wextra -O2 -std=c11" # mesma política de otimização nas duas versões
ENTRADAS=(entradas/pequena.txt entradas/media.txt entradas/grande.txt)

mkdir -p resultados

for file in "${ENTRADAS[@]}"; do
  if [ ! -f "$file" ]; then
    echo "Entrada ausente: $file" >&2
    exit 1
  fi
done

# shellcheck disable=SC2086
gcc $CFLAGS bucket_sort.c -o bucket_sort
# shellcheck disable=SC2086
gcc $CFLAGS thread_bucket_sort.c -pthread -o thread_bucket_sort

# Mesmo valor que sysconf(_SC_NPROCESSORS_ONLN), usado pelo programa paralelo.
MAX_CPUS="$(getconf _NPROCESSORS_ONLN)"

# Características da máquina (exigidas no relatório).
{
  echo "Data: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "Kernel: $(uname -srmo)"
  if command -v lscpu >/dev/null 2>&1; then
    lscpu | grep -E '^(Model name|CPU\(s\)|On-line CPU\(s\) list|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|CPU max MHz):' || true
  fi
  if command -v free >/dev/null 2>&1; then
    echo "Memória: $(free -h | awk '/^Mem:/ {print $2}')"
  fi
  echo "CPUs lógicas online (sysconf): $MAX_CPUS"
  echo "Compilador: $(gcc --version | head -n 1)"
  echo "Opções: $CFLAGS (+ -pthread na versão paralela)"
  echo "Repetições por configuração: $REPETICOES"
} > resultados/maquina.txt

campo() { # campo NOME TEXTO -> valor do campo "NOME: valor"
  printf '%s\n' "$2" | awk -F': ' -v key="$1" '$1 == key {print $2}'
}

{
  echo "arquivo,configuracao,threads,iteracao,tempo_segundos,valido"

  for file in "${ENTRADAS[@]}"; do
    for config in seq 2 4 8 max; do
      for iter in $(seq 1 "$REPETICOES"); do
        if [ "$config" = "seq" ]; then
          output=$(./bucket_sort "$file" 2>&1)
          threads=1
        elif [ "$config" = "max" ]; then
          output=$(./thread_bucket_sort "$file" --max 2>&1)
          threads=$(campo THREADS "$output")
        else
          output=$(./thread_bucket_sort "$file" "$config" 2>&1)
          threads=$(campo THREADS "$output")
        fi

        tempo=$(campo TEMPO_SEGUNDOS "$output")
        valido=$(campo VALIDO "$output")

        if [ -z "$tempo" ] || [ "$valido" != "SIM" ]; then
          echo "Execução inválida ($file, $config, iteração $iter):" >&2
          echo "$output" >&2
          exit 1
        fi

        printf '%s,%s,%s,%s,%s,%s\n' "$file" "$config" "$threads" "$iter" "$tempo" "$valido"
      done
    done
  done
} > resultados/benchmark.csv

python3 gerar_graficos.py

printf 'Resultados brutos:  resultados/benchmark.csv\n'
printf 'Resumo (média, desvio, speedup, eficiência): resultados/resumo.csv e resultados/resumo.md\n'
printf 'Gráficos:           resultados/tempo_medio.svg e resultados/speedup.svg\n'
printf 'Máquina:            resultados/maquina.txt\n'
