# mbtop Config Modernization & Application Logs - Implementation Plan

## Document Version
- **Created**: 2025-01-22
- **Branch**: `feature/logs-panel`
- **Last Commit**: `c4df7cd` - "fix: comprehensive Logs panel bug fixes and improvements"
- **Status**: Planning complete, ready for implementation

---

## Table of Contents
1. [Project Overview](#1-project-overview)
2. [Configuration Migration: INI → TOML](#2-configuration-migration-ini--toml)
3. [Data Structure Changes](#3-data-structure-changes)
4. [Implementation Phases](#4-implementation-phases)
5. [UI Mockups](#5-ui-mockups)
6. [Key Bindings](#6-key-bindings)
7. [File Change Summary](#7-file-change-summary)
8. [Open Questions](#8-open-questions)
9. [Continuation Prompt](#9-continuation-prompt)

---

## 1. Project Overview

**mbtop** is a macOS system monitor forked from btop++ with comprehensive Apple Silicon support. This document covers the planned migration from INI-style config (`mbtop.conf`) to TOML format (`mbtop.toml`) and the addition of application-specific log viewing capabilities.

### 1.1 Current State
- Bug fixes for Logs panel committed (`c4df7cd`)
- Config uses flat `unordered_map<string_view, T>` for `strings`, `bools`, `ints`
- Logs panel shows macOS unified logging (system logs) only
- No support for viewing application log files

### 1.2 Goals
1. Migrate config from INI to TOML format
2. Add per-process log file configuration
3. Enable viewing application logs alongside system logs
4. Add Log Config modal in Process Details view
5. Show custom display names and tag colors in process list

### 1.3 Non-Goals (Out of Scope)
- Backward compatibility with INI format (clean break)
- Remote log viewing
- Log aggregation from multiple files

---

## 2. Configuration Migration: INI → TOML

### 2.1 Rationale
The current `mbtop.conf` uses simple `key=value` format that cannot handle:
- Nested structures (logging.applications, logging.processes)
- Arrays of complex objects (ProcessLogConfig)
- Hierarchical organization

### 2.2 Decision
**Clean break** - TOML only, no backward compatibility period. One-time migration from INI to TOML.

### 2.3 File Locations
| File | Purpose |
|------|---------|
| `~/.config/mbtop/mbtop.toml` | New TOML config (primary) |
| `~/.config/mbtop/mbtop.conf` | Legacy INI config (migrated on first run) |
| `~/.config/mbtop/mbtop.conf.bak` | Backup of migrated INI |

### 2.4 Library Choice
**toml++** (header-only C++17 library)
- Repository: https://github.com/marzer/tomlplusplus
- Single header: `toml.hpp` (~1MB, ~28k lines)
- Drop into `include/toml.hpp`
- No build system changes required
- MIT License

### 2.5 New TOML Config Structure

```toml
#? Config file for mbtop v.X.X.X

[general]
update_ms = 2000
vim_keys = false
rounded_corners = true
terminal_sync = true
disable_mouse = false
# ... all existing general settings

[appearance]
color_theme = "Default"
theme_background = true
truecolor = true
graph_symbol = "braille"
# ... all existing appearance settings

[cpu]
graph_upper = "Auto"
graph_lower = "Auto"
single_graph = false
invert_lower = true
show_coretemp = true
bottom = false
# ... all CPU-related settings

[gpu]
bottom = false
mirror_graph = true
shown_gpus = "apple"
# ... all GPU-related settings

[power]
bottom = false
# ... power panel settings

[memory]
graphs = true
bar_mode = true
horizontal = true
show_disks = false
show_swap = true
below_net = false
# ... all memory-related settings

[network]
auto = true
sync = true
beside_mem = true
iface = ""
iface_filter = ""
graph_direction = 0
# ... all network-related settings

[process]
sorting = "cpu lazy"
reversed = false
tree = false
colors = true
gradient = true
per_core = false
left = false
full_width = false
tag_mode = "name"  # "name" = color program name only, "line" = color entire row
# ... all process-related settings

[logging]
level = "WARNING"
buffer_size = 500
export_path = "~/Desktop"
default_source = "system"  # "system" or "application"
color_full_line = false
below_proc = false

# Simple process name → log path mapping
[logging.applications]
mbtop = "~/.config/mbtop/mbtop.log"
nginx = "/var/log/nginx/error.log"

# Complex process matching with display customization
# tagged = visual highlighting in process list (independent of log settings)
# tag_color = color for highlighting (Nord Aurora: log_fault/error/info/debug_plus/debug)
[[logging.processes]]
name = "node"
command_pattern = ".*claude.*mcp.*"    # Regex match against cmdline
log_path = "~/.claude/logs/mcp.log"
display_name = "Claude MCP"            # Custom name in process list
tagged = true                          # Highlight in process list
tag_color = "log_debug_plus"           # Theme color (Nord Aurora green)

[[logging.processes]]
name = "python"
command_pattern = ".*uvicorn.*"
log_path = "/var/log/myapp/app.log"
display_name = "MyApp API"
tagged = true                          # Highlight in process list
tag_color = "log_info"                 # Yellow

# Example: Tag-only config (no log file, just visual highlighting)
[[logging.processes]]
name = "nginx"
tagged = true
tag_color = "log_error"                # Orange - no log_path, just tagging
```

### 2.6 Migration Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Config Load Flow                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Check for ~/.config/mbtop/mbtop.toml                    │
│     │                                                        │
│     ├─► EXISTS: Load TOML config                            │
│     │                                                        │
│     └─► NOT FOUND: Check for mbtop.conf                     │
│           │                                                  │
│           ├─► EXISTS: Migrate INI → TOML                    │
│           │     • Parse all INI values                       │
│           │     • Map to TOML sections                       │
│           │     • Write mbtop.toml                          │
│           │     • Rename mbtop.conf → mbtop.conf.bak        │
│           │     • Load from new TOML                        │
│           │                                                  │
│           └─► NOT FOUND: Create default TOML                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Data Structure Changes

### 3.1 New Config Structs (`mbtop_config.hpp`)

```cpp
namespace Config {
    // Process-specific log/tag configuration
    // Can be used for logging, tagging, or both independently
    struct ProcessLogConfig {
        string name;             // Process name to match
        string command_pattern;  // Optional regex for cmdline matching  
        string log_path;         // Path to log file (optional - for app logs)
        string display_name;     // Custom display name (optional)
        bool tagged = false;     // Visual highlighting in process list
        string tag_color;        // Theme color name (optional, requires tagged=true)
        
        // Runtime: compiled regex (not serialized)
        std::optional<std::regex> compiled_pattern;
        
        // Helper: check if this config has logging enabled
        bool has_logging() const { return !log_path.empty(); }
        
        // Helper: check if this config has tagging enabled
        bool has_tagging() const { return tagged && !tag_color.empty(); }
    };
    
    // Logging configuration section
    struct LoggingConfig {
        string level = "WARNING";
        string export_path;
        string default_source = "system";  // "system" or "application"
        int buffer_size = 500;
        bool color_full_line = false;
        bool below_proc = false;
        std::unordered_map<string, string> applications;  // simple name → path
        vector<ProcessLogConfig> processes;  // complex matching rules
    };
    
    // Main typed config (coexists with flat maps during transition)
    extern LoggingConfig logging;
    
    // === New Functions ===
    
    // Load config from TOML file
    void load_toml(const fs::path& conf_file, vector<string>& load_warnings);
    
    // Write config to TOML file
    void write_toml();
    
    // Migrate from INI to TOML format
    // Returns true if migration successful, false if no INI or error
    bool migrate_from_ini(const fs::path& ini_file, const fs::path& toml_file);
    
    // Find process log config by name and cmdline
    // Returns matching config or nullopt if no match
    std::optional<ProcessLogConfig> find_process_config(
        const string& name, 
        const string& cmdline
    );
    
    // Save a process log config (add or update)
    void save_process_config(const ProcessLogConfig& config);
    
    // Remove a process log config by name
    void remove_process_config(const string& name);
}
```

### 3.2 Logs Namespace Extensions (`mbtop_shared.hpp`)

```cpp
namespace Logs {
    // === Existing declarations (unchanged) ===
    extern string box;
    extern int x, y, width, height, min_width, min_height;
    extern bool shown, redraw;
    extern bool focused;
    extern bool paused;
    extern int scroll_offset;
    extern pid_t current_pid;
    extern string current_name;
    extern uint8_t level_filter;
    extern deque<LogEntry> entries;
    extern size_t max_entries;
    // ... modals, etc.
    
    // === NEW: Log Source Management ===
    
    enum class Source { 
        System,      // macOS unified logging (log stream)
        Application  // Application log file (text-based)
    };
    
    extern Source source;              // Current log source (default: System)
    extern string app_log_path;        // Path to application log file
    extern bool app_log_available;     // Whether app log exists/readable
    extern string custom_display_name; // Display name from config (optional)
    extern string custom_tag_color;    // Tag color from config (optional)
    
    // File reading state for application logs
    extern std::ifstream app_log_stream;  // Open file handle
    extern std::streampos app_log_pos;    // Last read position
    
    // Toggle between System and Application log sources
    // Called by 'S' key handler
    void toggle_source();
    
    // Resolve log config for a process (called when process is selected for logging)
    // Sets app_log_path, app_log_available, custom_display_name, custom_tag_color
    void resolve_config(pid_t pid, const string& name, const string& cmdline);
    
    // Collect logs from application log file (text-based parsing)
    // Called by collect() when source == Application
    void collect_app_logs();
    
    // Check if application log is available for current process
    bool has_app_log();
    
    // Get source indicator string for status bar
    // Returns "[S:Sys]" or "[S:App]"
    string get_source_indicator();
    
    // Get availability dots for UI
    // Returns "◉◉" (both available), "◉○" (system only), or "○◉" (app only)
    string get_availability_dots();
}
```

### 3.3 Menu Extensions (`mbtop_menu.hpp`)

```cpp
// Add to ControlType enum
enum class ControlType { 
    // ... existing types ...
    LogConfig,    // Process log configuration modal (full)
    ColorPicker   // Quick tag color picker modal
};

// Shared tag color definitions (Nord Aurora)
namespace TagColors {
    const array<string, 5> names = {"Rd", "Or", "Yl", "Gn", "Vi"};
    const array<string, 5> themes = {
        "log_fault",      // Red
        "log_error",      // Orange  
        "log_info",       // Yellow
        "log_debug_plus", // Green
        "log_debug"       // Violet
    };
}

// Color Picker modal state (quick tagging from Process Details)
namespace ColorPickerModal {
    extern bool active;
    extern string process_name;      // Process being tagged
    extern string process_cmdline;   // For config lookup/save
    extern int selected_color;       // 0-4, current selection
    
    void show(const string& name, const string& cmdline);
    void hide();
    bool handle_input(const string_view key);  // Returns true if modal closed
    string draw();
    void apply_and_save();           // Apply selected color, enable tag, save to config
}

// Log Config modal state (full configuration)
namespace LogConfigModal {
    extern bool active;
    extern string process_name;      // Current process name
    extern string process_cmdline;   // Current process cmdline
    extern string display_name;      // Editable display name
    extern string log_path;          // Editable log path
    extern bool tagged;              // Visual highlighting enabled
    extern int tag_color_idx;        // Selected color (0-4), only used if tagged=true
    extern int selected_field;       // 0=display_name, 1=log_path, 2=tagged, 3=color, 4=buttons
    extern int selected_button;      // 0=Save, 1=Remove, 2=Cancel
    
    void show(const string& name, const string& cmdline);
    void hide();
    bool handle_input(const string_view key);
    string draw();
}

// Tagged filter state (in Proc namespace or Config)
namespace Proc {
    // ... existing ...
    extern bool filter_tagged;       // When true, show only tagged processes
    
    void toggle_tagged_filter();     // 'a' key handler
}
```

---

## 4. Implementation Phases

### Phase 1: Add TOML++ Library
**Estimated time**: 15 minutes
**Files**: `include/toml.hpp`

**Steps**:
1. Download toml++ v3.4.0 single-header from GitHub releases
   ```bash
   curl -L https://github.com/marzer/tomlplusplus/releases/download/v3.4.0/toml.hpp \
     -o include/toml.hpp
   ```
2. Verify file size (~1MB) and content
3. Test compilation with simple include

**Verification**:
```cpp
#include "toml.hpp"
// Should compile without errors
```

### Phase 2: Config Module Extension
**Estimated time**: 2-3 hours
**Files**: `src/mbtop_config.hpp`, `src/mbtop_config.cpp`

**Steps**:
1. Add `#include "../include/toml.hpp"` to config.cpp
2. Define new structs: `ProcessLogConfig`, `LoggingConfig`
3. Add `extern LoggingConfig logging;` declaration
4. Implement `load_toml()`:
   - Parse TOML using toml++ API
   - Map sections to existing flat maps (strings, bools, ints)
   - Parse [logging] section into LoggingConfig struct
5. Implement `write_toml()`:
   - Build TOML document from current config state
   - Write with proper formatting and comments
6. Implement `migrate_from_ini()`:
   - Parse existing INI file
   - Map values to TOML structure
   - Write new TOML file
   - Rename old INI to .bak
7. Update `load()` entry point:
   - Check for .toml first
   - Fall back to .conf with migration
8. Update `write()` to use TOML format
9. Implement `find_process_config()`, `save_process_config()`, `remove_process_config()`

**Key toml++ APIs**:
```cpp
// Reading
auto config = toml::parse_file("mbtop.toml");
int update_ms = config["general"]["update_ms"].value_or(2000);
auto& processes = *config["logging"]["processes"].as_array();

// Writing
toml::table config;
config.insert("general", toml::table{{"update_ms", 2000}});
std::ofstream file("mbtop.toml");
file << config;
```

### Phase 3: Logs Source State
**Estimated time**: 1 hour
**Files**: `src/mbtop_shared.hpp`, `src/osx/mbtop_collect.cpp`

**Steps**:
1. Add `Source` enum and state variables to `Logs` namespace in header
2. Initialize new variables in mbtop_collect.cpp
3. Implement `toggle_source()`:
   ```cpp
   void Logs::toggle_source() {
       if (!app_log_available) return;  // Can't switch if no app log
       source = (source == Source::System) ? Source::Application : Source::System;
       entries.clear();  // Clear buffer on source change
       scroll_offset = 0;
       redraw = true;
   }
   ```
4. Implement `resolve_config()`:
   ```cpp
   void Logs::resolve_config(pid_t pid, const string& name, const string& cmdline) {
       current_pid = pid;
       current_name = name;
       
       // Reset app log state
       app_log_path.clear();
       app_log_available = false;
       custom_display_name.clear();
       custom_tag_color.clear();
       
       // Look up config
       if (auto config = Config::find_process_config(name, cmdline)) {
           app_log_path = config->log_path;
           custom_display_name = config->display_name;
           custom_tag_color = config->tag_color;
           
           // Check if file exists and is readable
           if (fs::exists(app_log_path)) {
               app_log_available = true;
           }
       }
       
       // Set source based on config default and availability
       if (Config::logging.default_source == "application" && app_log_available) {
           source = Source::Application;
       } else {
           source = Source::System;
       }
   }
   ```
5. Implement `has_app_log()`, `get_source_indicator()`, `get_availability_dots()`

### Phase 4: Application Log Parsing
**Estimated time**: 2 hours
**Files**: `src/osx/mbtop_collect.cpp`

**Supported formats** (detected in order):
1. **JSON Lines** - `{"timestamp":"...", "level":"...", "message":"..."}`
2. **Bracketed level** - `2025-01-22 10:30:00 [INFO] Message text`
3. **Colon level** - `2025-01-22 10:30:00 INFO: Message text`
4. **Syslog** - `Jan 22 10:30:00 hostname process[pid]: message`
5. **Plain text** - Treat each line as message with "Info" level

**Implementation**:
```cpp
void Logs::collect_app_logs() {
    if (app_log_path.empty() || !app_log_available) return;
    
    // Open file if not already open
    if (!app_log_stream.is_open()) {
        app_log_stream.open(app_log_path);
        if (!app_log_stream.good()) {
            app_log_available = false;
            return;
        }
        // Seek to end - N lines for initial load
        // (or beginning if file is small)
    }
    
    // Read new lines
    string line;
    while (std::getline(app_log_stream, line)) {
        if (line.empty()) continue;
        
        LogEntry entry;
        
        // Try JSON first
        if (line.starts_with("{")) {
            if (parse_json_log(line, entry)) {
                add_entry(std::move(entry));
                continue;
            }
        }
        
        // Try bracketed format: "2025-01-22 10:30:00 [INFO] message"
        if (parse_bracketed_log(line, entry)) {
            add_entry(std::move(entry));
            continue;
        }
        
        // Try syslog format
        if (parse_syslog(line, entry)) {
            add_entry(std::move(entry));
            continue;
        }
        
        // Fallback: plain text
        entry.timestamp = "";  // No timestamp
        entry.level = "Info";
        entry.message = line;
        add_entry(std::move(entry));
    }
    
    // Clear EOF flag for next read
    app_log_stream.clear();
    app_log_pos = app_log_stream.tellg();
}

void Logs::add_entry(LogEntry&& entry) {
    // Apply level filter
    if (!passes_filter(entry.level)) return;
    
    entries.push_back(std::move(entry));
    
    // Trim to max size
    while (entries.size() > max_entries) {
        entries.pop_front();
    }
}
```

### Phase 5: UI Updates
**Estimated time**: 2-3 hours

#### 5a. Logs Panel Status Bar (`mbtop_draw.cpp`)
**Location**: `Logs::draw()` function, status bar section

Add after filter indicator:
```cpp
// Source indicator
out += Fx::b + Theme::c("hi_fg") + "[" + Theme::c("title");
out += (source == Source::System) ? "S:Sys" : "S:App";
out += Theme::c("hi_fg") + "]" + Fx::ub;

// Availability dots
out += " ";
out += (source == Source::System) ? Theme::c("proc_misc") : Theme::c("inactive_fg");
out += "◉";  // System dot
out += app_log_available 
    ? ((source == Source::Application) ? Theme::c("proc_misc") : Theme::c("inactive_fg"))
    : Theme::c("inactive_fg");
out += "◉";  // App dot
```

#### 5b. Input Handler (`mbtop_input.cpp`)
**Location**: Logs panel input handling section

```cpp
// In Logs-focused input handling
else if (key == "s" or key == "S") {
    Logs::toggle_source();
    redraw = true;
}
```

#### 5c. Process Details Log Button (`mbtop_draw.cpp`)
**Location**: `Proc::draw()` detailed view section

Add new button after existing buttons:
```cpp
// Log config button with color preview and availability
string log_btn = "[Log ";

// Color preview (2 chars)
if (!Logs::custom_tag_color.empty()) {
    log_btn += Theme::c(Logs::custom_tag_color) + "██" + Fx::reset;
} else {
    log_btn += Theme::c("inactive_fg") + "██" + Fx::reset;
}

// Availability dots
log_btn += Theme::c("main_fg") + "◉";  // System always available
log_btn += Logs::app_log_available ? Theme::c("main_fg") : Theme::c("inactive_fg");
log_btn += "◉";
log_btn += "]";

// Add to button row with mouse mapping
```

#### 5d. Log Config Modal (`mbtop_menu.cpp`)
**New function**: `LogConfigModal::draw()`

```
┌─────────────── Process Log Config ───────────────┐
│                                                  │
│  Process: node                                   │
│  Command: /usr/bin/node ~/.claude/mcp-server.js  │
│                                                  │
│  Display Name: [Claude MCP                    ]  │
│  Log Path:     [~/.claude/logs/mcp.log        ]  │
│                                                  │
│  ═══════════════ Tagging ═══════════════════════ │
│  Tagged: [x]  (highlight in process list)        │
│                                                  │
│  Tag Color:  ██ ██ ██ ██ ██   (1-5 to select)   │
│              Rd Or Yl Gn Vi                      │
│              ▲                                   │
│                                                  │
│  [Save]  [Remove Config]  [Cancel]              │
│                                                  │
└──────────────────────────────────────────────────┘

Width: 52 chars
Height: 18 lines
```

**Notes**:
- Tagged checkbox is independent from Log Path (can tag without logging)
- Tag Color selection is only enabled when Tagged is checked
- Global setting `proc_tag_mode` in Settings→PROC controls name vs line coloring

**Input handling**:
- Tab/Shift+Tab: Cycle fields (Display Name → Log Path → Tagged → Tag Color → Buttons)
- Space: Toggle Tagged checkbox
- Enter: Activate/confirm field or button
- 1-5: Select tag color (when color field active and Tagged is checked)
- Escape: Cancel and close
- Arrow keys: Navigate buttons

### Phase 6: Process List Tagging
**Estimated time**: 1.5 hours
**Files**: `src/mbtop_draw.cpp`, `src/mbtop_config.cpp`

Implement visual tagging in process list (both detailed and compact views):

**Global Setting** (in Settings Menu → PROC section):
```cpp
// Config key: proc_tag_mode
// Values: "name" (default) or "line"
// "name" = color only the program name column
// "line" = color the entire row background
```

**Implementation**:
```cpp
// In Proc::draw() - for each process row
if (auto config = Config::find_process_config(proc.name, proc.cmd)) {
    if (config->has_tagging()) {
        string tag_color = Theme::c(config->tag_color);
        
        if (Config::getS("proc_tag_mode") == "line") {
            // Color entire row background
            out += tag_color + Fx::bg;  // Set background
            // ... render row ...
            out += Fx::reset;
        } else {
            // Color only program name
            // ... render other columns normally ...
            out += tag_color + proc_name + Fx::reset;
            // ... continue with other columns ...
        }
    }
}
```

**Visual Examples**:

Tag Mode = "name" (default):
```
┌─Proc─────────────────────────────────────────────┐
│  PID   Program         CPU%  Mem%  ...           │
│ 1234   node            2.3%  1.2%  ...  ← normal │
│ 5678   Claude MCP      5.1%  2.4%  ...  ← green  │
│ 9012   nginx           0.5%  0.8%  ...  ← orange │
└──────────────────────────────────────────────────┘
```

Tag Mode = "line":
```
┌─Proc─────────────────────────────────────────────┐
│  PID   Program         CPU%  Mem%  ...           │
│ 1234   node            2.3%  1.2%  ...  ← normal │
│▓5678   Claude MCP      5.1%  2.4%  ...▓ ← green bg│
│▓9012   nginx           0.5%  0.8%  ...▓ ← orange bg│
└──────────────────────────────────────────────────┘
```

**Works in both views**:
- Detailed process list (large Proc panel)
- Compact process list (small Proc panel)

---

## 5. UI Mockups

### 5.1 Logs Panel with Source Toggle

```
┌─Logs: Claude MCP (PID: 12345)────────────────────────────────────┐
│ [F:All▼] [S:App] ◉◉  [B:500] [P] [R] [X]        Pos: 1-20/156    │
├──────────────────────────────────────────────────────────────────┤
│ 10:30:01 [INFO] Server started on port 3000                      │
│ 10:30:02 [DEBUG] Loading configuration from ~/.claude/config     │
│ 10:30:02 [INFO] Connected to Claude API                          │
│ 10:30:05 [WARN] Rate limit approaching (80%)                     │
│ ...                                                              │
└──────────────────────────────────────────────────────────────────┘

Legend:
[F:All▼] - Filter dropdown (All/Debug/Info/Error/Fault)
[S:App]  - Source toggle (Sys/App) - press 'S' to toggle
◉◉       - Availability (filled=available, hollow=unavailable)
[B:500]  - Buffer size
[P]      - Pause toggle
[R]      - Reverse order toggle
[X]      - Export button
```

### 5.2 Process Details with Log Button

```
┌─Process Details: node (PID: 12345)───────────────────────────────┐
│                                                                  │
│  CPU: 2.3%  Memory: 156 MiB  Threads: 12  State: Running        │
│  Started: 2025-01-22 09:15:00  Runtime: 1h 15m 30s              │
│  Command: /usr/bin/node /Users/me/.claude/mcp-server.js         │
│                                                                  │
│  [Terminate] [Kill] [Signals▼] [Nice▼] [Follow] (●)██ [Log◉◉]  │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

Button breakdown:
- (●)██  = Quick Tag: radio button + color preview
          (●) = Tagged ON (click to toggle off → (○))
          (○) = Tagged OFF (click to toggle on → (●))
          ██  = Color preview (click opens Color Picker modal)
- [Log◉◉] = Log Config modal (L key)
          ◉◉ = System/App log availability dots
```

### 5.2b Color Picker Modal (Quick Tag)

Opens when clicking the color square (██) in Process Details:

```
┌─ Tag Color ─┐
│             │
│ ██ ██ ██ ██ ██ │
│ Rd Or Yl Gn Vi │
│             │
└─────────────┘

Width: 17 chars
Height: 5 lines
```

**Behavior**:
- Click color or press 1-5: Sets tag color, enables tagging, saves immediately, closes modal
- Escape: Close without changes
- If process wasn't tagged, selecting a color auto-enables tagging
- To remove tag: Use the radio button (●)→(○) in Process Details (not in this modal)

### 5.3 Log Config Modal

```
┌─────────────── Process Log Config ───────────────┐
│                                                  │
│  Process: node                                   │
│  Command: /usr/bin/node ~/.claude/mcp-server.js  │
│                                                  │
│  Display Name: [Claude MCP                    ]  │
│  Log Path:     [~/.claude/logs/mcp.log        ]  │
│                                                  │
│  ═══════════════ Tagging ═══════════════════════ │
│  Tagged: [x]  (highlight in process list)        │
│                                                  │
│  Tag Color:  ██ ██ ██ ██ ██   (1-5 to select)   │
│              Rd Or Yl Gn Vi                      │
│              ▲                                   │
│              └─ Currently selected (Green)       │
│                                                  │
│  [Save]  [Remove Config]  [Cancel]              │
│                                                  │
└──────────────────────────────────────────────────┘

Notes:
- Tagged checkbox and Tag Color are independent from Log Path
- You can tag a process without configuring logging
- Tag Color is only enabled when Tagged is checked
- Tag coloring mode (name vs line) is set in Settings → PROC
```

### 5.4 Process List with Tagging

**Header with Tagged Filter** (`a` key to toggle):
```
┌─Proc (7/156)────────────────────────────────────────────────────┐
│ Filter: [          ] [Tagged:ON]  cpu lazy▼ ◀▶ reverse:off      │
├─────────────────────────────────────────────────────────────────┤
│ Tree  PID    Program          Cpu%   Mem%  User     State       │
├─────────────────────────────────────────────────────────────────┤
│   ├─ 5678   Claude MCP        5.1%   2.4%  mbe      Running     │
│   ├─ 9012   nginx             0.5%   0.8%  root     Running     │
│   └─ 4567   redis             0.3%   1.2%  redis    Running     │
└─────────────────────────────────────────────────────────────────┘

[Tagged:ON]  = Show only tagged processes (press 'a' to toggle)
[Tagged:OFF] = Show all processes (default)
```

**Tag Mode = "name"** (colors only program name):
```
┌─Proc────────────────────────────────────────────────────────────┐
│ Tree  PID    Program          Cpu%   Mem%  User     State       │
├─────────────────────────────────────────────────────────────────┤
│   ├─ 1234   Terminal          0.2%   0.5%  mbe      Running     │
│   ├─ 5678   Claude MCP        5.1%   2.4%  mbe      Running     │  ← green name
│   │  └─ 5679  node            0.0%   0.1%  mbe      Sleeping    │
│   ├─ 9012   nginx             0.5%   0.8%  root     Running     │  ← orange name
│   └─ 3456   python            1.2%   3.1%  mbe      Running     │
└─────────────────────────────────────────────────────────────────┘
```

**Tag Mode = "line"** (colors entire row):
```
┌─Proc────────────────────────────────────────────────────────────┐
│ Tree  PID    Program          Cpu%   Mem%  User     State       │
├─────────────────────────────────────────────────────────────────┤
│   ├─ 1234   Terminal          0.2%   0.5%  mbe      Running     │
│▓▓▓├─ 5678   Claude MCP        5.1%   2.4%  mbe      Running   ▓▓│  ← green bg
│   │  └─ 5679  node            0.0%   0.1%  mbe      Sleeping    │
│▓▓▓├─ 9012   nginx             0.5%   0.8%  root     Running   ▓▓│  ← orange bg
│   └─ 3456   python            1.2%   3.1%  mbe      Running     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Key Bindings

### 6.1 Logs Panel (when focused)

| Key | Action | Notes |
|-----|--------|-------|
| `S` | Toggle source | System ↔ Application |
| `F` | Open filter modal | Existing |
| `B` | Open buffer size modal | Existing |
| `P` | Toggle pause | Existing |
| `R` | Toggle reverse order | Existing |
| `X` | Export logs | Existing |
| `↑`/`↓` | Scroll | Existing |
| `PgUp`/`PgDn` | Page scroll | Existing |
| `Home`/`End` | Jump to start/end | Existing |

### 6.2 Process List (when focused)

| Key | Action | Notes |
|-----|--------|-------|
| `a` | Toggle Tagged filter | Show only tagged processes |
| `f` | Focus filter input | Existing |
| `c` | Clear filter | Existing |
| `e` | Toggle tree view | Existing |
| `r` | Toggle reverse sort | Existing |
| ... | ... | (other existing keys) |

### 6.3 Process Details (when showing detailed view)

| Key | Action | Notes |
|-----|--------|-------|
| `a` | Toggle tag on/off | Quick tag toggle (radio button) |
| `c` | Open Color Picker | Quick color selection |
| `L` | Open Log Config modal | Full config (display name, log path, etc.) |
| `T` | Terminate process | Existing |
| `K` | Kill process | Existing |
| `N` | Nice adjustment | Existing |
| `F` | Follow process | Existing |

### 6.4 Color Picker Modal (Quick Tag)

| Key | Action |
|-----|--------|
| `1-5` | Select color, apply, save, and close |
| `Enter` | Apply selected color and close |
| `Escape` | Close without changes |
| `←`/`→` | Navigate colors |
| Click | Select color, apply, save, and close |

### 6.5 Log Config Modal

| Key | Action |
|-----|--------|
| `Tab` | Next field (Display Name → Log Path → Tagged → Tag Color → Buttons) |
| `Shift+Tab` | Previous field |
| `Space` | Toggle Tagged checkbox (when on Tagged field) |
| `1-5` | Select tag color (when on color field and Tagged is checked) |
| `Enter` | Confirm/activate field or button |
| `Escape` | Cancel and close |
| `←`/`→` | Navigate buttons |

### 6.6 Settings Menu (PROC section)

| Setting | Values | Description |
|---------|--------|-------------|
| `tag_mode` | "name" / "line" | How to display process tagging |

- **name**: Color only the program name column (default)
- **line**: Color the entire row background

---

## 7. File Change Summary

| File | Action | Changes |
|------|--------|---------|
| `include/toml.hpp` | **Create** | Add toml++ library |
| `src/mbtop_config.hpp` | Modify | Add structs, function declarations, `proc_tag_mode` |
| `src/mbtop_config.cpp` | Modify | TOML parsing, migration, new functions, add `proc_tag_mode` to strings |
| `src/mbtop_shared.hpp` | Modify | Add Logs source state, modals, `Proc::filter_tagged` |
| `src/osx/mbtop_collect.cpp` | Modify | App log collection, source handling |
| `src/mbtop_draw.cpp` | Modify | Status bar, quick tag button, Color Picker modal, process list tagging, Tagged filter indicator |
| `src/mbtop_input.cpp` | Modify | `a` key (tagged filter), `c` key (color picker), `L` key (log config), modal inputs |
| `src/mbtop_menu.hpp` | Modify | Add `ControlType::LogConfig`, `ControlType::ColorPicker`, modal namespaces |
| `src/mbtop_menu.cpp` | Modify | Log Config modal, Color Picker modal, Settings menu PROC section |

---

## 8. Open Questions

### Resolved
1. **Config format**: TOML (clean break, no INI compatibility) ✓
2. **Process matching**: By name + optional command regex ✓
3. **Log source toggle**: `S` key (Tab reserved for panel switching) ✓
4. **Default log source**: System (configurable via `default_source`) ✓
5. **Config UI**: Modal dialog from Process Details view (`L` key or `[Log ██]` button) ✓
6. **Color tags**: 5 Nord Aurora colors, selected with 1-5 keys ✓
7. **Tagging independence**: `tagged` bool is separate from `log_path` - can tag without logging ✓
8. **Tag mode setting**: Global `proc_tag_mode` in Settings→PROC, values: "name" or "line" ✓
9. **Tag color scope**: Enabled only when Tagged checkbox is checked ✓
10. **Tagged filter**: `a` key in Proc panel filters to show only tagged processes ✓
11. **Quick tagging**: Radio button (●)/(○) in Process Details toggles tag on/off directly ✓
12. **Quick color picker**: Click on color square opens minimal Color Picker modal ✓
13. **Color picker behavior**: Selecting a color auto-enables tagging and saves immediately ✓

### Still Open
1. **Command pattern auto-generation**: Should we auto-generate regex from command when saving, or leave empty?
   - Recommendation: Leave empty by default, user can add manually
   
2. **Path validation**: Validate log path exists before saving?
   - Recommendation: Warn but allow save (file might be created later)
   
3. **Process list display**: Show `display_name (original)` or just `display_name`?
   - Recommendation: Just `display_name` to save space, tooltip shows original

4. **Migration backup**: Keep `.conf.bak` or delete old config?
   - Recommendation: Keep `.conf.bak` for safety

---

## 9. Continuation Prompt

Use this prompt to continue implementation in a new session:

```
Continue implementing the mbtop config modernization, application logs, and process tagging features.

Current state:
- Planning document: docs/CONFIG_MODERNIZATION_PLAN.md
- Bug fixes committed (c4df7cd on feature/logs-panel branch)
- TOML config design complete (includes `[[logging.processes]]` with `tagged` field)
- Application logs feature designed but not implemented
- Process Details with quick tag (radio button + color picker) designed
- Log Config modal designed (full config: display name, log path, tagged, color)
- Color Picker modal designed (quick tag color selection)
- Process list tagging feature designed (name vs line mode)
- Tagged filter for process list designed ('a' key)

Branch: feature/logs-panel
Last commit: c4df7cd - "fix: comprehensive Logs panel bug fixes and improvements"

Next immediate task: Phase 1 - Add toml++ library to include/

Key files to modify:
- Config: src/mbtop_config.cpp, src/mbtop_config.hpp
- Logs: src/osx/mbtop_collect.cpp, src/mbtop_shared.hpp (namespace Logs)
- Input: src/mbtop_input.cpp
- Draw: src/mbtop_draw.cpp (process list tagging, quick tag button, modals)
- Menu: src/mbtop_menu.cpp, src/mbtop_menu.hpp (modals, Settings→PROC tag_mode)

Key features to implement:
1. TOML config with migration from INI
2. Application log viewing (S key to toggle source)
3. Tagged filter in Process list ('a' key - show only tagged processes)
4. Quick tagging in Process Details:
   - Radio button (●)/(○) to toggle tag on/off ('a' key)
   - Color square opens Color Picker modal ('c' key or click)
5. Color Picker modal (quick tag - select color and auto-save)
6. Log Config modal (L key) with Display Name, Log Path, Tagged, Tag Color
7. Process tagging independent from logging (`tagged` bool + `tag_color`)
8. Global `proc_tag_mode` setting: "name" or "line" (Settings → PROC)

Key bindings:
- 'a' in Proc list: Toggle Tagged filter (show only tagged)
- 'a' in Process Details: Toggle tag on/off (radio button)
- 'c' in Process Details: Open Color Picker modal
- 'L' in Process Details: Open full Log Config modal
- '1-5' in Color Picker: Select color and apply immediately
- 'S' in Logs panel: Toggle log source (System/Application)

User preferences:
- Clean break (no backward compatibility with INI)
- TOML format for config
- Nord Aurora colors for process tags (5 colors: Rd/Or/Yl/Gn/Vi)
- Keep .conf.bak backup during migration
- Tagging works in both detailed and compact process list views
- Quick tag saves immediately (no need to open full modal)

Please read docs/CONFIG_MODERNIZATION_PLAN.md first, then proceed with implementation.
```

---

## Appendix A: toml++ Quick Reference

```cpp
// === PARSING ===
#include "toml.hpp"

// Parse file
toml::table config = toml::parse_file("config.toml");

// Parse string
toml::table config = toml::parse("key = 'value'");

// === READING VALUES ===

// Simple values with defaults
int val = config["section"]["key"].value_or(42);
string str = config["section"]["key"].value_or("default"s);
bool flag = config["section"]["key"].value_or(false);

// Check if key exists
if (config["section"]["key"]) { ... }

// Get optional
std::optional<int> maybe_val = config["section"]["key"].value<int>();

// Iterate array
if (auto arr = config["section"]["items"].as_array()) {
    for (auto& elem : *arr) {
        // elem is toml::node
    }
}

// Iterate table
if (auto tbl = config["section"].as_table()) {
    for (auto& [key, value] : *tbl) {
        // key is std::string_view, value is toml::node
    }
}

// === WRITING ===

toml::table config;

// Simple values
config.insert("key", 42);
config.insert("str", "value");

// Nested tables
config.insert("section", toml::table{
    {"key1", "value1"},
    {"key2", 42}
});

// Arrays
config.insert("items", toml::array{1, 2, 3});

// Array of tables
toml::array processes;
processes.push_back(toml::table{
    {"name", "node"},
    {"log_path", "/var/log/node.log"}
});
config.insert("processes", std::move(processes));

// Write to file
std::ofstream file("config.toml");
file << config;

// Write with formatting
file << toml::toml_formatter{config};
```

---

## Appendix B: Existing Config Keys Reference

### Strings (39 keys)
```
color_theme, shown_boxes, graph_symbol, presets,
graph_symbol_cpu, graph_symbol_gpu, graph_symbol_pwr,
graph_symbol_mem, graph_symbol_net, graph_symbol_proc,
proc_sorting, proc_tag_mode,  ← NEW: "name" or "line"
cpu_graph_upper, cpu_graph_lower, cpu_sensor,
selected_battery, cpu_core_map, temp_scale, clock_format,
preset_names, preset_0, custom_cpu_name, disks_filter,
io_graph_speeds, net_iface, net_iface_filter, base_10_bitrate,
log_level, log_export_path, proc_filter, proc_command,
selected_name, custom_gpu_name0-5, show_gpu_info, shown_gpus
```

### Bools (68 keys)
```
theme_background, truecolor, rounded_corners, proc_reversed,
proc_tree, proc_colors, proc_gradient, proc_per_core, proc_gpu,
proc_mem_bytes, proc_cpu_graphs, proc_gpu_graphs, proc_show_cmd,
proc_show_threads, proc_show_user, proc_show_memory, proc_show_cpu,
proc_show_io, proc_show_io_read, proc_show_io_write, proc_show_state,
proc_show_priority, proc_show_nice, proc_show_ports, proc_show_virt,
proc_show_runtime, proc_show_cputime, proc_show_gputime,
proc_info_smaps, proc_left, proc_filter_kernel, cpu_invert_lower,
cpu_single_graph, cpu_bottom, gpu_bottom, pwr_bottom, show_uptime,
show_cpu_watts, check_temp, show_coretemp, show_cpu_freq, clock_12h,
show_hostname, show_uptime_header, show_username_header,
background_update, mem_graphs, mem_bar_mode, mem_below_net,
net_beside_mem, proc_full_width, logs_below_proc, log_color_full_line,
stacked_layout, zfs_arc_cached, show_swap, swap_disk, show_disks,
mem_horizontal, mem_show_used, mem_show_available, mem_show_cached,
mem_show_free, swap_show_used, swap_show_free, vram_show_used,
vram_show_free, only_physical, show_network_drives, use_fstab,
zfs_hide_datasets, show_io_stat, io_mode, swap_upload_download,
base_10_sizes, io_graph_combined, net_auto, net_sync, show_battery,
show_battery_watts, vim_keys, tty_mode, disk_free_priv, force_tty,
lowcolor, show_detailed, proc_filtering, proc_aggregate,
pause_proc_list, keep_dead_proc_usage, proc_banner_shown,
proc_follow_detailed, follow_process, update_following,
should_selection_return_to_followed, nvml_measure_pcie_speeds,
rsmi_measure_pcie_speeds, gpu_mirror_graph, terminal_sync,
save_config_on_exit, prevent_autosave, show_instance_indicator,
preview_unicode, disable_mouse
```

### Ints (19 keys)
```
update_ms, net_download, net_upload, net_graph_direction,
detailed_pid, restore_detailed_pid, selected_pid, followed_pid,
selected_depth, proc_start, proc_selected, proc_last_selected,
proc_followed, disk_selected, disk_start, mem_vram_mode,
mem_toggle_mode, swap_toggle_mode, vram_toggle_mode,
mem_start, mem_selected, log_buffer_size
```
