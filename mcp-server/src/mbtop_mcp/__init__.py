"""
mbtop MCP Server - Control process tagging and filtering in mbtop

This MCP server allows AI assistants to:
- Tag processes with colors and custom display names
- Filter mbtop to show only tagged processes
- Dynamically update what's visible in mbtop

mbtop will auto-reload the config within ~2 seconds of changes.
"""

from pathlib import Path
from typing import Optional
import tomlkit
from mcp.server.fastmcp import FastMCP

# Initialize FastMCP server
mcp = FastMCP(
    "mbtop",
    instructions="""
    MCP server for mbtop process monitor. Use these tools to:
    - Tag important processes with colors (Red, Orange, Yellow, Green, Violet, Blue)
    - Give processes meaningful display names
    - Filter mbtop to show only tagged processes
    
    mbtop auto-reloads config changes within ~2 seconds.
    """,
)

# Nord Aurora + Frost color mapping
AURORA_COLORS = {
    "red": "log_fault",
    "orange": "log_error",
    "yellow": "log_info",
    "green": "log_debug_plus",
    "violet": "log_debug",
    "blue": "tag_blue",
}

# Default config path
DEFAULT_CONFIG_PATH = Path.home() / ".config" / "mbtop" / "mbtop.toml"


def get_config_path() -> Path:
    """Get the mbtop config file path."""
    return DEFAULT_CONFIG_PATH


def load_config() -> tomlkit.TOMLDocument:
    """Load the mbtop TOML config file."""
    config_path = get_config_path()
    if not config_path.exists():
        raise FileNotFoundError(f"mbtop config not found at {config_path}")
    return tomlkit.parse(config_path.read_text())


def save_config(config: tomlkit.TOMLDocument) -> None:
    """Save the mbtop TOML config file."""
    config_path = get_config_path()
    config_path.write_text(tomlkit.dumps(config))


def find_process_config(
    config: tomlkit.TOMLDocument, name: str, command: str
) -> tuple[int, Optional[dict]]:
    """
    Find a process config by name AND command (both required).
    Returns (index, config) or (-1, None) if not found.
    """
    processes = config.get("logging", {}).get("processes", [])
    for i, proc in enumerate(processes):
        if proc.get("name") == name and proc.get("command", "") == command:
            return i, proc
    return -1, None


@mcp.tool()
def tag_process(
    name: str,
    command: str,
    color: str = "green",
    display_name: Optional[str] = None,
    log_path: Optional[str] = None,
) -> str:
    """
    Tag a process in mbtop with a color and optional display name.

    Args:
        name: Process name to tag (e.g., "python", "node", "nginx")
        command: Full command line to match (REQUIRED - e.g., "mbtop -p 3", "node server.js")
        color: Color name - one of: red, orange, yellow, green, violet
        display_name: Optional custom name to show instead of process name
        log_path: Optional path to log file for this process

    Returns:
        Success message with the tagged process details
    """
    if not command:
        return "Error: 'command' is required. Use the full command line (e.g., 'mbtop -p 3')"

    color_lower = color.lower()
    if color_lower not in AURORA_COLORS:
        return f"Error: Invalid color '{color}'. Valid colors: {', '.join(AURORA_COLORS.keys())}"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    # Ensure logging.processes exists
    if "logging" not in config:
        config["logging"] = tomlkit.table()
    if "processes" not in config["logging"]:
        config["logging"]["processes"] = tomlkit.aot()

    # Check if process already exists
    idx, existing = find_process_config(config, name, command)

    if existing is not None:
        # Update existing config
        existing["tagged"] = True
        existing["tag_color"] = AURORA_COLORS[color_lower]
        if display_name:
            existing["display_name"] = display_name
        if command:
            existing["command"] = command
        if log_path:
            existing["log_path"] = log_path
    else:
        # Create new process config
        proc = tomlkit.table()
        if display_name:
            proc["display_name"] = display_name
        if log_path:
            proc["log_path"] = log_path
        proc["name"] = name
        proc["command"] = command  # Required
        proc["tag_color"] = AURORA_COLORS[color_lower]
        proc["tagged"] = True
        config["logging"]["processes"].append(proc)

    save_config(config)

    display = display_name or name
    msg = f"Tagged '{name}' as '{display}' with {color} color."
    if log_path:
        msg += f" Log: {log_path}"
    msg += " mbtop will reload automatically."
    return msg


@mcp.tool()
def untag_process(name: str, command: str) -> str:
    """
    Remove tagging from a process (keeps other config like log_path).

    Args:
        name: Process name to untag
        command: Full command line to match (REQUIRED)

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    idx, existing = find_process_config(config, name, command)

    if existing is None:
        return f"No config found for process '{name}' with command '{command}'"

    # Remove tagging but keep other config
    existing["tagged"] = False
    existing.pop("tag_color", None)

    save_config(config)
    return f"Removed tagging from '{name}'. mbtop will reload automatically."


@mcp.tool()
def remove_process_config(name: str, command: str) -> str:
    """
    Completely remove a process config from mbtop.

    Args:
        name: Process name to remove
        command: Full command line to match (REQUIRED)

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    idx, existing = find_process_config(config, name, command)

    if existing is None:
        return f"No config found for process '{name}' with command '{command}'"

    del config["logging"]["processes"][idx]

    save_config(config)
    return f"Removed config for '{name}'. mbtop will reload automatically."


@mcp.tool()
def set_filter_tagged(enabled: bool) -> str:
    """
    Enable or disable the tagged process filter in mbtop.
    When enabled, mbtop only shows tagged processes.
    Only affects instances where PROC panel is visible.

    Args:
        enabled: True to show only tagged processes, False to show all

    Returns:
        Success message
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    # Ensure process section exists
    if "process" not in config:
        config["process"] = tomlkit.table()

    config["process"]["proc_filter_tagged"] = enabled

    save_config(config)

    status = "enabled" if enabled else "disabled"
    return f"Tagged filter {status}. Only instances showing PROC will be affected."


@mcp.tool()
def list_tagged_processes() -> str:
    """
    List all currently tagged processes in mbtop config.

    Returns:
        Formatted list of tagged processes with their colors and display names
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    tagged = [p for p in processes if p.get("tagged", False)]

    if not tagged:
        return "No tagged processes configured."

    # Reverse lookup for color names
    color_names = {v: k for k, v in AURORA_COLORS.items()}

    lines = ["Tagged processes:"]
    for proc in tagged:
        name = proc.get("name", "?")
        display = proc.get("display_name", name)
        color = color_names.get(proc.get("tag_color", ""), "unknown")
        command = proc.get("command", "")

        line = f"  - {name}"
        if display != name:
            line += f" (display: {display})"
        line += f" [{color}]"
        if command:
            line += f" cmd: {command[:50]}..."
        lines.append(line)

    # Also show filter status
    filter_enabled = config.get("process", {}).get("proc_filter_tagged", False)
    lines.append(f"\nTagged filter: {'enabled' if filter_enabled else 'disabled'}")

    return "\n".join(lines)


@mcp.tool()
def get_aurora_colors() -> str:
    """
    Get the available Nord Aurora colors for process tagging.

    Returns:
        List of available color names
    """
    return """Available colors (Nord Aurora palette):
  - red     (nord11 - #BF616A) - Alerts, errors, critical
  - orange  (nord12 - #D08770) - Warnings, attention needed
  - yellow  (nord13 - #EBCB8B) - Active work, in progress
  - green   (nord14 - #A3BE8C) - Healthy, OK, success
  - violet  (nord15 - #B48EAD) - Info, background, neutral"""


@mcp.tool()
def get_mbtop_status() -> str:
    """
    Get the current mbtop configuration status.

    Returns:
        Summary of mbtop config including tagged filter state and process count
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    tagged_count = len([p for p in processes if p.get("tagged", False)])
    filter_enabled = config.get("process", {}).get("proc_filter_tagged", False)

    return f"""mbtop Status:
  Config path: {get_config_path()}
  Process configs: {len(processes)}
  Tagged processes: {tagged_count}
  Tagged filter: {"enabled" if filter_enabled else "disabled"}"""


def main():
    """Run the MCP server."""
    mcp.run()


if __name__ == "__main__":
    main()
