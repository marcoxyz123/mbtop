/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#include "mbtop_menu.hpp"

#include "mbtop_config.hpp"
#include "mbtop_draw.hpp"
#include "mbtop_log.hpp"
#include "mbtop_shared.hpp"
#include "mbtop_theme.hpp"
#include "mbtop_tools.hpp"

#include <errno.h>
#include <signal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/mount.h>
#include "osx/apple_silicon_gpu.hpp"
#endif

#if defined(__linux__)
#include <mntent.h>
#endif

using std::array;
using std::ceil;
using std::max;
using std::min;
using std::ref;
using std::views::iota;

using namespace Tools;

namespace fs = std::filesystem;

namespace Menu {

   atomic<bool> active (false);
   string bg;
   bool redraw{true};
   int currentMenu = -1;
   msgBox messageBox;
   int signalToSend{};
   int signalKillRet{};

   const array<string, 32> P_Signals = {
	   "0",
#ifdef __linux__
#if defined(__hppa__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGSTKFLT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGXCPU",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGVTALRM",
		"SIGPROF", "SIGIO", "SIGWINCH", "SIGSTOP",
		"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU",
		"SIGURG", "SIGXFSZ", "SIGSYS"
#elif defined(__mips__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGWINCH",
		"SIGURG", "SIGIO", "SIGSTOP", "SIGTSTP",
		"SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM",
		"SIGPROF", "SIGXCPU", "SIGXFSZ"
#elif defined(__alpha__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGPWR", "SIGUSR1", "SIGUSR2"
#elif defined (__sparc__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGLOST", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT",	"SIGQUIT",	"SIGILL",
		"SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE",
		"SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT",
		"SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP",
		"SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGIO", "SIGPWR", "SIGSYS"
#endif
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGINFO", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "7", "SIGFPE",
		"SIGKILL", "10", "SIGSEGV", "12",
		"SIGPIPE", "SIGALRM", "SIGTERM", "16",
		"17", "18", "19", "20",
		"21", "22", "23", "24",
		"25", "26", "27", "28",
		"29", "30", "31"
#endif
	};

  std::unordered_map<string, Input::Mouse_loc> mouse_mappings;

   const array<array<string, 3>, 3> menu_normal = {
		array<string, 3>{
			"┌─┐┌─┐┌┬┐┌┬┐┬┌┐┌┌─┐┌─┐",
			"└─┐├┤  │  │ │││││ ┬└─┐",
			"└─┘└─┘ ┴  ┴ ┴┘└┘└─┘└─┘"
		},
		{
			"┬ ┬┌─┐┬  ┌─┐",
			"├─┤├┤ │  ├─┘",
			"┴ ┴└─┘┴─┘┴  "
		},
		{
			"┌─┐ ┬ ┬ ┬┌┬┐",
			"│─┼┐│ │ │ │ ",
			"└─┘└└─┘ ┴ ┴ "
		}
	};

	const array<array<string, 3>, 3> menu_selected = {
		array<string, 3>{
			"╔═╗╔═╗╔╦╗╔╦╗╦╔╗╔╔═╗╔═╗",
			"╚═╗╠╣  ║  ║ ║║║║║ ╦╚═╗",
			"╚═╝╚═╝ ╩  ╩ ╩╝╚╝╚═╝╚═╝"
		},
		{
			"╦ ╦╔═╗╦  ╔═╗",
			"╠═╣╠╣ ║  ╠═╝",
			"╩ ╩╚═╝╩═╝╩  "
		},
		{
			"╔═╗ ╦ ╦ ╦╔╦╗ ",
			"║═╬╗║ ║ ║ ║  ",
			"╚═╝╚╚═╝ ╩ ╩  "
		}
	};

	const array<int, 3> menu_width = {22, 12, 12};

	const vector<array<string, 2>> help_text = {
		{"Mouse 1", "Clicks buttons and selects in process list."},
		{"Mouse scroll", "Scrolls any scrollable list/text under cursor."},
		{"Esc, m", "Toggles main menu."},
		{"p", "Cycle view presets forwards."},
		{"shift + p", "Cycle view presets backwards."},
		{"1", "Toggle CPU box."},
		{"2", "Toggle MEM box."},
		{"3", "Toggle NET box."},
		{"4", "Toggle PROC box."},
		{"5", "Toggle GPU box."},
		{"6", "Toggle ANE split view (Apple Silicon)."},
		{"7", "Toggle Power panel (Apple Silicon)."},
		{"8", "Toggle Logs panel (requires Follow mode)."},
		{"P (Logs)", "Pause/resume log streaming."},
		{"L (Logs)", "Toggle live/historical log mode."},
		{"O (Logs)", "Cycle log level filter (All/Error/Info+)."},
		{"A", "GPU memory allocation menu (Apple Silicon)."},
		{"d", "Toggle disks view in MEM box."},
		{"F2, o", "Shows options."},
		{"F1, ?, h", "Shows this window."},
		{"ctrl + z", "Sleep program and put in background."},
		{"ctrl + r", "Reloads config file from disk."},
		{"q, ctrl + c", "Quits program."},
		{"+, -", "Add/Subtract 100ms to/from update timer."},
		{"Up, Down", "Select in process list."},
		{"Enter", "Show detailed information for selected process."},
		{"Spacebar", "Expand/collapse the selected process in tree view."},
		{"C", "Expand/collapse the selected process' children."},
		{"Pg Up, Pg Down", "Jump 1 page in process list."},
		{"Home, End", "Jump to first or last page in process list."},
		{"Left, Right", "Select previous/next sorting column."},
		{"b, n", "Select previous/next network device."},
		{"i", "Toggle disks io mode with big graphs."},
		{"Tab", "Cycle memory/disk selection mode."},
		{"M, B", "Toggle meter/braille display mode in MEM box."},
		{"T", "Toggle memory item visibility mode."},
		{"S", "Toggle swap item visibility mode."},
		{"V", "Toggle VRAM item visibility mode (Apple Silicon)."},
		{"z", "Toggle totals reset for current network device"},
		{"a", "Toggle auto scaling for the network graphs."},
		{"y", "Toggle synced scaling mode for network graphs."},
		{"f, /", "To enter a process filter. Start with ! for regex."},
		{"F", "Follow selected process."},
		{"u", "Pause process list."},
		{"delete", "Clear any entered filter."},
		{"c", "Toggle per-core cpu usage of processes."},
		{"r", "Reverse sorting order in processes box."},
		{"e", "Toggle processes tree view."},
		{"%", "Toggles memory display mode in processes box."},
		{"Selected +, -", "Expand/collapse the selected process in tree view."},
		{"Selected t", "Terminate selected process with SIGTERM - 15."},
		{"Selected k", "Kill selected process with SIGKILL - 9."},
		{"Selected s", "Select or enter signal to send to process."},
		{"Selected N", "Select new nice value for selected process."},
		{"", " "},
		{"", "VIM KEYS (options): h,j,k,l,g,G for navigation."},
		{"", "Help 'h' becomes 'H', kill 'k' becomes 'K'."},
		{"", " "},
		{"", "For bug reporting and project updates, visit:"},
		{"", "https://github.com/aristocratos/btop"},
	};

	const vector<vector<vector<string>>> categories = {
		{
			{"color_theme",
				"Set color theme.",
				"",
				"Choose from all theme files in (usually)",
				"\"/usr/[local/]share/mbtop/themes\" and",
				"\"~/.config/mbtop/themes\".",
				"",
				"\"Default\" for builtin default theme.",
				"\"TTY\" for builtin 16-color theme.",
				"",
				"For theme updates see:",
				"https://github.com/aristocratos/btop"},
			{"theme_background",
				"If the theme set background should be shown.",
				"",
				"Set to False if you want terminal background",
				"transparency."},
			{"truecolor",
				"Sets if 24-bit truecolor should be used.",
				"",
				"Will convert 24-bit colors to 256 color",
				"(6x6x6 color cube) if False.",
				"",
				"Set to False if your terminal doesn't have",
				"truecolor support and can't convert to",
				"256-color."},
			{"force_tty",
				"TTY mode.",
				"",
				"Set to true to force tty mode regardless",
				"if a real tty has been detected or not.",
				"",
				"Will force 16-color mode and TTY theme,",
				"set all graph symbols to \"tty\" and swap",
				"out other non tty friendly symbols."},
			{"vim_keys",
				"Enable vim keys.",
				"Set to True to enable \"h,j,k,l\" keys for",
				"directional control in lists.",
				"",
				"Conflicting keys for",
				"h (help) and k (kill)",
				"is accessible while holding shift."},
			{"disable_mouse",
				"Disable all mouse events."},
			{"presets",
				"Define presets for the layout of the boxes.",
				"",
				"Preset 0 is always all boxes shown with",
				"default settings.",
				"Max 9 presets.",
				"",
				"Format: \"box_name:P:G,box_name:P:G\"",
				"P=(0 or 1) for alternate positions.",
				"G=graph symbol to use for box.",
				"",
				"Use whitespace \" \" as separator between",
				"different presets.",
				"",
				"Example:",
				"\"mem:0:tty,proc:1:default cpu:0:braille\""},
			{"shown_boxes",
				"Manually set which boxes to show.",
				"",
				"Available values are \"cpu pwr mem net proc\".",
			#ifdef GPU_SUPPORT
				"Or \"gpu0\" through \"gpu5\" for GPU boxes.",
			#endif
				"Separate values with whitespace.",
				"",
				"Toggle between presets with key \"p\"."},
			{"update_ms",
				"Update time in milliseconds.",
				"",
				"Recommended 2000 ms or above for better",
				"sample times for graphs.",
				"",
				"Min value: 100 ms",
				"Max value: 86400000 ms = 24 hours."},
			{"rounded_corners",
				"Rounded corners on boxes.",
				"",
				"True or False",
				"",
				"Is always False if TTY mode is ON."},
			{"terminal_sync",
				"Output synchronization.",
				"",
				"Use terminal synchronized output sequences",
				"to reduce flickering on supported terminals.",
				"",
				"True or False."},
			{"graph_symbol",
				"Default symbols to use for graph creation.",
				"",
				"\"braille\", \"block\" or \"tty\".",
				"",
				"\"braille\" offers the highest resolution but",
				"might not be included in all fonts.",
				"",
				"\"block\" has half the resolution of braille",
				"but uses more common characters.",
				"",
				"\"tty\" uses only 3 different symbols but will",
				"work with most fonts.",
				"",
				"Note that \"tty\" only has half the horizontal",
				"resolution of the other two,",
				"so will show a shorter historical view."},
			{"clock_format",
				"Clock format using strftime syntax.",
				"(Displayed left of update interval)",
				"",
				"Empty string to disable clock.",
				"",
				"Examples of strftime formats:",
				"\"%X\" = locale HH:MM:SS",
				"\"%H:%M:%S\" = 24h format",
				"\"%I:%M:%S %p\" = 12h with AM/PM",
				"\"%d\" = day, \"%m\" = month, \"%y\" = year"},
			{"clock_12h",
				"Use 12-hour clock format.",
				"",
				"Shows time as HH:MM:SS AM/PM",
				"instead of 24-hour format.",
				"",
				"Overrides clock_format when enabled.",
				"",
				"True or False."},
			{"show_hostname",
				"Show hostname in CPU box header.",
				"(Displayed in center position)",
				"",
				"Strips \".local\" suffix but keeps",
				"full FQDN (e.g., host.domain.com).",
				"",
				"True or False."},
			{"show_uptime_header",
				"Show system uptime in header.",
				"(Displayed after preset button)",
				"",
				"Shows uptime in d:h:m:s format.",
				"",
				"True or False."},
			{"show_username_header",
				"Show current username in header.",
				"(Displayed after uptime)",
				"",
				"True or False."},
			{"preset_names",
				"Custom names for presets 1-9.",
				"",
				"Names separated by whitespace.",
				"Displayed after preset number.",
				"",
				"Example:",
				"\"Standard LLM Processes Power\"",
				"",
				"Leave empty to use default numbers."},
			{"base_10_sizes",
				"Use base 10 for bits and bytes sizes.",
				"",
				"Uses KB = 1000 instead of KiB = 1024,",
				"MB = 1000KB instead of MiB = 1024KiB,",
				"and so on.",
				"",
				"True or False."},
			{"background_update",
				"Update main ui when menus are showing.",
				"",
				"True or False.",
				"",
				"Set this to false if the menus is flickering",
				"too much for a comfortable experience."},
			{"show_battery",
				"Show battery stats.",
				"(Only visible if cpu box is enabled!)",
				"",
				"Show battery stats in the top right corner",
				"if a battery is present."},
			{"selected_battery",
				"Select battery.",
				"",
				"Which battery to use if multiple are present.",
				"Can be both batteries and UPS.",
				"",
				"\"Auto\" for auto detection."},
			{"show_battery_watts",
				"Show battery power.",
				"",
				"Show discharge power when discharging.",
				"Show charging power when charging."},
			{"log_level",
				"Set loglevel for error.log",
				"",
				"\"ERROR\", \"WARNING\", \"INFO\" and \"DEBUG\".",
				"",
				"The level set includes all lower levels,",
				"i.e. \"DEBUG\" will show all logging info."},
			{"save_config_on_exit",
				"Save config on exit.",
				"",
				"Automatically save current settings to",
				"config file on exit.",
				"",
				"When this is toggled from True to False",
				"a save is immediately triggered.",
				"This way a manual save can be done by",
				"toggling this setting on and off again."}
		},
		{
			{"cpu_bottom",
				"Cpu box location.",
				"",
				"Show cpu box at bottom of screen instead",
				"of top."},
			{"graph_symbol_cpu",
				"Graph symbol to use for graphs in cpu box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"cpu_graph_upper",
				"Cpu upper graph.",
				"",
				"Sets the CPU/GPU stat shown in upper half of",
				"the CPU graph.",
				"",
				"CPU:",
				"\"total\" = Total cpu usage. (Auto)",
				"\"user\" = User mode cpu usage.",
				"\"system\" = Kernel mode cpu usage.",
				"+ more depending on kernel.",
		#ifdef GPU_SUPPORT
				"",
				"GPU:",
				"\"gpu-totals\" = GPU usage split by device.",
				"\"gpu-vram-totals\" = VRAM usage split by GPU.",
				"\"gpu-pwr-totals\" = Power usage split by GPU.",
				"\"gpu-average\" = Avg usage of all GPUs.",
				"\"gpu-vram-total\" = VRAM usage of all GPUs.",
				"\"gpu-pwr-total\" = Power usage of all GPUs.",
				"Not all stats are supported on all devices."
		#endif
				},
			{"cpu_graph_lower",
				"Cpu lower graph.",
				"",
				"Sets the CPU/GPU stat shown in lower half of",
				"the CPU graph.",
				"",
				"CPU:",
				"\"total\" = Total cpu usage.",
				"\"user\" = User mode cpu usage.",
				"\"system\" = Kernel mode cpu usage.",
				"+ more depending on kernel.",
		#ifdef GPU_SUPPORT
				"",
				"GPU:",
				"\"gpu-totals\" = GPU usage split/device. (Auto)",
				"\"gpu-vram-totals\" = VRAM usage split by GPU.",
				"\"gpu-pwr-totals\" = Power usage split by GPU.",
				"\"gpu-average\" = Avg usage of all GPUs.",
				"\"gpu-vram-total\" = VRAM usage of all GPUs.",
				"\"gpu-pwr-total\" = Power usage of all GPUs.",
				"Not all stats are supported on all devices."
		#endif
				},
			{"cpu_invert_lower",
					"Toggles orientation of the lower CPU graph.",
					"",
					"True or False."},
			{"cpu_single_graph",
					"Completely disable the lower CPU graph.",
					"",
					"Shows only upper CPU graph and resizes it",
					"to fit to box height.",
					"",
					"True or False."},
		#ifdef GPU_SUPPORT
			{"show_gpu_info",
					"Show gpu info in cpu box.",
					"",
					"Toggles gpu stats in cpu box and the",
					"gpu graph (if \"cpu_graph_lower\" is set to",
					"\"Auto\").",
					"",
					"\"Auto\" to show when no gpu box is shown.",
					"\"On\" to always show.",
					"\"Off\" to never show."},
		#endif
			{"check_temp",
				"Enable cpu temperature reporting.",
				"",
				"True or False."},
			{"cpu_sensor",
				"Cpu temperature sensor.",
				"",
				"Select the sensor that corresponds to",
				"your cpu temperature.",
				"",
				"Set to \"Auto\" for auto detection."},
			{"show_coretemp",
				"Show temperatures for cpu cores.",
				"",
				"Only works if check_temp is True and",
				"the system is reporting core temps."},
			{"cpu_core_map",
				"Custom mapping between core and coretemp.",
				"",
				"Can be needed on certain cpus to get correct",
				"temperature for correct core.",
				"",
				"Use lm-sensors or similar to see which cores",
				"are reporting temperatures on your machine.",
				"",
				"Format: \"X:Y\"",
				"X=core with wrong temp.",
				"Y=core with correct temp.",
				"Use space as separator between multiple",
				"entries.",
				"",
				"Example: \"4:0 5:1 6:3\""},
			{"temp_scale",
				"Which temperature scale to use.",
				"",
				"Celsius, default scale.",
				"",
				"Fahrenheit, the american one.",
				"",
				"Kelvin, 0 = absolute zero, 1 degree change",
				"equals 1 degree change in Celsius.",
				"",
				"Rankine, 0 = absolute zero, 1 degree change",
				"equals 1 degree change in Fahrenheit."},
			{"show_cpu_freq",
				"Show CPU frequency.",
				"",
				"Can cause slowdowns on systems with many",
				"cores and certain kernel versions."},
		#ifdef __linux__
			{"freq_mode",
				"How the CPU frequency will be displayed.",
				"",
				"First, get the frequency from the first",
				"core.",
				"",
				"Range, show the lowest and the highest",
				"frequency.",
				"",
				"Lowest, the lowest frequency.",
				"",
				"Highest, the highest frequency.",
				"",
				"Average, sum and divide."},
		#endif
			{"custom_cpu_name",
				"Custom cpu model name in cpu percentage box.",
				"",
				"Empty string to disable."},
			{"show_uptime",
				"Shows the system uptime in the CPU box.",
				"",
				"Can also be shown in the clock by using",
				"\"/uptime\" in the formatting.",
				"",
				"True or False."},
			{"show_cpu_watts",
				"Shows the CPU power consumption in watts.",
				"",
				"Requires running `make setcap` or",
				"`make setuid` or running with sudo.",
				"",
				"True or False."},
		},
	#ifdef GPU_SUPPORT
		{
			{"nvml_measure_pcie_speeds",
				"Measure PCIe throughput on NVIDIA cards.",
				"",
				"May impact performance on certain cards.",
				"",
				"True or False."},
			{"rsmi_measure_pcie_speeds",
				"Measure PCIe throughput on AMD cards.",
				"",
				"May impact performance on certain cards.",
				"",
				"True or False."},
			{"graph_symbol_gpu",
				"Graph symbol to use for graphs in gpu box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"gpu_mirror_graph",
				"Horizontally mirror the GPU graph.",
				"",
				"True or False."},
			{"shown_gpus",
				"Manually set which gpu vendors to show.",
				"",
				"Available values are",
				"\"nvidia\", \"amd\", and \"intel\".",
				"Separate values with whitespace.",
				"",
				"A restart is required to apply changes."},
			{"custom_gpu_name0",
				"Custom gpu0 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name1",
				"Custom gpu1 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name2",
				"Custom gpu2 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name3",
				"Custom gpu3 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name4",
				"Custom gpu4 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name5",
				"Custom gpu5 model name in gpu stats box.",
				"",
				"Empty string to disable."},
		},
	#endif
		{
			{"mem_below_net",
				"Mem box location.",
				"",
				"Show mem box below net box instead of above."},
			{"net_beside_mem",
				"Net box beside mem.",
				"",
				"Show net box beside mem box instead of",
				"above/below. Pressing '3' cycles:",
				"hidden -> stacked -> beside.",
				"",
				"True or False."},
			{"graph_symbol_mem",
				"Graph symbol to use for graphs in mem box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"mem_graphs",
				"Show graphs for memory values.",
				"",
				"True or False."},
			{"show_disks",
				"Split memory box to also show disks.",
				"",
				"True or False."},
			{"show_io_stat",
				"Toggle IO activity graphs.",
				"",
				"Show small IO graphs that for disk activity",
				"(disk busy time) when not in IO mode.",
				"",
				"True or False."},
			{"io_mode",
				"Toggles io mode for disks.",
				"",
				"Shows big graphs for disk read/write speeds",
				"instead of used/free percentage meters.",
				"",
				"True or False."},
			{"io_graph_combined",
				"Toggle combined read and write graphs.",
				"",
				"Only has effect if \"io mode\" is True.",
				"",
				"True or False."},
			{"io_graph_speeds",
				"Set top speeds for the io graphs.",
				"",
				"Manually set which speed in MiB/s that",
				"equals 100 percent in the io graphs.",
				"(100 MiB/s by default).",
				"",
				"Format: \"device:speed\" separate disks with",
				"whitespace \" \".",
				"",
				"Example: \"/dev/sda:100, /dev/sdb:20\"."},
			{"show_swap",
				"If swap memory should be shown in memory box.",
				"",
				"True or False."},
			{"swap_disk",
				"Show swap as a disk.",
				"",
				"Ignores show_swap value above.",
				"Inserts itself after first disk."},
			{"only_physical",
				"Filter out non physical disks.",
				"",
				"Set this to False to include network disks,",
				"RAM disks and similar.",
				"",
				"True or False."},
			{"show_network_drives",
				"(macOS) Show network drives in disks list.",
				"",
				"Includes NFS, SMB, and AFP network shares.",
				"The protocol type will be shown",
				"in parentheses after the disk name.",
				"",
				"True or False."},
			{"use_fstab",
				"(Linux) Read disks list from /etc/fstab.",
				"",
				"This also disables only_physical.",
				"",
				"True or False."},
			{"zfs_hide_datasets",
				"(Linux) Hide ZFS datasets in disks list.",
				"",
				"Setting this to True will hide all datasets,",
				"and only show ZFS pools.",
				"",
				"(IO stats will be calculated per-pool)",
				"",
				"True or False."},
			{"disk_free_priv",
				"(Linux) Type of available disk space.",
				"",
				"Set to true to show how much disk space is",
				"available for privileged users.",
				"",
				"Set to false to show available for normal",
				"users."},
			{"disks_filter",
				"Optional filter for shown disks.",
				"",
				"Should be full path of a mountpoint.",
				"Separate multiple values with",
				"whitespace \" \".",
				"",
				"Only disks matching the filter will be shown.",
				"Prepend \033[3mexclude=\033[23m to only show disks ",
				"not matching the filter.",
				"",
				"Examples:",
				"/boot /home/user",
				"exclude=/boot /home/user"},
			{"zfs_arc_cached",
				"(Linux) Count ZFS ARC as cached memory.",
				"",
				"Add ZFS ARC used to cached memory and",
				"ZFS ARC available to available memory.",
				"These are otherwise reported by the Linux",
				"kernel as used memory.",
				"",
				"True or False."},
			{"mem_show_used",
				"Show 'Used' memory item.",
				"",
				"Toggle visibility of the Used memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_available",
				"Show 'Available' memory item.",
				"",
				"Toggle visibility of the Available",
				"memory bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_cached",
				"Show 'Cached' memory item.",
				"",
				"Toggle visibility of the Cached memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_free",
				"Show 'Free' memory item.",
				"",
				"Toggle visibility of the Free memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"swap_show_used",
				"Show 'Swap Used' item.",
				"",
				"Toggle visibility of the Swap Used",
				"bar in the Swap section.",
				"Can also toggle with Shift+S in mem box."},
			{"swap_show_free",
				"Show 'Swap Free' item.",
				"",
				"Toggle visibility of the Swap Free",
				"bar in the Swap section.",
				"Can also toggle with Shift+S in mem box."},
			{"vram_show_used",
				"Show 'VRAM Used' item.",
				"",
				"Toggle visibility of the VRAM Used",
				"bar in the VRAM section.",
				"Can also toggle with Shift+V in mem box."},
			{"vram_show_free",
				"Show 'VRAM Free' item.",
				"",
				"Toggle visibility of the VRAM Free",
				"bar in the VRAM section.",
				"Can also toggle with Shift+V in mem box."},
		},
		{
			{"graph_symbol_net",
				"Graph symbol to use for graphs in net box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"swap_upload_download",
				"Swap the positions of the upload and download",
				"graphs.",
				"",
				"This allows for a more \"intuitive\" view",
				"with download being down, on the bottom."},
			{"net_download",
				"Fixed network graph download value.",
				"",
				"Value in Mebibits, default \"100\".",
				"",
				"Can be toggled with auto button."},
			{"net_upload",
				"Fixed network graph upload value.",
				"",
				"Value in Mebibits, default \"100\".",
				"",
				"Can be toggled with auto button."},
			{"net_auto",
				"Start in network graphs auto rescaling mode.",
				"",
				"Ignores any values set above at start and",
				"rescales down to 10Kibibytes at the lowest.",
				"",
				"True or False."},
			{"net_sync",
				"Network scale sync.",
				"",
				"Syncs the scaling for download and upload to",
				"whichever currently has the highest scale.",
				"",
				"True or False."},
			{"net_iface",
				"Network Interface.",
				"",
				"Manually set the starting Network Interface.",
				"",
				"Will otherwise automatically choose the NIC",
				"with the highest total download since boot."},
		    {"base_10_bitrate",
			    "Base 10 bitrate",
			    "",
			    "True:  Use SI prefixes for bitrates",
			    "       (1000Kbps = 1Mbps)",
			    "False: Use binary prefixes for bitrates",
			    "       (1024Kibps = 1Mibps)",
			    "Auto:  Use the General -> Base 10 Sizes",
			    "       setting for bitrates",
			    "",
			    "True, False, or Auto",},
		},
		{
			{"proc_left",
				"Proc box location.",
				"",
				"Show proc box on left side of screen",
				"instead of right."},
			{"proc_full_width",
				"Proc full width (side-by-side mode).",
				"",
				"When net_beside_mem is active, show proc",
				"panel full width under both mem+net.",
				"Pressing '4' cycles: hidden -> under",
				"net -> full width.",
				"",
				"True or False."},
			{"graph_symbol_proc",
				"Graph symbol to use for graphs in proc box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"proc_sorting",
				"Processes sorting option.",
				"",
				"Possible values:",
				"\"pid\", \"program\", \"arguments\", \"threads\",",
				"\"user\", \"memory\", \"cpu lazy\" and",
				"\"cpu direct\".",
				"",
				"\"cpu lazy\" updates top process over time.",
				"\"cpu direct\" updates top process",
				"directly."},
			{"proc_reversed",
				"Reverse processes sorting order.",
				"",
				"True or False."},
			{"proc_tree",
				"Processes tree view.",
				"",
				"Set true to show processes grouped by",
				"parents with lines drawn between parent",
				"and child process."},
			{"proc_aggregate",
				"Aggregate child's resources in parent.",
				"",
				"In tree-view, include all child resources",
				"with the parent even while expanded."},
			{"proc_colors",
				"Enable colors in process view.",
				"",
				"True or False."},
			{"proc_gradient",
				"Enable process view gradient fade.",
				"",
				"Fades from top or current selection.",
				"Max fade value is equal to current themes",
				"\"inactive_fg\" color value."},
			{"proc_per_core",
				"Process usage per core.",
				"",
				"If process cpu usage should be of the core",
				"it's running on or usage of the total",
				"available cpu power.",
				"",
				"If true and process is multithreaded",
				"cpu usage can reach over 100%."},
			{"proc_mem_bytes",
				"Show memory as bytes in process list.",
				" ",
				"Will show percentage of total memory",
				"if False."},
			{"keep_dead_proc_usage",
				"Cpu and Mem usage for dead processes",
				"",
				"Set true if process should preserve the cpu",
				"and memory usage of when it died while",
				"paused."},
			{"proc_cpu_graphs",
				"Show cpu graph for each process.",
				"",
				"True or False"},
			{"proc_gpu_graphs",
				"Show gpu graph for each process.",
				"",
				"Only visible when GPU column is enabled.",
				"",
				"True or False"},
			{"proc_filter_kernel",
				"(Linux) Filter kernel processes from output.",
				"",
				"Set to 'True' to filter out internal",
				"processes started by the Linux kernel."},
			{"proc_follow_detailed",
				"Follow selected process with detailed view",
				"",
				"If set to 'True' then when opening the",
				"detailed view, the process will be",
				"followed in the list. Pressing enter",
				"again will close the detailed view",
				"and stop following the process."},
		}
	};

	msgBox::msgBox() {}
	msgBox::msgBox(int width, int boxtype, const vector<string>& content, const std::string_view title)
	: width(width), boxtype(boxtype) {
		auto tty_mode = Config::getB("tty_mode");
		auto rounded = Config::getB("rounded_corners");
		const auto& right_up = (tty_mode or not rounded ? Symbols::right_up : Symbols::round_right_up);
		const auto& left_up = (tty_mode or not rounded ? Symbols::left_up : Symbols::round_left_up);
		const auto& right_down = (tty_mode or not rounded ? Symbols::right_down : Symbols::round_right_down);
		const auto& left_down = (tty_mode or not rounded ? Symbols::left_down : Symbols::round_left_down);
		height = content.size() + 7;
		x = Term::width / 2 - width / 2;
		y = Term::height/2 - height/2;
		if (boxtype == 2) selected = 1;


		button_left = left_up + Symbols::h_line * 6 + Mv::l(7) + Mv::d(2) + left_down + Symbols::h_line * 6 + Mv::l(7) + Mv::u(1) + Symbols::v_line;
		button_right = Symbols::v_line + Mv::l(7) + Mv::u(1) + Symbols::h_line * 6 + right_up + Mv::l(7) + Mv::d(2) + Symbols::h_line * 6 + right_down + Mv::u(2);

		box_contents = Draw::createBox(x, y, width, height, Theme::c("hi_fg"), true, title) + Mv::d(1);
		for (const auto& line : content) {
			box_contents += Mv::save + Mv::r(max((size_t)0, (width / 2) - (Fx::uncolor(line).size() / 2) - 1)) + line + Mv::restore + Mv::d(1);
		}
	}

	string msgBox::operator()() {
		string out;
		int pos = width / 2 - (boxtype == 0 ? 6 : 14);
		const auto first_color = (selected == 0 ? Theme::c("hi_fg") : Theme::c("div_line"));
		out = Mv::d(1) + Mv::r(pos) + Fx::b + first_color + button_left + (selected == 0 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
			+ (boxtype == 0 ? "    Ok    " : "    Yes    ") + first_color + button_right;
		mouse_mappings["button1"] = Input::Mouse_loc{y + height - 4, x + pos + 1, 3, 12 + (boxtype > 0 ? 1 : 0)};
		if (boxtype > 0) {
			const auto second_color = (selected == 1 ? Theme::c("hi_fg") : Theme::c("div_line"));
			out += Mv::r(2) + second_color + button_left + (selected == 1 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
				+ "    No    " + second_color + button_right;
			mouse_mappings["button2"] = Input::Mouse_loc{y + height - 4, x + pos + 15 + (boxtype > 0 ? 1 : 0), 3, 12};
		}
		return box_contents + out + Fx::reset;
	}

	//? Process input
	int msgBox::input(const string& key) {
		if (key.empty()) return Invalid;

		if (is_in(key, "escape", "backspace", "q") or key == "button2") {
			return No_Esc;
		}
		else if (key == "button1" or (boxtype == 0 and str_to_upper(key) == "O")) {
			return Ok_Yes;
		}
		else if (is_in(key, "enter", "space")) {
			return selected + 1;
		}
		else if (boxtype == 0) {
			return Invalid;
		}
		else if (str_to_upper(key) == "Y") {
			return Ok_Yes;
		}
		else if (str_to_upper(key) == "N") {
			return No_Esc;
		}
		else if (is_in(key, "right", "tab")) {
			if (++selected > 1) selected = 0;
			return Select;
		}
		else if (is_in(key, "left", "shift_tab")) {
			if (--selected < 0) selected = 1;
			return Select;
		}

		return Invalid;
	}

	void msgBox::clear() {
		box_contents.clear();
		box_contents.shrink_to_fit();
		button_left.clear();
		button_left.shrink_to_fit();
		button_right.clear();
		button_right.shrink_to_fit();
		mouse_mappings.erase("button1");
		mouse_mappings.erase("button2");
	}

	enum menuReturnCodes {
		NoChange,
		Changed,
		Closed,
		Switch
	};

	static int signalChoose(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		static int x{};
		static int y{};
		static int selected_signal = -1;

		if (bg.empty()) selected_signal = -1;
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 40;
			y = Term::height/2 - 9;
			bg = Draw::createBox(x + 2, y, 78, 19, Theme::c("hi_fg"), true, "signals");
			bg += Mv::to(y+2, x+3) + Theme::c("title") + Fx::b + cjust("Send signal to PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name")), 30) + ")", 76);
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			int new_select = (key.length() > 7) ? stoi_safe(key.substr(7), -1) : -1;
			if (new_select >= 0 and new_select == selected_signal)
				goto ChooseEntering;
			else if (new_select >= 0)
				selected_signal = new_select;
		}
		else if (is_in(key, "enter", "space") and selected_signal >= 0) {
			ChooseEntering:
			signalKillRet = 0;
			if (s_pid < 1) {
				signalKillRet = ESRCH;
				menuMask.set(SignalReturn);
			}
			else if (kill(s_pid, selected_signal) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			return Closed;
		}
		else if (key.size() == 1 and isdigit(key.at(0)) and selected_signal < 10) {
			string combined = (selected_signal < 1 ? key : to_string(selected_signal) + key);
			selected_signal = std::min(stoi_safe(combined, selected_signal), 64);
		}
		else if (key == "backspace" and selected_signal != -1) {
			selected_signal = (selected_signal < 10 ? -1 : selected_signal / 10);
		}
		else if (is_in(key, "up", "k") and selected_signal != 16) {
			if (selected_signal == 1) selected_signal = 31;
			else if (selected_signal < 6) selected_signal += 25;
			else {
				bool offset = (selected_signal > 16);
				selected_signal -= 5;
				if (selected_signal <= 16 and offset) selected_signal--;
			}
		}
		else if (is_in(key, "down", "j")) {
			if (selected_signal == 31) selected_signal = 1;
			else if (selected_signal < 1 or selected_signal == 16) selected_signal = 1;
			else if (selected_signal > 26) selected_signal -= 25;
			else {
				bool offset = (selected_signal < 16);
				selected_signal += 5;
				if (selected_signal >= 16 and offset) selected_signal++;
				if (selected_signal > 31) selected_signal = 31;
			}
		}
		else if (is_in(key, "left", "h") and selected_signal > 0 and selected_signal != 16) {
			if (--selected_signal < 1) selected_signal = 31;
			else if (selected_signal == 16) selected_signal--;
		}
		else if (is_in(key, "right", "l") and selected_signal <= 31 and selected_signal != 16) {
			if (++selected_signal > 31) selected_signal = 1;
			else if (selected_signal == 16) selected_signal++;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y+4, cx = x+4;
			out = bg + Mv::to(cy++, x+3) + Theme::c("main_fg") + Fx::ub
				+ rjust("Enter signal number: ", 48) + Theme::c("hi_fg") + (selected_signal >= 0 ? to_string(selected_signal) : "") + Theme::c("main_fg") + Fx::bl + "█" + Fx::ubl;

			auto sig_str = to_string(selected_signal);
			for (int count = 0, i = 0; const auto& sig : P_Signals) {
				if (count == 0 or count == 16) { count++; continue; }
				if (i++ % 5 == 0) { ++cy; cx = x+4; }
				out += Mv::to(cy, cx);
				if (count == selected_signal) out += Theme::c("selected_bg") + Theme::c("selected_fg") + Fx::b + ljust(to_string(count), 3) + ljust('(' + sig + ')', 12) + Fx::reset;
				else out += Theme::c("hi_fg") + ljust(to_string(count), 3) + Theme::c("main_fg") + ljust('(' + sig + ')', 12);
				if (redraw) mouse_mappings["button_" + to_string(count)] = {cy, cx, 1, 15};
				count++;
				cx += 15;
			}

			cy++;
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "↑ ↓ ← →", 33, true) + Theme::c("main_fg") + Fx::ub + " | To choose signal.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("0-9", 33) + Theme::c("main_fg") + Fx::ub + " | Enter manually.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ENTER", 33) + Theme::c("main_fg") + Fx::ub + " | To send signal.";
			mouse_mappings["enter"] = {cy, x, 1, 73};
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ESC or \"q\"", 33) + Theme::c("main_fg") + Fx::ub + " | To abort.";
			mouse_mappings["escape"] = {cy, x, 1, 73};

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

	static int sizeError(const string& key) {
		if (redraw) {
			auto min_size = Term::get_min_size(Config::getS("shown_boxes"));
			vector<string> cont_vec {
				Fx::b + Theme::g("used")[100] + "Error:" + Theme::c("main_fg") + Fx::ub,
				"Terminal size too small!" + Fx::reset,
				"",
				"Current:  " + to_string(Term::width) + " x " + to_string(Term::height),
				"Required: " + to_string(min_size.at(0)) + " x " + to_string(min_size.at(1)),
				"",
				"Tip: Rearrange panels (keys 1-5,7)",
				"or resize terminal window.",
				"(Proc details needs height ≥13)" + Fx::reset };

			messageBox = Menu::msgBox{45, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int signalSend(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		if (s_pid == 0) return Closed;
		if (redraw) {
			atomic_wait(Runner::active);
			auto& p_name = (s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name"));
			vector<string> cont_vec = {
				Fx::b + Theme::c("main_fg") + "Send signal: " + Fx::ub + Theme::c("hi_fg") + to_string(signalToSend)
				+ (signalToSend > 0 and signalToSend <= 32 ? Theme::c("main_fg") + " (" + P_Signals.at(signalToSend) + ')' : ""),

				Fx::b + Theme::c("main_fg") + "To PID: " + Fx::ub + Theme::c("hi_fg") + to_string(s_pid) + Theme::c("main_fg") + " ("
				+ uresize(p_name, 16) + ')' + Fx::reset,
			};
			messageBox = Menu::msgBox{50, 1, cont_vec, (signalToSend > 1 and signalToSend <= 32 and signalToSend != 17 ? P_Signals.at(signalToSend) : "signal")};
			Global::overlay = messageBox();
		}
		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes) {
			signalKillRet = 0;
			if (kill(s_pid, signalToSend) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::Select) {
			Global::overlay = messageBox();
			return Changed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int signalReturn(const string& key) {
		if (redraw) {
			vector<string> cont_vec;
			cont_vec.push_back(Fx::b + Theme::g("used")[100] + "Failure:" + Theme::c("main_fg") + Fx::ub);
			if (signalKillRet == EINVAL) {
				cont_vec.push_back("Unsupported signal!" + Fx::reset);
			}
			else if (signalKillRet == EPERM) {
				cont_vec.push_back("Insufficient permissions to send signal!" + Fx::reset);
			}
			else if (signalKillRet == ESRCH) {
				cont_vec.push_back("Process not found!" + Fx::reset);
			}
			else {
				cont_vec.push_back("Unknown error! (errno: " + to_string(signalKillRet) + ')' + Fx::reset);
			}

			messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int mainMenu(const string& key) {
		enum MenuItems { Options, Help, Quit };
		static int y{};
		static int selected{};
		static vector<string> colors_selected;
		static vector<string> colors_normal;
		auto tty_mode = Config::getB("tty_mode");
		if (bg.empty()) selected = 0;
		int retval = Changed;

		if (redraw) {
			y = Term::height/2 - 10;
			bg = Draw::banner_gen(y, 0, true);
			if (not tty_mode) {
				colors_selected = {
					Theme::hex_to_color(Global::Banner_src.at(2).at(1)),  //? Use TOP colors (cool), skip 2 leaf lines
					Theme::hex_to_color(Global::Banner_src.at(4).at(1)),
					Theme::hex_to_color(Global::Banner_src.at(6).at(1))
				};
				colors_normal = {
					Theme::hex_to_color("#CC"),
					Theme::hex_to_color("#AA"),
					Theme::hex_to_color("#80")
				};
			}
		}
		else if (is_in(key, "escape", "q", "m", "mouse_click")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			if (int new_select = key.back() - '0'; new_select == selected)
				goto MainEntering;
			else
				selected = new_select;
		}
		else if (is_in(key, "enter", "space")) {
			MainEntering:
			switch (selected) {
				case Options:
					menuMask.set(Menus::Options);
					currentMenu = Menus::Options;
					return Switch;
				case Help:
					menuMask.set(Menus::Help);
					currentMenu = Menus::Help;
					return Switch;
				case Quit:
					clean_quit(0);
			}
		}
		else if (is_in(key, "down", "tab", "mouse_scroll_down", "j")) {
			if (++selected > 2) selected = 0;
		}
		else if (is_in(key, "up", "shift_tab", "mouse_scroll_up", "k")) {
			if (--selected < 0) selected = 2;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg + Fx::reset + Fx::b;
			auto cy = y + 8;  // One line spacing after banner
			for (const auto& i : iota(0, 3)) {
				if (tty_mode) out += (i == selected ? Theme::c("hi_fg") : Theme::c("main_fg"));
				const auto& menu = (not tty_mode and i == selected ? menu_selected[i] : menu_normal[i]);
				const auto& colors = (i == selected ? colors_selected : colors_normal);
				if (redraw) mouse_mappings["button_" + to_string(i)] = {cy, Term::width/2 - menu_width[i]/2, 3, menu_width[i]};
				for (int ic = 0; const auto& line : menu) {
					out += Mv::to(cy++, Term::width/2 - menu_width[i]/2) + (tty_mode ? "" : colors[ic++]) + line;
				}
			}
			//? Add version below QUIT
			auto version_str = "v" + Global::Version;
			out += Mv::to(cy + 1, Term::width/2 - (int)version_str.size()/2)
				+ Theme::c("main_fg") + Fx::b + Fx::i + version_str;
			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

[[maybe_unused]] static int optionsMenu(const string& key) {
 		enum Predispositions { isBool, isInt, isString, is2D, isBrowsable, isEditable};
		static int y{};
		static int x{};
		static int height{};
		static int page{};
		static int pages{};
		static int selected{};
		static int select_max{};
		static int item_height{};
		static int selected_cat{};
		static int max_items{};
		static int last_sel{};
		static bool editing{};
		static Draw::TextEdit editor;
		static string warnings;
		static bitset<8> selPred;
		static const std::unordered_map<string, std::reference_wrapper<const vector<string>>> optionsList = {
			{"color_theme", std::cref(Theme::themes)},
			{"log_level", std::cref(Logger::log_levels)},
			{"temp_scale", std::cref(Config::temp_scales)},
		#ifdef __linux__
			{"freq_mode", std::cref(Config::freq_modes)},
		#endif
			{"proc_sorting", std::cref(Proc::sort_vector)},
			{"graph_symbol", std::cref(Config::valid_graph_symbols)},
			{"graph_symbol_cpu", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_mem", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_net", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_proc", std::cref(Config::valid_graph_symbols_def)},
			{"cpu_graph_upper", std::cref(Cpu::available_fields)},
			{"cpu_graph_lower", std::cref(Cpu::available_fields)},
			{"cpu_sensor", std::cref(Cpu::available_sensors)},
			{"selected_battery", std::cref(Config::available_batteries)},
	        {"base_10_bitrate", std::cref(Config::base_10_bitrate_values)},
		#ifdef GPU_SUPPORT
			{"show_gpu_info", std::cref(Config::show_gpu_values)},
			{"graph_symbol_gpu", std::cref(Config::valid_graph_symbols_def)},
		#endif
		};
		auto tty_mode = Config::getB("tty_mode");
		auto vim_keys = Config::getB("vim_keys");
		if (max_items == 0) {
			for (const auto& cat : categories) {
				if ((int)cat.size() > max_items) max_items = cat.size();
			}
		}
		if (bg.empty()) {
			page = selected = selected_cat = last_sel = 0;
			redraw = true;
			Theme::updateThemes();
		}
		int retval = Changed;
		bool recollect{};
		bool screen_redraw{};
		bool theme_refresh{};

		//? Draw background if needed else process input
		if (redraw) {
			mouse_mappings.clear();
			selPred.reset();
			y = max(1, Term::height/2 - 3 - max_items);
			x = Term::width/2 - 39;
			height = min(Term::height - 7, max_items * 2 + 4);
			if (height % 2 != 0) height--;
			bg 	= Draw::banner_gen(y, 0, true)
				+ Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, fmt::format("{}tab{}", Theme::c("hi_fg"), Theme::c("main_fg")) + Symbols::right)
				+ Mv::to(y+8, x) + Theme::c("hi_fg") + Symbols::div_left + Theme::c("div_line") + Symbols::h_line * 29
				+ Symbols::div_up + Symbols::h_line * (78 - 32) + Theme::c("hi_fg") + Symbols::div_right
				+ Mv::to(y+6+height - 1, x+30) + Symbols::div_down + Theme::c("div_line");
			for (const auto& i : iota(0, height - 4)) {
				bg += Mv::to(y+9 + i, x + 30) + Symbols::v_line;
			}
		}
		else if (not warnings.empty() and not key.empty()) {
			auto ret = messageBox.input(key);
			if (ret == msgBox::msgReturn::Ok_Yes or ret == msgBox::msgReturn::No_Esc) {
				warnings.clear();
				messageBox.clear();
			}
		}
		else if (editing and not key.empty()) {
			if (is_in(key, "escape", "mouse_click")) {
				editor.clear();
				editing = false;
			}
			else if (key == "enter") {
				const auto& option = categories[selected_cat][item_height * page + selected][0];
				if (selPred.test(isString) and Config::stringValid(option, editor.text)) {
					Config::set(option, editor.text);
					if (option == "custom_cpu_name" or option.starts_with("custom_gpu_name"))
						screen_redraw = true;
					else if (is_in(option, "shown_boxes", "presets")) {
						screen_redraw = true;
						Config::current_preset = -1;
					}
					else if (option == "clock_format") {
						Draw::update_clock(true);
						screen_redraw = true;
					}
					else if (option == "preset_names") {
						screen_redraw = true;
					}
					else if (option == "cpu_core_map") {
						atomic_wait(Runner::active);
						Cpu::core_mapping = Cpu::get_core_mapping();
					}
				}
				else if (selPred.test(isInt) and Config::intValid(option, editor.text)) {
					Config::set(option, stoi_safe(editor.text));
				}
				else
					warnings = Config::validError;

				editor.clear();
				editing = false;
			}
			else if (not editor.command(key))
				retval = NoChange;
		}
		else if (key == "mouse_click") {
			const auto [mouse_x, mouse_y] = Input::mouse_pos;
			if (mouse_x < x or mouse_x > x + 80 or mouse_y < y + 6 or mouse_y > y + 6 + height) {
				return Closed;
			}
			else if (mouse_x < x + 30 and mouse_y > y + 8) {
				auto m_select = ceil((double)(mouse_y - y - 8) / 2) - 1;
				if (selected != m_select)
					selected = m_select;
				else if (selPred.test(isEditable))
					goto mouseEnter;
				else retval = NoChange;
			}
		}
		else if (is_in(key, "enter", "e", "E") and selPred.test(isEditable)) {
			mouseEnter:
			const auto& option = categories[selected_cat][item_height * page + selected][0];
			editor = Draw::TextEdit{Config::getAsString(option), selPred.test(isInt)};
			editing = true;
			mouse_mappings.clear();
		}
		else if (is_in(key, "escape", "q", "o", "backspace")) {
			return Closed;
		}
		else if (is_in(key, "down", "mouse_scroll_down") or (vim_keys and key == "j")) {
			if (++selected > select_max or selected >= item_height) {
				if (page < pages - 1) page++;
				else if (pages > 1) page = 0;
				selected = 0;
			}
		}
		else if (is_in(key, "up", "mouse_scroll_up") or (vim_keys and key == "k")) {
			if (--selected < 0) {
				if (page > 0) page--;
				else if (pages > 1) page = pages - 1;

				selected = item_height - 1;
			}
		}
		else if (pages > 1 and key == "page_down") {
			if (++page >= pages) page = 0;
			selected = 0;
			last_sel = -1;
		}
		else if (pages > 1 and key == "page_up") {
			if (--page < 0) page = pages - 1;
			selected = 0;
			last_sel = -1;
		}
		else if (key == "tab") {
			if (++selected_cat >= (int)categories.size()) selected_cat = 0;
			page = selected = 0;
		}
		else if (key == "shift_tab") {
			if (--selected_cat < 0) selected_cat = (int)categories.size() - 1;
			page = selected = 0;
		}
#ifdef GPU_SUPPORT
		else if (is_in(key, "1", "2", "3", "4", "5", "6") or key.starts_with("select_cat_")) {
#else
		else if (is_in(key, "1", "2", "3", "4", "5") or key.starts_with("select_cat_")) {
#endif
		selected_cat = key.back() - '0' - 1;
			page = selected = 0;
		}
		else if (is_in(key, "left", "right") or (vim_keys and is_in(key, "h", "l"))) {
			const auto& option = categories[selected_cat][item_height * page + selected][0];
			if (selPred.test(isInt)) {
				const int mod = (option == "update_ms" ? 100 : 1);
				long value = Config::getI(option);
				if (key == "right" or (vim_keys and key == "l")) value += mod;
				else value -= mod;

				if (Config::intValid(option, to_string(value)))
					Config::set(option, static_cast<int>(value));
				else {
					warnings = Config::validError;
				}
			}
			else if (selPred.test(isBool)) {
				Config::flip(option);
				screen_redraw = true;

				// Special handling for options that need additional action.
				if (option == "truecolor") {
					theme_refresh = true;
					Config::flip("lowcolor");
				}
				else if (option == "force_tty") {
					theme_refresh = true;
					Config::flip("tty_mode");
				}
				else if (is_in(option, "rounded_corners", "theme_background"))
					theme_refresh = true;
				else if (option == "background_update") {
					Runner::pause_output = false;
				}
				else if (option == "base_10_sizes") {
					recollect = true;
				}
				else if (option == "save_config_on_exit" and not Config::getB("save_config_on_exit")) {
					const bool old_write_new = Config::write_new;
					Config::write_new = true;
					Config::write();
					Config::write_new = old_write_new;
				}
				else if (option == "disable_mouse") {
					const auto is_mouse_enabled = !Config::getB("disable_mouse");
					std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
				}
				else if (option == "clock_12h") {
					Draw::update_clock(true);
				}
				else if (option == "show_hostname") {
					Draw::update_hostname(true);
				}
				else if (is_in(option, "show_uptime_header", "show_username_header")) {
					// These trigger a full redraw which is already set above
				}
			}
			else if (selPred.test(isBrowsable)) {
				auto& optList = optionsList.at(option).get();
				int i = v_index(optList, Config::getS(option));

				if ((key == "right" or (vim_keys and key == "l")) and ++i >= (int)optList.size()) i = 0;
				else if ((key == "left" or (vim_keys and key == "h")) and --i < 0) i = optList.size() - 1;
				Config::set(option, optList.at(i));

				if (option == "color_theme")
					theme_refresh = true;
				else if (option == "log_level") {
					Logger::set_log_level(optList.at(i));
					Logger::info("Logger set to {}", optList.at(i));
				}
				else if (option == "base_10_bitrate") {
				    recollect = true;
				}
				else if (is_in(option, "proc_sorting", "cpu_sensor", "show_gpu_info") or option.starts_with("graph_symbol") or option.starts_with("cpu_graph_"))
					screen_redraw = true;
			}
			else
				retval = NoChange;
		}
		else {
			retval = NoChange;
		}

		//? Draw the menu
		if (retval == Changed) {
			Config::unlock();
			auto& out = Global::overlay;
			out = bg;
			item_height = min((int)categories[selected_cat].size(), (int)floor((double)(height - 4) / 2));
			pages = ceil((double)categories[selected_cat].size() / item_height);
			if (page > pages - 1) page = pages - 1;
			select_max = min(item_height - 1, (int)categories[selected_cat].size() - 1 - item_height * page);
			if (selected > select_max) {
				selected = select_max;
			}

			//? Get variable properties for currently selected option
			if (selPred.none() or last_sel != (selected_cat << 8) + selected) {
				selPred.reset();
				last_sel = (selected_cat << 8) + selected;
				const auto& selOption = categories[selected_cat][item_height * page + selected][0];
				if (Config::ints.contains(selOption))
					selPred.set(isInt);
				else if (Config::bools.contains(selOption))
					selPred.set(isBool);
				else
					selPred.set(isString);

				if (not selPred.test(isString))
					selPred.set(is2D);
				else if (optionsList.contains(selOption)) {
					selPred.set(isBrowsable);
				}
				if (not selPred.test(isBrowsable) and (selPred.test(isString) or selPred.test(isInt)))
					selPred.set(isEditable);
			}

			//? Category buttons
			out += Mv::to(y+7, x+4);
		#ifdef GPU_SUPPORT
			for (int i = 0; const auto& m : {"general", "cpu", "gpu", "mem", "net", "proc"}) {
		#else
			for (int i = 0; const auto& m : {"general", "cpu", "mem", "net", "proc"}) {
		#endif
				out += Fx::b + (i == selected_cat
						? Theme::c("hi_fg") + '[' + Theme::c("title") + m + Theme::c("hi_fg") + ']'
						: Theme::c("hi_fg") + to_string(i + 1) + Theme::c("title") + m + ' ')
				#ifdef GPU_SUPPORT
					+ Mv::r(7);
				#else
					+ Mv::r(10);
				#endif

#if !defined(GPU_SUPPORT)
				constexpr static auto option_menu_tab_width = 15;
#else
				constexpr static auto option_menu_tab_width = 12;
#endif
				if (const auto button_name = fmt::format("select_cat_{}", i + 1); not editing and not mouse_mappings.contains(button_name)) {
					mouse_mappings[button_name] = {
						.line = y + 6,
						.col = x + 2 + (option_menu_tab_width * i),
						.height = 3,
						.width = option_menu_tab_width,
					};
				}
				i++;
			}
			if (pages > 1) {
				out += Mv::to(y+6 + height - 1, x+2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			//? Option name and value
			auto cy = y+9;
			for (int c = 0, i = max(0, item_height * page); c++ < item_height and i < (int)categories[selected_cat].size(); i++) {
				const auto& option = categories[selected_cat][i][0];
				const auto& value = (option == "color_theme" ? fs::path(Config::getS("color_theme")).stem().string() : Config::getAsString(option));

				out += Mv::to(cy++, x + 1) + (c-1 == selected ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("title"))
					+ Fx::b + cjust(capitalize(s_replace(option, "_", " "))
						+ (c-1 == selected and selPred.test(isBrowsable)
							? ' ' + to_string(v_index(optionsList.at(option).get(), (option == "color_theme" ? Config::getS("color_theme") : value)) + 1) + '/' + to_string(optionsList.at(option).get().size())
							: ""), 29);
				out	+= Mv::to(cy++, x + 1) + (c-1 == selected ? "" : Theme::c("main_fg")) + Fx::ub + "  "
					+ (c-1 == selected and editing ? cjust(editor(24), 34, true) : cjust(value, 25, true)) + "  ";

				if (c-1 == selected) {
					if (not editing and (selPred.test(is2D) or selPred.test(isBrowsable))) {
						out += Fx::b + Mv::to(cy-1, x+2) + Symbols::left + Mv::to(cy-1, x+28) + Symbols::right;
						mouse_mappings["left"] = {cy-2, x, 2, 5};
						mouse_mappings["right"] = {cy-2, x+25, 2, 5};
					}
					if (selPred.test(isEditable)) {
						out += Fx::b + Mv::to(cy-1, x+28 - (not editing and selPred.test(isInt) ? 2 : 0)) + (tty_mode ? "E" : Symbols::enter);
					}
					//? Description of selected option
					out += Fx::reset + Theme::c("title") + Fx::b;
					for (int cyy = y+7; const auto& desc : categories[selected_cat][i]) {
						if (cyy++ == y+7) continue;
						else if (cyy == y+10) out += Theme::c("main_fg") + Fx::ub;
						else if (cyy > y + height + 4) break;
						out += Mv::to(cyy, x+32) + desc;
					}
				}
			}

			if (not warnings.empty()) {
				messageBox = msgBox{min(78, (int)ulen(warnings) + 10), msgBox::BoxTypes::OK, {uresize(warnings, 74)}, "warning"};
				out += messageBox();
			}

			out += Fx::reset;
		}

		if (theme_refresh) {
			Theme::setTheme();
			Draw::banner_gen(0, 0, false, true);
			screen_redraw = true;
			redraw = true;
			optionsMenu("");
		}
		if (screen_redraw) {
			auto overlay_bkp = std::move(Global::overlay);
			auto clock_bkp = std::move(Global::clock);
			Draw::calcSizes();
			Global::overlay = std::move(overlay_bkp);
			Global::clock = std::move(clock_bkp);
			recollect = true;
		}
		if (recollect) {
			Runner::run("all", false, true);
			retval = NoChange;
		}

		return (redraw ? Changed : retval);
	}

	static int helpMenu(const string& key) {
		static int y{};
		static int x{};
		static int height{};
		static int page{};
		static int pages{};

		if (bg.empty()) page = 0;
		int retval = Changed;

		if (redraw) {
			y = max(1, Term::height/2 - 4 - (int)(help_text.size() / 2));
			x = Term::width/2 - 39;
			height = min(Term::height - 6, (int)help_text.size() + 3);
			pages = ceil((double)help_text.size() / (height - 3));
			page = 0;
			bg = Draw::banner_gen(y, 0, true);
			bg += Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, "help");
		}
		else if (is_in(key, "escape", "q", "h", "backspace", "space", "enter", "mouse_click")) {
			return Closed;
		}
		else if (pages > 1 and is_in(key, "down", "j", "page_down", "tab", "mouse_scroll_down")) {
			if (++page >= pages) page = 0;
		}
		else if (pages > 1 and is_in(key, "up", "k", "page_up", "shift_tab", "mouse_scroll_up")) {
			if (--page < 0) page = pages - 1;
		}
		else {
			retval = NoChange;
		}


		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg;
			if (pages > 1) {
				out += Mv::to(y+height+6, x + 2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			auto cy = y+7;
			out += Mv::to(cy++, x + 1) + Theme::c("title") + Fx::b + cjust("Key:", 20) + "Description:";
			for (int c = 0, i = max(0, (height - 3) * page); c++ < height - 3 and i < (int)help_text.size(); i++) {
				out += Mv::to(cy++, x + 1) + Theme::c("hi_fg") + Fx::b + cjust(help_text[i][0], 20)
					+ Theme::c("main_fg") + Fx::ub + help_text[i][1];
			}
			out += Fx::reset;
		}


		return (redraw ? Changed : retval);
	}

	static int reniceMenu(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		static int x{};
		static int y{};
		static int selected_nice = 0;
		static string nice_edit;

		if (bg.empty()) {
			selected_nice = 0;
			nice_edit.clear();
		}
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 25;
			y = Term::height/2 - 6;
			bg = Draw::createBox(x + 2, y, 50, 13, Theme::c("hi_fg"), true, "renice");
			bg += Mv::to(y+2, x+3) + Theme::c("title") + Fx::b + cjust("Renice PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name")), 15) + ")", 48);
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (is_in(key, "enter", "space")) {
			if (s_pid > 0) {
				if (not nice_edit.empty()) {
					selected_nice = stoi_safe(nice_edit, 0);
				}
				if (not Proc::set_priority(s_pid, selected_nice)) {
					// TODO: show error message
				}
			}
			return Closed;
		}
		else if (key.size() == 1 and (isdigit(key.at(0)) or (key.at(0) == '-' and nice_edit.empty()))) {
			nice_edit += key;
		}
		else if (key == "backspace" and not nice_edit.empty()) {
			nice_edit.pop_back();
		}
		else if (is_in(key, "up", "k")) {
			if (++selected_nice > 19) selected_nice = -20;
			nice_edit.clear();
		}
		else if (is_in(key, "down", "j")) {
			if (--selected_nice < -20) selected_nice = 19;
			nice_edit.clear();
		}
		else if (is_in(key, "left", "h")) {
			if ((selected_nice -= 5) < -20) selected_nice += 40;
			nice_edit.clear();
		}
		else if (is_in(key, "right", "l")) {
			if ((selected_nice += 5) > 19) selected_nice -= 40;
			nice_edit.clear();
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y+4;
			if (not nice_edit.empty()) {
				selected_nice = stoi_safe(nice_edit, 0);
			}
			out = bg + Mv::to(cy++, x+3) + Theme::c("main_fg") + Fx::ub
				+ rjust("Enter nice value: ", 30) + Theme::c("hi_fg") + (nice_edit.empty() ? to_string(selected_nice) : nice_edit) + Theme::c("main_fg") + Fx::bl + "█" + Fx::ubl;

			cy++;
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "↑ ↓", 20, true) + Theme::c("main_fg") + Fx::ub + " | To change value.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "← →", 20, true) + Theme::c("main_fg") + Fx::ub + " | To change value by 5.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("0-9", 20) + Theme::c("main_fg") + Fx::ub + " | Enter manually.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ENTER", 20) + Theme::c("main_fg") + Fx::ub + " | To set nice value.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ESC or 'q'", 20) + Theme::c("main_fg") + Fx::ub + " | To abort.";

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

#if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
	static int vramAllocMenu(const string& key) {
		static int x{};
		static int y{};
		static int box_width{};
		static int selected_option = 0;
		static string custom_value;
		static vector<std::pair<string, long long>> options;  // {label, value_mb}

		if (bg.empty()) {
			selected_option = 0;
			custom_value.clear();

			//? Calculate options based on total RAM
			long long total_ram = Gpu::get_total_ram();
			long long total_ram_mb = total_ram / (1024LL * 1024);

			options.clear();
			options.push_back({"Default (system managed)", 0});

			//? Generate options at 66%, 75%, 83%, 92% of RAM
			vector<int> percentages = {66, 75, 83, 92};
			for (int pct : percentages) {
				long long value_mb = (total_ram_mb * pct) / 100;
				long long value_gb = value_mb / 1024;
				string label = fmt::format("{} GiB ({}%)", value_gb, pct);
				options.push_back({label, value_mb});
			}
			options.push_back({"Custom...", -1});
		}

		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			box_width = 40;
			int box_height = static_cast<int>(options.size()) + 10;  // +2 for Save/Cancel buttons
			x = Term::width/2 - box_width/2;
			y = Term::height/2 - box_height/2;
			bg = Draw::createBox(x, y, box_width, box_height, Theme::c("hi_fg"), true, "GPU Memory Allocation");

			//? Show current limit
			int64_t current_limit_mb = 0;
			size_t size = sizeof(current_limit_mb);
			sysctlbyname("iogpu.wired_limit_mb", &current_limit_mb, &size, nullptr, 0);
			string current_str = (current_limit_mb > 0)
				? fmt::format("{:.1f} GiB", static_cast<double>(current_limit_mb) / 1024.0)
				: "System default";
			bg += Mv::to(y+2, x+2) + Theme::c("title") + Fx::b + cjust("Current: " + current_str, box_width - 4) + Fx::ub;

			//? Clear mouse mappings for this menu
			mouse_mappings.clear();
		}
		else if (is_in(key, "escape", "q", "vram_cancel")) {
			return Closed;
		}
		else if (is_in(key, "enter", "space", "vram_save")) {
			if (selected_option >= 0 and selected_option < static_cast<int>(options.size())) {
				long long value_mb = options[selected_option].second;

				if (value_mb == -1) {
					//? Custom option - parse input
					if (not custom_value.empty()) {
						value_mb = stoi_safe(custom_value, 0) * 1024;  // Convert GB to MB
					} else {
						return (redraw ? Changed : NoChange);  // Stay in menu if no value entered
					}
				}

				//? Execute the allocation change
				Logger::debug("Calling set_gpu_memory_limit({})", value_mb);
				bool result = Gpu::set_gpu_memory_limit(value_mb);
				Logger::debug("set_gpu_memory_limit returned: {}", result);
				//? Note: Don't force GPU refresh - let it happen naturally to avoid race conditions
			}
			Logger::debug("Returning Closed from vramAllocMenu");
			return Closed;
		}
		//? Handle mouse clicks on option lines
		else if (key.starts_with("vram_opt")) {
			int opt_num = stoi_safe(key.substr(8), -1);
			if (opt_num >= 0 and opt_num < static_cast<int>(options.size())) {
				if (selected_option == opt_num) {
					//? Double-click behavior: if already selected, apply it
					if (options[opt_num].second != -1 or not custom_value.empty()) {
						//? Trigger save action for non-custom or custom with value
						long long value_mb = options[selected_option].second;
						if (value_mb == -1 and not custom_value.empty()) {
							value_mb = stoi_safe(custom_value, 0) * 1024;
						}
						if (value_mb >= 0) {
							Logger::debug("Calling set_gpu_memory_limit({})", value_mb);
							Gpu::set_gpu_memory_limit(value_mb);
							return Closed;
						}
					}
				} else {
					selected_option = opt_num;
					custom_value.clear();
				}
			}
		}
		else if (key.size() == 1 and isdigit(key.at(0))) {
			//? Allow number input for custom value
			if (options[selected_option].second == -1) {
				if (custom_value.size() < 3) custom_value += key;  // Max 3 digits (999 GB)
			}
		}
		else if (key == "backspace") {
			if (not custom_value.empty()) custom_value.pop_back();
		}
		else if (is_in(key, "up", "k")) {
			if (--selected_option < 0) selected_option = static_cast<int>(options.size()) - 1;
			custom_value.clear();
		}
		else if (is_in(key, "down", "j")) {
			if (++selected_option >= static_cast<int>(options.size())) selected_option = 0;
			custom_value.clear();
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y + 4;
			out = bg;

			//? Draw options with mouse mappings
			for (size_t i = 0; i < options.size(); ++i) {
				bool is_selected = (static_cast<int>(i) == selected_option);
				string prefix = is_selected ? Theme::c("hi_fg") + Fx::b + "> " : "  ";
				string suffix = is_selected ? Fx::ub + Theme::c("main_fg") : "";

				if (options[i].second == -1 and is_selected) {
					//? Custom option with input field
					string input_display = custom_value.empty() ? "_" : custom_value + " GiB" + Fx::bl + "█" + Fx::ubl;
					out += Mv::to(cy, x+2) + prefix + "Custom: " + input_display + suffix;
				} else {
					out += Mv::to(cy, x+2) + prefix + options[i].first + suffix;
				}

				//? Add mouse mapping for this option line
				mouse_mappings["vram_opt" + to_string(i)] = Input::Mouse_loc{cy, x+2, 1, box_width - 4};
				cy++;
			}

			//? Draw separator line
			cy++;

			//? Draw Save and Cancel buttons
			int btn_y = ++cy;
			int save_x = x + 6;
			int cancel_x = x + box_width - 16;

			//? Save button
			out += Mv::to(btn_y, save_x) + Theme::c("hi_fg") + "[" + Theme::c("title") + Fx::b + " Save " + Fx::ub + Theme::c("hi_fg") + "]";
			mouse_mappings["vram_save"] = Input::Mouse_loc{btn_y, save_x, 1, 8};

			//? Cancel button
			out += Mv::to(btn_y, cancel_x) + Theme::c("hi_fg") + "[" + Theme::c("main_fg") + Fx::b + " Cancel " + Fx::ub + Theme::c("hi_fg") + "]";
			mouse_mappings["vram_cancel"] = Input::Mouse_loc{btn_y, cancel_x, 1, 10};

			//? Help text
			out += Mv::to(++cy, x+2) + Theme::c("inactive_fg") + "↑↓/Click Navigate  Enter/Save Apply  Esc Cancel";

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}
#else
	//? Stub for non-Apple platforms
	static int vramAllocMenu(const string&) {
		return Closed;
	}
#endif

	//? Process column toggle menu - allows user to show/hide columns
	static int procColumnToggleMenu(const string& key) {
		static int x{};
		static int y{};
		static int box_width{};
		static int selected_option = 0;
		static int scroll_offset = 0;

		//? Column definitions: {config_key, display_name, bottom_only, side_only}
		struct ColumnDef {
			string config_key;
			string display_name;
			bool bottom_only;
			bool side_only;
		};

		static const vector<ColumnDef> columns = {
			{"proc_show_cmd",      "Command",       false, false},
			{"proc_show_threads",  "Threads",       false, false},
			{"proc_show_user",     "User",          false, false},
			{"proc_show_memory",   "Memory",        false, false},
			{"proc_show_cpu",      "CPU%",          false, false},
			{"proc_show_io",       "I/O (combined)", false, true},   //? Side layout only - combined IO
			{"proc_show_io_read",  "IO/R",          true,  false},  //? Bottom layout only
			{"proc_show_io_write", "IO/W",          true,  false},  //? Bottom layout only
			{"proc_show_state",    "State",         true,  false},
			{"proc_show_priority", "Priority",      true,  false},
			{"proc_show_nice",     "Nice",          true,  false},
			{"proc_show_ports",    "Ports",         true,  false},
			{"proc_show_virt",     "Virtual Mem",   true,  false},
			{"proc_show_runtime",  "Runtime",       true,  false},
			{"proc_show_cputime",  "CPU Time",      true,  false},
			{"proc_show_gputime",  "GPU Time",      true,  false},
		};

		auto& out = Global::overlay;
		int retval = Changed;

		bool bottom_layout = Config::getB("proc_full_width");
		int visible_rows = 10;  //? Max visible rows in the menu

		if (redraw) {
			selected_option = 0;
			scroll_offset = 0;
			box_width = 36;
			int box_height = visible_rows + 6;  //? header + footer + padding
			x = Term::width/2 - box_width/2;
			y = Term::height/2 - box_height/2;
			bg = Draw::createBox(x, y, box_width, box_height, Theme::c("hi_fg"), true, "Toggle Columns");

			//? Show layout indicator
			string layout_str = bottom_layout ? "Bottom Layout (Full)" : "Side Layout (Compact)";
			bg += Mv::to(y+2, x+2) + Theme::c("title") + Fx::b + cjust(layout_str, box_width - 4) + Fx::ub;

			mouse_mappings.clear();
		}
		else if (is_in(key, "escape", "q", "T", "col_close")) {
			return Closed;
		}
		else if (is_in(key, "up", "k")) {
			if (--selected_option < 0) selected_option = static_cast<int>(columns.size()) - 1;
			//? Adjust scroll to keep selection visible
			if (selected_option < scroll_offset) scroll_offset = selected_option;
			if (selected_option >= scroll_offset + visible_rows) scroll_offset = selected_option - visible_rows + 1;
		}
		else if (is_in(key, "down", "j")) {
			if (++selected_option >= static_cast<int>(columns.size())) selected_option = 0;
			//? Adjust scroll
			if (selected_option < scroll_offset) scroll_offset = selected_option;
			if (selected_option >= scroll_offset + visible_rows) scroll_offset = selected_option - visible_rows + 1;
		}
		else if (is_in(key, "enter", "space")) {
			//? Toggle the selected column
			const auto& col = columns[selected_option];
			Config::flip(col.config_key);
			Proc::redraw = true;
		}
		else if (key.starts_with("col_opt")) {
			int opt_num = stoi_safe(key.substr(7), -1);
			if (opt_num >= 0 and opt_num < static_cast<int>(columns.size())) {
				if (selected_option == opt_num) {
					//? Double-click toggles
					const auto& col = columns[opt_num];
					Config::flip(col.config_key);
					Proc::redraw = true;
				} else {
					selected_option = opt_num;
				}
			}
		}
		else if (key == "mouse_scroll_up") {
			if (scroll_offset > 0) scroll_offset--;
		}
		else if (key == "mouse_scroll_down") {
			if (scroll_offset < static_cast<int>(columns.size()) - visible_rows) scroll_offset++;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y + 4;
			out = bg;

			//? Draw column options
			int end_idx = min(static_cast<int>(columns.size()), scroll_offset + visible_rows);
			for (int i = scroll_offset; i < end_idx; ++i) {
				const auto& col = columns[i];
				bool is_selected = (i == selected_option);
				bool is_enabled = Config::getB(col.config_key);
				//? Column is applicable if: (not bottom_only OR in bottom_layout) AND (not side_only OR in side_layout)
				bool is_applicable = (not col.bottom_only or bottom_layout) and (not col.side_only or not bottom_layout);

				string prefix = is_selected ? Theme::c("hi_fg") + Fx::b + "> " : "  ";
				string suffix = is_selected ? Fx::ub : "";

				//? Checkbox indicator - use dot (•) to match Settings menu style
				string checkbox;
				if (not is_applicable) {
					checkbox = Theme::c("inactive_fg") + "[-]";  //? Not applicable in this layout
				} else if (is_enabled) {
					checkbox = Theme::c("hi_fg") + "[•]";
				} else {
					checkbox = Theme::c("main_fg") + "[ ]";
				}

				string name_color = is_applicable ? Theme::c("main_fg") : Theme::c("inactive_fg");
				string layout_indicator = col.bottom_only ? Theme::c("inactive_fg") + " (B)" :
				                          col.side_only ? Theme::c("inactive_fg") + " (S)" : "";

				out += Mv::to(cy, x+2) + prefix + checkbox + " " + name_color + col.display_name + layout_indicator + suffix;

				//? Mouse mapping
				mouse_mappings["col_opt" + to_string(i)] = Input::Mouse_loc{cy, x+2, 1, box_width - 4};
				cy++;
			}

			//? Scroll indicators
			if (scroll_offset > 0) {
				out += Mv::to(y+3, x + box_width - 4) + Theme::c("hi_fg") + "▲";
			}
			if (scroll_offset + visible_rows < static_cast<int>(columns.size())) {
				out += Mv::to(y + 4 + visible_rows, x + box_width - 4) + Theme::c("hi_fg") + "▼";
			}

			//? Footer
			int btn_y = y + visible_rows + 5;
			int close_x = x + box_width/2 - 4;
			out += Mv::to(btn_y, close_x) + Theme::c("hi_fg") + "[" + Theme::c("title") + Fx::b + " Close " + Fx::ub + Theme::c("hi_fg") + "]";
			mouse_mappings["col_close"] = Input::Mouse_loc{btn_y, close_x, 1, 9};

			//? Help text
			out += Mv::to(++btn_y, x+2) + Theme::c("inactive_fg") + "↑↓ Navigate  Space Toggle  Esc Close";
			out += Mv::to(++btn_y, x+2) + Theme::c("inactive_fg") + "(B)=Bottom only  (S)=Side only";

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

	//* Wrapper to call the new MenuV2 options menu
	//? Set to 1 to use the new MenuV2 options menu instead of the old one
	#define USE_MENUV2_OPTIONS 1

	#ifdef USE_MENUV2_OPTIONS
	//? Forward declaration for MenuV2::optionsMenuV2 (defined later in this file)
	int optionsMenuV2_wrapper(const string& key);
	#endif

	static int optionsMenuWrapper(const string& key) {
		#ifdef USE_MENUV2_OPTIONS
		return optionsMenuV2_wrapper(key);
		#else
		return optionsMenu(key);
		#endif
	}

	//* Add menus here and update enum Menus in header
	const auto menuFunc = vector{
		ref(sizeError),
		ref(signalChoose),
		ref(signalSend),
		ref(signalReturn),
		ref(optionsMenuWrapper),
		ref(helpMenu),
		ref(reniceMenu),
		ref(vramAllocMenu),
		ref(procColumnToggleMenu),
		ref(mainMenu),
	};
	bitset<16> menuMask;

	void process(const std::string_view key) {
		if (menuMask.none()) {
			Menu::active = false;
			Global::overlay.clear();
			Global::overlay.shrink_to_fit();
			Runner::pause_output = false;
			bg.clear();
			bg.shrink_to_fit();
			currentMenu = -1;
			// Always clear screen and trigger full refresh when menu closes
			// This prevents artifacts from modal windows overlaying panels
			std::cout << Term::clear << std::flush;  // Clear screen completely first
			if (MenuV2::theme_changed_pending) {
				Theme::setTheme();         // Load the new theme colors
			}
			if (MenuV2::theme_changed_pending or MenuV2::boxes_changed_pending) {
				Draw::calcSizes();         // Recalculate all box sizes and clear caches
			}
			Global::resized = true;        // Trigger full refresh in main loop
			MenuV2::theme_changed_pending = false;
			MenuV2::boxes_changed_pending = false;
			mouse_mappings.clear();
			return;
		}

		if (currentMenu < 0 or not menuMask.test(currentMenu)) {
			Menu::active = true;
			redraw = true;
			if (((menuMask.test(Main) or menuMask.test(Options) or menuMask.test(Help) or menuMask.test(SignalChoose))
			and (Term::width < 80 or Term::height < 24))
			or (Term::width < 50 or Term::height < 20)) {
				menuMask.reset();
				menuMask.set(SizeError);
			}

			for (const auto& i : iota(0, (int)menuMask.size())) {
				if (menuMask.test(i)) currentMenu = i;
			}

		}

		auto retCode = menuFunc.at(currentMenu)(key.data());
		if (retCode == Closed) {
			menuMask.reset(currentMenu);
			mouse_mappings.clear();
			bg.clear();
			Runner::pause_output = false;
			process();
		}
		else if (redraw) {
			redraw = false;
			Runner::run("all", true, true);
		}
		else if (retCode == Changed)
			Runner::run("overlay");
		else if (retCode == Switch) {
			Runner::pause_output = false;
			bg.clear();
			redraw = true;
			mouse_mappings.clear();
			process();
		}
	}

	void show(int menu, int signal) {
		menuMask.set(menu);
		signalToSend = signal;
		process();
	}
}

//? ====================== MenuV2 Implementation ======================
namespace MenuV2 {
	using namespace Tools;
	using namespace std;

	//? Flag to track theme change during menu session - triggers full refresh on exit
	bool theme_changed_pending = false;

	//? Flag to track box layout change during menu session - triggers full refresh on exit
	bool boxes_changed_pending = false;

	//? ==================== Clock Format Display Helper ====================
	//? Translates strftime codes to human-readable examples
	string clockFormatToReadable(const string& fmt) {
		static const std::unordered_map<string, string> clock_display = {
			{"%X", "14:30:45 (locale)"},
			{"%H:%M", "14:30"},
			{"%H:%M:%S", "14:30:45"},
			{"%I:%M %p", "2:30 PM"},
			{"%I:%M:%S %p", "2:30:45 PM"},
			{"", "(disabled)"}
		};
		auto it = clock_display.find(fmt);
		return (it != clock_display.end()) ? it->second : fmt;
	}

	//? ==================== Box Toggle Helpers ====================
	//? Maps pseudo-config keys to box names in shown_boxes
	static const std::unordered_map<string, string> box_key_to_name = {
		{"box_show_cpu", "cpu"},
		{"box_show_gpu", "gpu0"},
		{"box_show_pwr", "pwr"},
		{"box_show_mem", "mem"},
		{"box_show_net", "net"},
		{"box_show_proc", "proc"}
	};

	//? Check if a box is enabled in shown_boxes
	bool isBoxEnabled(const string& key) {
		auto it = box_key_to_name.find(key);
		if (it == box_key_to_name.end()) return false;
		const string& box_name = it->second;
		const string& shown = Config::getS("shown_boxes");
		return shown.find(box_name) != string::npos;
	}

	//? Toggle a box in shown_boxes
	void toggleBox(const string& key) {
		auto it = box_key_to_name.find(key);
		if (it == box_key_to_name.end()) return;
		const string& box_name = it->second;
		string shown = Config::getS("shown_boxes");

		size_t pos = shown.find(box_name);
		if (pos != string::npos) {
			// Remove the box - handle leading/trailing spaces
			size_t end = pos + box_name.length();
			// Remove trailing space if exists
			if (end < shown.length() && shown[end] == ' ') end++;
			// Or remove leading space if at end
			else if (pos > 0 && shown[pos-1] == ' ') pos--;
			shown.erase(pos, end - pos);
		} else {
			// Add the box at appropriate position (maintain order: cpu pwr gpu0 mem net proc)
			const vector<string> order = {"cpu", "pwr", "gpu0", "mem", "net", "proc"};
			vector<string> current_boxes;
			for (const auto& b : ssplit(shown)) {
				current_boxes.push_back(b);
			}
			current_boxes.push_back(box_name);
			// Sort by order
			std::sort(current_boxes.begin(), current_boxes.end(), [&order](const string& a, const string& b) {
				auto pos_a = std::find(order.begin(), order.end(), a);
				auto pos_b = std::find(order.begin(), order.end(), b);
				return pos_a < pos_b;
			});
			shown.clear();
			for (size_t i = 0; i < current_boxes.size(); i++) {
				if (i > 0) shown += " ";
				shown += current_boxes[i];
			}
		}
		Config::set("shown_boxes", shown);
		boxes_changed_pending = true;
		Config::current_preset = -1;
	}

	//? Check if key is a box toggle pseudo-config
	bool isBoxToggleKey(const string& key) {
		return box_key_to_name.find(key) != box_key_to_name.end();
	}

	//? ==================== UI Component Renderers ====================

	string drawToggle(bool value, bool selected, bool tty_mode, bool disabled) {
		const string bracket_color = disabled ? Theme::c("inactive_fg") : Theme::c("main_fg");
		const string on_char = tty_mode ? "[x]" : bracket_color + "[" + Theme::c("proc_misc") + "●" + bracket_color + "]";
		const string off_char = tty_mode ? "[ ]" : bracket_color + "[" + Theme::c("inactive_fg") + " " + bracket_color + "]";

		string result;
		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}
		result += (value ? on_char : off_char);
		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	//? Draw radio buttons with optional focus indicator
	//? current_idx: currently selected value, focus_idx: which option has keyboard focus (-1 = no focus)
	string drawRadio(const vector<string>& options, int current_idx, int focus_idx, bool tty_mode) {
		string result;
		const string sel_char = tty_mode ? "(*)" : Theme::c("hi_fg") + "●" + Fx::reset;
		const string unsel_char = tty_mode ? "( )" : Theme::c("inactive_fg") + "○" + Fx::reset;

		for (size_t i = 0; i < options.size(); i++) {
			if (i > 0) result += " ";
			bool is_current = (static_cast<int>(i) == current_idx);
			bool is_focused = (static_cast<int>(i) == focus_idx);

			// Show focus with highlight background
			if (is_focused) {
				result += Theme::c("selected_bg") + Theme::c("selected_fg");
			}
			result += (is_current ? sel_char : unsel_char) + " " + options[i];
			if (is_focused) {
				result += Fx::reset;
			}
		}
		return result;
	}

	//? Draw radio buttons with disabled (red) options
	//? disabled_mask: bitmask of which options to show in red (e.g., 0b110 = options 1 and 2 disabled)
	string drawRadioWithDisabled(const vector<string>& options, int current_idx, int focus_idx, int disabled_mask, bool tty_mode) {
		string result;
		const string sel_char = tty_mode ? "(*)" : Theme::c("hi_fg") + "●";
		const string unsel_char = tty_mode ? "( )" : Theme::c("inactive_fg") + "○";
		const string red_color = "\x1b[38;5;203m";  // Nord red (color 203 ≈ #ff5f5f)

		for (size_t i = 0; i < options.size(); i++) {
			if (i > 0) result += " ";
			bool is_current = (static_cast<int>(i) == current_idx);
			bool is_focused = (static_cast<int>(i) == focus_idx);
			bool is_disabled = (disabled_mask & (1 << i)) != 0;

			// Show focus with highlight background
			if (is_focused and not is_disabled) {
				result += Theme::c("selected_bg") + Theme::c("selected_fg");
			}

			if (is_disabled) {
				// Show disabled options in red (entire option including text)
				result += red_color + (is_current ? "●" : "○") + " " + options[i] + Fx::reset;
			} else {
				// Enabled: use main_fg for the text to ensure proper color
				result += (is_current ? sel_char : unsel_char) + Theme::c("main_fg") + " " + options[i];
			}

			if (is_focused and not is_disabled) {
				result += Fx::reset;
			}
		}
		return result;
	}

	//? Overload for backward compatibility (no focus tracking)
	string drawRadio(const vector<string>& options, int current_idx, bool selected, bool tty_mode) {
		return drawRadio(options, current_idx, selected ? current_idx : -1, tty_mode);
	}

	//? Draw multi-select checkboxes with disabled options
	//? selected_mask: bitmask of which options are checked (e.g., 0b101 = options 0 and 2 checked)
	//? disabled_mask: bitmask of which options are disabled (shown in red)
	string drawMultiCheck(const vector<string>& options, int selected_mask, int focus_idx, int disabled_mask, bool tty_mode) {
		string result;
		const string checked_char = tty_mode ? "(o)" : "●";
		const string unchecked_char = tty_mode ? "( )" : "○";
		const string red_color = "\x1b[38;5;203m";  // Nord red

		for (size_t i = 0; i < options.size(); i++) {
			if (i > 0) result += "  ";  // Extra spacing between checkboxes
			bool is_checked = (selected_mask & (1 << i)) != 0;
			bool is_focused = (static_cast<int>(i) == focus_idx);
			bool is_disabled = (disabled_mask & (1 << i)) != 0;

			// Show focus with highlight background
			if (is_focused and not is_disabled) {
				result += Theme::c("selected_bg") + Theme::c("selected_fg");
			}

			if (is_disabled) {
				// Disabled options shown in red
				result += red_color + (is_checked ? checked_char : unchecked_char) + " " + options[i] + Fx::reset;
			} else {
				// Enabled: checkbox color based on checked state
				string checkbox_color = is_checked ? Theme::c("hi_fg") : Theme::c("inactive_fg");
				result += checkbox_color + (is_checked ? checked_char : unchecked_char) + Theme::c("main_fg") + " " + options[i];
			}

			if (is_focused and not is_disabled) {
				result += Fx::reset;
			}
		}
		return result;
	}

	//? Draw multiple toggles on a single row: "[●] Used  [ ] Available  [●] Cached  [●] Free"
	//? options: list of labels for each toggle
	//? values: list of boolean values (true = on, false = off) for each toggle
	//? focus_idx: which toggle has keyboard focus (-1 = none)
	string drawToggleRow(const vector<string>& options, const vector<bool>& values, int focus_idx, bool tty_mode) {
		string result;
		for (size_t i = 0; i < options.size(); i++) {
			if (i > 0) result += "  ";  // Spacing between toggles

			bool is_checked = (i < values.size()) ? values[i] : false;
			bool is_focused = (static_cast<int>(i) == focus_idx);

			// Toggle bracket styling
			const string bracket_color = Theme::c("main_fg");
			string on_char = tty_mode ? "[x]" : bracket_color + "[" + Theme::c("proc_misc") + "●" + bracket_color + "]";
			string off_char = tty_mode ? "[ ]" : bracket_color + "[" + Theme::c("inactive_fg") + " " + bracket_color + "]";

			// Show focus with highlight background
			if (is_focused) {
				result += Theme::c("selected_bg") + Theme::c("selected_fg");
			}

			result += (is_checked ? on_char : off_char) + " " + Theme::c("main_fg") + options[i];

			if (is_focused) {
				result += Fx::reset;
			}
		}
		return result;
	}

	string drawSlider(int min_val, int max_val, int current, int width, bool selected, bool tty_mode) {
		if (width < 10) width = 10;

		// Track width is width minus brackets and arrows: [ ◀ track ▶ ]
		int track_width = width - 4;
		float range = static_cast<float>(max_val - min_val);
		float ratio = (range > 0) ? static_cast<float>(current - min_val) / range : 0.0f;
		int pos = static_cast<int>(ratio * track_width);
		if (pos < 0) pos = 0;
		if (pos > track_width - 1) pos = track_width - 1;

		string bar;
		const string track_char = tty_mode ? "-" : "─";
		const string handle_char = tty_mode ? "O" : "●";
		const string left_arrow = tty_mode ? "<" : "◀";
		const string right_arrow = tty_mode ? ">" : "▶";

		bar += "[";
		// Left arrow inside bracket (clickable)
		bar += Theme::c("hi_fg") + left_arrow + Theme::c("main_fg");

		for (int i = 0; i < track_width; i++) {
			if (i == pos) {
				bar += Theme::c("proc_misc") + handle_char + Theme::c("main_fg");
			} else if (i < pos) {
				bar += Theme::c("proc_misc") + track_char + Theme::c("main_fg");
			} else {
				bar += Theme::c("inactive_fg") + track_char + Theme::c("main_fg");
			}
		}

		// Right arrow inside bracket (clickable)
		bar += Theme::c("hi_fg") + right_arrow + Theme::c("main_fg");
		bar += "]";

		string result;
		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}
		result += bar + " " + to_string(current);
		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	string drawSelect(const string& value, int width, bool selected, bool open) {
		string result;
		string display_val = value;
		if (static_cast<int>(display_val.size()) > width - 4) {
			display_val = display_val.substr(0, width - 7) + "...";
		}

		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}

		result += "[ " + ljust(display_val, width - 4) + (open ? "▲" : "▼") + "]";

		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	//? ==================== Preset Preview Renderer ====================
	//? Clean implementation for 13 distinct layout combinations
	//?
	//? Single panel (3 layouts):
	//?   1. MEM alone → fills all
	//?   2. NET alone → fills all
	//?   3. PROC alone → fills all
	//?
	//? Two panels (6 layouts):
	//?   4. MEM + NET Left    → MEM top, NET under (min height)
	//?   5. MEM + NET Right   → MEM left, NET right (same height)
	//?   6. MEM + PROC Right  → MEM left, PROC right
	//?   7. MEM + PROC Wide   → MEM top, PROC fills bottom
	//?   12. NET + PROC Right → NET left, PROC right
	//?   13. NET + PROC Wide  → NET top (min), PROC fills bottom
	//?
	//? Three panels (4 layouts):
	//?   8.  MEM + NET Left + PROC Right  → MEM top-left, NET under MEM, PROC right
	//?   9.  MEM + NET Left + PROC Wide   → MEM top, NET under, PROC wide bottom
	//?   10. MEM + NET Right + PROC Right → MEM left, NET+PROC stacked right
	//?   11. MEM + NET Right + PROC Wide  → MEM+NET side by side, PROC wide bottom

	string drawPresetPreview(const PresetDef& preset, int width, int height) {
		if (width < 20 or height < 8) return "Preview too small";

		vector<string> lines(height, string(width, ' '));
		bool use_unicode = Config::getB("preview_unicode");

		// Draw a box with label centered
		auto drawBox = [&](int bx, int by, int bw, int bh, const string& label) {
			if (by < 0 or by >= height or bx < 0 or bx >= width or bw <= 0 or bh <= 0) return;

			// Top border
			for (int i = bx; i < bx + bw and i < width; i++) {
				if (by < height) lines[by][i] = (i == bx) ? '+' : (i == bx + bw - 1) ? '+' : '-';
			}
			// Bottom border
			int bottom_y = by + bh - 1;
			if (bottom_y < height and bottom_y >= 0) {
				for (int i = bx; i < bx + bw and i < width; i++) {
					lines[bottom_y][i] = (i == bx) ? '+' : (i == bx + bw - 1) ? '+' : '-';
				}
			}
			// Sides
			for (int row = by + 1; row < by + bh - 1 and row < height; row++) {
				if (row >= 0) {
					if (bx < width) lines[row][bx] = '|';
					if (bx + bw - 1 < width) lines[row][bx + bw - 1] = '|';
				}
			}
			// Center label
			if (bh >= 3 and not label.empty()) {
				int label_y = by + bh / 2;
				if (label_y < height and label_y >= 0) {
					int label_start = bx + (bw - static_cast<int>(label.size())) / 2;
					for (size_t i = 0; i < label.size(); i++) {
						int lx = label_start + static_cast<int>(i);
						if (lx >= 0 and lx < width) lines[label_y][lx] = label[i];
					}
				}
			}
		};

		int y_pos = 0;

		// Panel state
		bool has_mem = preset.mem_enabled;
		bool has_net = preset.net_enabled;
		bool has_proc = preset.proc_enabled;

		// Count top and bottom panels
		int top_panel_count = 0;
		int bottom_panel_count = 0;
		if (preset.cpu_enabled) { if (preset.cpu_bottom) bottom_panel_count++; else top_panel_count++; }
		if (preset.gpu_enabled) { if (preset.gpu_bottom) bottom_panel_count++; else top_panel_count++; }
		if (preset.pwr_enabled) { if (preset.pwr_bottom) bottom_panel_count++; else top_panel_count++; }

		// Calculate panel heights - if no lower panels, top/bottom panels fill the space
		bool no_lower_panels = not has_mem and not has_net and not has_proc;
		int total_tb_panels = top_panel_count + bottom_panel_count;

		int top_panel_height = 3;  // Default fixed height
		int bottom_panel_height = 3;
		int bottom_reserved = bottom_panel_count * 3;

		if (no_lower_panels and total_tb_panels > 0) {
			// No lower panels - top/bottom panels fill entire space proportionally
			int panel_h = height / total_tb_panels;
			if (panel_h < 3) panel_h = 3;
			top_panel_height = panel_h;
			bottom_panel_height = panel_h;
			bottom_reserved = bottom_panel_count * panel_h;
		}

		// Draw top panels (CPU, GPU, PWR if at top position)
		if (preset.cpu_enabled and not preset.cpu_bottom) {
			drawBox(0, y_pos, width, top_panel_height, "CPU");
			y_pos += top_panel_height;
		}
		if (preset.gpu_enabled and not preset.gpu_bottom) {
			drawBox(0, y_pos, width, top_panel_height, "GPU");
			y_pos += top_panel_height;
		}
		if (preset.pwr_enabled and not preset.pwr_bottom) {
			drawBox(0, y_pos, width, top_panel_height, "PWR");
			y_pos += top_panel_height;
		}

		int remaining = height - y_pos - bottom_reserved;
		if (remaining < 3) remaining = 3;

		// Build labels with type indicator
		string mem_label = "MEM";
		if (has_mem) {
			mem_label = (preset.mem_type == MemType::Horizontal) ? "MEM-H" : "MEM-V";
			if (preset.mem_type == MemType::Vertical and preset.show_disk) {
				mem_label = "MEM+D";
			}
		}
		string net_label = "NET";
		string proc_label = "PROC";

		// Sizing constants
		const int min_h = 3;  // Minimum panel height
		int half_w = width / 2;

		// Count active lower panels
		int panel_count = (has_mem ? 1 : 0) + (has_net ? 1 : 0) + (has_proc ? 1 : 0);

		//? Check if ANY panel is enabled (including CPU/GPU/PWR)
		bool any_panel_enabled = preset.cpu_enabled or preset.gpu_enabled or preset.pwr_enabled or
		                         has_mem or has_net or has_proc;
		if (not any_panel_enabled) {
			return "No panels selected";
		}

		//? ==================== SINGLE PANEL LAYOUTS ====================
		if (panel_count == 1) {
			// Layout 1, 2, 3: Single panel fills everything
			if (has_mem) drawBox(0, y_pos, width, remaining, mem_label);
			else if (has_net) drawBox(0, y_pos, width, remaining, net_label);
			else if (has_proc) drawBox(0, y_pos, width, remaining, proc_label);
		}

		//? ==================== TWO PANEL LAYOUTS ====================
		else if (panel_count == 2) {

			if (has_mem and has_net and not has_proc) {
				// Layouts 4-5: MEM + NET
				if (preset.net_position == NetPosition::Left) {
					// Layout 4: MEM + NET Left → MEM top, NET under (min height)
					int mem_h = remaining - min_h;
					if (mem_h < min_h) mem_h = remaining / 2;
					drawBox(0, y_pos, width, mem_h, mem_label);
					drawBox(0, y_pos + mem_h, width, remaining - mem_h, net_label);
				}
				else {
					// Layout 5: MEM + NET Right → MEM left, NET right (same height)
					drawBox(0, y_pos, half_w, remaining, mem_label);
					drawBox(half_w, y_pos, width - half_w, remaining, net_label);
				}
			}

			else if (has_mem and has_proc and not has_net) {
				// Layouts 6-7: MEM + PROC
				if (preset.proc_position == ProcPosition::Right) {
					// Layout 6: MEM + PROC Right → MEM left, PROC right
					drawBox(0, y_pos, half_w, remaining, mem_label);
					drawBox(half_w, y_pos, width - half_w, remaining, proc_label);
				}
				else {
					// Layout 7: MEM + PROC Wide → MEM top, PROC fills bottom
					int mem_h = min_h;
					if (remaining - mem_h < min_h) mem_h = remaining / 2;
					drawBox(0, y_pos, width, mem_h, mem_label);
					drawBox(0, y_pos + mem_h, width, remaining - mem_h, proc_label);
				}
			}

			else if (has_net and has_proc and not has_mem) {
				// Layouts 12-13: NET + PROC (no MEM)
				if (preset.proc_position == ProcPosition::Right) {
					// Layout 12: NET + PROC Right → NET left, PROC right
					drawBox(0, y_pos, half_w, remaining, net_label);
					drawBox(half_w, y_pos, width - half_w, remaining, proc_label);
				}
				else {
					// Layout 13: NET + PROC Wide → NET top (min), PROC fills bottom
					drawBox(0, y_pos, width, min_h, net_label);
					drawBox(0, y_pos + min_h, width, remaining - min_h, proc_label);
				}
			}
		}

		//? ==================== THREE PANEL LAYOUTS ====================
		else if (panel_count == 3) {

			bool net_left = (preset.net_position == NetPosition::Left);
			bool proc_right = (preset.proc_position == ProcPosition::Right);

			if (net_left and proc_right) {
				// Layout 8: MEM + NET Left + PROC Right
				// MEM top-left, NET under MEM (both on left), PROC fills right
				int left_w = half_w;
				int right_w = width - half_w;
				int mem_h = remaining - min_h;
				if (mem_h < min_h) mem_h = remaining / 2;

				drawBox(0, y_pos, left_w, mem_h, mem_label);              // MEM top-left
				drawBox(0, y_pos + mem_h, left_w, remaining - mem_h, net_label);  // NET under MEM
				drawBox(left_w, y_pos, right_w, remaining, proc_label);   // PROC right full height
			}
			else if (net_left and not proc_right) {
				// Layout 9: MEM + NET Left + PROC Wide
				// MEM top, NET under MEM, PROC wide at bottom
				int top_section = remaining - min_h;  // Space for MEM + NET
				if (top_section < min_h * 2) top_section = remaining * 2 / 3;
				int mem_h = top_section - min_h;
				if (mem_h < min_h) mem_h = top_section / 2;
				int net_h = top_section - mem_h;

				drawBox(0, y_pos, width, mem_h, mem_label);                      // MEM top
				drawBox(0, y_pos + mem_h, width, net_h, net_label);              // NET under MEM
				drawBox(0, y_pos + top_section, width, remaining - top_section, proc_label);  // PROC wide bottom
			}
			else if (not net_left and proc_right) {
				// Layout 10: MEM + NET Right + PROC Right
				// MEM left, NET above PROC on right
				int left_w = half_w;
				int right_w = width - half_w;
				int net_h = min_h;
				int proc_h = remaining - net_h;
				if (proc_h < min_h) {
					net_h = remaining / 2;
					proc_h = remaining - net_h;
				}

				drawBox(0, y_pos, left_w, remaining, mem_label);           // MEM left full height
				drawBox(left_w, y_pos, right_w, net_h, net_label);         // NET top-right
				drawBox(left_w, y_pos + net_h, right_w, proc_h, proc_label);  // PROC under NET
			}
			else {
				// Layout 11: MEM + NET Right + PROC Wide
				// MEM + NET side by side on top, PROC wide at bottom
				int top_h = min_h;
				int proc_h = remaining - top_h;
				if (proc_h < min_h) {
					top_h = remaining / 2;
					proc_h = remaining - top_h;
				}

				drawBox(0, y_pos, half_w, top_h, mem_label);               // MEM top-left
				drawBox(half_w, y_pos, width - half_w, top_h, net_label);  // NET top-right
				drawBox(0, y_pos + top_h, width, proc_h, proc_label);      // PROC wide bottom
			}
		}

		// Draw bottom panels (order from bottom: CPU at very bottom, GPU above, PWR above GPU)
		int bottom_y = height;
		if (preset.cpu_enabled and preset.cpu_bottom) {
			bottom_y -= bottom_panel_height;
			drawBox(0, bottom_y, width, bottom_panel_height, "CPU");
		}
		if (preset.gpu_enabled and preset.gpu_bottom) {
			bottom_y -= bottom_panel_height;
			drawBox(0, bottom_y, width, bottom_panel_height, "GPU");
		}
		if (preset.pwr_enabled and preset.pwr_bottom) {
			bottom_y -= bottom_panel_height;
			drawBox(0, bottom_y, width, bottom_panel_height, "PWR");
		}

		// Add layout indicator at bottom
		string indicator;
		indicator += "C=";
		indicator += preset.cpu_enabled ? (preset.cpu_bottom ? "B" : "T") : "-";
		indicator += " G=";
		indicator += preset.gpu_enabled ? (preset.gpu_bottom ? "B" : "T") : "-";
		indicator += " W=";  // W for PWR (Power)
		indicator += preset.pwr_enabled ? (preset.pwr_bottom ? "B" : "T") : "-";
		indicator += " M=";
		indicator += has_mem ? (preset.mem_type == MemType::Vertical ? "V" : "H") : "-";
		indicator += " N=";
		if (has_net) {
			indicator += preset.net_position == NetPosition::Left ? "L" :
			             preset.net_position == NetPosition::Right ? "R" : "W";
		} else {
			indicator += "-";
		}
		indicator += " P=";
		if (has_proc) {
			indicator += preset.proc_position == ProcPosition::Right ? "R" : "W";
		} else {
			indicator += "-";
		}

		// Place indicator on last line
		if (height >= 2) {
			int ind_x = (width - static_cast<int>(indicator.length())) / 2;
			if (ind_x < 0) ind_x = 0;
			for (size_t i = 0; i < indicator.length() and ind_x + static_cast<int>(i) < width; i++) {
				lines[height - 1][ind_x + i] = indicator[i];
			}
		}

		// Convert to final output string
		string result;
		if (use_unicode) {
			// Helper to check if position has a specific character
			auto hasChar = [&](int row, int col, char c) -> bool {
				if (row < 0 or row >= height or col < 0 or col >= width) return false;
				return lines[row][col] == c;
			};
			
			// Check for line characters at neighbors
			auto hasUp = [&](int row, int col) -> bool {
				return hasChar(row - 1, col, '|') or hasChar(row - 1, col, '+');
			};
			auto hasDown = [&](int row, int col) -> bool {
				return hasChar(row + 1, col, '|') or hasChar(row + 1, col, '+');
			};
			auto hasLeft = [&](int row, int col) -> bool {
				return hasChar(row, col - 1, '-') or hasChar(row, col - 1, '+');
			};
			auto hasRight = [&](int row, int col) -> bool {
				return hasChar(row, col + 1, '-') or hasChar(row, col + 1, '+');
			};
			
			for (int row = 0; row < height; row++) {
				for (int col = 0; col < width; col++) {
					char c = lines[row][col];
					if (c == '-') {
						result += "─";
					} else if (c == '|') {
						result += "│";
					} else if (c == '+') {
						// Determine junction type based on neighbors
						bool up = hasUp(row, col);
						bool down = hasDown(row, col);
						bool left = hasLeft(row, col);
						bool right = hasRight(row, col);
						
						if (up and down and left and right) {
							result += "┼";  // Cross
						} else if (down and right and not up and not left) {
							result += "┌";  // Top-left corner
						} else if (down and left and not up and not right) {
							result += "┐";  // Top-right corner
						} else if (up and right and not down and not left) {
							result += "└";  // Bottom-left corner
						} else if (up and left and not down and not right) {
							result += "┘";  // Bottom-right corner
						} else if (left and right and down and not up) {
							result += "┬";  // T-junction down
						} else if (left and right and up and not down) {
							result += "┴";  // T-junction up
						} else if (up and down and right and not left) {
							result += "├";  // T-junction right
						} else if (up and down and left and not right) {
							result += "┤";  // T-junction left
						} else if (left and right) {
							result += "─";  // Horizontal line
						} else if (up and down) {
							result += "│";  // Vertical line
						} else {
							result += "┼";  // Default to cross for unknown cases
						}
					} else {
						result += c;
					}
				}
				if (row < height - 1) result += "\n";
			}
		} else {
			// ASCII mode - keep original characters
			for (const auto& line : lines) {
				result += line + "\n";
			}
			if (not result.empty()) result.pop_back();
		}
		
		return result;
	}

	//? ==================== Category Definitions ====================

	const vector<Category>& getCategories() {
		static vector<Category> cats;

		if (cats.empty()) {
			// Category 1: System
			cats.push_back({
				"System", "",
				{
					{"Update", {
						{"update_ms", "Update Interval", "Update time in milliseconds (100-86400000)", ControlType::Slider, {}, "", 100, 10000, 100},
						{"background_update", "Background Update", "Update UI when menus are showing", ControlType::Toggle, {}, "", 0, 0, 0},
						{"terminal_sync", "Terminal Sync", "Use synchronized output to reduce flickering", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Input", {
						{"vim_keys", "Vim Keys (hjkl)", "Enable h,j,k,l for directional control", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disable_mouse", "Disable Mouse", "Disable all mouse events", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Units", {
						{"base_10_sizes", "Base 10 Sizes", "Use KB=1000 instead of KiB=1024", ControlType::Toggle, {}, "", 0, 0, 0},
						{"base_10_bitrate", "Bitrate Units", "How to display network bitrates", ControlType::Radio, {"Auto", "True", "False"}, "", 0, 0, 0},
					}},
					{"Logging", {
						{"log_level", "Log Level", "Logging verbosity for error.log", ControlType::Radio, {"DISABLED", "ERROR", "WARNING", "INFO", "DEBUG"}, "", 0, 0, 0},
					}},
					{"Config", {
						{"save_config_on_exit", "Save on Exit", "Auto-save settings when exiting", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Multi Instance", {
						{"prevent_autosave", "Prevent AutoSave", "Secondary instances won't save config (restart required)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_instance_indicator", "Show Indicator", "Show P (Primary) or S (Secondary) indicator in header", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Advanced", {
						{"cpu_core_map", "Temp Sensor Map", "Map core temps to sensors (x:y format, Linux/BSD only)", ControlType::Text, {}, "", 0, 0, 0},
					#ifdef __linux__
						{"freq_mode", "Frequency Mode", "CPU frequency display mode", ControlType::Radio, {"first", "range", "lowest", "highest", "average"}, "", 0, 0, 0},
					#endif
					}},
					{"Compatibility", {
						{"lowcolor", "Low Color Mode", "Use 256-color mode instead of truecolor", ControlType::Toggle, {}, "", 0, 0, 0},
						{"tty_mode", "TTY Mode", "Force TTY compatible output (16 colors)", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				},
				false, {}
			});

			// Category 2: Appearance
			cats.push_back({
				"Appearance", "",
				{
					{"Theme", {
						{"color_theme", "Color Theme", "Select color theme from themes folder", ControlType::Select, {}, "color_theme", 0, 0, 0},
						{"theme_background", "Theme Background", "Show theme background color", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Colors", {
						{"truecolor", "True Color (24-bit)", "Use 24-bit colors (disable for 256-color terminals)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"force_tty", "Force TTY Mode", "Force 16-color TTY mode", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Borders", {
						{"rounded_corners", "Rounded Corners", "Use rounded corners on boxes", ControlType::Toggle, {}, "", 0, 0, 0},
						{"preview_unicode", "Unicode Preview", "Use Unicode box-drawing for preset preview", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Graph Style", {
						{"graph_symbol", "Default Graph Symbol", "Symbol style for all graphs", ControlType::Radio, {"braille", "block", "tty"}, "", 0, 0, 0},
					}},
					{"Title Bar", {
						{"clock_format", "Clock Format", "Time display style in header", ControlType::Select, {}, "clock_format", 0, 0, 0},
						{"show_hostname", "Show Hostname", "Display hostname in header (centered)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_uptime_header", "Show Uptime", "Display uptime in header", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_username_header", "Show Username", "Display username in header", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Battery", {
						{"show_battery", "Show Battery", "Display battery status if present", ControlType::Toggle, {}, "", 0, 0, 0},
						{"selected_battery", "Battery", "Which battery to monitor", ControlType::Select, {}, "selected_battery", 0, 0, 0},
						{"show_battery_watts", "Show Battery Power", "Display charge/discharge power", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Layout", {
						{"box_show_cpu", "CPU", "Show CPU panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_gpu", "GPU", "Show GPU panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_pwr", "PWR", "Show Power panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_mem", "MEM", "Show Memory panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_net", "NET", "Show Network panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_proc", "PROC", "Show Processes panel", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				},
				false, {}
			});

			// Category 3: Panels (with sub-tabs)
			cats.push_back({
				"Panels", "",
				{
					//? ==================== CPU Sub-tab ====================
					{"CPU | Display", {
						{"show_uptime", "Show Uptime", "Display uptime in CPU box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_cpu_freq", "Show Frequency", "Display CPU frequency", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_cpu_watts", "Show Power", "Display CPU power consumption", ControlType::Toggle, {}, "", 0, 0, 0},
						{"custom_cpu_name", "Custom CPU Name", "Override CPU model name (empty to disable)", ControlType::Text, {}, "", 0, 0, 0},
					}},
					{"CPU | Temperature", {
						{"check_temp", "Show Temperature", "Enable CPU temperature display", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_coretemp", "Show Core Temps", "Display per-core temperatures", ControlType::Toggle, {}, "", 0, 0, 0},
						{"temp_scale", "Temperature Scale", "Temperature unit", ControlType::Radio, {"celsius", "fahrenheit", "kelvin", "rankine"}, "", 0, 0, 0},
						{"cpu_sensor", "Temperature Sensor", "Select CPU temperature sensor", ControlType::Select, {}, "cpu_sensor", 0, 0, 0},
					}},
					{"CPU | Graphs", {
						{"graph_symbol_cpu", "Graph Symbol", "Symbol for CPU graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
						{"cpu_graph_upper", "Upper Graph", "Stat for upper half of CPU graph", ControlType::Select, {}, "cpu_graph_upper", 0, 0, 0},
						{"cpu_graph_lower", "Lower Graph", "Stat for lower half of CPU graph", ControlType::Select, {}, "cpu_graph_lower", 0, 0, 0},
						{"cpu_single_graph", "Single Graph", "Disable lower graph (upper only)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"cpu_invert_lower", "Invert Lower", "Flip orientation of lower graph", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				#ifdef GPU_SUPPORT
					{"CPU | GPU Info", {
						{"show_gpu_info", "Show GPU Info", "Display GPU stats in CPU box", ControlType::Radio, {"Auto", "On", "Off"}, "", 0, 0, 0},
					}},

					//? ==================== GPU Sub-tab ====================
					{"GPU | Display", {
						{"gpu_mirror_graph", "Mirror Graph", "Mirror GPU graph horizontally", ControlType::Toggle, {}, "", 0, 0, 0},
						{"nvml_measure_pcie_speeds", "Measure PCIe Speeds", "Enable PCIe bandwidth measurement (NVIDIA)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"shown_gpus", "GPU Vendors", "Select which GPU vendors to monitor", ControlType::MultiCheck, {"Apple", "NVIDIA", "AMD", "Intel"}, "", 0, 0, 0},
					}},
					{"GPU | Graphs", {
						{"graph_symbol_gpu", "Graph Symbol", "Symbol for GPU graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
				#endif

					//? ==================== Memory Sub-tab ====================
					{"Mem | Display", {
						{"mem_below_net", "Memory Below Net", "Place memory box below network box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"net_beside_mem", "Net Beside Mem", "Show network box beside memory box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_graphs", "Show Mem Graphs", "Display memory usage graphs", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_horizontal", "Horizontal Layout", "Show memory graphs side by side", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_bar_mode", "Bar Mode", "Use meter bars instead of graphs", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_swap", "Show Swap", "Display swap memory info", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_disk", "Swap as Disk", "Show swap as disk entry", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Mem | Charts", {
						{"Mem", "Mem", "Select memory charts to display", ControlType::ToggleRow, {"Used", "Available", "Cached", "Free"}, "mem_show_", 0, 0, 0},
						{"Swap", "Swap", "Select swap charts to display", ControlType::ToggleRow, {"Used", "Free"}, "swap_show_", 0, 0, 0},
						{"Vram", "Vram", "Select VRAM charts to display", ControlType::ToggleRow, {"Used", "Free"}, "vram_show_", 0, 0, 0},
					}},
					{"Mem | Graphs", {
						{"graph_symbol_mem", "Graph Symbol", "Symbol for memory graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
					{"Mem | ZFS", {
						{"zfs_arc_cached", "ZFS ARC as Cache", "Count ZFS ARC as cached memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"zfs_hide_datasets", "Hide ZFS Datasets", "Hide individual ZFS datasets from disk list", ControlType::Toggle, {}, "", 0, 0, 0},
					}},

					//? ==================== Disk Sub-tab ====================
					{"Disk | Display", {
						{"show_disks", "Show Disks", "Enable disk usage display", ControlType::Toggle, {}, "", 0, 0, 0},
						{"only_physical", "Only Physical", "Hide virtual/loop devices", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_network_drives", "Network Drives", "Show NFS/SMB/AFP network drives", ControlType::Toggle, {}, "", 0, 0, 0},
						{"use_fstab", "Use fstab", "Use fstab for disk detection", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disk_free_priv", "Free as Privileged", "Calculate free space as privileged user", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disks_filter", "Disks to Show", "Select disks to display (empty = show all)", ControlType::Select, {}, "disks_filter", 0, 0, 0},
					}},
					{"Disk | I/O", {
						{"show_io_stat", "Show I/O Stats", "Display disk I/O statistics", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_mode", "I/O Mode", "Toggle detailed I/O display mode", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_graph_combined", "Combined I/O Graph", "Combine read/write into single graph", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_graph_speeds", "Graph Speeds", "I/O graph maximum speed (MiB/s)", ControlType::Select, {}, "io_graph_speeds", 0, 0, 0},
					}},

					//? ==================== Network Sub-tab ====================
					{"Net | Display", {
						{"net_auto", "Auto Select", "Auto-select primary network interface", ControlType::Toggle, {}, "", 0, 0, 0},
						{"net_iface", "Interface", "Select network interface to display", ControlType::Select, {}, "net_iface", 0, 0, 0},
						{"net_iface_filter", "Interfaces to Show", "Select interfaces for cycling (empty = show all)", ControlType::Select, {}, "net_iface_filter", 0, 0, 0},
						{"net_sync", "Sync Scales", "Synchronize upload/download graph scales", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_upload_download", "Swap Up/Down", "Swap upload and download positions", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"NET | Reference", {
						{"net_download", "Download Reference", "Download speed reference value (Mebibits)", ControlType::Slider, {}, "", 1, 10000, 10},
						{"net_upload", "Upload Reference", "Upload speed reference value (Mebibits)", ControlType::Slider, {}, "", 1, 10000, 10},
					}},
					{"Net | Graphs", {
						{"graph_symbol_net", "Graph Symbol", "Symbol for network graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},

					//? ==================== Processes Sub-tab ====================
					{"Proc | Display", {
						{"proc_left", "Processes Left", "Place processes box on left side", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_full_width", "Full Width", "Show proc panel full width (with net_beside_mem)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_tree", "Tree View", "Show process tree hierarchy", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_colors", "Enable Colors", "Enable colored process info", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gradient", "Gradient", "Enable gradient colors in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_per_core", "Per Core", "Show per-core CPU usage", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_mem_bytes", "Memory as Bytes", "Show memory usage in bytes", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_show_cmd", "Show Command", "Show command column", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Behavior", {
						{"proc_aggregate", "Aggregate", "Aggregate child process stats in tree view", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_info_smaps", "Use smaps", "Use smaps for accurate memory (slower)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_filter_kernel", "Filter Kernel", "Filter out kernel processes (Linux)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_follow_detailed", "Follow Detailed", "Follow selected process in detailed view", ControlType::Toggle, {}, "", 0, 0, 0},
						{"keep_dead_proc_usage", "Keep Dead Usage", "Preserve CPU/mem usage for dead processes", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Sorting", {
						{"proc_sorting", "Default Sort", "Default process sorting column", ControlType::Select, {}, "proc_sorting", 0, 0, 0},
						{"proc_reversed", "Reverse Sort", "Reverse sort order", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Graphs", {
						{"proc_cpu_graphs", "CPU Graphs", "Show mini CPU graphs in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gpu", "GPU Column", "Show GPU usage column (Apple Silicon)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gpu_graphs", "GPU Graphs", "Show mini GPU graphs in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"graph_symbol_proc", "Graph Symbol", "Symbol for process graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
					{"Proc | Logs", {
						{"logs_below_proc", "Position Below", "Show logs panel below proc instead of beside", ControlType::Toggle, {}, "", 0, 0, 0},
						{"log_color_full_line", "Color Full Line", "Color entire log line (off = only [X] marker colored)", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				},
				true, {"CPU", "GPU", "PWR", "Memory", "Network", "Processes"}
			});

			// Category 4: Presets
			cats.push_back({
				"Presets", "",
				{
					{"Presets", {
						// Presets use special handling - not regular options
					}},
				},
				false, {}
			});
		}

		return cats;
	}

	//? ==================== Preset Conversion ====================

	//? ==================== Preset Config String Format ====================
	//? Format: box:param1:param2:...,box:param1:...
	//?
	//? cpu:0:symbol         - CPU panel (0 = always full width)
	//? gpu0:0:symbol        - GPU panel
	//? pwr:0:symbol         - Power panel
	//? mem:type:symbol:meter:disk  - Memory panel
	//?     type: 0=Horizontal, 1=Vertical
	//?     meter: 0=Bar, 1=Meter (only used if type=V)
	//?     disk: 0=hidden, 1=shown (only used if type=V)
	//? net:pos:symbol       - Network panel
	//?     pos: 0=Left, 1=Right, 2=Wide
	//? proc:pos:symbol      - Process panel
	//?     pos: 0=Right, 1=Wide

	string PresetDef::toConfigString() const {
		string result;
		vector<string> boxes;

		// Top panels (always full width)
		// cpu:pos:symbol, gpu0:pos:symbol, pwr:pos:symbol where pos: 0=top, 1=bottom
		if (cpu_enabled) boxes.push_back("cpu:" + to_string(cpu_bottom ? 1 : 0) + ":" + graph_symbol);
		if (gpu_enabled) boxes.push_back("gpu0:" + to_string(gpu_bottom ? 1 : 0) + ":" + graph_symbol);
		if (pwr_enabled) boxes.push_back("pwr:" + to_string(pwr_bottom ? 1 : 0) + ":" + graph_symbol);

		// MEM panel: mem:type:symbol:meter:disk:mem_vis:swap_vis:vram_vis
		// mem_vis bitmask: bit0=used, bit1=available, bit2=cached, bit3=free
		// swap_vis bitmask: bit0=used, bit1=free
		// vram_vis bitmask: bit0=used, bit1=free
		if (mem_enabled) {
			int type_val = (mem_type == MemType::Vertical) ? 1 : 0;
			int meter_val = mem_graph_meter ? 1 : 0;
			int disk_val = show_disk ? 1 : 0;
			int mem_vis = (mem_show_used ? 1 : 0) | (mem_show_available ? 2 : 0) |
			              (mem_show_cached ? 4 : 0) | (mem_show_free ? 8 : 0);
			int swap_vis = (swap_show_used ? 1 : 0) | (swap_show_free ? 2 : 0);
			int vram_vis = (vram_show_used ? 1 : 0) | (vram_show_free ? 2 : 0);
			boxes.push_back("mem:" + to_string(type_val) + ":" + graph_symbol + ":" +
			                to_string(meter_val) + ":" + to_string(disk_val) + ":" +
			                to_string(mem_vis) + ":" + to_string(swap_vis) + ":" + to_string(vram_vis));
		}

		// NET panel: net:pos:symbol:direction
		if (net_enabled) {
			int pos_val = (net_position == NetPosition::Left) ? 0 :
			              (net_position == NetPosition::Right) ? 1 : 2;
			boxes.push_back("net:" + to_string(pos_val) + ":" + graph_symbol + ":" + to_string(net_graph_direction));
		}

		// PROC panel: proc:pos:symbol
		if (proc_enabled) {
			int pos_val = (proc_position == ProcPosition::Right) ? 0 : 1;
			boxes.push_back("proc:" + to_string(pos_val) + ":" + graph_symbol);
		}

		// Join with commas
		for (size_t i = 0; i < boxes.size(); i++) {
			if (i > 0) result += ",";
			result += boxes[i];
		}

		return result;
	}

	PresetDef PresetDef::fromConfigString(const string& config, const string& preset_name) {
		PresetDef preset;
		preset.name = preset_name;

		// Start with everything disabled
		preset.cpu_enabled = false;
		preset.gpu_enabled = false;
		preset.pwr_enabled = false;
		preset.mem_enabled = false;
		preset.net_enabled = false;
		preset.proc_enabled = false;

		auto parts = ssplit(config, ',');
		for (const auto& part : parts) {
			auto box_parts = ssplit(part, ':');
			if (box_parts.empty()) continue;

			string box_name = box_parts[0];
			int param1 = (box_parts.size() > 1) ? stoi_safe(box_parts[1], 0) : 0;
			string symbol = (box_parts.size() > 2) ? string(box_parts[2]) : "default";

			// Capture first non-default symbol
			if (preset.graph_symbol == "default" and symbol != "default") {
				preset.graph_symbol = symbol;
			}

			if (box_name == "cpu") {
				preset.cpu_enabled = true;
				// pos: 0=top, 1=bottom
				preset.cpu_bottom = (param1 == 1);
			}
			else if (box_name.starts_with("gpu")) {
				preset.gpu_enabled = true;
				// pos: 0=top, 1=bottom
				preset.gpu_bottom = (param1 == 1);
			}
			else if (box_name == "pwr") {
				preset.pwr_enabled = true;
				// pos: 0=top, 1=bottom
				preset.pwr_bottom = (param1 == 1);
			}
			else if (box_name == "mem") {
				preset.mem_enabled = true;
				// type: 0=H, 1=V
				preset.mem_type = (param1 == 1) ? MemType::Vertical : MemType::Horizontal;
				// meter (field 3): 0=Bar, 1=Meter
				if (box_parts.size() > 3) {
					preset.mem_graph_meter = (stoi_safe(box_parts[3], 0) == 1);
				}
				// disk (field 4): 0=hidden, 1=shown
				if (box_parts.size() > 4) {
					preset.show_disk = (stoi_safe(box_parts[4], 0) == 1);
				}
				// mem_vis (field 5): bitmask for mem chart visibility (default 15 = all visible)
				if (box_parts.size() > 5) {
					int mem_vis = stoi_safe(box_parts[5], 15);
					preset.mem_show_used = (mem_vis & 1) != 0;
					preset.mem_show_available = (mem_vis & 2) != 0;
					preset.mem_show_cached = (mem_vis & 4) != 0;
					preset.mem_show_free = (mem_vis & 8) != 0;
				}
				// swap_vis (field 6): bitmask for swap chart visibility (default 3 = all visible)
				if (box_parts.size() > 6) {
					int swap_vis = stoi_safe(box_parts[6], 3);
					preset.swap_show_used = (swap_vis & 1) != 0;
					preset.swap_show_free = (swap_vis & 2) != 0;
				}
				// vram_vis (field 7): bitmask for vram chart visibility (default 3 = all visible)
				if (box_parts.size() > 7) {
					int vram_vis = stoi_safe(box_parts[7], 3);
					preset.vram_show_used = (vram_vis & 1) != 0;
					preset.vram_show_free = (vram_vis & 2) != 0;
				}
			}
			else if (box_name == "net") {
				preset.net_enabled = true;
				// pos: 0=Left, 1=Right, 2=Wide
				preset.net_position = (param1 == 0) ? NetPosition::Left :
				                      (param1 == 1) ? NetPosition::Right : NetPosition::Wide;
				// direction (field 4): 0=RTL, 1=LTR, 2=TTB, 3=BTT
				if (box_parts.size() > 3) {
					int dir = stoi_safe(box_parts[3], 0);
					preset.net_graph_direction = (dir >= 0 and dir <= 3) ? dir : 0;
				}
			}
			else if (box_name == "proc") {
				preset.proc_enabled = true;
				// pos: 0=Right, 1=Wide
				preset.proc_position = (param1 == 0) ? ProcPosition::Right : ProcPosition::Wide;
			}
		}

		// Apply constraints after parsing
		preset.enforceConstraints();

		return preset;
	}

	//? Enforce layout constraints after any modification
	void PresetDef::enforceConstraints() {
		// Rule 1: MEM Horizontal forces Bar and no Disk
		if (mem_type == MemType::Horizontal) {
			mem_graph_meter = false;  // Force Bar
			show_disk = false;        // Force no disk
		}

		// Rule 2: NET position depends on MEM
		if (not mem_enabled) {
			// No MEM: NET can only be Wide
			net_position = NetPosition::Wide;
		}
		else if (net_position == NetPosition::Wide) {
			// MEM enabled but NET was Wide: reset to Right (valid default)
			net_position = NetPosition::Right;
		}

		// Rule 3: If only PROC (no MEM, no NET), PROC fills all → force Wide
		if (proc_enabled and not mem_enabled and not net_enabled) {
			proc_position = ProcPosition::Wide;
		}

		// Rule 4: If only NET (no MEM, no PROC), position is Wide
		if (net_enabled and not mem_enabled and not proc_enabled) {
			net_position = NetPosition::Wide;
		}
	}

	vector<PresetDef> getPresets() {
		vector<PresetDef> presets;

		// Get preset names from config
		auto preset_names_str = Config::getS("preset_names");
		auto names = ssplit(preset_names_str);

		// Read from Config::preset_list which is the actual runtime preset list
		for (size_t i = 0; i < Config::preset_list.size() && i < 9; i++) {
			string name = (i < names.size()) ? string(names[i]) : "";
			string config_str;

			// For preset 0, check if there's a saved "preset_0" config first
			if (i == 0) {
				config_str = Config::getS("preset_0");
				if (config_str.empty()) {
					config_str = Config::preset_list[i];
				}
			} else {
				config_str = Config::preset_list[i];
			}

			presets.push_back(PresetDef::fromConfigString(config_str, name));
		}

		// Ensure we have at least one preset
		if (presets.empty()) {
			presets.push_back(PresetDef{});  // Default preset
		}

		return presets;
	}

	void savePresets(const vector<PresetDef>& presets) {
		string names_str;
		string configs_str;

		// Build names for all presets (including preset 0)
		for (size_t i = 0; i < presets.size() && i < 9; i++) {
			if (i > 0) names_str += " ";
			names_str += presets[i].name.empty() ? ("Preset" + to_string(i + 1)) : presets[i].name;
		}

		// Update preset_list[0] directly with the first preset's config
		if (!presets.empty() && !Config::preset_list.empty()) {
			Config::preset_list[0] = presets[0].toConfigString();
		}

		// Build config string for presets 1-N
		// The "presets" config string contains space-separated preset definitions
		for (size_t i = 1; i < presets.size() && i < 9; i++) {
			if (i > 1) configs_str += " ";
			configs_str += presets[i].toConfigString();
		}

		Config::set("preset_names", names_str);
		Config::set("presets", configs_str);
		// Also save preset 0 separately so it persists across restarts
		if (!presets.empty()) {
			Config::set("preset_0", presets[0].toConfigString());
		}

		// Refresh Config::preset_list so 'p' key uses the updated presets
		// presetsValid() will prepend preset_list[0] and add presets from configs_str
		Config::presetsValid(configs_str);
	}

	//? ==================== Dynamic Choices Helper ====================

	//? Get dynamic choices for Select controls based on choices_ref
	vector<string> getDynamicChoices(const string& choices_ref) {
		vector<string> choices;

		if (choices_ref == "net_iface") {
			// Return available network interfaces
			choices.push_back("");  // Empty = auto-select
			for (const auto& iface : Net::interfaces) {
				choices.push_back(iface);
			}
		}
		else if (choices_ref == "net_iface_filter") {
			// Return available network interfaces for filtering (multi-select)
			choices.push_back("");  // Empty = show all interfaces
			for (const auto& iface : Net::interfaces) {
				choices.push_back(iface);
			}
		}
		else if (choices_ref == "disks_filter") {
			// Return available disk mountpoints for filtering
			// Query system directly for mounted disks (don't rely on Mem::collect which may be filtered)
			choices.push_back("");  // Empty = show all disks

#if defined(__APPLE__)
			// macOS: Use getmntinfo() to get all mounted filesystems
			struct statfs *stfs = nullptr;
			int count = getmntinfo(&stfs, MNT_NOWAIT);
			for (int i = 0; i < count; i++) {
				string mountpoint = stfs[i].f_mntonname;
				string fstype = stfs[i].f_fstypename;
				uint32_t flags = stfs[i].f_flags;

				// Skip system/internal filesystems
				if (fstype == "autofs" or fstype == "devfs") continue;
				if (flags & MNT_DONTBROWSE) continue;  // Skip nobrowse volumes (internal macOS APFS)

				choices.push_back(mountpoint);
			}
#elif defined(__linux__)
			// Linux: Use getmntent() to read /proc/mounts
			FILE* mtab = setmntent("/proc/mounts", "r");
			if (mtab != nullptr) {
				struct mntent* entry;
				while ((entry = getmntent(mtab)) != nullptr) {
					string mountpoint = entry->mnt_dir;
					string fstype = entry->mnt_type;

					// Skip virtual/system filesystems
					if (fstype == "proc" or fstype == "sysfs" or fstype == "devtmpfs" or
					    fstype == "devpts" or fstype == "tmpfs" or fstype == "cgroup" or
					    fstype == "cgroup2" or fstype == "securityfs" or fstype == "pstore" or
					    fstype == "debugfs" or fstype == "tracefs" or fstype == "configfs" or
					    fstype == "fusectl" or fstype == "mqueue" or fstype == "hugetlbfs" or
					    fstype == "binfmt_misc" or fstype == "autofs" or fstype == "rpc_pipefs" or
					    fstype == "nfsd" or fstype == "overlay")
						continue;

					// Skip snap mounts
					if (mountpoint.find("/snap/") != string::npos) continue;

					choices.push_back(mountpoint);
				}
				endmntent(mtab);
			}
#else
			// FreeBSD/other: Use getmntinfo() similar to macOS
			struct statfs *stfs = nullptr;
			int count = getmntinfo(&stfs, MNT_NOWAIT);
			for (int i = 0; i < count; i++) {
				string mountpoint = stfs[i].f_mntonname;
				choices.push_back(mountpoint);
			}
#endif
		}
		else if (choices_ref == "color_theme") {
			// Return available themes - Theme::themes is a vector<string>
			for (const auto& theme : Theme::themes) {
				choices.push_back(theme);
			}
		}
		else if (choices_ref == "proc_sorting") {
			// Fixed choices for process sorting
			choices = {"pid", "program", "arguments", "threads", "user", "memory", "cpu lazy", "cpu direct"};
		}
		else if (choices_ref == "cpu_sensor") {
			// Return available CPU temperature sensors
			choices.push_back("");  // Empty = auto-select
			for (const auto& sensor : Cpu::available_sensors) {
				choices.push_back(sensor);
			}
		}
		else if (choices_ref == "cpu_graph_upper" or choices_ref == "cpu_graph_lower") {
			// Fixed choices for CPU graph stats
			choices = {"total", "user", "system", "iowait", "irq", "steal", "guest"};
		}
		else if (choices_ref == "selected_battery") {
			// Return available batteries
			choices.push_back("Auto");
			// Add detected batteries if available
		}
		else if (choices_ref == "clock_format") {
			// Common clock format choices
			choices = {"%X", "%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p", ""};
		}
		else if (choices_ref == "io_graph_speeds") {
			// Common I/O speed presets in MiB/s
			choices = {
				"",           // Auto-scale (empty = auto)
				"10",         // 10 MiB/s (slow HDD)
				"50",         // 50 MiB/s (HDD)
				"100",        // 100 MiB/s (default, fast HDD)
				"250",        // 250 MiB/s (SATA SSD)
				"500",        // 500 MiB/s (fast SATA SSD)
				"1000",       // 1000 MiB/s (NVMe)
				"2000",       // 2000 MiB/s (fast NVMe)
				"5000",       // 5000 MiB/s (PCIe 4.0 NVMe)
				"10000"       // 10000 MiB/s (PCIe 5.0 NVMe)
			};
		}

		return choices;
	}

	//? ==================== Main Options Menu V2 ====================

	//? Preset editor state (namespace scope for persistence across function calls)
	bool ns_in_preset_editor = false;
	int ns_editing_preset_idx = -1;  //? Which preset we're editing (-1 = new preset)

	int optionsMenuV2(const string& key) {
		// This is a placeholder - full implementation requires substantial UI work
		// For now, redirect to the classic menu
		// TODO: Implement full V2 menu with tabs, proper controls, preset editor

		// Static state variables
		static int selected_cat = 0;
		static int selected_subcat = 0;
		static int selected_option = 0;
		static int scroll_offset = 0;
		static bool editing = false;
		static Draw::TextEdit editor;
		static bool dropdown_open = false;
		static int dropdown_selection = 0;
		static vector<string> dropdown_choices;
		static string dropdown_key;            //? Track which option key opened the dropdown
		static bool theme_refresh = false;
		static std::set<string> disk_filter_selections;  //? Track selected disks to show (multi-select)
		static std::set<string> net_iface_filter_selections;  //? Track selected interfaces to show (multi-select)

		//? Presets category state (0-based: System=0, Appearance=1, Panels=2, Presets=3)
		static int selected_preset = 0;         //? Currently selected preset (0-8)
		//? MOVED: static bool in_preset_editor = false; -> now ns_in_preset_editor at namespace scope
		static int preset_button_focus = 0;     //? 0=list, 1=Edit, 2=Delete, 3=New

		//? MultiCheck state for shown_gpus
		static int multicheck_focus = 0;        //? Currently focused checkbox item

		//? ToggleRow state for memory chart visibility
		static int togglerow_focus = 0;         //? Currently focused toggle item in row

		const int PRESETS_CATEGORY = 3;         //? Index of Presets category (0-based)

		const auto& cats = getCategories();
		auto tty_mode = Config::getB("tty_mode");
		auto vim_keys = Config::getB("vim_keys");

		// Dialog dimensions - responsive height (min 30, max 60), always centered
		const int dialog_width = 76;
		const int min_dialog_height = 30;
		const int max_dialog_height = 60;
		const int banner_height = 9;      // MBTOP banner is 9 rows
		const int dialog_spacing = 0;     // mask_padding provides the 1 line gap

		// Calculate what we can show based on available space
		// Need: top_margin(1) + banner + dialog + bottom_margin(1)
		const int bottom_margin = 1;  // 1 line gap at bottom of dialog
		const int total_with_banner = banner_height + dialog_spacing + min_dialog_height + 2 + bottom_margin;

		bool show_banner = (Term::height >= total_with_banner);

		// Calculate dialog height based on available space
		int available_for_dialog;
		if (show_banner) {
			available_for_dialog = Term::height - banner_height - dialog_spacing - 2 - bottom_margin;
		} else {
			available_for_dialog = Term::height - 4 - bottom_margin;
		}
		const int dialog_height = max(min_dialog_height, min(available_for_dialog, max_dialog_height));

		// Calculate positions - everything centered horizontally and vertically as a group
		const int x = (Term::width - dialog_width) / 2;

		// Calculate total height of the visual unit (banner + dialog)
		const int top_margin = 1;  // Always have 1 blank line above MBTOP banner
		int total_unit_height = dialog_height;
		if (show_banner) total_unit_height += banner_height + dialog_spacing;

		// Start y position: center the entire unit vertically, but ensure top margin
		int unit_start_y = max(1 + top_margin, (Term::height - total_unit_height) / 2);

		int y;
		if (show_banner) {
			// Banner at top, 1 line gap, then dialog
			y = unit_start_y + banner_height + dialog_spacing;
		} else {
			// Just dialog, centered
			y = unit_start_y;
		}

		auto& out = Global::overlay;
		int retval = Menu::Changed;

		// Lambda to rebuild Menu::bg with current theme colors
		// This is called on initial draw AND after theme changes
		auto rebuildMenuBg = [&]() {
			Menu::mouse_mappings.clear();
			Menu::bg.clear();

			// Draw MBTOP banner if there's room
			if (show_banner) {
				Menu::bg = Draw::banner_gen(unit_start_y, 0, true);
			}

			// Draw blank mask around the dialog box (1 char padding)
			const int mask_padding = 1;
			int mask_x = max(1, x - mask_padding);
			int mask_y = max(1, y - mask_padding);
			int mask_width = min(dialog_width + mask_padding * 2, Term::width - mask_x + 1);
			int mask_height = min(dialog_height + mask_padding * 2, Term::height - mask_y + 1);
			string blank_line(mask_width, ' ');
			for (int row = 0; row < mask_height; row++) {
				Menu::bg += Mv::to(mask_y + row, mask_x) + Theme::c("main_bg") + blank_line;
			}

			// Draw dialog background
			Menu::bg += Draw::createBox(x, y, dialog_width, dialog_height, Theme::c("hi_fg"), true, "MBTOP Settings");

			// Add close button [X] in top-right corner of title bar
			int close_btn_x = x + dialog_width - 5;
			Menu::bg += Mv::to(y, close_btn_x) + Theme::c("hi_fg") + "[" + Theme::c("inactive_fg") + "×" + Theme::c("hi_fg") + "]";

			// Mouse mapping for close button
			Menu::mouse_mappings["close_settings"] = {y, close_btn_x, 1, 3};

			// Mouse mappings for tabs - calculate actual positions based on tab text widths
			int tab_x = x + 2;
			for (size_t i = 0; i < cats.size(); i++) {
				string tab_text = cats[i].icon.empty() ? cats[i].name : cats[i].icon + " " + cats[i].name;
				int tab_width = static_cast<int>(tab_text.size()) + 2;  // +2 for brackets/spaces
				Menu::mouse_mappings["tab_" + to_string(i)] = {
					y + 1, tab_x, 3, tab_width
				};
				tab_x += tab_width + 2;  // +2 for spacing between tabs
			}

			// Double-line separator under categories
			const string double_h_line = "═";
			Menu::bg += Mv::to(y + 3, x) + Theme::c("hi_fg") + "╞"
				+ Theme::c("div_line");
			for (int i = 0; i < dialog_width - 2; i++) Menu::bg += double_h_line;
			Menu::bg += Theme::c("hi_fg") + "╡";
		};

		// Handle theme change - DON'T apply during menu, only mark for full app refresh on exit
		if (theme_refresh) {
			theme_refresh = false;
			theme_changed_pending = true;         // Mark for full refresh when menu closes
		}

		if (Menu::redraw) {
			rebuildMenuBg();
		}

		//? Handle close button [x] - always works, even when in preset editor
		if (key == "close_settings") {
			ns_in_preset_editor = false;  // Reset editor state if open
			return Menu::Closed;
		}

		//? Handle preset editor mode - delegate to presetEditor but Menu::bg is already built
		if (ns_in_preset_editor) {
			int editor_result = presetEditor(key, ns_editing_preset_idx, x, y, dialog_width, dialog_height);
			if (editor_result == Menu::Closed) {
				ns_in_preset_editor = false;
				preset_button_focus = 0;  // Reset focus to preset list after closing editor
				rebuildMenuBg();           // Rebuild entire menu background to clear editor overlay
				Menu::redraw = true;       // Force redraw to clear overlay and draw fresh
				// Fall through to redraw the Presets panel immediately
			}
			else {
				return editor_result;
			}
		}

		if (dropdown_open) {
			// Handle dropdown selection modal
			const bool is_disk_filter = (dropdown_key == "disks_filter");
			const bool is_net_filter = (dropdown_key == "net_iface_filter");
			const bool is_multi_select = is_disk_filter or is_net_filter;

			if (is_in(key, "escape", "q", "backspace") or key == "close_dropdown") {
				//? For multi-select filters, save selections before closing
				if (is_disk_filter) {
					string filter_str;
					for (const auto& disk : disk_filter_selections) {
						if (not filter_str.empty()) filter_str += " ";
						filter_str += disk;
					}
					Config::set("disks_filter", filter_str);
				}
				else if (is_net_filter) {
					string filter_str;
					for (const auto& iface : net_iface_filter_selections) {
						if (not filter_str.empty()) filter_str += " ";
						filter_str += iface;
					}
					Config::set("net_iface_filter", filter_str);
				}
				dropdown_open = false;
				dropdown_choices.clear();
				disk_filter_selections.clear();
				net_iface_filter_selections.clear();
				//? Clear screen and trigger full redraw to remove dropdown artifacts
				std::cout << Term::clear << std::flush;
				Global::resized = true;
			}
			else if (key == "enter" or key == "space") {
				if (is_multi_select) {
					//? Multi-select: toggle checkbox
					if (dropdown_selection < static_cast<int>(dropdown_choices.size())) {
						const string& selected_item = dropdown_choices[dropdown_selection];
						if (selected_item.empty()) {
							//? First item "(Show All)" - clear filter to show all
							if (is_disk_filter) disk_filter_selections.clear();
							if (is_net_filter) net_iface_filter_selections.clear();
						} else {
							//? Toggle item in show list
							auto& selections = is_disk_filter ? disk_filter_selections : net_iface_filter_selections;
							if (selections.contains(selected_item)) {
								selections.erase(selected_item);
							} else {
								selections.insert(selected_item);
							}
						}
						Menu::redraw = true;
					}
				} else {
					//? Other dropdowns: apply single selection
					const auto& cat = cats[selected_cat];
					if (selected_subcat < static_cast<int>(cat.subcats.size())) {
						const auto& subcat = cat.subcats[selected_subcat];
						if (selected_option < static_cast<int>(subcat.options.size())) {
							const auto& opt = subcat.options[selected_option];
							if (dropdown_selection < static_cast<int>(dropdown_choices.size())) {
								Config::set(opt.key, dropdown_choices[dropdown_selection]);
								// Trigger theme refresh for theme-related options
								if (opt.key == "color_theme") {
									theme_refresh = true;
								}
								// Sync clock_12h config when clock_format changes
								else if (opt.key == "clock_format") {
									const string& fmt = dropdown_choices[dropdown_selection];
									// 12-hour formats contain %I (hour) or %p (AM/PM)
									bool is_12h = (fmt.find("%I") != string::npos or fmt.find("%p") != string::npos);
									Config::set("clock_12h", is_12h);
								}
								Menu::redraw = true;  // Redraw to show new selection
							}
						}
					}
					dropdown_open = false;
					dropdown_choices.clear();
					//? Clear screen and trigger full redraw to remove dropdown artifacts
					std::cout << Term::clear << std::flush;
					Global::resized = true;
				}
			}
			else if (is_in(key, "down", "j", "mouse_scroll_down") or (vim_keys and key == "j")) {
				if (dropdown_selection < static_cast<int>(dropdown_choices.size()) - 1) {
					dropdown_selection++;
					Menu::redraw = true;
				}
			}
			else if (is_in(key, "up", "k", "mouse_scroll_up") or (vim_keys and key == "k")) {
				if (dropdown_selection > 0) {
					dropdown_selection--;
					Menu::redraw = true;
				}
			}
			//? Handle dropdown item click via mouse
			else if (key.starts_with("dropdown_item_")) {
				try {
					int item_idx = std::stoi(key.substr(14));
					if (item_idx < 0 or item_idx >= static_cast<int>(dropdown_choices.size())) {
						retval = Menu::NoChange;
					}
					else {
					dropdown_selection = item_idx;
					Menu::redraw = true;
					//? Single-click selects, so simulate enter for single-select dropdowns
					if (not is_disk_filter and not is_net_filter) {
						//? Apply single selection and close dropdown
						const auto& cat = cats[selected_cat];
						if (selected_subcat < static_cast<int>(cat.subcats.size())) {
							const auto& subcat = cat.subcats[selected_subcat];
							if (selected_option < static_cast<int>(subcat.options.size())) {
								const auto& opt = subcat.options[selected_option];
								Config::set(opt.key, dropdown_choices[dropdown_selection]);
								if (opt.key == "color_theme") {
									theme_refresh = true;
								}
								else if (opt.key == "clock_format") {
									const string& fmt = dropdown_choices[dropdown_selection];
									bool is_12h = (fmt.find("%I") != string::npos or fmt.find("%p") != string::npos);
									Config::set("clock_12h", is_12h);
								}
							}
						}
						dropdown_open = false;
						dropdown_choices.clear();
						std::cout << Term::clear << std::flush;
						Global::resized = true;
					}
					//? For multi-select dropdowns, just toggle on click
					else {
						const string& selected_item = dropdown_choices[dropdown_selection];
						if (selected_item.empty()) {
							if (is_disk_filter) disk_filter_selections.clear();
							if (is_net_filter) net_iface_filter_selections.clear();
						} else {
							auto& selections = is_disk_filter ? disk_filter_selections : net_iface_filter_selections;
							if (selections.contains(selected_item)) {
								selections.erase(selected_item);
							} else {
								selections.insert(selected_item);
							}
						}
					}
					}  // close else block for valid item_idx
				} catch (const std::exception&) {
					retval = Menu::NoChange;
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}
		else if (editing) {
			// Handle text editing
			if (is_in(key, "escape", "mouse_click")) {
				editor.clear();
				editing = false;
			}
			else if (key == "enter") {
				// Apply edited value
				const auto& cat = cats[selected_cat];
				if (selected_subcat < static_cast<int>(cat.subcats.size())) {
					const auto& subcat = cat.subcats[selected_subcat];
					if (selected_option < static_cast<int>(subcat.options.size())) {
						const auto& opt = subcat.options[selected_option];
						if (opt.control == ControlType::Text or opt.control == ControlType::Slider) {
							if (Config::stringValid(opt.key, editor.text)) {
								Config::set(opt.key, editor.text);
							}
						}
					}
				}
				editor.clear();
				editing = false;
			}
			else if (not editor.command(key)) {
				retval = Menu::NoChange;
			}
		}
		else {
			// Helper lambda to get current option
			auto getCurrentOption = [&]() -> const OptionDef* {
				const auto& cat = cats[selected_cat];
				if (selected_subcat < static_cast<int>(cat.subcats.size())) {
					const auto& subcat = cat.subcats[selected_subcat];
					if (selected_option < static_cast<int>(subcat.options.size())) {
						return &subcat.options[selected_option];
					}
				}
				return nullptr;
			};

			// Navigation handling
			if (is_in(key, "escape", "q", "backspace")) {
				// Just return - full app theme refresh happens when entire menu system closes
				return Menu::Closed;
			}
			//? Handle tab clicks FIRST - before category-specific handling
			else if (key.starts_with("tab_")) {
				int tab_idx = key.back() - '0';
				if (tab_idx >= 0 and tab_idx < static_cast<int>(cats.size())) {
					selected_cat = tab_idx;
					selected_subcat = 0;
					selected_option = 0;
					scroll_offset = 0;
					preset_button_focus = 0;  // Reset preset focus when switching tabs
					Menu::redraw = true;
				}
			}

			//? ==================== Presets Category Special Key Handling ====================
			else if (selected_cat == PRESETS_CATEGORY) {
				auto presets = getPresets();
				const int total_presets = static_cast<int>(presets.size());

				if (is_in(key, "up", "k", "mouse_scroll_up") or (vim_keys and key == "k")) {
					if (preset_button_focus == 0) {
						// Navigate preset list
						selected_preset = (selected_preset - 1 + total_presets) % total_presets;
					}
				}
				else if (is_in(key, "down", "j", "mouse_scroll_down") or (vim_keys and key == "j")) {
					if (preset_button_focus == 0) {
						// Navigate preset list
						selected_preset = (selected_preset + 1) % total_presets;
					}
				}
				else if (is_in(key, "left", "h") or (vim_keys and key == "h")) {
					// Move focus between list and buttons
					// Skip Delete (focus=2) if only one preset exists
					if (preset_button_focus > 0) {
						preset_button_focus--;
						if (preset_button_focus == 2 and total_presets <= 1) {
							preset_button_focus--;  // Skip Delete
						}
					}
				}
				else if (is_in(key, "right", "l") or (vim_keys and key == "l")) {
					// Move focus to buttons (0=list, 1=Edit, 2=Delete, 3=New)
					// Skip Delete (focus=2) if only one preset exists
					if (preset_button_focus < 3) {
						preset_button_focus++;
						if (preset_button_focus == 2 and total_presets <= 1) {
							preset_button_focus++;  // Skip Delete
						}
					}
				}
				else if (is_in(key, "enter", "space") or key == "\r" or key == "\n") {
					if (preset_button_focus == 0 or preset_button_focus == 1) {
						// Enter on list item or Edit button -> edit selected preset
						ns_in_preset_editor = true;
						ns_editing_preset_idx = selected_preset;  // Store which preset we're editing
						rebuildMenuBg();  // Ensure Menu::bg is ready for editor overlay
						return presetEditor("", selected_preset, x, y, dialog_width, dialog_height);
					}
					else if (preset_button_focus == 2) {
						// Delete selected preset (only if more than one preset exists)
						if (total_presets > 1) {
							auto presets_copy = presets;
							presets_copy.erase(presets_copy.begin() + selected_preset);
							savePresets(presets_copy);
							// Adjust selection if we deleted the last one
							if (selected_preset >= static_cast<int>(presets_copy.size())) {
								selected_preset = static_cast<int>(presets_copy.size()) - 1;
							}
							preset_button_focus = 0;  // Return focus to list
						}
					}
					else if (preset_button_focus == 3) {
						// New preset -> open editor with -1 to create new preset
						ns_in_preset_editor = true;
						ns_editing_preset_idx = -1;  // -1 means new preset
						rebuildMenuBg();
						return presetEditor("", -1, x, y, dialog_width, dialog_height);
					}
				}
				else if (key.size() == 1 and key[0] >= '1' and key[0] <= '9') {
					// Quick select preset 1-9
					int preset_num = key[0] - '1';
					if (preset_num < total_presets) {
						selected_preset = preset_num;
						preset_button_focus = 0;
					}
				}
				else if (key == "tab") {
					selected_cat = (selected_cat + 1) % static_cast<int>(cats.size());
					selected_subcat = 0;
					selected_option = 0;
					scroll_offset = 0;
					Menu::redraw = true;
				}
				else if (key == "shift_tab") {
					selected_cat = (selected_cat - 1 + static_cast<int>(cats.size())) % static_cast<int>(cats.size());
					selected_subcat = 0;
					selected_option = 0;
					scroll_offset = 0;
					Menu::redraw = true;
				}
				//? Handle preset list item click via mouse
				else if (key.starts_with("preset_")) {
					try {
						int preset_idx = std::stoi(key.substr(7));
						if (preset_idx >= 0 and preset_idx < total_presets) {
							selected_preset = preset_idx;
							preset_button_focus = 0;  // Focus on list
							Menu::redraw = true;
						}
					} catch (const std::exception&) {
						retval = Menu::NoChange;
					}
				}
				//? Handle button clicks via mouse
				else if (key == "btn_edit") {
					preset_button_focus = 1;
					ns_in_preset_editor = true;
					ns_editing_preset_idx = selected_preset;  // Store which preset we're editing
					rebuildMenuBg();
					return presetEditor("", selected_preset, x, y, dialog_width, dialog_height);
				}
				else if (key == "btn_delete") {
					if (total_presets > 1) {
						auto presets_copy = presets;
						presets_copy.erase(presets_copy.begin() + selected_preset);
						savePresets(presets_copy);
						if (selected_preset >= static_cast<int>(presets_copy.size())) {
							selected_preset = static_cast<int>(presets_copy.size()) - 1;
						}
						preset_button_focus = 0;
						Menu::redraw = true;
					}
				}
				else if (key == "btn_new") {
					preset_button_focus = 3;
					ns_in_preset_editor = true;
					ns_editing_preset_idx = -1;  // -1 means new preset
					rebuildMenuBg();
					return presetEditor("", -1, x, y, dialog_width, dialog_height);
				}
				else {
					retval = Menu::NoChange;
				}
			}
			//? ==================== End Presets Special Key Handling ====================

			else if (key == "tab") {
				selected_cat = (selected_cat + 1) % static_cast<int>(cats.size());
				selected_subcat = 0;
				selected_option = 0;
				scroll_offset = 0;
				Menu::redraw = true;
			}
			else if (key == "shift_tab") {
				selected_cat = (selected_cat - 1 + static_cast<int>(cats.size())) % static_cast<int>(cats.size());
				selected_subcat = 0;
				selected_option = 0;
				scroll_offset = 0;
				Menu::redraw = true;
			}
			else if (is_in(key, "right", "l") or (vim_keys and key == "l")) {
				// Right arrow: change Radio/Select/Slider value or switch tab if no option selected
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Find current value index and cycle to next
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx + 1) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Get dynamic choices and cycle to next
						auto choices = getDynamicChoices(opt->choices_ref);
						if (not choices.empty()) {
							string current = Config::getAsString(opt->key);
							int idx = 0;
							for (size_t i = 0; i < choices.size(); i++) {
								if (choices[i] == current) {
									idx = static_cast<int>(i);
									break;
								}
							}
							idx = (idx + 1) % static_cast<int>(choices.size());
							Config::set(opt->key, choices[idx]);
							// Trigger theme refresh for color_theme
							if (opt->key == "color_theme") {
								theme_refresh = true;
							}
						}
					}
					else if (opt->control == ControlType::MultiCheck and not opt->choices.empty()) {
						//? Move focus to next checkbox item (circular, skip disabled)
						int num_choices = static_cast<int>(opt->choices.size());
						int disabled_mask = 0;

						//? Get disabled mask for platform-aware options
						if (opt->key == "shown_gpus") {
							#if defined(__APPLE__) && defined(GPU_SUPPORT)
							bool is_apple_silicon = Gpu::apple_silicon_gpu.is_available();
							disabled_mask = is_apple_silicon ? 0b1110 : 0b0001;
							#else
							disabled_mask = 0b0001;
							#endif
						}

						//? Find next enabled item
						int start = multicheck_focus;
						do {
							multicheck_focus = (multicheck_focus + 1) % num_choices;
						} while ((disabled_mask & (1 << multicheck_focus)) and multicheck_focus != start);
					}
					else if (opt->control == ControlType::ToggleRow and not opt->choices.empty()) {
						//? Move focus to next toggle in row (circular)
						int num_choices = static_cast<int>(opt->choices.size());
						togglerow_focus = (togglerow_focus + 1) % num_choices;
					}
					else if (opt->control == ControlType::Slider) {
						int current = Config::getI(opt->key);
						int new_val = min(current + opt->step, opt->max_val);
						Config::set(opt->key, new_val);
					}
					else if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
							// Enable/disable mouse reporting immediately
							else if (opt->key == "disable_mouse") {
								const auto is_mouse_enabled = not Config::getB("disable_mouse");
								std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
							}
						}
					}
				}
			}
			else if (is_in(key, "left", "h") or (vim_keys and key == "h")) {
				// Left arrow: change Radio/Select/Slider value or switch tab if no option selected
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Find current value index and cycle to previous
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx - 1 + static_cast<int>(opt->choices.size())) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Get dynamic choices and cycle to previous
						auto choices = getDynamicChoices(opt->choices_ref);
						if (not choices.empty()) {
							string current = Config::getAsString(opt->key);
							int idx = 0;
							for (size_t i = 0; i < choices.size(); i++) {
								if (choices[i] == current) {
									idx = static_cast<int>(i);
									break;
								}
							}
							idx = (idx - 1 + static_cast<int>(choices.size())) % static_cast<int>(choices.size());
							Config::set(opt->key, choices[idx]);
							// Trigger theme refresh for color_theme
							if (opt->key == "color_theme") {
								theme_refresh = true;
							}
						}
					}
					else if (opt->control == ControlType::MultiCheck and not opt->choices.empty()) {
						//? Move focus to previous checkbox item (circular, skip disabled)
						int num_choices = static_cast<int>(opt->choices.size());
						int disabled_mask = 0;

						//? Get disabled mask for platform-aware options
						if (opt->key == "shown_gpus") {
							#if defined(__APPLE__) && defined(GPU_SUPPORT)
							bool is_apple_silicon = Gpu::apple_silicon_gpu.is_available();
							disabled_mask = is_apple_silicon ? 0b1110 : 0b0001;
							#else
							disabled_mask = 0b0001;
							#endif
						}

						//? Find previous enabled item
						int start = multicheck_focus;
						do {
							multicheck_focus = (multicheck_focus - 1 + num_choices) % num_choices;
						} while ((disabled_mask & (1 << multicheck_focus)) and multicheck_focus != start);
					}
					else if (opt->control == ControlType::ToggleRow and not opt->choices.empty()) {
						//? Move focus to previous toggle in row (circular)
						int num_choices = static_cast<int>(opt->choices.size());
						togglerow_focus = (togglerow_focus - 1 + num_choices) % num_choices;
					}
					else if (opt->control == ControlType::Slider) {
						int current = Config::getI(opt->key);
						int new_val = max(current - opt->step, opt->min_val);
						Config::set(opt->key, new_val);
					}
					else if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
							// Enable/disable mouse reporting immediately
							else if (opt->key == "disable_mouse") {
								const auto is_mouse_enabled = not Config::getB("disable_mouse");
								std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
							}
						}
					}
				}
			}
			else if (is_in(key, "down", "j", "mouse_scroll_down") or (vim_keys and key == "j")) {
				const auto& cat = cats[selected_cat];
				if (not cat.subcats.empty()) {
					int total_options = 0;
					for (const auto& sc : cat.subcats) {
						total_options += static_cast<int>(sc.options.size());
					}
					if (total_options > 0) {
						// Move to next option, wrapping subcategories
						selected_option++;
						const auto& subcat = cat.subcats[selected_subcat];
						if (selected_option >= static_cast<int>(subcat.options.size())) {
							selected_option = 0;
							selected_subcat = (selected_subcat + 1) % static_cast<int>(cat.subcats.size());
						}
					}
				}
			}
			else if (is_in(key, "up", "k", "mouse_scroll_up") or (vim_keys and key == "k")) {
				const auto& cat = cats[selected_cat];
				if (not cat.subcats.empty()) {
					selected_option--;
					if (selected_option < 0) {
						selected_subcat = (selected_subcat - 1 + static_cast<int>(cat.subcats.size())) % static_cast<int>(cat.subcats.size());
						const auto& subcat = cat.subcats[selected_subcat];
						selected_option = max(0, static_cast<int>(subcat.options.size()) - 1);
					}
				}
			}
			else if (is_in(key, "enter", "space")) {
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
							// Enable/disable mouse reporting immediately
							else if (opt->key == "disable_mouse") {
								const auto is_mouse_enabled = not Config::getB("disable_mouse");
								std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
							}
							//? Logs layout change requires size recalculation
							else if (opt->key == "logs_below_proc") {
								if (Logs::shown) {
									Draw::calcSizes();
									Global::resized = true;
								}
							}
						}
					}
					else if (opt->control == ControlType::Text) {
						editor = Draw::TextEdit{Config::getAsString(opt->key), false};
						editing = true;
					}
					else if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Cycle to next option on enter
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx + 1) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Open dropdown modal for Select
						dropdown_choices = getDynamicChoices(opt->choices_ref);
						if (not dropdown_choices.empty()) {
							string current = Config::getAsString(opt->key);
							dropdown_selection = 0;
							for (size_t i = 0; i < dropdown_choices.size(); i++) {
								if (dropdown_choices[i] == current) {
									dropdown_selection = static_cast<int>(i);
									break;
								}
							}
							dropdown_key = opt->key;  //? Track which option opened the dropdown
							dropdown_open = true;

							//? For disk filter, initialize multi-select state from config
							if (opt->key == "disks_filter") {
								disk_filter_selections.clear();
								string current_filter = Config::getS("disks_filter");
								if (not current_filter.empty()) {
									// Parse space-separated filter entries
									std::istringstream iss(current_filter);
									string disk_path;
									while (iss >> disk_path) {
										disk_filter_selections.insert(disk_path);
									}
								}
							}
							//? For net interface filter, initialize multi-select state from config
							else if (opt->key == "net_iface_filter") {
								net_iface_filter_selections.clear();
								string current_filter = Config::getS("net_iface_filter");
								if (not current_filter.empty()) {
									// Parse space-separated filter entries
									std::istringstream iss(current_filter);
									string iface_name;
									while (iss >> iface_name) {
										net_iface_filter_selections.insert(iface_name);
									}
								}
							}
						}
					}
					else if (opt->control == ControlType::MultiCheck and not opt->choices.empty()) {
						//? Toggle the focused checkbox item
						int disabled_mask = 0;

						//? Get disabled mask for platform-aware options
						if (opt->key == "shown_gpus") {
							#if defined(__APPLE__) && defined(GPU_SUPPORT)
							bool is_apple_silicon = Gpu::apple_silicon_gpu.is_available();
							disabled_mask = is_apple_silicon ? 0b1110 : 0b0001;
							#else
							disabled_mask = 0b0001;
							#endif
						}

						//? Only toggle if not disabled
						if (not (disabled_mask & (1 << multicheck_focus))) {
							string current = Config::getS(opt->key);
							string lower_choice = opt->choices[multicheck_focus];
							std::transform(lower_choice.begin(), lower_choice.end(), lower_choice.begin(), ::tolower);

							//? Toggle: add or remove from space-separated list
							if (current.find(lower_choice) != string::npos) {
								//? Remove the value
								size_t pos = current.find(lower_choice);
								current.erase(pos, lower_choice.size());
								//? Clean up extra spaces
								while (current.find("  ") != string::npos) {
									current.replace(current.find("  "), 2, " ");
								}
								if (not current.empty() and current.front() == ' ') current.erase(0, 1);
								if (not current.empty() and current.back() == ' ') current.pop_back();
							} else {
								//? Add the value
								if (current.empty()) {
									current = lower_choice;
								} else {
									current += " " + lower_choice;
								}
							}
							Config::set(opt->key, current);
						}
					}
					else if (opt->control == ControlType::ToggleRow and not opt->choices.empty()) {
						//? Toggle the focused item in the row
						//? Build config key: prefix + lowercase choice
						if (togglerow_focus >= 0 and togglerow_focus < static_cast<int>(opt->choices.size())) {
							string config_key = opt->choices_ref;
							string lower_choice = opt->choices[togglerow_focus];
							std::transform(lower_choice.begin(), lower_choice.end(), lower_choice.begin(), ::tolower);
							config_key += lower_choice;
							Config::flip(config_key);
						}
					}
				}
			}
			//? Handle close button click
			else if (key == "close_settings") {
				return Menu::Closed;
			}
			else if (key.starts_with("tab_")) {
				int tab_idx = key.back() - '0';
				if (tab_idx >= 0 and tab_idx < static_cast<int>(cats.size())) {
					selected_cat = tab_idx;
					selected_subcat = 0;
					selected_option = 0;
					scroll_offset = 0;
					Menu::redraw = true;
				}
			}
			//? Handle option row click via mouse (format: "opt_subcat_option")
			else if (key.starts_with("opt_")) {
				//? Parse "opt_X_Y" where X is subcat index, Y is option index
				size_t first_underscore = key.find('_', 4);
				if (first_underscore != string::npos) {
					try {
						int sc_idx = std::stoi(key.substr(4, first_underscore - 4));
						int opt_idx = std::stoi(key.substr(first_underscore + 1));
						const auto& cat = cats[selected_cat];
						if (sc_idx >= 0 and sc_idx < static_cast<int>(cat.subcats.size())) {
						const auto& subcat = cat.subcats[sc_idx];
						if (opt_idx >= 0 and opt_idx < static_cast<int>(subcat.options.size())) {
							selected_subcat = sc_idx;
							selected_option = opt_idx;
							Menu::redraw = true;

							//? For toggles, clicking toggles the value directly
							const auto& opt = subcat.options[opt_idx];
							if (opt.control == ControlType::Toggle) {
								if (isBoxToggleKey(opt.key)) {
									toggleBox(opt.key);
								} else {
									Config::flip(opt.key);
									if (is_in(opt.key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
										theme_refresh = true;
										if (opt.key == "truecolor") Config::flip("lowcolor");
										else if (opt.key == "force_tty") Config::flip("tty_mode");
									}
									else if (opt.key == "disable_mouse") {
										const auto is_mouse_enabled = not Config::getB("disable_mouse");
										std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
									}
									else if (opt.key == "logs_below_proc") {
										//? Logs layout change requires size recalculation
										if (Logs::shown) {
											Draw::calcSizes();
											Global::resized = true;
										}
									}
								}
							}
							//? For Select controls, open dropdown on click
							else if (opt.control == ControlType::Select and not opt.choices_ref.empty()) {
								dropdown_choices = getDynamicChoices(opt.choices_ref);
								dropdown_key = opt.key;
								if (not dropdown_choices.empty()) {
									dropdown_open = true;
									dropdown_selection = 0;
									//? Find current value's position in choices
									string current = Config::getS(opt.key);
									for (size_t i = 0; i < dropdown_choices.size(); i++) {
										if (dropdown_choices[i] == current) {
											dropdown_selection = static_cast<int>(i);
											break;
										}
									}

									//? For disk filter, initialize multi-select state from config
									if (opt.key == "disks_filter") {
										disk_filter_selections.clear();
										string current_filter = Config::getS("disks_filter");
										if (not current_filter.empty()) {
											std::istringstream iss(current_filter);
											string disk_path;
											while (iss >> disk_path) {
												disk_filter_selections.insert(disk_path);
											}
										}
									}
									//? For net interface filter, initialize multi-select state from config
									else if (opt.key == "net_iface_filter") {
										net_iface_filter_selections.clear();
										string current_filter = Config::getS("net_iface_filter");
										if (not current_filter.empty()) {
											std::istringstream iss(current_filter);
											string iface_name;
											while (iss >> iface_name) {
												net_iface_filter_selections.insert(iface_name);
											}
										}
									}
								}
							}
						}
						}
					} catch (const std::exception&) {
						retval = Menu::NoChange;
					}
				}
			}
			//? Handle slider arrow clicks (format: "slider_dec_key" or "slider_inc_key")
			else if (key.starts_with("slider_dec_")) {
				string opt_key = key.substr(11);  // Skip "slider_dec_"
				// Find the option first to validate key and get step value
				bool found = false;
				for (const auto& cat : cats) {
					if (found) break;
					for (const auto& subcat : cat.subcats) {
						if (found) break;
						for (const auto& opt : subcat.options) {
							if (opt.key == opt_key and opt.control == ControlType::Slider) {
								int current = Config::getI(opt_key);
								int new_val = max(current - opt.step, opt.min_val);
								Config::set(opt_key, new_val);
								found = true;
								break;
							}
						}
					}
				}
			}
			else if (key.starts_with("slider_inc_")) {
				string opt_key = key.substr(11);  // Skip "slider_inc_"
				// Find the option first to validate key and get step value
				bool found = false;
				for (const auto& cat : cats) {
					if (found) break;
					for (const auto& subcat : cat.subcats) {
						if (found) break;
						for (const auto& opt : subcat.options) {
							if (opt.key == opt_key and opt.control == ControlType::Slider) {
								int current = Config::getI(opt_key);
								int new_val = min(current + opt.step, opt.max_val);
								Config::set(opt_key, new_val);
								found = true;
								break;
							}
						}
					}
				}
			}
			//? Handle radio button clicks (format: "radio_key_N" where N is option index)
			else if (key.starts_with("radio_")) {
				// Parse key format: "radio_configkey_index"
				// Find the option first by matching known keys, then extract the index
				string remainder = key.substr(6);  // Skip "radio_"
				bool found = false;
				for (const auto& cat : cats) {
					if (found) break;
					for (const auto& subcat : cat.subcats) {
						if (found) break;
						for (const auto& opt : subcat.options) {
							if (opt.control == ControlType::Radio and remainder.starts_with(opt.key + "_")) {
								// Found matching option, extract index
								string idx_str = remainder.substr(opt.key.size() + 1);
								try {
									int choice_idx = std::stoi(idx_str);
									if (choice_idx >= 0 and choice_idx < static_cast<int>(opt.choices.size())) {
										Config::set(opt.key, opt.choices[choice_idx]);
										found = true;
									}
								} catch (...) {
									// Invalid index, ignore
								}
								break;
							}
						}
					}
				}
			}
			//? Handle ToggleRow clicks (format: "togglerow_prefix_N" where N is toggle index)
			else if (key.starts_with("togglerow_")) {
				// Parse key format: "togglerow_config_prefix_index"
				string remainder = key.substr(10);  // Skip "togglerow_"
				bool found = false;
				for (const auto& cat : cats) {
					if (found) break;
					for (const auto& subcat : cat.subcats) {
						if (found) break;
						for (const auto& opt : subcat.options) {
							if (opt.control == ControlType::ToggleRow and remainder.starts_with(opt.choices_ref)) {
								// Found matching option, extract index
								string idx_str = remainder.substr(opt.choices_ref.size());
								try {
									int toggle_idx = std::stoi(idx_str);
									if (toggle_idx >= 0 and toggle_idx < static_cast<int>(opt.choices.size())) {
										// Build config key and toggle it
										string config_key = opt.choices_ref;
										string lower_choice = opt.choices[toggle_idx];
										std::transform(lower_choice.begin(), lower_choice.end(), lower_choice.begin(), ::tolower);
										config_key += lower_choice;
										Config::flip(config_key);
										togglerow_focus = toggle_idx;
										found = true;
									}
								} catch (...) {
									// Invalid index, ignore
								}
								break;
							}
						}
					}
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}

		// Check theme_refresh AGAIN after key handling (in case user just changed theme)
		// DON'T apply during menu - only mark for full app refresh on exit
		if (theme_refresh) {
			theme_refresh = false;
			theme_changed_pending = true;         // Mark for full refresh when menu closes
		}

		// Draw content
		if (retval == Menu::Changed or Menu::redraw) {
			Config::unlock();  //? Flush any pending config changes from boolsTmp/intsTmp/stringsTmp to main maps
			out = Menu::bg;

			//? Clear dynamic mouse mappings (keep tab and close_settings mappings from rebuildMenuBg)
			std::erase_if(Menu::mouse_mappings, [](const auto& item) {
				return not item.first.starts_with("tab_") and item.first != "close_settings";
			});

			// Draw tab bar dynamically (so highlighting updates on category change)
			out += Mv::to(y + 2, x + 2);
			for (size_t i = 0; i < cats.size(); i++) {
				bool is_sel = (static_cast<int>(i) == selected_cat);
				string tab_text = cats[i].icon.empty() ? cats[i].name : cats[i].icon + " " + cats[i].name;
				out += (is_sel ? Theme::c("hi_fg") + Fx::b + "[" + Theme::c("title") : Theme::c("inactive_fg") + " ")
					+ tab_text
					+ (is_sel ? Theme::c("hi_fg") + "]" + Fx::ub : " ");
				out += "  ";
			}

			const auto& cat = cats[selected_cat];
			int content_y = y + 5;  // One line space after category tabs separator
			int content_x = x + 2;
			int content_width = dialog_width - 4;
			int max_lines = dialog_height - 10;  // Reserve space for help section at bottom

			//? ==================== Presets Category Special Rendering ====================
			if (selected_cat == PRESETS_CATEGORY) {
				auto presets = getPresets();

				// Draw preset list on left side
				const int list_width = 30;
				const int preview_x = content_x + list_width + 4;
				const int preview_width = content_width - list_width - 6;
				const int preview_height = max_lines - 4;

				// Header for preset list
				out += Mv::to(content_y, content_x) + Theme::c("title") + Fx::b
					+ Symbols::h_line + " Presets " + string(list_width - 11, '-') + Fx::ub;

				// Draw preset list
				for (int i = 0; i < static_cast<int>(presets.size()) and i < max_lines - 2; i++) {
					bool is_sel = (i == selected_preset and preset_button_focus == 0);
					const auto& preset = presets[i];
					string name = preset.name.empty() ? ("Preset " + to_string(i + 1)) : preset.name;

					// Truncate if too long
					if (static_cast<int>(name.size()) > list_width - 4) {
						name = name.substr(0, list_width - 7) + "...";
					}

					int preset_row_y = content_y + 2 + i;
					out += Mv::to(preset_row_y, content_x);
					if (is_sel) {
						out += Theme::c("selected_bg") + Theme::c("selected_fg");
					} else if (i == selected_preset) {
						out += Theme::c("hi_fg");  // Highlighted but not focused
					} else {
						out += Theme::c("main_fg");
					}

					out += to_string(i + 1) + ". " + ljust(name, list_width - 4);

					if (is_sel) {
						out += Fx::reset;
					}

					//? Mouse mapping for preset list item
					Menu::mouse_mappings["preset_" + to_string(i)] = {preset_row_y, content_x, 1, list_width};
				}

				// Draw buttons below the list
				int button_y = content_y + 2 + static_cast<int>(presets.size()) + 1;
				int button_x = content_x;
				out += Mv::to(button_y, button_x);

				// Edit button
				bool edit_sel = (preset_button_focus == 1);
				out += (edit_sel ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("hi_fg"))
					+ "[ Edit ]" + Fx::reset + "  ";
				Menu::mouse_mappings["btn_edit"] = {button_y, button_x, 1, 8};
				button_x += 10;  // "[ Edit ]" + "  "

				// Delete button (only shown if more than one preset exists)
				if (presets.size() > 1) {
					bool delete_sel = (preset_button_focus == 2);
					out += (delete_sel ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("hi_fg"))
						+ "[ Delete ]" + Fx::reset + "  ";
					Menu::mouse_mappings["btn_delete"] = {button_y, button_x, 1, 10};
					button_x += 12;  // "[ Delete ]" + "  "
				}

				// New button
				bool new_sel = (preset_button_focus == 3);
				out += (new_sel ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("hi_fg"))
					+ "[ New ]" + Fx::reset;
				Menu::mouse_mappings["btn_new"] = {button_y, button_x, 1, 7};

				// Draw preview on right side
				out += Mv::to(content_y, preview_x) + Theme::c("title") + Fx::b
					+ Symbols::h_line + " Preview " + string(preview_width - 11, '-') + Fx::ub;

				// Get the selected preset and draw preview
				const auto& selected = presets[selected_preset];
				string preview = drawPresetPreview(selected, preview_width - 2, preview_height);

				// Split preview into lines and position them
				int py = content_y + 2;
				string line;
				for (char c : preview) {
					if (c == '\n') {
						out += Mv::to(py++, preview_x + 1) + Theme::c("main_fg") + line;
						line.clear();
					} else {
						line += c;
					}
				}
				if (not line.empty()) {
					out += Mv::to(py, preview_x + 1) + Theme::c("main_fg") + line;
				}

				// Draw separator and help section
				out += Mv::to(y + dialog_height - 5, x) + Theme::c("hi_fg") + Symbols::div_left
					+ Theme::c("div_line");
				for (int i = 0; i < dialog_width - 2; i++) out += Symbols::h_line;
				out += Theme::c("hi_fg") + Symbols::div_right;

				// Help text
				string help_text = "Select a preset to preview, Edit to modify, or New to create";
				int help_x = x + (dialog_width - static_cast<int>(help_text.size())) / 2;
				out += Mv::to(y + dialog_height - 3, help_x) + Theme::c("inactive_fg") + help_text;

				// Navigation hints (centered)
				const int nav_help_len = 60;  // "Tab:Category  ↑↓:Select  ←→:Buttons  1-9:Quick  Enter:Action"
				int nav_help_x = x + (dialog_width - nav_help_len) / 2;
				out += Mv::to(y + dialog_height - 1, nav_help_x)
					+ Theme::c("hi_fg") + "Tab" + Theme::c("main_fg") + ":Category  "
					+ Theme::c("hi_fg") + "↑↓" + Theme::c("main_fg") + ":Select  "
					+ Theme::c("hi_fg") + "←→" + Theme::c("main_fg") + ":Buttons  "
					+ Theme::c("hi_fg") + "1-9" + Theme::c("main_fg") + ":Quick  "
					+ Theme::c("hi_fg") + "Enter" + Theme::c("main_fg") + ":Action";
			}
			//? ==================== End Presets Special Rendering ====================
			else {

			// First pass: calculate total lines and find selected item's line position
			int total_lines = 0;
			int selected_line = 0;
			int selected_header_line = 0;  // Track header line for selected subcategory
			for (size_t sc_idx = 0; sc_idx < cat.subcats.size(); sc_idx++) {
				const auto& subcat = cat.subcats[sc_idx];

				// Track header line for selected subcategory
				if (static_cast<int>(sc_idx) == selected_subcat) {
					selected_header_line = total_lines;
				}

				total_lines++;  // Subcategory header
				total_lines++;  // Spacing after header

				for (size_t opt_idx = 0; opt_idx < subcat.options.size(); opt_idx++) {
					if (static_cast<int>(sc_idx) == selected_subcat and
						static_cast<int>(opt_idx) == selected_option) {
						selected_line = total_lines;
					}
					total_lines++;
				}
				total_lines++;  // Spacing between subcategories
			}

			// Adjust scroll_offset to keep selection visible (including its header)
			if (selected_header_line < scroll_offset) {
				// Scroll up to show header when selected item's header is above view
				scroll_offset = selected_header_line;
			}
			else if (selected_line >= scroll_offset + max_lines) {
				scroll_offset = selected_line - max_lines + 1;
			}
			// Clamp scroll_offset to valid range
			scroll_offset = max(0, min(scroll_offset, max(0, total_lines - max_lines)));

			bool can_scroll_up = scroll_offset > 0;
			bool can_scroll_down = (scroll_offset + max_lines) < total_lines;

			int current_line = 0;
			int visible_line = 0;

			// Render subcategories and options (with scroll offset)
			for (size_t sc_idx = 0; sc_idx < cat.subcats.size() and visible_line < max_lines; sc_idx++) {
				const auto& subcat = cat.subcats[sc_idx];

				// Subcategory header
				if (current_line >= scroll_offset and visible_line < max_lines) {
					string line_fill;
					int fill_len = max(0, content_width - static_cast<int>(subcat.name.size()) - 4);
					for (int i = 0; i < fill_len; i++) line_fill += Symbols::h_line;
					out += Mv::to(content_y + visible_line, content_x)
						+ Theme::c("title") + Fx::b + Symbols::h_line + " " + subcat.name + " "
						+ line_fill
						+ Fx::ub;
					visible_line++;
				}
				current_line++;

				// Spacing after subcategory header
				if (current_line >= scroll_offset and visible_line < max_lines) {
					visible_line++;
				}
				current_line++;

				// Options in subcategory
				for (size_t opt_idx = 0; opt_idx < subcat.options.size() and visible_line < max_lines; opt_idx++) {
					if (current_line >= scroll_offset and visible_line < max_lines) {
						const auto& opt = subcat.options[opt_idx];
						bool is_selected = (static_cast<int>(sc_idx) == selected_subcat and
											static_cast<int>(opt_idx) == selected_option);

						string line;
						if (is_selected) {
							line += Theme::c("selected_bg") + Theme::c("selected_fg");
						} else {
							line += Theme::c("main_fg");
						}

						// Option label (left side)
						string label = ljust(opt.label, 25);

						// Option value (right side)
						string value_str;
						switch (opt.control) {
							case ControlType::Toggle: {
								bool val = isBoxToggleKey(opt.key) ? isBoxEnabled(opt.key) : Config::getB(opt.key);
								value_str = drawToggle(val, is_selected, tty_mode);
								break;
							}
							case ControlType::Radio: {
								int current_idx = 0;
								string current_val = Config::getS(opt.key);
								for (size_t i = 0; i < opt.choices.size(); i++) {
									if (opt.choices[i] == current_val) {
										current_idx = static_cast<int>(i);
										break;
									}
								}
								value_str = drawRadio(opt.choices, current_idx, is_selected, tty_mode);
								break;
							}
							case ControlType::Slider: {
								int current_val = Config::getI(opt.key);
								value_str = drawSlider(opt.min_val, opt.max_val, current_val, 20, is_selected, tty_mode);
								break;
							}
							case ControlType::Select: {
								string full_val = Config::getS(opt.key);
								string display_val = full_val;
								// For color_theme, show just the theme name without path
								if (opt.key == "color_theme") {
									display_val = fs::path(display_val).stem().string();
								}
								// For clock_format, show human-readable example
								else if (opt.key == "clock_format") {
									display_val = clockFormatToReadable(full_val);
								}
								// For empty values, show placeholder
								if (display_val.empty()) {
									display_val = "(auto)";
								}
								value_str = drawSelect(display_val, 16, is_selected, dropdown_open and is_selected);
								// Add color preview for theme selection
								if (opt.key == "color_theme") {
									value_str += " " + Theme::previewColors(full_val);
								}
								break;
							}
							case ControlType::Text:
								if (editing and is_selected) {
									value_str = "[" + editor(18) + "]";
								} else {
									string text_val = Config::getS(opt.key);
									if (text_val.empty()) text_val = "(empty)";
									value_str = "[" + ljust(text_val, 18) + "]";
								}
								break;
							case ControlType::MultiCheck: {
								//? Parse current value (space-separated string like "apple nvidia")
								string current = Config::getS(opt.key);
								int selected_mask = 0;
								int disabled_mask = 0;

								//? Check which options are selected
								for (size_t i = 0; i < opt.choices.size(); i++) {
									string lower_choice = opt.choices[i];
									std::transform(lower_choice.begin(), lower_choice.end(), lower_choice.begin(), ::tolower);
									if (current.find(lower_choice) != string::npos) {
										selected_mask |= (1 << i);
									}
								}

								//? Platform-aware disabled mask and selection for GPU vendors
								if (opt.key == "shown_gpus") {
									#if defined(__APPLE__) && defined(GPU_SUPPORT)
									bool is_apple_silicon = Gpu::apple_silicon_gpu.is_available();
									if (is_apple_silicon) {
										//? Apple Silicon: Only "Apple" is available (index 0)
										disabled_mask = 0b1110;  // NVIDIA(1), AMD(2), Intel(3) disabled
										//? Force correct display: only Apple should be selected
										selected_mask = 0b0001;  // Only Apple selected
									} else {
										//? Intel Mac: "Apple" is disabled
										disabled_mask = 0b0001;  // Apple(0) disabled
										//? Clear Apple selection if somehow set on Intel Mac
										selected_mask &= ~0b0001;
									}
									#else
									//? Non-Apple: "Apple" option disabled
									disabled_mask = 0b0001;  // Apple(0) disabled
									//? Clear Apple selection on non-Apple platforms
									selected_mask &= ~0b0001;
									#endif
								}

								value_str = drawMultiCheck(opt.choices, selected_mask,
									is_selected ? multicheck_focus : -1, disabled_mask, tty_mode);
								break;
							}
							case ControlType::ToggleRow: {
								//? ToggleRow: multiple toggles on single row
								//? opt.key = row label (e.g., "Mem")
								//? opt.choices = toggle labels ["Used", "Available", "Cached", "Free"]
								//? opt.choices_ref = config key prefix (e.g., "mem_show_")
								vector<bool> toggle_values;
								for (const auto& choice : opt.choices) {
									// Build config key: prefix + lowercase choice
									string config_key = opt.choices_ref;
									string lower_choice = choice;
									std::transform(lower_choice.begin(), lower_choice.end(), lower_choice.begin(), ::tolower);
									config_key += lower_choice;
									toggle_values.push_back(Config::getB(config_key));
								}
								value_str = drawToggleRow(opt.choices, toggle_values,
									is_selected ? togglerow_focus : -1, tty_mode);
								break;
							}
							default:
								value_str = Config::getAsString(opt.key);
								break;
						}

						line += label + value_str;
						if (is_selected) {
							line += Fx::reset;
						}

						int opt_row_y = content_y + visible_line;
						out += Mv::to(opt_row_y, content_x) + line;

						//? Mouse mapping for option row - click to select this option
						//? For sliders and radios, only map the label area to avoid conflict with control mappings
						const int label_width = 25;
						if (opt.control == ControlType::Slider) {
							// Only map label area for sliders to avoid conflict with arrow mappings
							Menu::mouse_mappings["opt_" + to_string(sc_idx) + "_" + to_string(opt_idx)] = {
								opt_row_y, content_x, 1, label_width
							};
							// Add slider-specific mouse mappings for arrows inside brackets [◀---▶]
							const int slider_width = 20;  // Must match value in drawSlider call
							// Left arrow (◀) at label_width + 1 (after '[')
							Menu::mouse_mappings["slider_dec_" + opt.key] = {
								opt_row_y, content_x + label_width + 1, 1, 1
							};
							// Right arrow (▶) at label_width + slider_width - 2 (before ']')
							Menu::mouse_mappings["slider_inc_" + opt.key] = {
								opt_row_y, content_x + label_width + slider_width - 2, 1, 1
							};
						} else if (opt.control == ControlType::Radio and not opt.choices.empty()) {
							// Only map label area for radios
							Menu::mouse_mappings["opt_" + to_string(sc_idx) + "_" + to_string(opt_idx)] = {
								opt_row_y, content_x, 1, label_width
							};
							// Add mouse mappings for each radio option: "● Label ○ Label2 ○ Label3"
							int radio_x = content_x + label_width;
							for (size_t ri = 0; ri < opt.choices.size(); ri++) {
								// Each option: symbol(1) + space(1) + text + space separator(1)
								int option_width = 2 + static_cast<int>(opt.choices[ri].size());
								Menu::mouse_mappings["radio_" + opt.key + "_" + to_string(ri)] = {
									opt_row_y, radio_x, 1, option_width
								};
								radio_x += option_width + 1;  // +1 for space between options
							}
						} else if (opt.control == ControlType::ToggleRow and not opt.choices.empty()) {
							// Only map label area for ToggleRow
							Menu::mouse_mappings["opt_" + to_string(sc_idx) + "_" + to_string(opt_idx)] = {
								opt_row_y, content_x, 1, label_width
							};
							// Add mouse mappings for each toggle: "[●] Used  [ ] Available ..."
							int toggle_x = content_x + label_width;
							for (size_t ti = 0; ti < opt.choices.size(); ti++) {
								// Each toggle: bracket(3) + space(1) + text + double-space separator(2)
								int toggle_width = 4 + static_cast<int>(opt.choices[ti].size());
								Menu::mouse_mappings["togglerow_" + opt.choices_ref + to_string(ti)] = {
									opt_row_y, toggle_x, 1, toggle_width
								};
								toggle_x += toggle_width + 2;  // +2 for double-space between toggles
							}
						} else {
							Menu::mouse_mappings["opt_" + to_string(sc_idx) + "_" + to_string(opt_idx)] = {
								opt_row_y, content_x, 1, content_width
							};
						}

						visible_line++;
					}
					current_line++;
				}

				// Spacing between subcategories
				if (current_line >= scroll_offset and visible_line < max_lines) {
					visible_line++;
				}
				current_line++;
			}

			// Separator line above help section with scroll indicators
			out += Mv::to(y + dialog_height - 5, x) + Theme::c("hi_fg") + Symbols::div_left
				+ Theme::c("div_line");
			for (int i = 0; i < dialog_width - 2; i++) out += Symbols::h_line;
			out += Theme::c("hi_fg") + Symbols::div_right;

			// Add scroll indicators on the right side of the separator
			if (can_scroll_up or can_scroll_down) {
				string scroll_indicator;
				if (can_scroll_up) scroll_indicator += "↑";
				if (can_scroll_down) scroll_indicator += "↓";
				int indicator_x = x + dialog_width - 3 - static_cast<int>(scroll_indicator.size());
				out += Mv::to(y + dialog_height - 5, indicator_x) + Theme::c("hi_fg") + scroll_indicator;
			}

			// Help text (centered) with spacing above and below
			if (selected_subcat < static_cast<int>(cat.subcats.size())) {
				const auto& subcat = cat.subcats[selected_subcat];
				if (selected_option < static_cast<int>(subcat.options.size())) {
					const auto& opt = subcat.options[selected_option];
					string help_text = opt.help;
					// Truncate if too long for dialog width
					int max_help_len = content_width;
					if (static_cast<int>(help_text.size()) > max_help_len) {
						help_text = help_text.substr(0, max_help_len - 3) + "...";
					}
					// Center the help text
					int help_x = x + (dialog_width - static_cast<int>(help_text.size())) / 2;
					out += Mv::to(y + dialog_height - 3, help_x)
						+ Theme::c("inactive_fg") + help_text;
				}
			}

			// Navigation hints at very bottom (centered)
			const int cat_help_len = 55;  // "Tab:Category  ↑↓:Navigate  Enter:Toggle/Edit  Esc:Close"
			int cat_help_x = x + (dialog_width - cat_help_len) / 2;
			out += Mv::to(y + dialog_height - 1, cat_help_x)
				+ Theme::c("hi_fg") + "Tab" + Theme::c("main_fg") + ":Category  "
				+ Theme::c("hi_fg") + "↑↓" + Theme::c("main_fg") + ":Navigate  "
				+ Theme::c("hi_fg") + "Enter" + Theme::c("main_fg") + ":Toggle/Edit  "
				+ Theme::c("hi_fg") + "Esc" + Theme::c("main_fg") + ":Close";

			} //? End of else block for non-Presets categories

			// Draw dropdown modal if open
			if (dropdown_open and not dropdown_choices.empty()) {
				const bool is_theme_dropdown = (dropdown_key == "color_theme");
				const bool is_disk_dropdown = (dropdown_key == "disks_filter");
				const bool is_net_dropdown = (dropdown_key == "net_iface_filter");
				const bool is_multi_select_dropdown = is_disk_dropdown or is_net_dropdown;
				const int dropdown_max_height = min(15, static_cast<int>(dropdown_choices.size()) + 2 + (is_multi_select_dropdown ? 1 : 0));
				//? Wider dropdown for themes and disks
				const int dropdown_width = is_theme_dropdown ? 58 : (is_disk_dropdown ? 95 : (is_net_dropdown ? 50 : 40));
				const int dropdown_x = x + (dialog_width - dropdown_width) / 2;
				const int dropdown_y = y + (dialog_height - dropdown_max_height) / 2;

				// Get dropdown title
				string dropdown_title = "Select Option";
				if (is_theme_dropdown) dropdown_title = "Select Theme";
				else if (is_disk_dropdown) dropdown_title = "Select Disks to Show";
				else if (is_net_dropdown) dropdown_title = "Select Interfaces to Show";

				// Draw dropdown background box
				out += Draw::createBox(dropdown_x, dropdown_y, dropdown_width, dropdown_max_height,
					Theme::c("hi_fg"), true, dropdown_title);

				// Add close button [×] in top-right corner of dropdown
				int dd_close_x = dropdown_x + dropdown_width - 5;
				out += Mv::to(dropdown_y, dd_close_x) + Theme::c("hi_fg") + "[" + Theme::c("inactive_fg") + "×" + Theme::c("hi_fg") + "]";
				Menu::mouse_mappings["close_dropdown"] = {dropdown_y, dd_close_x, 1, 3};

				// Get disk info map for disk dropdown - query system directly (not Mem::collect which filters)
				struct DiskDisplayInfo { string name; string device; };
				std::unordered_map<string, DiskDisplayInfo> disk_display_map;
				if (is_disk_dropdown) {
#if defined(__APPLE__)
					struct statfs *stfs = nullptr;
					int count = getmntinfo(&stfs, MNT_NOWAIT);
					for (int i = 0; i < count; i++) {
						string mountpoint = stfs[i].f_mntonname;
						string device = stfs[i].f_mntfromname;
						string fstype = stfs[i].f_fstypename;
						uint32_t flags = stfs[i].f_flags;

						// Skip system filesystems (same logic as getDynamicChoices)
						if (fstype == "autofs" or fstype == "devfs") continue;
						if (flags & MNT_DONTBROWSE) continue;

						// Extract display name from mountpoint
						string display_name;
						if (mountpoint.starts_with("/Volumes/"))
							display_name = fs::path(mountpoint).filename().string();
						else if (mountpoint == "/")
							display_name = "Macintosh HD";
						else
							display_name = mountpoint;

						disk_display_map[mountpoint] = {display_name, device};
					}
#elif defined(__linux__)
					FILE* mtab = setmntent("/proc/mounts", "r");
					if (mtab != nullptr) {
						struct mntent* entry;
						while ((entry = getmntent(mtab)) != nullptr) {
							string mountpoint = entry->mnt_dir;
							string device = entry->mnt_fsname;
							string fstype = entry->mnt_type;

							// Skip virtual filesystems
							if (fstype == "proc" or fstype == "sysfs" or fstype == "devtmpfs" or
							    fstype == "tmpfs" or fstype == "cgroup" or fstype == "cgroup2")
								continue;

							string display_name = fs::path(mountpoint).filename().string();
							if (display_name.empty()) display_name = mountpoint;

							disk_display_map[mountpoint] = {display_name, device};
						}
						endmntent(mtab);
					}
#endif
				}

				// Draw column headers for multi-select dropdowns
				int content_start_y = dropdown_y + 1;
				if (is_disk_dropdown) {
					out += Mv::to(content_start_y, dropdown_x + 2);
					out += Theme::c("title") + Fx::b;
					out += "Show " + ljust("Name", 18) + "  " + ljust("Mountpoint", 32) + "  " + ljust("Device", 20);
					out += Fx::ub + Fx::reset;
					content_start_y++;
				}
				else if (is_net_dropdown) {
					out += Mv::to(content_start_y, dropdown_x + 2);
					out += Theme::c("title") + Fx::b;
					out += "Show " + ljust("Interface", 40);
					out += Fx::ub + Fx::reset;
					content_start_y++;
				}

				// Calculate visible range for scrolling - keep cursor centered
				const int visible_items = dropdown_max_height - 2 - (is_multi_select_dropdown ? 1 : 0);
				const int total_items = static_cast<int>(dropdown_choices.size());
				const int half_visible = visible_items / 2;
				int scroll_start = 0;

				// Center-focused scrolling: selection stays in middle of visible area
				if (dropdown_selection > half_visible) {
					scroll_start = dropdown_selection - half_visible;
				}
				// Don't scroll past the end - cursor moves down when near end of list
				if (scroll_start + visible_items > total_items) {
					scroll_start = std::max(0, total_items - visible_items);
				}

				// Draw dropdown items
				const bool is_clock_dropdown = (dropdown_key == "clock_format");
				for (int i = 0; i < visible_items and (scroll_start + i) < static_cast<int>(dropdown_choices.size()); i++) {
					int item_idx = scroll_start + i;
					string item_full_path = dropdown_choices[item_idx];  //? Keep full path for theme preview
					string item_display = item_full_path;

					bool is_selected_item = (item_idx == dropdown_selection);
					int item_row_y = content_start_y + i;
					out += Mv::to(item_row_y, dropdown_x + 2);

					//? Mouse mapping for dropdown item
					Menu::mouse_mappings["dropdown_item_" + to_string(item_idx)] = {
						item_row_y, dropdown_x + 2, 1, dropdown_width - 4
					};

					if (is_disk_dropdown) {
						//? Disk dropdown: show Name | Mountpoint | Device columns
						string disk_name = "(No Filter)";
						string disk_mount = "";
						string disk_dev = "";

						if (not item_full_path.empty() and disk_display_map.contains(item_full_path)) {
							const auto& disk = disk_display_map.at(item_full_path);
							disk_name = disk.name.empty() ? "(unnamed)" : disk.name;
							disk_mount = item_full_path;
							disk_dev = disk.device;
						} else if (item_full_path.empty()) {
							disk_name = "(Show All Disks)";
							disk_mount = "";
							disk_dev = "";
						} else {
							// Fallback if not found in map
							disk_name = fs::path(item_full_path).filename().string();
							if (disk_name.empty()) disk_name = item_full_path;
							disk_mount = item_full_path;
						}

						// Truncate long values (reduced by 4 to make room for checkbox)
						if (disk_name.size() > 18) disk_name = disk_name.substr(0, 15) + "...";
						if (disk_mount.size() > 32) disk_mount = disk_mount.substr(0, 29) + "...";
						if (disk_dev.size() > 20) disk_dev = disk_dev.substr(0, 17) + "...";

						//? Determine checkbox state - checked means disk will be shown
						bool is_checked = disk_filter_selections.contains(item_full_path);
						string checkbox = is_checked ? "●" : "○";
						//? First item "(Show All)" has no checkbox
						if (item_full_path.empty()) checkbox = " ";

						if (is_selected_item) {
							out += Theme::c("hi_fg") + Fx::b + "> ";
						} else {
							out += Theme::c("main_fg") + "  ";
						}
						out += checkbox + " " + ljust(disk_name, 18) + "  " + ljust(disk_mount, 32) + "  " + ljust(disk_dev, 20);
						if (is_selected_item) out += Fx::ub;
					}
					else if (is_net_dropdown) {
						//? Network interface dropdown: show Interface name with checkbox
						string iface_name = item_full_path.empty() ? "(Show All Interfaces)" : item_full_path;

						//? Determine checkbox state - checked means interface will be shown
						bool is_checked = net_iface_filter_selections.contains(item_full_path);
						string checkbox = is_checked ? "●" : "○";
						//? First item "(Show All)" has no checkbox
						if (item_full_path.empty()) checkbox = " ";

						if (is_selected_item) {
							out += Theme::c("hi_fg") + Fx::b + "> ";
						} else {
							out += Theme::c("main_fg") + "  ";
						}
						out += checkbox + " " + ljust(iface_name, 40);
						if (is_selected_item) out += Fx::ub;
					}
					else {
						// For color_theme, show just the theme name
						if (item_display.find('/') != string::npos) {
							item_display = fs::path(item_display).stem().string();
						}
						// For clock_format, show human-readable examples
						else if (is_clock_dropdown) {
							item_display = clockFormatToReadable(item_full_path);
						}

						// Empty value shows as "(auto)" or "(none)"
						if (item_display.empty()) {
							item_display = "(auto)";
						}

						// For theme dropdown, reserve space for color preview (8×2 chars + 7 spaces + 1 leading = 24)
						const int preview_width = is_theme_dropdown ? 24 : 0;
						const int max_name_width = dropdown_width - 6 - preview_width;

						// Truncate if too long
						if (static_cast<int>(item_display.size()) > max_name_width) {
							item_display = item_display.substr(0, max_name_width - 3) + "...";
						}

						if (is_selected_item) {
							out += Theme::c("hi_fg") + Fx::b + "> " + ljust(item_display, max_name_width);
						} else {
							out += Theme::c("main_fg") + "  " + ljust(item_display, max_name_width);
						}

						// Add color preview for theme dropdown
						if (is_theme_dropdown) {
							out += " " + Theme::previewColors(item_full_path);
						}

						if (is_selected_item) {
							out += Fx::ub;
						}
					}
				}

				// Show scroll indicators if needed
				if (scroll_start > 0) {
					out += Mv::to(dropdown_y, dropdown_x + dropdown_width - 4) + Theme::c("hi_fg") + "↑";
				}
				if (scroll_start + visible_items < static_cast<int>(dropdown_choices.size())) {
					out += Mv::to(dropdown_y + dropdown_max_height - 1, dropdown_x + dropdown_width - 4) + Theme::c("hi_fg") + "↓";
				}
			}

			out += Fx::reset;
		}

		return (Menu::redraw ? Menu::Changed : retval);
	}

	//? ==================== Preset Editor ====================
	//? Clean implementation matching the 13 layout model
	//?
	//? Fields:
	//?   0. Name (text)
	//?   1. CPU (toggle), 2. GPU (toggle), 3. PWR (toggle)
	//?   4. MEM enabled (toggle)
	//?   5. MEM Type (radio: H, V) - if MEM enabled
	//?   6. MEM Graph (radio: Bar, Meter) - if MEM enabled AND type=V
	//?   7. Show Disk (toggle) - if MEM enabled AND type=V
	//?   8. NET enabled (toggle)
	//?   9. NET Position (radio: Left, Right) - if NET enabled AND MEM enabled
	//?   10. PROC enabled (toggle)
	//?   11. PROC Position (radio: Right, Wide) - if PROC enabled
	//?   12. Graph Symbol (radio)
	//?   13. Save, 14. Cancel

	int presetEditor(const string& key, int preset_idx, int parent_x, int parent_y, int parent_w, int parent_h) {
		// Static state
		static PresetDef current_preset;
		static int selected_field = 0;
		static bool initialized = false;
		static Draw::TextEdit name_editor;
		static bool editing_name = false;
		static int radio_focus = 0;
		static int togglerow_editor_focus = 0;  //? Focus index within ToggleRow fields

		auto tty_mode = Config::getB("tty_mode");

		// Field enumeration
		enum Fields {
			Name = 0,
			CPU, CPU_Pos, GPU, GPU_Pos, PWR, PWR_Pos,
			MEM, MEM_Type, MEM_Graph, Show_Disk,
			Mem_Charts, Swap_Charts, Vram_Charts,  //? Memory chart visibility toggles
			NET, NET_Pos, NET_Dir,  //? NET_Dir: graph direction (RTL, LTR, TTB, BTT)
			PROC, PROC_Pos,
			GraphSymbol,
			Save, Cancel
		};
		const int total_fields = 22;  //? Updated for NET_Dir field

		// Initialize on first call
		if (not initialized) {
			auto presets = getPresets();
			if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
				current_preset = presets[preset_idx];
			} else {
				current_preset = PresetDef{};
				current_preset.name = "New-Preset";
			}
			initialized = true;
			selected_field = 0;
			radio_focus = 0;
			Menu::redraw = true;
		}

		// ============ HELPER FUNCTIONS ============

		// Check if field is editable based on current state
		auto isFieldEditable = [&](int field) -> bool {
			switch (field) {
				case CPU_Pos:
					return current_preset.cpu_enabled;
				case GPU_Pos:
					return current_preset.gpu_enabled;
				case PWR_Pos:
					return current_preset.pwr_enabled;
				case MEM_Type:
				case MEM_Graph:
				case Show_Disk:
				case Mem_Charts:
				case Swap_Charts:
				case Vram_Charts:
					return current_preset.mem_enabled;
				case NET_Pos:
					return current_preset.net_enabled && current_preset.mem_enabled;
				case NET_Dir:
					return current_preset.net_enabled;  // Direction always editable when NET enabled
				case PROC_Pos:
					return current_preset.proc_enabled;
				case GraphSymbol:
					return true;  // Global graph style - always editable
				default:
					return true;
			}
		};

		// Additional constraint: MEM_Graph and Show_Disk only if type=V
		auto isMemOptionEditable = [&](int field) -> bool {
			if (not current_preset.mem_enabled) return false;
			if (field == MEM_Graph or field == Show_Disk) {
				return current_preset.mem_type == MemType::Vertical;
			}
			return true;
		};

		// Check if field is a radio type
		auto isRadioField = [](int field) -> bool {
			return field == CPU_Pos or field == GPU_Pos or field == PWR_Pos or
			       field == MEM_Type or field == MEM_Graph or
			       field == NET_Pos or field == NET_Dir or field == PROC_Pos or field == GraphSymbol;
		};

		//? Check if field is a ToggleRow type (multiple toggles on one row)
		auto isToggleRowField = [](int field) -> bool {
			return field == Mem_Charts or field == Swap_Charts or field == Vram_Charts;
		};

		//? Get number of toggles in a ToggleRow field
		auto getToggleRowCount = [](int field) -> int {
			switch (field) {
				case Mem_Charts: return 4;   // Used, Available, Cached, Free
				case Swap_Charts: return 2;  // Used, Free
				case Vram_Charts: return 2;  // Used, Free
				default: return 0;
			}
		};

		//? Get toggle labels for a ToggleRow field
		auto getToggleRowLabels = [](int field) -> vector<string> {
			switch (field) {
				case Mem_Charts: return {"Used", "Available", "Cached", "Free"};
				case Swap_Charts: return {"Used", "Free"};
				case Vram_Charts: return {"Used", "Free"};
				default: return {};
			}
		};

		// Get number of options for radio field
		auto getRadioMaxOptions = [](int field) -> int {
			switch (field) {
				case CPU_Pos: return 2;     // Top, Bottom
				case GPU_Pos: return 2;     // Top, Bottom
				case PWR_Pos: return 2;     // Top, Bottom
				case MEM_Type: return 2;    // H, V
				case MEM_Graph: return 2;   // Bar, Meter
				case NET_Pos: return 2;     // Left, Right
				case NET_Dir: return 4;     // ◀, ▶, ▼, ▲
				case PROC_Pos: return 2;    // Right, Wide
				case GraphSymbol: return 4; // default, braille, block, tty
				default: return 0;
			}
		};

		// Get current radio value
		auto getRadioValue = [&](int field) -> int {
			switch (field) {
				case CPU_Pos: return current_preset.cpu_bottom ? 1 : 0;
				case GPU_Pos: return current_preset.gpu_bottom ? 1 : 0;
				case PWR_Pos: return current_preset.pwr_bottom ? 1 : 0;
				case MEM_Type: return static_cast<int>(current_preset.mem_type);
				case MEM_Graph: return current_preset.mem_graph_meter ? 1 : 0;
				case NET_Pos: return static_cast<int>(current_preset.net_position);
				case NET_Dir: return current_preset.net_graph_direction;
				case PROC_Pos: return static_cast<int>(current_preset.proc_position);
				case GraphSymbol: {
					vector<string> syms = {"default", "braille", "block", "tty"};
					for (size_t i = 0; i < syms.size(); i++) {
						if (syms[i] == current_preset.graph_symbol) return static_cast<int>(i);
					}
					return 0;
				}
				default: return 0;
			}
		};

		// Set radio value
		auto setRadioValue = [&](int field, int val) {
			switch (field) {
				case CPU_Pos:
					current_preset.cpu_bottom = (val == 1);
					break;
				case GPU_Pos:
					current_preset.gpu_bottom = (val == 1);
					break;
				case PWR_Pos:
					current_preset.pwr_bottom = (val == 1);
					break;
				case MEM_Type:
					current_preset.mem_type = static_cast<MemType>(val);
					//? Auto-deselect show_disk when switching to Horizontal (no room for disk display)
					if (current_preset.mem_type == MemType::Horizontal) {
						current_preset.show_disk = false;
					}
					break;
				case MEM_Graph:
					current_preset.mem_graph_meter = (val == 1);
					break;
				case NET_Pos:
					current_preset.net_position = static_cast<NetPosition>(val);
					break;
				case NET_Dir:
					current_preset.net_graph_direction = val;
					break;
				case PROC_Pos:
					current_preset.proc_position = static_cast<ProcPosition>(val);
					break;
				case GraphSymbol: {
					vector<string> syms = {"default", "braille", "block", "tty"};
					if (val >= 0 and val < static_cast<int>(syms.size())) {
						current_preset.graph_symbol = syms[val];
					}
					break;
				}
			}
		};

		// Skip to next valid field
		auto nextValidField = [&](int dir) {
			int attempts = 0;
			do {
				selected_field = (selected_field + dir + total_fields) % total_fields;
				attempts++;
				// Skip disabled fields
				if (not isFieldEditable(selected_field)) continue;
				if ((selected_field == MEM_Graph or selected_field == Show_Disk) and
				    not isMemOptionEditable(selected_field)) continue;
				break;
			} while (attempts < total_fields);

			if (isRadioField(selected_field)) {
				radio_focus = getRadioValue(selected_field);
			}
			if (isToggleRowField(selected_field)) {
				togglerow_editor_focus = 0;  // Reset to first toggle when entering a ToggleRow
			}
		};

		// ============ DIALOG LAYOUT - 2 COLUMN DESIGN ============
		const int margin = 2;
		const int max_width = parent_w - margin * 2;
		const int max_height = parent_h - margin * 2;
		// Use more horizontal space for 2-column layout
		const int dialog_width = std::min(100, std::max(80, max_width));
		const int dialog_height = std::max(22, std::min(28, max_height));
		const int x = parent_x + (parent_w - dialog_width) / 2;
		const int y = parent_y + (parent_h - dialog_height) / 2;

		// Column layout: left for options, right for preview
		const int left_col_width = 42;  // Options column
		const int right_col_start = x + left_col_width + 2;
		const int right_col_width = dialog_width - left_col_width - 4;

		auto& out = Global::overlay;
		int retval = Menu::Changed;

		// ============ KEY HANDLING ============
		if (editing_name) {
			if (is_in(key, "escape", "enter") or key == "\r" or key == "\n") {
				if (key == "enter" or key == "\r" or key == "\n") {
					//? Substitute spaces with dashes on save (spaces break config parsing)
					string safe_name = name_editor.text;
					std::replace(safe_name.begin(), safe_name.end(), ' ', '-');
					current_preset.name = safe_name;
				}
				name_editor.clear();
				editing_name = false;
			}
			//? Handle Save/Cancel clicks while editing name - save name first, then process action
			else if (key == "editor_save") {
				//? Save the name being edited
				string safe_name = name_editor.text;
				std::replace(safe_name.begin(), safe_name.end(), ' ', '-');
				current_preset.name = safe_name;
				name_editor.clear();
				editing_name = false;
				//? Now save the preset
				auto presets = getPresets();
				int new_preset_idx = preset_idx;
				if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
					presets[preset_idx] = current_preset;
				} else {
					presets.push_back(current_preset);
					new_preset_idx = static_cast<int>(presets.size()) - 1;
				}
				savePresets(presets);
				Config::current_preset = new_preset_idx;
				Config::apply_preset(current_preset.toConfigString());
				Draw::calcSizes();
				Global::resized = true;
				Runner::run("all", false, true);
				initialized = false;
				return Menu::Closed;
			}
			else if (key == "editor_cancel" or key == "close_preset_editor") {
				//? Cancel without saving - discard name edit
				name_editor.clear();
				editing_name = false;
				initialized = false;
				return Menu::Closed;
			}
			else {
				//? Substitute spaces with dashes while typing
				string input_key = (key == " ") ? "-" : key;
				name_editor.command(input_key);
			}
		}
		else {
			if (is_in(key, "escape", "q")) {
				initialized = false;
				return Menu::Closed;
			}
			else if (is_in(key, "down", "j", "tab")) {
				nextValidField(1);
			}
			else if (is_in(key, "up", "k", "shift_tab")) {
				nextValidField(-1);
			}
			else if (is_in(key, "left", "h")) {
				if (isRadioField(selected_field) and isFieldEditable(selected_field)) {
					int max_opts = getRadioMaxOptions(selected_field);
					radio_focus = (radio_focus - 1 + max_opts) % max_opts;
					setRadioValue(selected_field, radio_focus);
					current_preset.enforceConstraints();
				}
				else if (isToggleRowField(selected_field) and current_preset.mem_enabled) {
					int max_toggles = getToggleRowCount(selected_field);
					togglerow_editor_focus = (togglerow_editor_focus - 1 + max_toggles) % max_toggles;
				}
			}
			else if (is_in(key, "right", "l")) {
				if (isRadioField(selected_field) and isFieldEditable(selected_field)) {
					int max_opts = getRadioMaxOptions(selected_field);
					radio_focus = (radio_focus + 1) % max_opts;
					setRadioValue(selected_field, radio_focus);
					current_preset.enforceConstraints();
				}
				else if (isToggleRowField(selected_field) and current_preset.mem_enabled) {
					int max_toggles = getToggleRowCount(selected_field);
					togglerow_editor_focus = (togglerow_editor_focus + 1) % max_toggles;
				}
			}
			else if (is_in(key, "enter", "space") or key == "\r" or key == "\n") {
				switch (selected_field) {
					case Name:
						name_editor = Draw::TextEdit{current_preset.name, false};
						editing_name = true;
						break;
					case CPU:
						current_preset.cpu_enabled = not current_preset.cpu_enabled;
						break;
					case GPU:
						current_preset.gpu_enabled = not current_preset.gpu_enabled;
						break;
					case PWR:
						current_preset.pwr_enabled = not current_preset.pwr_enabled;
						break;
					case MEM:
						current_preset.mem_enabled = not current_preset.mem_enabled;
						current_preset.enforceConstraints();
						break;
					case Show_Disk:
						if (isMemOptionEditable(Show_Disk)) {
							current_preset.show_disk = not current_preset.show_disk;
						}
						break;
					case Mem_Charts:
					case Swap_Charts:
					case Vram_Charts:
						//? Toggle the focused toggle within the ToggleRow (modifies preset, not live config)
						if (current_preset.mem_enabled) {
							if (selected_field == Mem_Charts) {
								switch (togglerow_editor_focus) {
									case 0: current_preset.mem_show_used = not current_preset.mem_show_used; break;
									case 1: current_preset.mem_show_available = not current_preset.mem_show_available; break;
									case 2: current_preset.mem_show_cached = not current_preset.mem_show_cached; break;
									case 3: current_preset.mem_show_free = not current_preset.mem_show_free; break;
								}
							} else if (selected_field == Swap_Charts) {
								switch (togglerow_editor_focus) {
									case 0: current_preset.swap_show_used = not current_preset.swap_show_used; break;
									case 1: current_preset.swap_show_free = not current_preset.swap_show_free; break;
								}
							} else if (selected_field == Vram_Charts) {
								switch (togglerow_editor_focus) {
									case 0: current_preset.vram_show_used = not current_preset.vram_show_used; break;
									case 1: current_preset.vram_show_free = not current_preset.vram_show_free; break;
								}
							}
						}
						break;
					case NET:
						current_preset.net_enabled = not current_preset.net_enabled;
						current_preset.enforceConstraints();
						break;
					case PROC:
						current_preset.proc_enabled = not current_preset.proc_enabled;
						current_preset.enforceConstraints();
						break;
					case CPU_Pos:
					case GPU_Pos:
					case PWR_Pos:
					case MEM_Type:
					case MEM_Graph:
					case NET_Pos:
					case PROC_Pos:
					case GraphSymbol:
						// Radio fields: Enter moves to next field
						nextValidField(1);
						break;
					case Save: {
						auto presets = getPresets();
						int new_preset_idx = preset_idx;
						if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
							presets[preset_idx] = current_preset;
						} else {
							presets.push_back(current_preset);
							new_preset_idx = static_cast<int>(presets.size()) - 1;  // Index of newly added preset
						}
						savePresets(presets);

						Config::current_preset = new_preset_idx;
						Config::apply_preset(current_preset.toConfigString());
						Draw::calcSizes();
						Global::resized = true;
						Runner::run("all", false, true);

						initialized = false;
						return Menu::Closed;
					}
					case Cancel:
						initialized = false;
						return Menu::Closed;
				}
			}
			//? Handle mouse click on Save button
			else if (key == "editor_save") {
				auto presets = getPresets();
				int new_preset_idx = preset_idx;
				if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
					presets[preset_idx] = current_preset;
				} else {
					presets.push_back(current_preset);
					new_preset_idx = static_cast<int>(presets.size()) - 1;  // Index of newly added preset
				}
				savePresets(presets);

				Config::current_preset = new_preset_idx;
				Config::apply_preset(current_preset.toConfigString());
				Draw::calcSizes();
				Global::resized = true;
				Runner::run("all", false, true);

				initialized = false;
				return Menu::Closed;
			}
			//? Handle mouse click on Cancel button or close button [×]
			else if (key == "editor_cancel" or key == "close_preset_editor") {
				initialized = false;
				return Menu::Closed;
			}
			//? Handle mouse click on field rows (pe_field_N)
			else if (key.starts_with("pe_field_")) {
				try {
					int clicked_field = std::stoi(key.substr(9));  // "pe_field_".length() == 9
					if (clicked_field >= 0 and clicked_field < total_fields) {
						// Check if field is editable
						bool can_select = isFieldEditable(clicked_field);
						if ((clicked_field == MEM_Graph or clicked_field == Show_Disk) and not isMemOptionEditable(clicked_field)) {
							can_select = false;
						}

						if (can_select) {
							selected_field = clicked_field;

							// For toggle fields, also toggle on click
							switch (clicked_field) {
								case CPU:
									current_preset.cpu_enabled = not current_preset.cpu_enabled;
									break;
								case GPU:
									current_preset.gpu_enabled = not current_preset.gpu_enabled;
									break;
								case PWR:
									current_preset.pwr_enabled = not current_preset.pwr_enabled;
									break;
								case MEM:
									current_preset.mem_enabled = not current_preset.mem_enabled;
									current_preset.enforceConstraints();
									break;
								case Show_Disk:
									current_preset.show_disk = not current_preset.show_disk;
									break;
								case NET:
									current_preset.net_enabled = not current_preset.net_enabled;
									current_preset.enforceConstraints();
									break;
								case PROC:
									current_preset.proc_enabled = not current_preset.proc_enabled;
									current_preset.enforceConstraints();
									break;
								case Name:
									// Enter edit mode for name field
									name_editor = Draw::TextEdit{current_preset.name, false};
									editing_name = true;
									break;
								case Save: {
									// Handle Save button click (same as editor_save)
									auto presets = getPresets();
									int new_preset_idx = preset_idx;
									if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
										presets[preset_idx] = current_preset;
									} else {
										presets.push_back(current_preset);
										new_preset_idx = static_cast<int>(presets.size()) - 1;  // Index of newly added preset
									}
									savePresets(presets);

									Config::current_preset = new_preset_idx;
									Config::apply_preset(current_preset.toConfigString());
									Draw::calcSizes();
									Global::resized = true;
									Runner::run("all", false, true);

									initialized = false;
									return Menu::Closed;
								}
								case Cancel:
									// Handle Cancel button click (same as editor_cancel)
									initialized = false;
									return Menu::Closed;
								default:
									// For radio fields, just select (use left/right to change)
									if (isRadioField(clicked_field)) {
										radio_focus = getRadioValue(clicked_field);
									}
									break;
							}
							retval = Menu::Changed;
						} else {
							retval = Menu::NoChange;
						}
					} else {
						retval = Menu::NoChange;
					}
				} catch (...) {
					retval = Menu::NoChange;
				}
			}
			//? Handle mouse click on radio options (pe_radio_FIELD_N)
			else if (key.starts_with("pe_radio_")) {
				try {
					string remainder = key.substr(9);  // Skip "pe_radio_"
					size_t underscore = remainder.find('_');
					if (underscore != string::npos) {
						int field = std::stoi(remainder.substr(0, underscore));
						int option_idx = std::stoi(remainder.substr(underscore + 1));

						// Verify field is editable
						bool can_edit = isFieldEditable(field);
						if ((field == MEM_Graph or field == Show_Disk) and not isMemOptionEditable(field)) {
							can_edit = false;
						}

						if (can_edit and isRadioField(field)) {
							selected_field = field;
							radio_focus = option_idx;
							setRadioValue(field, option_idx);
							retval = Menu::Changed;
						} else {
							retval = Menu::NoChange;
						}
					}
				} catch (...) {
					retval = Menu::NoChange;
				}
			}
			//? Handle mouse click on ToggleRow toggles (pe_togglerow_FIELD_N)
			else if (key.starts_with("pe_togglerow_")) {
				try {
					string remainder = key.substr(13);  // Skip "pe_togglerow_"
					size_t underscore = remainder.find('_');
					if (underscore != string::npos) {
						int field = std::stoi(remainder.substr(0, underscore));
						int toggle_idx = std::stoi(remainder.substr(underscore + 1));

						// Verify ToggleRow field is editable (MEM must be enabled)
						if (isToggleRowField(field) and current_preset.mem_enabled) {
							selected_field = field;
							togglerow_editor_focus = toggle_idx;

							// Toggle the clicked toggle (modifies preset, not live config)
							if (field == Mem_Charts) {
								switch (toggle_idx) {
									case 0: current_preset.mem_show_used = not current_preset.mem_show_used; break;
									case 1: current_preset.mem_show_available = not current_preset.mem_show_available; break;
									case 2: current_preset.mem_show_cached = not current_preset.mem_show_cached; break;
									case 3: current_preset.mem_show_free = not current_preset.mem_show_free; break;
								}
							} else if (field == Swap_Charts) {
								switch (toggle_idx) {
									case 0: current_preset.swap_show_used = not current_preset.swap_show_used; break;
									case 1: current_preset.swap_show_free = not current_preset.swap_show_free; break;
								}
							} else if (field == Vram_Charts) {
								switch (toggle_idx) {
									case 0: current_preset.vram_show_used = not current_preset.vram_show_used; break;
									case 1: current_preset.vram_show_free = not current_preset.vram_show_free; break;
								}
							}
							retval = Menu::Changed;
						} else {
							retval = Menu::NoChange;
						}
					}
				} catch (...) {
					retval = Menu::NoChange;
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}

		// ============ RENDERING - 2 COLUMN LAYOUT ============
		// NOTE: Preset editor ALWAYS renders to ensure mouse mappings stay valid
		// This is acceptable for a modal dialog with limited complexity
		{
			out = Menu::bg;

			// Draw blank mask around the dialog (1 char padding for visibility)
			const int mask_padding = 1;
			int mask_x = std::max(1, x - mask_padding);
			int mask_y = std::max(1, y - mask_padding);
			int mask_width = std::min(dialog_width + mask_padding * 2, Term::width - mask_x + 1);
			int mask_height = std::min(dialog_height + mask_padding * 2, Term::height - mask_y + 1);
			string mask_line(mask_width, ' ');
			for (int row = 0; row < mask_height; row++) {
				out += Mv::to(mask_y + row, mask_x) + Theme::c("main_bg") + mask_line;
			}

			// Draw main box
			out += Draw::createBox(x, y, dialog_width, dialog_height, Theme::c("hi_fg"), true, "Preset Editor");

			// Add close button [×] in top-right corner
			int editor_close_x = x + dialog_width - 5;
			out += Mv::to(y, editor_close_x) + Theme::c("hi_fg") + "[" + Theme::c("inactive_fg") + "×" + Theme::c("hi_fg") + "]";
			Menu::mouse_mappings["close_preset_editor"] = {y, editor_close_x, 1, 3};

			// Draw vertical divider between columns
			for (int row = 1; row < dialog_height - 1; row++) {
				out += Mv::to(y + row, x + left_col_width) + Theme::c("hi_fg") + "│";
			}
			// Connect divider to top/bottom borders
			out += Mv::to(y, x + left_col_width) + Theme::c("hi_fg") + "┬";
			out += Mv::to(y + dialog_height - 1, x + left_col_width) + Theme::c("hi_fg") + "┴";

			const int label_x = x + 2;
			const int value_x = x + 14;
			int row_y = y + 2;

			// Helper to draw a field row (compact for 2-col layout)
			auto drawField = [&](int field, const string& label, const string& value, bool is_toggle = false) {
				bool is_sel = (selected_field == field);
				bool is_editable = isFieldEditable(field);
				if ((field == MEM_Graph or field == Show_Disk) and not isMemOptionEditable(field)) {
					is_editable = false;
				}

				string fg = is_editable ? (is_sel ? Theme::c("hi_fg") : Theme::c("main_fg"))
				                         : Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";

				out += Mv::to(row_y, label_x) + fg + marker + label;
				out += Mv::to(row_y, value_x);

				if (is_toggle) {
					out += drawToggle(value == "ON", is_sel and is_editable, tty_mode, not is_editable);
				} else {
					out += fg + value;
				}

				//? Mouse mapping for this field row (click to select, click toggle to toggle)
				Menu::mouse_mappings["pe_field_" + to_string(field)] = {row_y, label_x, 1, left_col_width - 2};

				row_y++;
			};

			// Helper to draw radio options (compact)
			auto drawRadioField = [&](int field, const string& label, const vector<string>& options) {
				bool is_sel = (selected_field == field);
				bool is_editable = isFieldEditable(field);
				if ((field == MEM_Graph or field == Show_Disk) and not isMemOptionEditable(field)) {
					is_editable = false;
				}

				string fg = is_editable ? (is_sel ? Theme::c("hi_fg") : Theme::c("main_fg"))
				                         : Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";

				out += Mv::to(row_y, label_x) + fg + marker + label;
				out += Mv::to(row_y, value_x);

				int current = getRadioValue(field);
				int focus = is_sel ? radio_focus : -1;
				out += drawRadio(options, current, focus, tty_mode);

				//? Mouse mapping for label area (click to select field)
				Menu::mouse_mappings["pe_field_" + to_string(field)] = {row_y, label_x, 1, value_x - label_x};

				//? Mouse mappings for each radio option (click to select option)
				if (is_editable) {
					int radio_x = value_x;
					for (size_t ri = 0; ri < options.size(); ri++) {
						int option_width = 2 + static_cast<int>(ulen(options[ri]));
						Menu::mouse_mappings["pe_radio_" + to_string(field) + "_" + to_string(ri)] = {
							row_y, radio_x, 1, option_width
						};
						radio_x += option_width + 1;
					}
				}

				row_y++;
			};

			// Helper to draw toggle with inline position options (CPU/GPU/PWR rows)
			// Combines toggle and position on same line to save vertical space
			auto drawToggleWithPosition = [&](int toggle_field, const string& label, bool enabled,
			                                  int pos_field, const vector<string>& pos_options, int current_pos) {
				bool toggle_sel = (selected_field == toggle_field);
				bool pos_sel = (selected_field == pos_field);
				bool toggle_editable = isFieldEditable(toggle_field);
				bool pos_editable = enabled && isFieldEditable(pos_field);

				// Draw label with marker based on which field is selected
				string fg = toggle_editable ? ((toggle_sel || pos_sel) ? Theme::c("hi_fg") : Theme::c("main_fg"))
				                            : Theme::c("inactive_fg");
				string marker = (toggle_sel || pos_sel) ? ">" : " ";

				out += Mv::to(row_y, label_x) + fg + marker + label;
				out += Mv::to(row_y, value_x);

				// Reset to main_fg before toggle if only position is selected (so bracket stays white)
				if (pos_sel && !toggle_sel) {
					out += Theme::c("main_fg");
				}

				// Draw toggle
				out += drawToggle(enabled, toggle_sel && toggle_editable, tty_mode);

				// Draw position options inline after toggle
				int pos_start_x = value_x + 4;  // 3 for toggle + 1 space
				out += Mv::to(row_y, pos_start_x);

				if (enabled) {
					// Draw active radio buttons
					int focus = pos_sel ? radio_focus : -1;
					out += drawRadio(pos_options, current_pos, focus, tty_mode);
				} else {
					// Draw inactive position indicator
					out += Theme::c("inactive_fg") + "(" + pos_options[0] + ")";
				}

				// Mouse mapping for toggle field (label + toggle area)
				Menu::mouse_mappings["pe_field_" + to_string(toggle_field)] = {row_y, label_x, 1, pos_start_x - label_x};

				// Mouse mappings for position options
				if (pos_editable) {
					int radio_x = pos_start_x;
					for (size_t ri = 0; ri < pos_options.size(); ri++) {
						int option_width = 2 + static_cast<int>(ulen(pos_options[ri]));
						Menu::mouse_mappings["pe_radio_" + to_string(pos_field) + "_" + to_string(ri)] = {
							row_y, radio_x, 1, option_width
						};
						radio_x += option_width + 1;
					}
					// Note: Don't add pe_field_ mapping here as it would overlap with pe_radio_ mappings
					// The pe_radio_ handler already sets selected_field when clicked
				} else {
					// Map disabled position area (clicking shows it's disabled)
					Menu::mouse_mappings["pe_field_" + to_string(pos_field)] = {row_y, pos_start_x, 1, 10};
				}

				row_y++;
			};

			//? Helper to draw a ToggleRow field (multiple toggles on one row, or two rows for Mem_Charts)
			auto drawToggleRowField = [&](int field, const string& label) {
				bool is_sel = (selected_field == field);
				bool is_editable = current_preset.mem_enabled;  // Only editable when MEM is enabled

				string fg = is_editable ? (is_sel ? Theme::c("hi_fg") : Theme::c("main_fg"))
				                         : Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";

				// Get toggle values from preset (not live config)
				vector<string> labels = getToggleRowLabels(field);
				vector<bool> values;
				if (field == Mem_Charts) {
					values = {current_preset.mem_show_used, current_preset.mem_show_available,
					          current_preset.mem_show_cached, current_preset.mem_show_free};
				} else if (field == Swap_Charts) {
					values = {current_preset.swap_show_used, current_preset.swap_show_free};
				} else if (field == Vram_Charts) {
					values = {current_preset.vram_show_used, current_preset.vram_show_free};
				}

				int focus_idx = (is_sel && is_editable) ? togglerow_editor_focus : -1;

				// Mouse mapping for field label
				Menu::mouse_mappings["pe_field_" + to_string(field)] = {row_y, label_x, 1, value_x - label_x};

				//? Special handling for Mem_Charts: split across two lines
				if (field == Mem_Charts) {
					// Line 1: Label + Used, Available
					out += Mv::to(row_y, label_x) + fg + marker + label;
					out += Mv::to(row_y, value_x);
					vector<string> line1_labels = {"Used", "Available"};
					vector<bool> line1_values = {values[0], values[1]};
					int line1_focus = (focus_idx >= 0 && focus_idx <= 1) ? focus_idx : -1;
					out += drawToggleRow(line1_labels, line1_values, line1_focus, tty_mode);

					// Mouse mappings for first two toggles
					if (is_editable) {
						int toggle_x = value_x;
						for (size_t ti = 0; ti < 2; ti++) {
							int toggle_width = 4 + static_cast<int>(line1_labels[ti].size());
							Menu::mouse_mappings["pe_togglerow_" + to_string(field) + "_" + to_string(ti)] = {
								row_y, toggle_x, 1, toggle_width
							};
							toggle_x += toggle_width + 2;
						}
					}
					row_y++;

					// Line 2: Cached, Free (indented, no label)
					out += Mv::to(row_y, value_x);
					vector<string> line2_labels = {"Cached", "Free"};
					vector<bool> line2_values = {values[2], values[3]};
					int line2_focus = (focus_idx >= 2 && focus_idx <= 3) ? (focus_idx - 2) : -1;
					out += drawToggleRow(line2_labels, line2_values, line2_focus, tty_mode);

					// Mouse mappings for last two toggles
					if (is_editable) {
						int toggle_x = value_x;
						for (size_t ti = 0; ti < 2; ti++) {
							int toggle_width = 4 + static_cast<int>(line2_labels[ti].size());
							Menu::mouse_mappings["pe_togglerow_" + to_string(field) + "_" + to_string(ti + 2)] = {
								row_y, toggle_x, 1, toggle_width
							};
							toggle_x += toggle_width + 2;
						}
					}
					row_y++;
				}
				else {
					// Standard single-line rendering for other ToggleRow fields
					out += Mv::to(row_y, label_x) + fg + marker + label;
					out += Mv::to(row_y, value_x);
					out += drawToggleRow(labels, values, focus_idx, tty_mode);

					// Mouse mappings for each toggle in the row
					if (is_editable) {
						int toggle_x = value_x;
						for (size_t ti = 0; ti < labels.size(); ti++) {
							int toggle_width = 4 + static_cast<int>(labels[ti].size());  // [x] + label
							Menu::mouse_mappings["pe_togglerow_" + to_string(field) + "_" + to_string(ti)] = {
								row_y, toggle_x, 1, toggle_width
							};
							toggle_x += toggle_width + 2;  // spacing between toggles
						}
					}
					row_y++;
				}
			};

			// ==================== LEFT COLUMN: OPTIONS ====================

			// ---- NAME ----
			{
				bool is_sel = (selected_field == Name);
				string marker = is_sel ? ">" : " ";
				out += Mv::to(row_y, label_x) + (is_sel ? Theme::c("hi_fg") : Theme::c("main_fg"));
				out += marker + "Name:";
				out += Mv::to(row_y, value_x);
				if (editing_name) {
					out += Theme::c("hi_fg") + Fx::ul + name_editor.text + Fx::uul + " ";
				} else {
					out += Theme::c("main_fg") + "[" + current_preset.name + "]";
				}
				//? Mouse mapping for Name field
				Menu::mouse_mappings["pe_field_" + to_string(Name)] = {row_y, label_x, 1, left_col_width - 2};
				row_y++;
			}

			// ---- TOP PANELS (compact: toggle + position on same line) ----
			out += Mv::to(row_y, label_x) + Theme::c("title") + Fx::b + "─ Top ─" + Fx::ub;
			row_y++;

			drawToggleWithPosition(CPU, "CPU", current_preset.cpu_enabled,
			                       CPU_Pos, {"Top", "Bottom"}, getRadioValue(CPU_Pos));
			drawToggleWithPosition(GPU, "GPU", current_preset.gpu_enabled,
			                       GPU_Pos, {"Top", "Bottom"}, getRadioValue(GPU_Pos));
			drawToggleWithPosition(PWR, "PWR", current_preset.pwr_enabled,
			                       PWR_Pos, {"Top", "Bottom"}, getRadioValue(PWR_Pos));

			// ---- MEMORY SECTION ----
			out += Mv::to(row_y, label_x) + Theme::c("title") + Fx::b + "─ Memory ─" + Fx::ub;
			row_y++;

			drawField(MEM, "MEM", current_preset.mem_enabled ? "ON" : "OFF", true);
			drawRadioField(MEM_Type, "Type", {"Horizontal", "Vertical"});
			drawRadioField(MEM_Graph, "Graph", {"Bar", "Meter"});
			drawField(Show_Disk, "Disk", current_preset.show_disk ? "ON" : "OFF", true);
			//? Memory chart visibility toggles
			drawToggleRowField(Mem_Charts, "Mem");
			drawToggleRowField(Swap_Charts, "Swap");
			drawToggleRowField(Vram_Charts, "Vram");

			// ---- NETWORK SECTION ----
			out += Mv::to(row_y, label_x) + Theme::c("title") + Fx::b + "─ Network ─" + Fx::ub;
			row_y++;

			drawField(NET, "NET", current_preset.net_enabled ? "ON" : "OFF", true);
			if (current_preset.mem_enabled) {
				drawRadioField(NET_Pos, "Position", {"Left", "Right"});
			} else {
				bool is_sel = (selected_field == NET_Pos);
				string fg = Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";
				out += Mv::to(row_y, label_x) + fg + marker + "Position";
				out += Mv::to(row_y, value_x) + fg + "(Wide)";
				Menu::mouse_mappings["pe_field_" + to_string(NET_Pos)] = {row_y, label_x, 1, left_col_width - 2};
				row_y++;
			}

			//? NET Direction control
			if (current_preset.net_enabled) {
				drawRadioField(NET_Dir, "Direction", {"◀", "▶", "▼", "▲"});
			} else {
				bool is_sel = (selected_field == NET_Dir);
				string fg = Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";
				out += Mv::to(row_y, label_x) + fg + marker + "Direction";
				out += Mv::to(row_y, value_x) + fg + "(n/a)";
				Menu::mouse_mappings["pe_field_" + to_string(NET_Dir)] = {row_y, label_x, 1, left_col_width - 2};
				row_y++;
			}

			// ---- PROCESS SECTION ----
			out += Mv::to(row_y, label_x) + Theme::c("title") + Fx::b + "─ Process ─" + Fx::ub;
			row_y++;

			drawField(PROC, "PROC", current_preset.proc_enabled ? "ON" : "OFF", true);
			drawRadioField(PROC_Pos, "Position", {"Right", "Wide"});

			// ---- GRAPH SYMBOL (split across two lines) ----
			{
				bool is_sel = (selected_field == GraphSymbol);
				bool is_editable = isFieldEditable(GraphSymbol);
				string fg = is_editable ? (is_sel ? Theme::c("hi_fg") : Theme::c("main_fg"))
				                         : Theme::c("inactive_fg");
				string marker = is_sel ? ">" : " ";

				// Line 1: Label + first two options
				int graph_start_y = row_y;
				out += Mv::to(row_y, label_x) + fg + marker + "Style";
				out += Mv::to(row_y, value_x);
				int current = getRadioValue(GraphSymbol);
				int focus = is_sel ? radio_focus : -1;
				out += drawRadio({"default", "braille"}, current, (focus <= 1) ? focus : -1, tty_mode);

				//? Mouse mappings for first row radio options (default=0, braille=1)
				int radio_x = value_x;
				Menu::mouse_mappings["pe_radio_" + to_string(GraphSymbol) + "_0"] = {row_y, radio_x, 1, 9};  // "● default"
				radio_x += 10;
				Menu::mouse_mappings["pe_radio_" + to_string(GraphSymbol) + "_1"] = {row_y, radio_x, 1, 9};  // "○ braille"
				row_y++;

				// Line 2: Indent + remaining options
				out += Mv::to(row_y, value_x);
				// Adjust indices for second row (block=2, tty=3 become 0,1 in display)
				int adj_current = (current >= 2) ? current - 2 : -1;
				int adj_focus = (is_sel and focus >= 2) ? focus - 2 : -1;
				out += drawRadio({"block", "tty"}, adj_current, adj_focus, tty_mode);

				//? Mouse mappings for second row radio options (block=2, tty=3)
				radio_x = value_x;
				Menu::mouse_mappings["pe_radio_" + to_string(GraphSymbol) + "_2"] = {row_y, radio_x, 1, 7};  // "○ block"
				radio_x += 8;
				Menu::mouse_mappings["pe_radio_" + to_string(GraphSymbol) + "_3"] = {row_y, radio_x, 1, 5};  // "○ tty"
				row_y++;

				//? Mouse mapping for GraphSymbol field label (click to select field)
				Menu::mouse_mappings["pe_field_" + to_string(GraphSymbol)] = {graph_start_y, label_x, 1, value_x - label_x};
			}

			// ---- BUTTONS at bottom of left column ----
			int buttons_y = y + dialog_height - 3;
			{
				bool save_sel = (selected_field == Save);
				bool cancel_sel = (selected_field == Cancel);
				int btn_x = label_x + 2;

				out += Mv::to(buttons_y, btn_x);
				if (save_sel) {
					out += Theme::c("hi_fg") + Fx::b + "[ SAVE ]" + Fx::ub;
				} else {
					out += Theme::c("main_fg") + "[ Save ]";
				}
				//? Mouse mappings for Save button (both action and field selection)
				Menu::mouse_mappings["editor_save"] = {buttons_y, btn_x, 1, 8};
				Menu::mouse_mappings["pe_field_" + to_string(Save)] = {buttons_y, btn_x, 1, 8};

				out += "  ";
				btn_x += 10;  // "[ Save ]" + "  "

				if (cancel_sel) {
					out += Theme::c("hi_fg") + Fx::b + "[ CANCEL ]" + Fx::ub;
				} else {
					out += Theme::c("main_fg") + "[ Cancel ]";
				}
				//? Mouse mappings for Cancel button (both action and field selection)
				Menu::mouse_mappings["editor_cancel"] = {buttons_y, btn_x, 1, 10};
				Menu::mouse_mappings["pe_field_" + to_string(Cancel)] = {buttons_y, btn_x, 1, 10};
			}

			// ==================== RIGHT COLUMN: PREVIEW ====================
			int preview_x = right_col_start + 1;
			int preview_y = y + 2;
			int preview_width = right_col_width - 2;
			int preview_height = dialog_height - 4;

			// Preview header
			out += Mv::to(y + 1, right_col_start + 2) + Theme::c("title") + Fx::b + "Preview" + Fx::ub;

			if (preview_height >= 8 and preview_width >= 20) {
				string preview = drawPresetPreview(current_preset, preview_width, preview_height);
				int py = preview_y;
				// Split by newlines and output whole lines to preserve UTF-8 characters
				string line;
				for (size_t i = 0; i < preview.size(); i++) {
					if (preview[i] == '\n') {
						out += Mv::to(py++, preview_x) + Theme::c("main_fg") + line;
						line.clear();
					} else {
						line += preview[i];
					}
				}
				if (not line.empty()) {
					out += Mv::to(py, preview_x) + Theme::c("main_fg") + line;
				}
			} else {
				out += Mv::to(preview_y + preview_height / 2, preview_x) + Theme::c("inactive_fg") + "(Preview too small)";
			}

			// Keyboard help at bottom of editor dialog (centered)
			const int help_len = 60;  // "↑↓:Navigate  ←→:Option  Space:Toggle  Enter:Save  Esc:Cancel"
			int help_x = x + (dialog_width - help_len) / 2;
			out += Mv::to(y + dialog_height - 1, help_x)
				+ Theme::c("hi_fg") + "↑↓" + Theme::c("main_fg") + ":Navigate  "
				+ Theme::c("hi_fg") + "←→" + Theme::c("main_fg") + ":Option  "
				+ Theme::c("hi_fg") + "Space" + Theme::c("main_fg") + ":Toggle  "
				+ Theme::c("hi_fg") + "Enter" + Theme::c("main_fg") + ":Save  "
				+ Theme::c("hi_fg") + "Esc" + Theme::c("main_fg") + ":Cancel";

			out += Fx::reset;
		}

		return (Menu::redraw ? Menu::Changed : retval);
	}

} // namespace MenuV2

//? Wrapper function in Menu namespace to call MenuV2::optionsMenuV2
namespace Menu {
	int optionsMenuV2_wrapper(const string& key) {
		return MenuV2::optionsMenuV2(key);
	}
}
