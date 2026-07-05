# BIOS Monitor

Monitor de sistema em tempo real para Linux com estética **retro BIOS old school** (anos 90). Interface TUI construída com **ncurses**, coleta de dados em thread separada via **pthreads**.

```
┌─ Hardware Summary ─────────────┐ ┌─ System Status ────────────────┐
│ CPU Usage .............. 47%   │ │ sda1 .................... 62%  │
│ [████████░░░░░░░░░░░░░░░░]     │ │ R:12.3 W:4.1 MB/s on /         │
│ Model ........ Intel i7-12700K  │ │ eth0 .... DN:1024 UP:256 KB/s  │
│ Frequency ............. 4200MHz │ │ Load Avg .... 0.45 0.38 0.32   │
└────────────────────────────────┘ └────────────────────────────────┘
```

## Funcionalidades

- **Painel esquerdo (Hardware Summary):** CPU (modelo, frequência, uso total e por núcleo), temperatura, cache L2/L3, RAM/swap, GPU (NVML ou sysfs)
- **Painel direito (System Status):** discos (uso %, I/O), rede (upload/download), sensores, uptime, load average, top processos
- Barras de progresso ASCII, sparklines de histórico (60s)
- Modo **BIOS Cyan** (padrão) e **Hacker Green** (tecla `C`)
- Atualização a cada 1 segundo sem flicker

## Dependências

### Ubuntu / Debian / Mint

```bash
sudo apt install build-essential cmake pkg-config libncurses-dev libsensors-dev
```

Para GPU NVIDIA (opcional):

```bash
sudo apt install nvidia-driver-535  # ou driver adequado — inclui libnvidia-ml
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake ncurses lm_sensors
```

### Fedora

```bash
sudo dnf install gcc cmake make ncurses-devel lm_sensors-devel
```

## Compilação

```bash
cd bios-monitor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Sem NVML (apenas sysfs):

```bash
cmake -B build -DBIOS_MONITOR_NVML=OFF
cmake --build build
```

## Execução

```bash
./build/bios-monitor
```

Recomendado: terminal com pelo menos **100×30** colunas/linhas.

### Atalhos

| Tecla | Ação |
|-------|------|
| `Q` | Sair |
| `R` | Forçar refresh imediato |
| `C` | Alternar BIOS Cyan ↔ Hacker Green |
| `1` / `←` | Selecionar painel esquerdo |
| `2` / `→` | Selecionar painel direito |

## Fontes de dados

| Módulo | Fonte |
|--------|-------|
| CPU | `/proc/stat`, `/proc/cpuinfo`, `/sys/devices/system/cpu/` |
| Memória | `/proc/meminfo` |
| GPU | NVML (`libnvidia-ml`) ou `/sys/class/drm/` |
| Rede | `/proc/net/dev` |
| Disco | `/proc/mounts`, `statvfs`, `/proc/diskstats` |
| Sensores | `libsensors`, `/sys/class/thermal/` |
| Processos | `/proc/[pid]/stat` |
| Sistema | `sysinfo(2)` |

## Estrutura do projeto

```
bios-monitor/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── common.h
│   ├── cpu.h, memory.h, gpu.h, network.h
│   ├── disk.h, sensors.h, process.h
│   ├── data.h, ui.h, util.h
└── src/
    ├── main.c
    ├── cpu.c, memory.c, gpu.c, network.c
    ├── disk.c, sensors.c, process.c
    ├── data.c, ui.c, util.c
```

## Notas de robustez

- **Sem alocação dinâmica** — structs fixas na stack/estado global; sem `malloc`/`free`
- **`BIOS_STRNCPY`** — macro que sempre null-termina strings copiadas
- **Leitura única por ciclo** — `/proc/stat`, `/proc/meminfo` e `/proc/diskstats` abertos uma vez por segundo
- **Processos** — CPU calculada só para os top 200 por RAM; `proc_prev` compactado removendo PIDs mortos
- **Sleep** — `nanosleep()` com retry em `EINTR` via `bios_sleep_ms()`
- **UI** — redesenha só quando há snapshot novo, input do usuário ou tick de 1 Hz (relógio do header)

## Licença

MIT — use e modifique livremente.
