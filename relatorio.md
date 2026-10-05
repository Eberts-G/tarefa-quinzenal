# Relatório – Bucket Sort: sequencial x pthreads

## Identificação

- **Problema escolhido:** 1 – Bucket Sort
- **Disciplina:** Sistemas Operacionais (Prof. Lucca Abbado Neres) – Tarefa Semanal
- **Integrantes:** _(preencher: nome completo de cada integrante do grupo, de 3 a 4 estudantes)_

## Máquina e ambiente

Os dados abaixo são gravados automaticamente em `resultados/maquina.txt` pelo script de benchmark. Copie os valores para cá depois de executá-lo.

| Item | Valor |
|---|---|
| Sistema operacional | Ubuntu via WSL 2 |
| Processador | _(preencher: campo "Model name" de `maquina.txt`)_ |
| CPUs lógicas online (`sysconf(_SC_NPROCESSORS_ONLN)`) | _(preencher: campo "CPUs lógicas online")_ |
| Memória | _(preencher)_ |
| Compilador | GCC 15.2 |
| Opções de compilação | `-Wall -Wextra -O2 -std=c11` nas duas versões (`-pthread` na paralela) |

## Metodologia

**Entradas:** `entradas/pequena.txt` (20 000 inteiros), `entradas/media.txt` (120 000) e `entradas/grande.txt` (400 000). A mesma entrada é lida pelas duas versões.

**Versão sequencial (`bucket_sort.c`):** encontra mínimo e máximo, distribui os N valores em N baldes proporcionais à faixa de valores, ordena cada balde com `qsort` e concatena.

**Versão paralela (`thread_bucket_sort.c`):** divide o vetor em blocos contíguos, um por thread. Cada thread executa o mesmo bucket sort sequencial sobre o seu bloco, em memória exclusiva, então não há dado compartilhado escrito por mais de uma thread e não são necessários mutexes. Depois do `pthread_join`, a thread principal intercala os blocos ordenados (merge k-way). Essa intercalação é sequencial, e isso limita o ganho (ver análise).

**Configurações:** sequencial, 2, 4, 8 threads e o máximo de CPUs lógicas online (`--max`). Quando o máximo coincide com 2, 4 ou 8, a execução `max` continua sendo registrada separadamente, como exige o enunciado; o CSV guarda o número real de threads de cada execução.

**Repetições:** 3 por combinação entrada × configuração. Os tempos individuais estão em `resultados/benchmark.csv`.

**Critério de tempo (igual nas duas versões):** `clock_gettime(CLOCK_MONOTONIC)` ao redor de toda a ordenação, depois da leitura do arquivo. Na paralela, o intervalo inclui criação das threads, bucket sort dos blocos, join e merge. Ficam fora da medição: leitura do arquivo, geração da sequência de referência e verificação.

**Verificação:** a versão paralela compara o resultado, elemento a elemento (`memcmp`), com a saída da versão sequencial e confere se está em ordem crescente. A execução só é aceita com `VALIDO: SIM`; o script aborta se alguma execução for inválida.

**Métricas:** média e desvio padrão amostral dos tempos; speedup `Sp = Tseq / Tp`; eficiência `Ep = Sp / p`, com `p` o número real de threads.

## Resultados

- Resultados brutos: [resultados/benchmark.csv](resultados/benchmark.csv)
- Resumo: [resultados/resumo.csv](resultados/resumo.csv)
- Gráfico de tempo médio: [resultados/tempo_medio.svg](resultados/tempo_medio.svg)
- Gráfico de speedup: [resultados/speedup.svg](resultados/speedup.svg)

Tabela da execução original do grupo. Depois de reexecutar `benchmark_bucket_sort.sh`, substitua esta tabela pelo conteúdo de `resultados/resumo.md`. Nele a coluna de threads do `max` e a sua eficiência já vêm preenchidas.

| Entrada | Configuração | Threads (p) | Média (s) | Desvio padrão (s) | Speedup | Eficiência |
|---|---|---:|---:|---:|---:|---:|
| pequena.txt | Sequencial | 1 | 0.0040 | 0.0009 | 1.00 | 1.00 |
| pequena.txt | 2 threads | 2 | 0.0051 | 0.0022 | 0.79 | 0.39 |
| pequena.txt | 4 threads | 4 | 0.0034 | 0.0014 | 1.17 | 0.29 |
| pequena.txt | 8 threads | 8 | 0.0023 | 0.0001 | 1.75 | 0.22 |
| pequena.txt | Máximo de CPUs lógicas | _(preencher)_ | 0.0028 | 0.0016 | 1.42 | _(Sp/p)_ |
| media.txt | Sequencial | 1 | 0.0361 | 0.0084 | 1.00 | 1.00 |
| media.txt | 2 threads | 2 | 0.0338 | 0.0049 | 1.07 | 0.53 |
| media.txt | 4 threads | 4 | 0.0232 | 0.0072 | 1.56 | 0.39 |
| media.txt | 8 threads | 8 | 0.0251 | 0.0148 | 1.44 | 0.18 |
| media.txt | Máximo de CPUs lógicas | _(preencher)_ | 0.0397 | 0.0038 | 0.91 | _(Sp/p)_ |
| grande.txt | Sequencial | 1 | 0.2244 | 0.0021 | 1.00 | 1.00 |
| grande.txt | 2 threads | 2 | 0.1202 | 0.0456 | 1.87 | 0.93 |
| grande.txt | 4 threads | 4 | 0.1152 | 0.0102 | 1.95 | 0.49 |
| grande.txt | 8 threads | 8 | 0.0859 | 0.0102 | 2.61 | 0.33 |
| grande.txt | Máximo de CPUs lógicas | _(preencher)_ | 0.0885 | 0.0354 | 2.53 | _(Sp/p)_ |

## Análise

**Onde houve ganho.** O ganho claro aparece na entrada grande: o tempo cai de 0,224 s (sequencial) para 0,086 s com 8 threads (speedup 2,61). Com 2 threads o speedup é 1,87 (eficiência 0,93), próximo do ideal. Com 4 threads sobe só para 1,95, então a eficiência cai para 0,49.

**Saturação.** O speedup cresce menos que o número de threads: a eficiência diminui a cada aumento de `p` (0,93 → 0,49 → 0,33 na entrada grande). Duas causas prováveis: (1) o merge final é sequencial, e por isso a fração não paralelizável cresce em relação ao tempo total conforme os blocos ficam menores (lei de Amdahl); (2) as threads disputam cache e largura de banda de memória, pois cada uma aloca baldes e percorre sua parte do vetor. Na entrada grande, a configuração com o máximo de CPUs (2,53) ficou ligeiramente abaixo de 8 threads (2,61). Isso é compatível com saturação, mas a diferença está dentro do desvio padrão e não permite afirmar que uma seja melhor que a outra.

**Onde houve piora.** Nas entradas menores o paralelismo não compensa de forma consistente:
- `pequena.txt` com 2 threads: speedup de 0,79, ou seja, mais lento que o sequencial.
- `media.txt` com o máximo de CPUs: speedup de 0,91, também mais lento. Com 8 threads o ganho (1,44) ficou abaixo do obtido com 4 threads (1,56).

Nessas entradas o trabalho dura poucos milissegundos, comparável ao custo de criar e juntar threads e de fazer o merge.

**Variabilidade.** Os desvios padrão são grandes em relação às médias (por exemplo, `pequena.txt` com 2 threads: 0,0051 ± 0,0022 s; `grande.txt` com 2 threads: 0,120 ± 0,046 s). Com apenas 3 repetições, medindo poucos milissegundos e rodando em WSL 2 (sujeito à interferência do Windows), diferenças pequenas entre configurações não são conclusivas. Os resultados que contrariam a expectativa de que mais threads é melhor foram mantidos e estão discutidos acima.

**Conclusão.** A versão paralela compensa quando há volume de trabalho suficiente (400 000 elementos). Para 20 000 e 120 000 elementos, o ganho é pequeno ou inexistente, e o número de threads que maximiza o desempenho não é o maior disponível.

## Limitações

- Apenas 3 repetições por configuração, o que dá estimativas de desvio padrão pouco robustas.
- Execução em WSL 2, que pode introduzir ruído de escalonamento.
- Merge final sequencial: é uma escolha de projeto que limita o speedup máximo.
