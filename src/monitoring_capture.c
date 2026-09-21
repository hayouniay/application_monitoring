#define _GNU_SOURCE

#include "monitoring_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* System-wide CPU utilization (independent of per-process metrics)         */
/* ------------------------------------------------------------------------- */

static double read_cpu_percent(void) {
  static unsigned long long prev_idle = 0;
  static unsigned long long prev_total = 0;
  static int has_previous = 0;

  FILE *fp;
  char line[256];

  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  unsigned long long total;
  unsigned long long idle_all;
  double percent = 0.0;

  fp = fopen("/proc/stat", "r");

  if (fp == NULL) {
    return 0.0;
  }

  if (fgets(line, sizeof(line), fp) == NULL) {
    fclose(fp);
    return 0.0;
  }

  fclose(fp);

  (void)sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user,
               &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

  total = user + nice + system + idle + iowait + irq + softirq + steal;
  idle_all = idle + iowait;

  if (has_previous && total > prev_total) {

    const unsigned long long total_delta = total - prev_total;
    const unsigned long long idle_delta =
        (idle_all >= prev_idle) ? (idle_all - prev_idle) : 0;

    if (total_delta > 0) {
      percent = 100.0 * (1.0 - ((double)idle_delta / (double)total_delta));
    }
  }

  prev_idle = idle_all;
  prev_total = total;
  has_previous = 1;

  if (percent < 0.0) {
    percent = 0.0;
  }

  if (percent > 100.0) {
    percent = 100.0;
  }

  return percent;
}

/* ------------------------------------------------------------------------- */
/* System-wide memory/swap utilization                                      */
/* ------------------------------------------------------------------------- */

static int read_mem_percent(double *mem_percent, double *swap_percent) {
  FILE *fp;
  char line[256];

  unsigned long long mem_total = 0;
  unsigned long long mem_available = 0;
  unsigned long long mem_free = 0;
  unsigned long long swap_total = 0;
  unsigned long long swap_free = 0;
  unsigned long long available;

  fp = fopen("/proc/meminfo", "r");

  if (fp == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {

    unsigned long long value;

    if (sscanf(line, "MemTotal: %llu kB", &value) == 1) {
      mem_total = value;
    } else if (sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
      mem_available = value;
    } else if (sscanf(line, "MemFree: %llu kB", &value) == 1) {
      mem_free = value;
    } else if (sscanf(line, "SwapTotal: %llu kB", &value) == 1) {
      swap_total = value;
    } else if (sscanf(line, "SwapFree: %llu kB", &value) == 1) {
      swap_free = value;
    }
  }

  fclose(fp);

  if (mem_total == 0) {
    return -1;
  }

  available = (mem_available > 0) ? mem_available : mem_free;

  *mem_percent = 100.0 * (1.0 - ((double)available / (double)mem_total));

  *swap_percent = (swap_total > 0)
                      ? 100.0 * (1.0 - ((double)swap_free / (double)swap_total))
                      : 0.0;

  if (*mem_percent < 0.0) {
    *mem_percent = 0.0;
  }

  if (*mem_percent > 100.0) {
    *mem_percent = 100.0;
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Series management                                                         */
/* ------------------------------------------------------------------------- */

void capture_series_init(NeoCaptureSeries *series) {

  if (series == NULL) {
    return;
  }

  series->items = NULL;
  series->count = 0;
  series->capacity = 0;
  series->start_time = 0;
}

void capture_series_free(NeoCaptureSeries *series) {

  if (series == NULL) {
    return;
  }

  free(series->items);

  series->items = NULL;
  series->count = 0;
  series->capacity = 0;
}

static int capture_series_add(NeoCaptureSeries *series,
                              const NeoCaptureSample *sample) {

  if (series->count >= series->capacity) {

    const size_t new_capacity =
        series->capacity == 0 ? INITIAL_CAPACITY : series->capacity * 2;

    NeoCaptureSample *tmp = realloc(series->items, new_capacity * sizeof(*tmp));

    if (tmp == NULL) {
      return -1;
    }

    series->items = tmp;
    series->capacity = new_capacity;
  }

  series->items[series->count] = *sample;
  ++series->count;

  return 0;
}

int capture_sample(NeoCaptureSeries *series, const NeoProcessList *list) {
  NeoCaptureSample sample;
  double mem_percent = 0.0;
  double swap_percent = 0.0;
  double read_rate = 0.0;
  double write_rate = 0.0;
  size_t i;

  if (series == NULL || list == NULL) {
    return -1;
  }

  memset(&sample, 0, sizeof(sample));

  if (series->count == 0) {
    series->start_time = time(NULL);
  }

  sample.elapsed_seconds = difftime(time(NULL), series->start_time);
  sample.cpu_percent = read_cpu_percent();

  /* Best-effort: leaves 0.0/0.0 on failure rather than aborting the
   * whole capture over one unreadable /proc/meminfo read. */
  (void)read_mem_percent(&mem_percent, &swap_percent);

  sample.mem_percent = mem_percent;
  sample.swap_percent = swap_percent;

  for (i = 0; i < list->count; ++i) {
    read_rate += list->items[i].io_read_mb_s;
    write_rate += list->items[i].io_write_mb_s;
  }

  sample.io_read_mb_s = read_rate;
  sample.io_write_mb_s = write_rate;
  sample.process_count = list->count;

  return capture_series_add(series, &sample);
}

int capture_append_sample(NeoCaptureSeries *series, double cpu_percent,
                          double mem_percent, double swap_percent,
                          double io_read_mb_s, double io_write_mb_s,
                          size_t process_count) {
  NeoCaptureSample sample;

  if (series == NULL) {
    return -1;
  }

  memset(&sample, 0, sizeof(sample));

  if (series->count == 0) {
    series->start_time = time(NULL);
  }

  sample.elapsed_seconds = difftime(time(NULL), series->start_time);
  sample.cpu_percent = cpu_percent;
  sample.mem_percent = mem_percent;
  sample.swap_percent = swap_percent;
  sample.io_read_mb_s = io_read_mb_s;
  sample.io_write_mb_s = io_write_mb_s;
  sample.process_count = process_count;

  return capture_series_add(series, &sample);
}

/* ------------------------------------------------------------------------- */
/* CSV export                                                                */
/* ------------------------------------------------------------------------- */

int capture_write_csv(const NeoCaptureSeries *series, const char *path) {
  FILE *fp;
  size_t i;

  if (series == NULL || path == NULL) {
    return -1;
  }

  fp = fopen(path, "w");

  if (fp == NULL) {
    return -1;
  }

  fprintf(fp, "elapsed_seconds,cpu_percent,mem_percent,swap_percent,"
              "io_read_mb_s,io_write_mb_s,process_count\n");

  for (i = 0; i < series->count; ++i) {

    const NeoCaptureSample *s = &series->items[i];

    fprintf(fp, "%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%zu\n", s->elapsed_seconds,
            s->cpu_percent, s->mem_percent, s->swap_percent, s->io_read_mb_s,
            s->io_write_mb_s, s->process_count);
  }

  fclose(fp);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Single-sample reader (for live/rolling displays)                         */
/* ------------------------------------------------------------------------- */

void capture_read_system(double *cpu_percent, double *mem_percent,
                         double *swap_percent) {
  const double cpu = read_cpu_percent();
  double mem = 0.0;
  double swap = 0.0;

  (void)read_mem_percent(&mem, &swap);

  if (cpu_percent != NULL) {
    *cpu_percent = cpu;
  }

  if (mem_percent != NULL) {
    *mem_percent = mem;
  }

  if (swap_percent != NULL) {
    *swap_percent = swap;
  }
}

/* ------------------------------------------------------------------------- */
/* HTML report export                                                        */
/* ------------------------------------------------------------------------- */

int capture_write_html_report(const NeoCaptureSeries *series,
                              const char *path) {
  FILE *fp;
  size_t i;
  double total_seconds;

  if (series == NULL || path == NULL) {
    return -1;
  }

  fp = fopen(path, "w");

  if (fp == NULL) {
    return -1;
  }

  total_seconds = (series->count > 0)
                      ? series->items[series->count - 1].elapsed_seconds
                      : 0.0;

  fprintf(
      fp,
      "<!DOCTYPE html>\n"
      "<html lang=\"en\">\n"
      "<head>\n"
      "<meta charset=\"UTF-8\">\n"
      "<title>NEO Monitoring Services - Capture Report</title>\n"
      "<script "
      "src=\"https://cdn.jsdelivr.net/npm/chart.js@4.4.4/dist/"
      "chart.umd.min.js\"></script>\n"
      "<style>\n"
      "  body { font-family: -apple-system, 'Segoe UI', Roboto, sans-serif; "
      "background:#f4f6f9; color:#1c2430; margin:0; padding:24px; }\n"
      "  h1 { font-size: 20px; margin-bottom: 4px; }\n"
      "  .meta { color:#5a6472; font-size:13px; margin-bottom:20px; }\n"
      "  .controls { margin-bottom: 20px; }\n"
      "  select { padding:6px 10px; border-radius:6px; border:1px solid "
      "#d3d9e2; font-size:14px; }\n"
      "  .chart-card { background:#ffffff; border:1px solid #e0e4ea; "
      "border-radius:8px; padding:16px; margin-bottom:20px; }\n"
      "  .chart-card h2 { font-size:15px; margin:0 0 12px 0; "
      "color:#3a4453; }\n"
      "  canvas { max-height: 320px; }\n"
      "</style>\n"
      "</head>\n"
      "<body>\n"
      "<h1>NEO Monitoring Services - Capture Report</h1>\n"
      "<div class=\"meta\">%zu sample(s) captured over %.1f second(s)</div>\n"
      "<div class=\"controls\">\n"
      "  <label for=\"graphSelect\">Show: </label>\n"
      "  <select id=\"graphSelect\" onchange=\"neoShowGraph(this.value)\">\n"
      "    <option value=\"all\">All graphs</option>\n"
      "    <option value=\"cpu\">CPU only</option>\n"
      "    <option value=\"mem\">Memory only</option>\n"
      "    <option value=\"swap\">Swap only</option>\n"
      "    <option value=\"io\">I/O only</option>\n"
      "  </select>\n"
      "</div>\n"
      "<div id=\"card-cpu\" class=\"chart-card\"><h2>CPU Usage (%%)</h2>"
      "<canvas id=\"chartCpu\"></canvas></div>\n"
      "<div id=\"card-mem\" class=\"chart-card\"><h2>Memory Usage (%%)</h2>"
      "<canvas id=\"chartMem\"></canvas></div>\n"
      "<div id=\"card-swap\" class=\"chart-card\"><h2>Swap Usage (%%)</h2>"
      "<canvas id=\"chartSwap\"></canvas></div>\n"
      "<div id=\"card-io\" class=\"chart-card\">"
      "<h2>Aggregate Process I/O (MB/s)</h2>"
      "<canvas id=\"chartIo\"></canvas></div>\n"
      "<script>\n",
      series->count, total_seconds);

  fprintf(fp, "const neoLabels = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.1f", (i == 0 ? "" : ","),
            series->items[i].elapsed_seconds);
  }
  fprintf(fp, "];\n");

  fprintf(fp, "const neoCpu = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.2f", (i == 0 ? "" : ","), series->items[i].cpu_percent);
  }
  fprintf(fp, "];\n");

  fprintf(fp, "const neoMem = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.2f", (i == 0 ? "" : ","), series->items[i].mem_percent);
  }
  fprintf(fp, "];\n");

  fprintf(fp, "const neoSwap = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.2f", (i == 0 ? "" : ","), series->items[i].swap_percent);
  }
  fprintf(fp, "];\n");

  fprintf(fp, "const neoIoRead = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.3f", (i == 0 ? "" : ","), series->items[i].io_read_mb_s);
  }
  fprintf(fp, "];\n");

  fprintf(fp, "const neoIoWrite = [");
  for (i = 0; i < series->count; ++i) {
    fprintf(fp, "%s%.3f", (i == 0 ? "" : ","), series->items[i].io_write_mb_s);
  }
  fprintf(fp, "];\n");

  fprintf(fp,
          "function neoMakeChart(id, label, data, color) {\n"
          "  return new Chart(document.getElementById(id), {\n"
          "    type: 'line',\n"
          "    data: { labels: neoLabels, datasets: [{ label: label, data: "
          "data, borderColor: color, backgroundColor: color + '33', fill: "
          "true, tension: 0.2, pointRadius: 0 }] },\n"
          "    options: { responsive: true, scales: { x: { title: { display: "
          "true, text: 'Elapsed (s)' } }, y: { beginAtZero: true } }, "
          "plugins: { legend: { display: true } } }\n"
          "  });\n"
          "}\n"
          "neoMakeChart('chartCpu', 'CPU %%', neoCpu, '#2f6fed');\n"
          "neoMakeChart('chartMem', 'Memory %%', neoMem, '#e0574c');\n"
          "neoMakeChart('chartSwap', 'Swap %%', neoSwap, '#d9a441');\n"
          "new Chart(document.getElementById('chartIo'), {\n"
          "  type: 'line',\n"
          "  data: { labels: neoLabels, datasets: [\n"
          "    { label: 'Read MB/s', data: neoIoRead, borderColor: "
          "'#2fa84f', backgroundColor: '#2fa84f33', fill: true, tension: "
          "0.2, pointRadius: 0 },\n"
          "    { label: 'Write MB/s', data: neoIoWrite, borderColor: "
          "'#8a4fe0', backgroundColor: '#8a4fe033', fill: true, tension: "
          "0.2, pointRadius: 0 }\n"
          "  ]},\n"
          "  options: { responsive: true, scales: { x: { title: { display: "
          "true, text: 'Elapsed (s)' } }, y: { beginAtZero: true } }, "
          "plugins: { legend: { display: true } } }\n"
          "});\n"
          "function neoShowGraph(which) {\n"
          "  const cards = { cpu: 'card-cpu', mem: 'card-mem', swap: "
          "'card-swap', io: 'card-io' };\n"
          "  Object.keys(cards).forEach(function(key) {\n"
          "    document.getElementById(cards[key]).style.display = (which === "
          "'all' || which === key) ? 'block' : 'none';\n"
          "  });\n"
          "}\n"
          "</script>\n"
          "</body>\n"
          "</html>\n");

  fclose(fp);

  return 0;
}
