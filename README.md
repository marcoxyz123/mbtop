<p align="center">
  <img src="Img/logo.png" alt="mbtop logo">
</p>

<h3 align="center">The Apple Silicon System Monitor</h3>

<p align="center">
  <b>mbtop</b> — A powerful terminal-based system monitor optimized for Apple Silicon Macs
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#themes">Themes</a> •
  <a href="#license">License</a>
</p>

---

## Features

**mbtop** brings comprehensive Apple Silicon monitoring to your terminal:

### Apple Silicon Exclusive
- **GPU Monitoring** — Real-time GPU utilization, frequency, and temperature
- **Power Monitoring** — CPU, GPU, and ANE power draw in watts
- **Apple Neural Engine (ANE)** — Monitor ANE utilization
- **VRAM Tracking** — Video memory usage for GPU workloads
- **Unified Memory** — Full memory bandwidth and pressure monitoring

### Core Features
- **CPU Monitoring** — Per-core usage, frequency, and temperature
- **Memory** — Used, available, cached, with visual graphs
- **Disk I/O** — Read/write speeds and usage per disk
- **Network** — Upload/download speeds with graphs
- **Process List** — Sortable, filterable, with tree view
- **Responsive UI** — Adapts gracefully to terminal size

### User Experience
- **Themes** — Multiple built-in themes + custom theme support
- **Mouse Support** — Full mouse interaction
- **Vim Keys** — Navigate with h/j/k/l
- **Customizable** — Extensive configuration options

---

## Screenshots

<p align="center">
  <img src="Img/mbtop-1.png" alt="mbtop main view" width="100%">
</p>

<p align="center">
  <img src="Img/mbtop-2.png" alt="mbtop with GPU monitoring" width="100%">
</p>

<p align="center">
  <img src="Img/mbtop-3.png" alt="mbtop power monitoring" width="100%">
</p>

<p align="center">
  <img src="Img/mbtop-4.png" alt="mbtop process view" width="100%">
</p>

<p align="center">
  <img src="Img/mbtop-5.png" alt="mbtop compact view" width="100%">
</p>

---

## Installation

### macOS (Apple Silicon)

#### Homebrew (Coming Soon)
```bash
brew install mbtop
```

#### Manual Installation
```bash
# Clone the repository
git clone https://github.com/marcoxyz123/mbtop.git
cd mbtop

# Build
make

# Install (optional)
sudo make install
```

### Linux

#### From Source
```bash
git clone https://github.com/marcoxyz123/mbtop.git
cd mbtop
make
sudo make install
```

> **Note:** Apple Silicon specific features (GPU, Power, ANE) are only available on macOS.

---

## Requirements

### macOS
- macOS 12.0 (Monterey) or later
- Apple Silicon (M1/M2/M3/M4) recommended
- Xcode Command Line Tools

### Linux
- GCC 10+ or Clang 16+
- GNU Make
- C++23 support

---

## Configuration

Configuration file location: `~/.config/mbtop/mbtop.conf`

### Key Options

```ini
# Color theme
color_theme = "Default"

# Update time in milliseconds
update_ms = 1000

# Show CPU graph
show_cpu_graph = true

# Show GPU info (macOS only)
show_gpu_info = true

# Show power stats (Apple Silicon only)
show_power_stats = true

# Process sorting
proc_sorting = "cpu lazy"

# Tree view
proc_tree = false
```

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `h` `j` `k` `l` | Vim-style navigation |
| `↑` `↓` `←` `→` | Arrow navigation |
| `Enter` | Expand/collapse in tree view |
| `t` | Toggle tree view |
| `r` | Reverse sort order |
| `f` | Filter processes |
| `c` | Toggle per-core CPU |
| `g` | Toggle GPU info |
| `m` | Cycle memory display |
| `n` | Cycle network display |
| `d` | Cycle disk display |
| `p` | Toggle power stats |
| `q` `Esc` | Quit |
| `?` `F1` | Help menu |

---

## Themes

mbtop includes several built-in themes:

- **Default** — Balanced colors for most terminals
- **TTY** — High contrast for TTY/console
- **Dracula** — Popular dark theme
- **Nord** — Arctic, bluish theme
- **Gruvbox** — Retro groove colors
- **OneDark** — Atom One Dark inspired

Custom themes can be placed in `~/.config/mbtop/themes/`

---

## Comparison with Other Tools

### System Monitoring

| Feature | mbtop | htop | btop | mactop |
|---------|-------|------|------|--------|
| Apple Silicon GPU | ✅ | ❌ | ❌ | ✅ |
| GPU Power (Watts) | ✅ | ❌ | ❌ | ❌ |
| CPU Power (Watts) | ✅ | ❌ | ❌ | ❌ |
| ANE Monitoring | ✅ | ❌ | ❌ | ❌ |
| VRAM Tracking | ✅ | ❌ | ❌ | ❌ |
| Per-Core CPU | ✅ | ✅ | ✅ | ✅ |
| CPU Temperature | ✅ | ❌ | ✅ | ✅ |
| Memory Details | ✅ | ✅ | ✅ | Basic |
| Swap Usage | ✅ | ✅ | ✅ | ❌ |
| Network I/O | ✅ | ❌ | ✅ | ❌ |
| Disk I/O | ✅ | ❌ | ✅ | ❌ |

### Process Management

| Feature | mbtop | htop | btop | mactop |
|---------|-------|------|------|--------|
| Full Process List | ✅ | ✅ | ✅ | Limited |
| Process Tree View | ✅ | ✅ | ✅ | ❌ |
| Process I/O Stats | ✅ | ✅ | ✅ | ❌ |
| Process Memory Details | ✅ | ✅ | ✅ | Basic |
| Process Filtering | ✅ | ✅ | ✅ | ❌ |
| Process Kill/Signal | ✅ | ✅ | ✅ | ❌ |
| Process Nice/Priority | ✅ | ✅ | ✅ | ❌ |
| Per-Process GPU | ✅ | ❌ | ❌ | ❌ |
| Custom Sort Fields | ✅ | ✅ | ✅ | ❌ |
| Process User Filter | ✅ | ✅ | ✅ | ❌ |

### User Interface

| Feature | mbtop | htop | btop | mactop |
|---------|-------|------|------|--------|
| Themes | ✅ | ❌ | ✅ | ❌ |
| Custom Themes | ✅ | ❌ | ✅ | ❌ |
| Mouse Support | ✅ | ✅ | ✅ | ❌ |
| Vim Keybindings | ✅ | ❌ | ✅ | ❌ |
| Responsive Layout | ✅ | ✅ | ✅ | ❌ |
| Unicode Graphs | ✅ | ❌ | ✅ | ❌ |
| Box Customization | ✅ | ✅ | ✅ | ❌ |

### Platform Support

| Feature | mbtop | htop | btop | mactop |
|---------|-------|------|------|--------|
| macOS (Apple Silicon) | ✅ | ✅ | ✅ | ✅ |
| macOS (Intel) | ✅ | ✅ | ✅ | ❌ |
| Linux | ✅ | ✅ | ✅ | ❌ |
| FreeBSD | ✅ | ✅ | ✅ | ❌ |
| NetBSD/OpenBSD | ✅ | ✅ | ✅ | ❌ |

---

## Building from Source

### Dependencies

**macOS:**
```bash
xcode-select --install
```

**Linux (Debian/Ubuntu):**
```bash
sudo apt install build-essential gcc-12 g++-12
```

**Linux (Fedora):**
```bash
sudo dnf install gcc-c++ make
```

### Build Options

```bash
# Standard build
make

# Debug build
make DEBUG=true

# Verbose build
make VERBOSE=true

# Clean build
make clean && make
```

---

## Credits

mbtop is a fork of [btop++](https://github.com/aristocratos/btop) by aristocratos, enhanced with comprehensive Apple Silicon support.

**Author:** Marco Berger

---

## License

[Apache License 2.0](LICENSE)

```
Copyright 2024-2025 Marco Berger
Copyright 2021 aristocratos

Licensed under the Apache License, Version 2.0
```
