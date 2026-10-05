# Relatório – Bucket Sort: sequencial x pthreads

## Identificação

- **Problema escolhido:** 1 – Bucket Sort
- **Disciplina:** Sistemas Operacionais (Prof. Lucca Abbado Neres) – Tarefa Semanal
- **Integrantes:** _(preencher: nome completo de cada integrante do grupo, de 3 a 4 estudantes)_

## Máquina e ambiente

Dados gravados por `benchmark_bucket_sort.sh` em `resultados/maquina.txt` (execução de 05/10/2026).

| Item | Valor |
|---|---|
| Sistema operacional | Ubuntu via WSL 2 (kernel 6.18.33.2-microsoft-standard-WSL2, x86_64) |
| Processador | 13th Gen Intel(R) Core(TM) i5-13420H |
| Topologia informada pelo `lscpu` | 1 socket, 5 núcleos por socket, 2 threads por núcleo |
| CPUs lógicas online (`sysconf(_SC_NPROCESSORS_ONLN)`) | **10** |
| Memória | 7,6 GiB |
| Compilador | GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| Opções de compilação | `-Wall -Wextra -O2 -std=c11` nas duas versões (`-pthread` na paralela) |

A configuração "máximo de CPUs lógicas" usa, portanto, **10 threads**. Esse valor não coincide com 2, 4 nem 8.

## Metodologia

**Entradas:** `entradas/pequena.txt` (20 000 inteiros), `entradas/media.txt` (120 000) e `entradas/grande.txt` (400 000). A mesma entrada é lida pelas duas versões.

**Versão sequencial (`bucket_sort.c`):** encontra mínimo e máximo, distribui os N valores em N baldes proporcionais à faixa de valores, ordena cada balde com `qsort` e concatena.

**Versão paralela (`thread_bucket_sort.c`):** divide o vetor em blocos contíguos, um por thread. Cada thread executa o mesmo bucket sort sequencial sobre o seu bloco, em memória exclusiva, então nenhum dado compartilhado é escrito por mais de uma thread e não são necessários mutexes. Depois do `pthread_join`, a thread principal intercala os blocos ordenados (merge k-way). Essa intercalação é sequencial e percorre os `p` blocos a cada elemento escrito, então seu custo cresce com o número de threads.

**Configurações:** sequencial, 2, 4, 8 threads e o máximo de CPUs lógicas online (`--max`, 10 threads nesta máquina).

**Repetições:** pelo menos 3 por combinação entrada × configuração (número exato na coluna `repeticoes` de `resultados/resumo.csv`). Os tempos individuais estão em `resultados/benchmark.csv`.

**Critério de tempo (igual nas duas versões):** `clock_gettime(CLOCK_MONOTONIC)` ao redor de toda a ordenação, depois da leitura do arquivo. Na paralela, o intervalo inclui criação das threads, bucket sort dos blocos, join e merge. Ficam fora da medição: leitura do arquivo, geração da sequência de referência e verificação.

**Verificação:** a versão paralela compara o resultado, elemento a elemento (`memcmp`), com a saída da versão sequencial e confere se está em ordem crescente. A execução só é aceita com `VALIDO: SIM`; o script aborta se alguma execução for inválida. Todas as execuções foram válidas.

**Métricas:** média e desvio padrão amostral dos tempos; speedup `Sp = Tseq / Tp`; eficiência `Ep = Sp / p`, com `p` o número real de threads.

## Resultados

- Resultados brutos: [resultados/benchmark.csv](resultados/benchmark.csv)
- Resumo: [resultados/resumo.csv](resultados/resumo.csv)
- Gráfico de tempo médio: [resultados/tempo_medio.svg](resultados/tempo_medio.svg)
- Gráfico de speedup: [resultados/speedup.svg](resultados/speedup.svg)

| Entrada | Configuração | Threads (p) | Média (s) | Desvio padrão (s) | Speedup | Eficiência |
|---|---|---:|---:|---:|---:|---:|
| pequena.txt | Sequencial | 1 | 0.0021 | 0.0005 | 1.00 | 1.00 |
| pequena.txt | 2 threads | 2 | 0.0024 | 0.0007 | 0.89 | 0.44 |
| pequena.txt | 4 threads | 4 | 0.0020 | 0.0006 | 1.04 | 0.26 |
| pequena.txt | 8 threads | 8 | 0.0027 | 0.0008 | 0.78 | 0.10 |
| pequena.txt | Máximo de CPUs lógicas | 10 | 0.0026 | 0.0007 | 0.81 | 0.08 |
| media.txt | Sequencial | 1 | 0.0202 | 0.0054 | 1.00 | 1.00 |
| media.txt | 2 threads | 2 | 0.0121 | 0.0032 | 1.66 | 0.83 |
| media.txt | 4 threads | 4 | 0.0123 | 0.0027 | 1.64 | 0.41 |
| media.txt | 8 threads | 8 | 0.0158 | 0.0035 | 1.28 | 0.16 |
| media.txt | Máximo de CPUs lógicas | 10 | 0.0182 | 0.0046 | 1.11 | 0.11 |
| grande.txt | Sequencial | 1 | 0.1233 | 0.0106 | 1.00 | 1.00 |
| grande.txt | 2 threads | 2 | 0.0792 | 0.0166 | 1.56 | 0.78 |
| grande.txt | 4 threads | 4 | 0.0670 | 0.0070 | 1.84 | 0.46 |
| grande.txt | 8 threads | 8 | 0.0726 | 0.0077 | 1.70 | 0.21 |
| grande.txt | Máximo de CPUs lógicas | 10 | 0.0758 | 0.0161 | 1.63 | 0.16 |

## Análise

**Entrada pequena (20 000 elementos): sem ganho.** Nenhuma configuração paralela superou a sequencial de forma clara: speedup de 0,89 (2 threads), 1,04 (4), 0,78 (8) e 0,81 (10). Com 8 e 10 threads a versão paralela foi mais lenta. O trabalho dura cerca de 2 ms, comparável ao custo de criar e juntar threads e de fazer o merge, e os desvios padrão (0,0005 a 0,0008 s) são de 25% a 30% da média. Por isso as diferenças entre 2 e 4 threads não são conclusivas.

**Entrada média (120 000 elementos): ganho moderado, que diminui com mais threads.** O speedup foi 1,66 com 2 threads (eficiência 0,83), 1,64 com 4, 1,28 com 8 e 1,11 com 10. O melhor resultado ficou em 2 a 4 threads, e a partir daí o ganho cai.

**Entrada grande (400 000 elementos): melhor ganho, com saturação.** O melhor resultado foi com 4 threads (speedup 1,84, eficiência 0,46), seguido de 8 (1,70), 10 (1,63) e 2 (1,56). De 4 a 10 threads os tempos médios (0,0670 s, 0,0726 s e 0,0758 s) diferem menos do que os desvios padrão (0,007 a 0,016 s), então o desempenho se estabiliza: usar mais threads que 4 não trouxe ganho mensurável.

**Eficiência.** A eficiência cai em todas as entradas à medida que `p` aumenta. Com 10 threads é de 0,08 a 0,16. O ganho existe, mas muito abaixo do ideal (`Sp = p`). Causas prováveis, não isoladas em testes separados:
- o merge final é sequencial e seu custo cresce com o número de blocos, pois percorre os `p` blocos a cada elemento escrito;
- o processador informa 5 núcleos com 2 threads por núcleo, então com mais threads que núcleos físicos duas threads dividem a mesma unidade de execução e o mesmo cache;
- as threads disputam largura de banda de memória, e o WSL 2 compete com o Windows pelos mesmos núcleos.

**Variabilidade.** Vários desvios padrão são altos em relação à média (por exemplo, `media.txt` sequencial: 0,0202 ± 0,0054 s; `grande.txt` com 2 threads: 0,0792 ± 0,0166 s). Uma execução anterior do mesmo experimento, na mesma máquina e com o mesmo código, deu valores bem diferentes: por exemplo, speedup de 2,85 com 10 threads em `media.txt` e de 0,94 com 8 threads em `grande.txt`, contra 1,11 e 1,70 nesta execução. Essa execução anterior não foi preservada. A diferença mostra que, com poucas repetições e tempos de milissegundos no WSL 2, os resultados individuais variam bastante entre execuções. Só as tendências gerais se mantiveram: nenhum ganho na entrada pequena, ganho moderado nas maiores e queda de eficiência com mais threads.

**Conclusão.** Mais threads não significa melhor desempenho. Nesta máquina o maior speedup observado foi 1,84 (`grande.txt`, 4 threads); para a entrada pequena a versão paralela não compensou, e para as maiores o melhor número de threads ficou entre 2 e 4. Usar as 10 CPUs lógicas não foi melhor que usar 4.

## Limitações

- Poucas repetições por configuração (o número exato está na coluna `repeticoes` de `resultados/resumo.csv`), o que dá estimativas de desvio padrão pouco robustas. O script aceita mais, com `REPETICOES=10 ./benchmark_bucket_sort.sh`.
- Execução em WSL 2, que pode introduzir ruído de escalonamento.
- Merge final sequencial com custo proporcional ao número de threads: é uma escolha de projeto que limita o speedup.
- As causas da perda de eficiência com muitas threads são hipóteses; não foram medidas em separado (por exemplo, medindo o tempo do merge isoladamente).
