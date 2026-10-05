# Bucket Sort: sequencial x pthreads

Implementação sequencial e paralela (pthreads) do Bucket Sort em C, com comparação experimental de tempo. Problema 1 da Tarefa Semanal de Sistemas Operacionais.

**Integrantes:** _  Ana Clara Cavasotto Polla, Bruno Martins Rauber, Gisela Talita Eberts, Guilherme Ribeiro Maciel,
Matheus Leverentz de Lara  _

## Arquivos

| Arquivo | Função |
|---|---|
| `bucket_sort.c` | versão sequencial |
| `thread_bucket_sort.c` | versão paralela com `pthread.h` |
| `benchmark_bucket_sort.sh` | compila, executa todas as configurações e gera CSV, resumo, gráficos e `maquina.txt` |
| `gerar_graficos.py` | calcula média, desvio padrão, speedup e eficiência e gera os SVGs |
| `entradas/` | `pequena.txt`, `media.txt`, `grande.txt` |
| `resultados/` | `benchmark.csv`, `resumo.csv`, `resumo.md`, `tempo_medio.svg`, `speedup.svg`, `maquina.txt` |
| `relatorio.md` | relatório com máquina, metodologia, resultados e análise |

## Compilar

Na pasta do projeto (no WSL, por exemplo `cd /mnt/c/.../Aula09-10`):

```bash
gcc -Wall -Wextra -O2 -std=c11 bucket_sort.c -o bucket_sort
gcc -Wall -Wextra -O2 -std=c11 thread_bucket_sort.c -pthread -o thread_bucket_sort
```

As duas versões usam a mesma política de otimização (`-O2`).

## Executar

```bash
./bucket_sort entradas/grande.txt                 # sequencial

./thread_bucket_sort entradas/grande.txt 4        # 4 threads
./thread_bucket_sort entradas/grande.txt --threads 4
./thread_bucket_sort entradas/grande.txt --max    # máximo de CPUs lógicas online (padrão se omitido)
```

Sem argumento de arquivo, os dois programas usam `entradas/pequena.txt`. Argumentos inválidos (por exemplo `abc` como número de threads) geram mensagem de uso e erro, sem usar um valor padrão em silêncio.

Formato da entrada: a primeira linha contém `N`, seguida de `N` inteiros.

```text
10
8 3 10 1 5 7 9 2 6 4
```

Saída dos programas (`THREADS` e `MAX_CPUS_ONLINE` só na versão paralela):

```text
MODALIDADE: PTHREADS
ARQUIVO: entradas/grande.txt
THREADS: 4
MAX_CPUS_ONLINE: 8
N: 400000
VALIDO: SIM
TEMPO_SEGUNDOS: 0.115200000
PRIMEIRO: ...
ULTIMO: ...
```

## Comparação completa

```bash
chmod +x benchmark_bucket_sort.sh
./benchmark_bucket_sort.sh
```

O script mede, para cada entrada (pequena, média, grande), a versão sequencial e as versões paralelas com 2, 4, 8 threads e com o máximo de CPUs lógicas online. São 3 repetições por combinação; use `REPETICOES=5 ./benchmark_bucket_sort.sh` para mais. Ele aborta se alguma execução for inválida.

Colunas de `resultados/benchmark.csv`: `arquivo, configuracao, threads, iteracao, tempo_segundos, valido`. A coluna `threads` guarda o número real de threads usado, inclusive na configuração `max`, o que registra quando o máximo coincide com 2, 4 ou 8.

`resultados/resumo.csv` e `resultados/resumo.md` trazem média, desvio padrão, speedup `Sp = Tseq/Tp` e eficiência `Ep = Sp/p` para cada combinação.

## Metodologia de medição

- Mesma entrada nas duas versões.
- Tempo medido com `clock_gettime(CLOCK_MONOTONIC)` ao redor de toda a ordenação, **sem** a leitura do arquivo, nas duas versões. Na paralela, a medição inclui criação das threads, ordenação dos blocos, join e merge.
- Verificação fora da medição: a versão paralela compara o resultado com o da sequencial (`memcmp`) e confere se está em ordem crescente. A sequencial confere a ordem crescente.
- O algoritmo sequencial não foi alterado para ficar artificialmente pior: cada thread da versão paralela executa exatamente esse mesmo bucket sort sobre o seu bloco.
