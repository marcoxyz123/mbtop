# mbtop MCP Process Tagging Runbook

This runbook documents how to use the mbtop MCP server for process tagging, filtering, and log viewing automation.

## Available MCP Tools

### Config-Based Tools (TOML file)

These tools modify the mbtop config file. Changes are auto-reloaded within ~2 seconds.

| Tool | Description |
|------|-------------|
| `tag_process` | Tag a process with color, display name, and optional log path |
| `untag_process` | Remove tagging from a process |
| `remove_process_config` | Completely remove a process config |
| `set_filter_tagged` | Enable/disable showing only tagged processes |
| `list_tagged_processes` | List all tagged processes |
| `get_mbtop_status` | Get current config status |
| `get_aurora_colors` | Show available tag colors |

### Socket-Based Tools (Real-time control)

These tools communicate with mbtop via Unix socket for immediate UI control.
Requires mbtop to be running.

| Tool | Description |
|------|-------------|
| `show_logs_panel` | Show the Logs panel (beside or below proc) |
| `hide_logs_panel` | Hide the Logs panel |
| `select_process` | Select a process in the process list |
| `activate_process_log` | Select process AND show its logs in one action |
| `set_log_level_filter` | Set log level filter (all/debug/info/error/fault) |
| `get_mbtop_live_state` | Query mbtop's current UI state |

## Available Colors (Nord Aurora + Frost)

| Color | Hex | Use Case |
|-------|-----|----------|
| red | #BF616A | Alerts, errors, critical |
| orange | #D08770 | Warnings, attention needed |
| yellow | #EBCB8B | Active work, in progress |
| green | #A3BE8C | Healthy, OK, success |
| violet | #B48EAD | Info, background, neutral |
| blue | #81A1C1 | Frost blue, calm, secondary |

## Command Matching

**IMPORTANT:** The `command` parameter must match the process command line exactly OR use wildcards.

### Exact Match
Use the full path as shown in mbtop's Command column:
```
/System/Library/CoreServices/Finder.app/Contents/MacOS/Finder
/Applications/Tabby.app/Contents/MacOS/Tabby
```

### Wildcard Match
Use `*` for processes with varying arguments:
```
*com.docker.backend*    # Matches all Docker backend processes
*node*server.js*        # Matches node server processes
```

### Finding the Correct Command
```bash
# Use ps to see actual command lines
ps -eo args | grep <process_name>

# Example output for Docker:
# /Applications/Docker.app/Contents/MacOS/com.docker.backend --autostart
# /Applications/Docker.app/Contents/MacOS/com.docker.backend services --autostart
```

## Demo Workflow: Mario Kart Party

This demo showcases all 6 colors by tagging processes as Mario Kart characters.

### Step 1: Show Available Colors
```python
mcp_mbtop_get_aurora_colors()
```

### Step 2: Tag the Racers

```python
# Mario - Red
mcp_mbtop_tag_process(
    name="Finder",
    command="/System/Library/CoreServices/Finder.app/Contents/MacOS/Finder",
    color="red",
    display_name="Mario"
)

# Bowser - Orange
mcp_mbtop_tag_process(
    name="Bartender 5",
    command="/Applications/Bartender 5.app/Contents/MacOS/Bartender 5",
    color="orange",
    display_name="Bowser"
)

# Toad - Yellow
mcp_mbtop_tag_process(
    name="Tabby",
    command="/Applications/Tabby.app/Contents/MacOS/Tabby",
    color="yellow",
    display_name="Toad"
)

# Luigi - Green
mcp_mbtop_tag_process(
    name="mbtop",
    command="mbtop",
    color="green",
    display_name="Luigi"
)

# Donkey Kong - Blue (wildcard for multiple docker processes)
mcp_mbtop_tag_process(
    name="com.docker.backend",
    command="*com.docker.backend*",
    color="blue",
    display_name="Donkey Kong"
)

# Princess Peach - Violet
mcp_mbtop_tag_process(
    name="Safari",
    command="/System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/Contents/MacOS/Safari",
    color="violet",
    display_name="Princess Peach"
)
```

### Step 3: Enable Tagged Filter
```python
mcp_mbtop_set_filter_tagged(enabled=True)
```

### Step 4: Verify the Party
```python
mcp_mbtop_list_tagged_processes()
```

Expected output:
```
Tagged processes:
  - Finder (display: Mario) [red]
  - Bartender 5 (display: Bowser) [orange]
  - Tabby (display: Toad) [yellow]
  - mbtop (display: Luigi) [green]
  - com.docker.backend (display: Donkey Kong) [blue]
  - Safari (display: Princess Peach) [violet]

Tagged filter: enabled
```

### Step 5: Race Over - Clean Up
```python
# Untag all racers
mcp_mbtop_untag_process(name="Finder", command="/System/Library/CoreServices/Finder.app/Contents/MacOS/Finder")
mcp_mbtop_untag_process(name="Bartender 5", command="/Applications/Bartender 5.app/Contents/MacOS/Bartender 5")
mcp_mbtop_untag_process(name="Tabby", command="/Applications/Tabby.app/Contents/MacOS/Tabby")
mcp_mbtop_untag_process(name="mbtop", command="mbtop")
mcp_mbtop_untag_process(name="com.docker.backend", command="*com.docker.backend*")
mcp_mbtop_untag_process(name="Safari", command="/System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/Contents/MacOS/Safari")

# Disable filter
mcp_mbtop_set_filter_tagged(enabled=False)
```

### Step 6: Verify Clean State
```python
mcp_mbtop_get_mbtop_status()
```

Expected: `Tagged processes: 0`, `Tagged filter: disabled`

## Spotlight Workflow: Single Process with Logs

Focus on a single process and view its application log.

### Manual Workflow (Config + Keyboard)

```python
# 1. Tag the process with log file
mcp_mbtop_tag_process(
    name="mbtop",
    command="mbtop",
    color="green",
    display_name="MBTOP",
    log_path="~/.config/mbtop/mbtop.log"
)

# 2. Enable tagged filter (isolate the process)
mcp_mbtop_set_filter_tagged(enabled=True)

# 3. MANUAL: Press '8' to open Logs panel
# 4. MANUAL: Select the process to view its log
```

### Automated Workflow (Socket-Based)

```python
# 1. Tag the process with log file (config-based)
mcp_mbtop_tag_process(
    name="mbtop",
    command="mbtop",
    color="green",
    display_name="MBTOP",
    log_path="~/.config/mbtop/mbtop.log"
)

# 2. Enable tagged filter (config-based)
mcp_mbtop_set_filter_tagged(enabled=True)

# 3. Activate process log in one action (socket-based, real-time)
mcp_mbtop_activate_process_log(name="mbtop", command="mbtop")

# 4. Optionally filter logs by level (socket-based)
mcp_mbtop_set_log_level_filter("error")
```

### One-Shot Spotlight

For quickly focusing on a single process with its logs:

```python
# All-in-one: tag + filter + show logs
mcp_mbtop_tag_process(name="mbtop", command="mbtop", color="green", log_path="~/.config/mbtop/mbtop.log")
mcp_mbtop_set_filter_tagged(enabled=True)
mcp_mbtop_activate_process_log(name="mbtop")
```

## Troubleshooting

### Process Not Visible After Tagging

1. **Check command matches exactly** - Use `ps -eo args | grep <name>` to find the exact command
2. **Use wildcards for processes with arguments** - e.g., `*docker*` instead of exact path
3. **Verify tagging is enabled** - Check `tagged: true` and `tag_color` is set
4. **Config reload delay** - mbtop reloads config within ~2 seconds

### Multiple Config Entries

The MCP tools match on BOTH `name` AND `command`. If you use different commands for the same process name, multiple entries are created. Use `remove_process_config` to clean up duplicates.

### Checking Config State

```python
mcp_mbtop_get_mbtop_status()  # Overview
mcp_mbtop_list_tagged_processes()  # Detailed tagged list
```

Or inspect directly:
```bash
grep -A5 '^\[\[logging.processes\]\]' ~/.config/mbtop/mbtop.toml
```

## Implementation Notes

### Config File Location
`~/.config/mbtop/mbtop.toml`

### Auto-Reload
mbtop watches the config file and reloads within ~2 seconds of changes.

### Tag Color Mapping
The MCP server maps friendly color names to internal theme colors:
```python
AURORA_COLORS = {
    "red": "log_fault",
    "orange": "log_error",
    "yellow": "log_info",
    "green": "log_debug_plus",
    "violet": "log_debug",
    "blue": "tag_blue",
}
```
