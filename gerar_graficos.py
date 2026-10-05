import csv
import math
import os
import statistics
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.abspath(__file__))
CSV_PATH = os.environ.get("BENCHMARK_CSV", os.path.join(ROOT, "resultados", "benchmark.csv"))
OUT_DIR = os.path.join(ROOT, "resultados")
os.makedirs(OUT_DIR, exist_ok=True)

CONFIGS = ["seq", "2", "4", "8", "max"]
FILES = ["entradas/pequena.txt", "entradas/media.txt", "entradas/grande.txt"]
# Paleta segura para daltonismo (Okabe-Ito)
PALETTE = ["#0072B2", "#E69F00", "#009E73", "#D55E00", "#CC79A7"]

with open(CSV_PATH, newline="") as csv_file:
    rows = list(csv.DictReader(csv_file))


def threads_of(file_name, config):
    """Número real de threads (p) da configuração; None se desconhecido."""
    for row in rows:
        if row["arquivo"] == file_name and row["configuracao"] == config:
            if row.get("threads"):
                return int(row["threads"])
    if config == "seq":
        return 1
    if config.isdigit():
        return int(config)
    return None


# ---------------------------------------------------------------- estatísticas
summary = {}
for file_name in FILES:
    summary[file_name] = {}
    for config in CONFIGS:
        values = [
            float(r["tempo_segundos"])
            for r in rows
            if r["arquivo"] == file_name and r["configuracao"] == config
        ]
        summary[file_name][config] = {
            "n": len(values),
            "valores": values,
            "media": statistics.mean(values) if values else 0.0,
            "desvio": statistics.stdev(values) if len(values) > 1 else 0.0,
            "p": threads_of(file_name, config),
        }

for file_name in FILES:
    t_seq = summary[file_name]["seq"]["media"]
    for config in CONFIGS:
        entry = summary[file_name][config]
        # Sp = Tseq / Tp ; Ep = Sp / p
        entry["speedup"] = t_seq / entry["media"] if entry["media"] > 0 else 0.0
        entry["eficiencia"] = entry["speedup"] / entry["p"] if entry["p"] else None

max_threads = next((summary[f]["max"]["p"] for f in FILES if summary[f]["max"]["p"]), None)


def config_label(config, with_p=True):
    if config == "seq":
        return "Seq."
    if config == "max":
        return f"Máx ({max_threads})" if (max_threads and with_p) else "Máx"
    return config


# ---------------------------------------------------------------- tabelas
with open(os.path.join(OUT_DIR, "resumo.csv"), "w", newline="", encoding="utf-8") as out:
    writer = csv.writer(out)
    writer.writerow(["arquivo", "configuracao", "threads", "repeticoes",
                     "media_s", "desvio_padrao_s", "speedup", "eficiencia"])
    for file_name in FILES:
        for config in CONFIGS:
            e = summary[file_name][config]
            writer.writerow([
                file_name, config, e["p"] if e["p"] else "",
                e["n"], f'{e["media"]:.9f}', f'{e["desvio"]:.9f}',
                f'{e["speedup"]:.3f}',
                f'{e["eficiencia"]:.3f}' if e["eficiencia"] is not None else "",
            ])

with open(os.path.join(OUT_DIR, "resumo.md"), "w", encoding="utf-8") as out:
    out.write("| Entrada | Configuração | Threads (p) | Média (s) | Desvio padrão (s) | Speedup | Eficiência |\n")
    out.write("|---|---|---:|---:|---:|---:|---:|\n")
    for file_name in FILES:
        for config in CONFIGS:
            e = summary[file_name][config]
            name = {"seq": "Sequencial", "max": "Máximo de CPUs lógicas"}.get(config, f"{config} threads")
            p = e["p"] if e["p"] else "?"
            eff = f'{e["eficiencia"]:.2f}' if e["eficiencia"] is not None else "n/d"
            out.write(f'| {os.path.basename(file_name)} | {name} | {p} | {e["media"]:.4f} | '
                      f'{e["desvio"]:.4f} | {e["speedup"]:.2f} | {eff} |\n')


# ---------------------------------------------------------------- gráficos
def nice_max(value):
    if value <= 0:
        return 1.0
    exp = math.floor(math.log10(value))
    frac = value / 10 ** exp
    for step in (1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10):
        if frac <= step:
            return step * 10 ** exp
    return 10 ** (exp + 1)


def fmt(value):
    return f"{value:.3g}"


def svg_panels(title, out_name, key, y_label, shared_scale, reference=None, error_bars=False):
    width, height = 1200, 500
    top, bottom = 80, 140           # área do gráfico: y de 80 a (height - 140)
    plot_h = height - top - bottom
    slot = (width - 40) / len(FILES)
    plot_w = slot - 90

    def top_value(f):
        return max(summary[f][c][key] + (summary[f][c]["desvio"] if error_bars else 0) for c in CONFIGS)

    global_max = nice_max(max(top_value(f) for f in FILES))

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width/2}" y="32" text-anchor="middle" font-size="20" font-family="Arial" font-weight="bold">{escape(title)}</text>',
    ]

    for idx, file_name in enumerate(FILES):
        x0 = 20 + idx * slot + 70
        y_base = top + plot_h
        y_max = global_max if shared_scale else nice_max(top_value(file_name))

        svg.append(f'<text x="{x0 + plot_w/2}" y="{top - 20}" text-anchor="middle" font-size="14" font-family="Arial" font-weight="bold">{escape(os.path.basename(file_name))}</text>')
        svg.append(f'<line x1="{x0}" y1="{y_base}" x2="{x0 + plot_w}" y2="{y_base}" stroke="black" stroke-width="1.2"/>')
        svg.append(f'<line x1="{x0}" y1="{top}" x2="{x0}" y2="{y_base}" stroke="black" stroke-width="1.2"/>')
        svg.append(f'<text x="{x0 - 52}" y="{top + plot_h/2}" text-anchor="middle" font-size="12" font-family="Arial" transform="rotate(-90 {x0 - 52} {top + plot_h/2})">{escape(y_label)}</text>')

        for t in range(5):
            tick = y_max * t / 4
            y = y_base - plot_h * t / 4
            svg.append(f'<line x1="{x0}" y1="{y}" x2="{x0 + plot_w}" y2="{y}" stroke="#dddddd" stroke-width="0.8"/>')
            svg.append(f'<text x="{x0 - 6}" y="{y + 4}" text-anchor="end" font-size="10" font-family="Arial">{fmt(tick)}</text>')

        if reference is not None and reference <= y_max:
            y_ref = y_base - plot_h * reference / y_max
            svg.append(f'<line x1="{x0}" y1="{y_ref}" x2="{x0 + plot_w}" y2="{y_ref}" stroke="#444444" stroke-width="1" stroke-dasharray="5,4"/>')

        step = plot_w / len(CONFIGS)
        bar_w = step * 0.68
        for j, config in enumerate(CONFIGS):
            e = summary[file_name][config]
            value = e[key]
            bar_h = value / y_max * plot_h
            bx = x0 + j * step + (step - bar_w) / 2
            by = y_base - bar_h
            cx = bx + bar_w / 2
            svg.append(f'<rect x="{bx}" y="{by}" width="{bar_w}" height="{bar_h}" fill="{PALETTE[j]}"/>')
            label_y = by - 6
            if error_bars and e["desvio"] > 0:
                y_hi = y_base - min(value + e["desvio"], y_max) / y_max * plot_h
                y_lo = y_base - max(value - e["desvio"], 0) / y_max * plot_h
                svg.append(f'<line x1="{cx}" y1="{y_hi}" x2="{cx}" y2="{y_lo}" stroke="black" stroke-width="1.2"/>')
                svg.append(f'<line x1="{cx - 5}" y1="{y_hi}" x2="{cx + 5}" y2="{y_hi}" stroke="black" stroke-width="1.2"/>')
                svg.append(f'<line x1="{cx - 5}" y1="{y_lo}" x2="{cx + 5}" y2="{y_lo}" stroke="black" stroke-width="1.2"/>')
                label_y = y_hi - 5
            text = f"{value:.4f}" if key == "media" else f"{value:.2f}"
            svg.append(f'<text x="{cx}" y="{label_y}" text-anchor="middle" font-size="10" font-family="Arial">{text}</text>')
            svg.append(f'<text x="{cx}" y="{y_base + 16}" text-anchor="middle" font-size="10" font-family="Arial">{escape(config_label(config, with_p=False))}</text>')

    # legenda
    legend_y = height - 60
    item_w = 150
    start_x = width / 2 - item_w * len(CONFIGS) / 2
    names = {"seq": "Sequencial", "2": "2 threads", "4": "4 threads", "8": "8 threads",
             "max": f"Máx. CPUs lógicas ({max_threads})" if max_threads else "Máx. CPUs lógicas"}
    for j, config in enumerate(CONFIGS):
        lx = start_x + j * item_w
        svg.append(f'<rect x="{lx}" y="{legend_y}" width="12" height="12" fill="{PALETTE[j]}"/>')
        svg.append(f'<text x="{lx + 18}" y="{legend_y + 11}" font-size="11" font-family="Arial">{escape(names[config])}</text>')

    note = "Barras = média das repetições"
    if error_bars:
        note += "; traço vertical = ± 1 desvio padrão; cada gráfico tem escala própria"
    else:
        note += "; linha tracejada = speedup 1 (sem ganho)"
    svg.append(f'<text x="{width/2}" y="{height - 20}" text-anchor="middle" font-size="11" font-family="Arial" fill="#444444">{escape(note)}</text>')
    svg.append('</svg>')

    with open(os.path.join(OUT_DIR, f"{out_name}.svg"), "w", encoding="utf-8") as out_file:
        out_file.write("\n".join(svg))


svg_panels("Tempo médio de ordenação: sequencial x pthreads", "tempo_medio", "media",
           "Tempo (s)", shared_scale=False, error_bars=True)
svg_panels("Speedup (Tseq / Tp) por configuração", "speedup", "speedup",
           "Speedup", shared_scale=True, reference=1.0)

print("Arquivos gerados em:", OUT_DIR)
for name in ("resumo.csv", "resumo.md", "tempo_medio.svg", "speedup.svg"):
    print(os.path.join(OUT_DIR, name))
