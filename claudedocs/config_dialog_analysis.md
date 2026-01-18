# MBTOP Config Dialog Redesign Analysis v2

## Design Philosophy
- **4 Main Categories** instead of 10 (simpler navigation)
- **Visual Preset Builder** with graphical layout editor
- **No sub-category carousel needed** - fewer categories = direct access
- **Proper controls**: Toggles, Selects, Radio buttons (no True/False text)

---

## Category Structure

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              MBTOP SETTINGS                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│   [ System ]     [ Appearance ]     [ Panels ]     [ Presets ]              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Category 1: SYSTEM ⚙️
General system and input settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Update** | | | |
| Update Interval | `update_ms` | Select | 500ms / 1s / 2s / 3s / 5s / 10s |
| Background Update | `background_update` | Toggle | |
| Terminal Sync | `terminal_sync` | Toggle | |
| **Input** | | | |
| Vim Keys (hjkl) | `vim_keys` | Toggle | |
| Disable Mouse | `disable_mouse` | Toggle | |
| **Units** | | | |
| Base 10 Sizes (KB vs KiB) | `base_10_sizes` | Toggle | |
| Bitrate Units | `base_10_bitrate` | Radio | Auto / Base 10 / Binary |
| **Logging** | | | |
| Log Level | `log_level` | Radio | ERROR / WARNING / INFO / DEBUG |
| **Config** | | | |
| Save on Exit | `save_config_on_exit` | Toggle | |

---

## Category 2: APPEARANCE 🎨
Visual styling and theme settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Theme** | | | |
| Color Theme | `color_theme` | Select | (from themes folder) |
| Show Theme Background | `theme_background` | Toggle | |
| **Colors** | | | |
| True Color (24-bit) | `truecolor` | Toggle | |
| Force TTY Mode | `force_tty` | Toggle | |
| **Borders** | | | |
| Rounded Corners | `rounded_corners` | Toggle | |
| **Default Graph Style** | | | |
| Graph Symbol | `graph_symbol` | Radio | Braille / Block / TTY |
| **Title Bar** | | | |
| Clock Format | `clock_format` | Select | HH:MM:SS / HH:MM / 12h AM/PM / Locale / Custom |
| 12-Hour Clock | `clock_12h` | Toggle | |
| Show Hostname | `show_hostname` | Toggle | |
| Show Uptime in Header | `show_uptime_header` | Toggle | |
| Show Username in Header | `show_username_header` | Toggle | |
| **Battery** | | | |
| Show Battery | `show_battery` | Toggle | |
| Battery Selection | `selected_battery` | Select | (detected batteries) |
| Show Battery Power | `show_battery_watts` | Toggle | |

---

## Category 3: PANELS 📊
Per-panel configuration (sub-tabs for each panel type)

### Panel Sub-Tabs
```
┌─────────────────────────────────────────────────────────────────────────────┐
│   [ CPU ]  [ GPU ]  [ PWR ]  [ Memory ]  [ Network ]  [ Processes ]         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### CPU Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Display** | | | |
| Show Uptime | `show_uptime` | Toggle | |
| Show CPU Frequency | `show_cpu_freq` | Toggle | |
| Frequency Mode *(Linux)* | `freq_mode` | Radio | First / Range / Lowest / Highest / Average |
| Custom CPU Name | `custom_cpu_name` | Text | (optional) |
| **Power** | | | |
| Show CPU Power | `show_cpu_watts` | Toggle | |
| **Temperature** | | | |
| Show Temperature | `check_temp` | Toggle | |
| Show Core Temps | `show_coretemp` | Toggle | |
| Temperature Scale | `temp_scale` | Radio | Celsius / Fahrenheit / Kelvin / Rankine |
| Temperature Sensor | `cpu_sensor` | Select | (detected sensors) |
| Core Temp Mapping | `cpu_core_map` | Text | (advanced, optional) |
| **Graphs** | | | |
| Graph Symbol | `graph_symbol_cpu` | Radio | Default / Braille / Block / TTY |
| Upper Graph Stat | `cpu_graph_upper` | Select | (detected stats) |
| Lower Graph Stat | `cpu_graph_lower` | Select | (detected stats) |
| Single Graph Mode | `cpu_single_graph` | Toggle | |
| Invert Lower Graph | `cpu_invert_lower` | Toggle | |
| **GPU Info in CPU Box** | | | |
| Show GPU Info | `show_gpu_info` | Radio | Auto / On / Off |

### GPU Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Display** | | | |
| Shown GPU Vendors | `shown_gpus` | Multi-Select | nvidia / amd / intel |
| Mirror GPU Graph | `gpu_mirror_graph` | Toggle | |
| Graph Symbol | `graph_symbol_gpu` | Radio | Default / Braille / Block / TTY |
| **Custom Names** | | | |
| GPU 0-5 Names | `custom_gpu_name0-5` | Text | (optional each) |
| **PCIe Monitoring** | | | |
| NVIDIA PCIe Speeds | `nvml_measure_pcie_speeds` | Toggle | |
| AMD PCIe Speeds | `rsmi_measure_pcie_speeds` | Toggle | |

### PWR Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| Graph Symbol | `graph_symbol_pwr` | Radio | Default / Braille / Block / TTY |

### Memory Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Display Mode** | | | |
| Use Graphs | `mem_graphs` | Toggle | |
| Use Bar Mode | `mem_bar_mode` | Toggle | |
| Graph Symbol | `graph_symbol_mem` | Radio | Default / Braille / Block / TTY |
| **Memory Items** | | | |
| Show Used | `mem_show_used` | Toggle | |
| Show Available | `mem_show_available` | Toggle | |
| Show Cached | `mem_show_cached` | Toggle | |
| Show Free | `mem_show_free` | Toggle | |
| **Swap** | | | |
| Show Swap | `show_swap` | Toggle | |
| Show as Disk | `swap_disk` | Toggle | |
| Show Swap Used | `swap_show_used` | Toggle | |
| Show Swap Free | `swap_show_free` | Toggle | |
| **VRAM** | | | |
| Show VRAM Used | `vram_show_used` | Toggle | |
| Show VRAM Free | `vram_show_free` | Toggle | |
| **ZFS** | | | |
| Count ARC as Cached | `zfs_arc_cached` | Toggle | |
| **Disks** | | | |
| Show Disks | `show_disks` | Toggle | |
| Show IO Activity | `show_io_stat` | Toggle | |
| IO Graph Mode | `io_mode` | Toggle | |
| Combined IO Graph | `io_graph_combined` | Toggle | |
| Physical Disks Only | `only_physical` | Toggle | |
| Show Network Drives | `show_network_drives` | Toggle | |
| Use /etc/fstab | `use_fstab` | Toggle | |
| Hide ZFS Datasets | `zfs_hide_datasets` | Toggle | |
| Disk Filter | `disks_filter` | Text | (optional) |
| IO Graph Speeds | `io_graph_speeds` | Text | (optional) |
| Show Privileged Space | `disk_free_priv` | Toggle | |

### Network Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Display** | | | |
| Graph Symbol | `graph_symbol_net` | Radio | Default / Braille / Block / TTY |
| Auto Scale Graphs | `net_auto` | Toggle | |
| Sync Upload/Download | `net_sync` | Toggle | |
| Swap Up/Down Position | `swap_upload_download` | Toggle | |
| **Speed Limits** | | | |
| Download Limit (MiB/s) | `net_download` | Slider | 10-1000 |
| Upload Limit (MiB/s) | `net_upload` | Slider | 10-1000 |
| **Interface** | | | |
| Network Interface | `net_iface` | Select | (detected interfaces) |

### Processes Panel Settings

| Option | Config Key | Input Type | Values |
|--------|------------|------------|--------|
| **Sorting** | | | |
| Sort By | `proc_sorting` | Select | pid / program / arguments / threads / user / memory / cpu lazy / cpu direct / gpu |
| Reverse Order | `proc_reversed` | Toggle | |
| **Display** | | | |
| Graph Symbol | `graph_symbol_proc` | Radio | Default / Braille / Block / TTY |
| Tree View | `proc_tree` | Toggle | |
| Use CPU Colors | `proc_colors` | Toggle | |
| Gradient Background | `proc_gradient` | Toggle | |
| Per-Core CPU % | `proc_per_core` | Toggle | |
| **Columns** | | | |
| Show Command Column | `proc_show_cmd` | Toggle | |
| Memory as Bytes | `proc_mem_bytes` | Toggle | |
| CPU Mini Graphs | `proc_cpu_graphs` | Toggle | |
| GPU Mini Graphs | `proc_gpu_graphs` | Toggle | |
| **GPU** | | | |
| Show GPU Usage | `proc_gpu` | Toggle | |
| **Advanced** | | | |
| Use /proc/smaps | `proc_info_smaps` | Toggle | |
| Filter Kernel Processes | `proc_filter_kernel` | Toggle | |
| Aggregate Children | `proc_aggregate` | Toggle | |
| **Behavior** | | | |
| Follow Selected in Detail | `proc_follow_detailed` | Toggle | |
| Keep Dead Process Stats | `keep_dead_proc_usage` | Toggle | |

---

## Category 4: PRESETS 🎛️
**Visual Preset Builder** - The star feature!

### Preset List View
```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ─ Presets ──────────────────────────────────────────────────────────  [New] │
│                                                                              │
│   1. Standard          [Edit] [Delete]                                       │
│   2. LLM Monitor       [Edit] [Delete]                                       │
│   3. Processes Only    [Edit] [Delete]                                       │
│   4. Power Focus       [Edit] [Delete]                                       │
│   5. CPU/MEM           [Edit] [Delete]                                       │
│   6. (empty)           [Edit] [Delete]                                       │
│   7. (empty)           [Edit] [Delete]                                       │
│   8. (empty)           [Edit] [Delete]                                       │
│   9. (empty)           [Edit] [Delete]                                       │
│                                                                              │
│   Tip: Press 1-9 in main view to quickly switch presets                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Visual Preset Editor (when clicking Edit or New)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ─ Edit Preset: "Standard" ─────────────────────────────────────────────────│
│                                                                              │
│  Preset Name: [Standard____________]                          [New]          │
│                                                                              │
│  ┌──── Panels ────┐     ┌──────────────── Preview ──────────────────┐       │
│  │                │     │ ┌──────────────────────────────────────┐  │       │
│  │  [✓] CPU       │     │ │               CPU                    │  │       │
│  │  [✓] GPU       │     │ ├──────────────────────────────────────┤  │       │
│  │  [✓] PWR       │     │ │               PWR                    │  │       │
│  │  ────────────  │     │ ├─────────────────┬────────────────────┤  │       │
│  │  MEM Layout:   │     │ │                 │                    │  │       │
│  │  ◉ Horizontal  │     │ │     MEM H       │      NET H         │  │       │
│  │  ○ Vertical    │     │ │                 │                    │  │       │
│  │  ────────────  │     │ ├─────────────────┴────────────────────┤  │       │
│  │  NET Layout:   │     │ │                                      │  │       │
│  │  ◉ Horizontal  │     │ │             PROC H                   │  │       │
│  │  ○ Vertical    │     │ │                                      │  │       │
│  │  ────────────  │     │ └──────────────────────────────────────┘  │       │
│  │  PROC Layout:  │     └───────────────────────────────────────────┘       │
│  │  ○ Vertical    │                                                          │
│  │  ◉ Horizontal  │     Graph Style:  ◉ Default  ○ Braille  ○ Block  ○ TTY  │
│  │                │                                                          │
│  │  Disk Mode:    │                         [Cancel]  [Save]                │
│  │  ○ Hidden      │                                                          │
│  │  ◉ Bar/Meter   │                                                          │
│  │  ○ Graph       │                                                          │
│  └────────────────┘                                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Panel Layout Definitions

| Code | Panel | Description |
|------|-------|-------------|
| **CPU** | CPU | Always top, full width |
| **GPU** | GPU | Under CPU (if enabled) |
| **PWR** | Power | Under GPU/CPU |
| **MEM H** | Memory Horizontal | Memory graphs side-by-side, disks hidden |
| **MEM V** | Memory Vertical | Memory with disk panel, stacked |
| **NET H** | Network Horizontal | Under memory (when MEM H) |
| **NET V** | Network Vertical | Next to memory (when MEM V) |
| **PROC H** | Processes Horizontal | Full width at bottom |
| **PROC V** | Processes Vertical | Right side column |

### Layout Combinations (Visual Examples)

**Layout 1: Full Stack (Default)**
```
┌────────────────────────────────┐
│             CPU                │
├────────────────────────────────┤
│             PWR                │
├───────────────┬────────────────┤
│    MEM H      │     NET H      │
├───────────────┴────────────────┤
│           PROC H               │
└────────────────────────────────┘
```

**Layout 2: Vertical Split**
```
┌────────────────────────────────┐
│             CPU                │
├───────────────┬────────────────┤
│               │                │
│    MEM V      │    PROC V      │
│               │                │
│   ─────────   │                │
│    NET V      │                │
│               │                │
└───────────────┴────────────────┘
```

**Layout 3: Process Focus**
```
┌────────────────────────────────┐
│             CPU                │
├────────────────────────────────┤
│                                │
│           PROC H               │
│                                │
└────────────────────────────────┘
```

**Layout 4: Memory Focus**
```
┌────────────────────────────────┐
│             CPU                │
├────────────────────────────────┤
│                                │
│    MEM V (with disks)          │
│                                │
└────────────────────────────────┘
```

### Preset Data Structure (Internal)

```cpp
struct Preset {
    string name;              // User-defined name
    bool cpu_enabled;         // CPU always recommended
    bool gpu_enabled;         // GPU panel
    bool pwr_enabled;         // Power panel

    enum class MemLayout { Hidden, Horizontal, Vertical } mem_layout;
    enum class DiskMode { Hidden, BarMeter, Graph } disk_mode;
    enum class NetLayout { Hidden, Horizontal, Vertical } net_layout;
    enum class ProcLayout { Hidden, Horizontal, Vertical } proc_layout;

    string graph_symbol;      // default/braille/block/tty
};
```

### Preset Config String Format (for storage)
```
# Format: name|cpu|gpu|pwr|mem_layout|disk_mode|net_layout|proc_layout|graph
# Example presets:
preset1=Standard|1|0|1|H|B|H|H|default
preset2=LLM Monitor|1|1|1|V|H|V|V|braille
preset3=Processes|1|0|0|H|H|H|H|default
preset4=Power Focus|1|1|1|H|H|H|V|block
preset5=CPU/MEM|1|0|0|V|B|H|H|default
```

---

## UI Navigation

### Main Dialog
```
[Esc] Close    [Tab] Next Category    [Shift+Tab] Prev Category
[↑/↓] Select   [←/→] Change Value     [Enter] Edit/Toggle
```

### Preset Editor
```
[Esc] Cancel   [Enter] Save           [Tab] Next Field
[↑/↓] Select   [Space] Toggle         [←/→] Radio Select
```

---

## Implementation Plan

### Phase 1: Dialog Framework
1. New vertical dialog layout
2. 4-category tab bar
3. Scrollable content area
4. Help line at bottom

### Phase 2: Control Components
1. Toggle switch component
2. Radio button group component
3. Select dropdown component
4. Slider component

### Phase 3: Category Views
1. System category view
2. Appearance category view
3. Panels category with sub-tabs

### Phase 4: Preset Builder
1. Preset list view
2. Visual layout preview component
3. Panel selection with layout radio buttons
4. Real-time preview updates
5. Save/load preset logic

### Phase 5: Polish
1. Keyboard navigation
2. Mouse support
3. Theme integration
4. Validation & error handling

---

## Summary of Changes

| Aspect | Old | New |
|--------|-----|-----|
| Categories | 10+ scattered | 4 focused |
| Navigation | Pagination | Tabs + sub-tabs |
| Boolean inputs | "True/False" text | Toggle switches |
| Preset config | Cryptic string | Visual builder |
| Layout | Horizontal | Vertical |
| Help | Per-option in list | Single line at bottom |
