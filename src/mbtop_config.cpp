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

#include <array>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include <fmt/base.h>
#include <fmt/core.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <unistd.h>

#include "mbtop_config.hpp"
#include "mbtop_log.hpp"
#include "mbtop_shared.hpp"
#include "mbtop_tools.hpp"

#if defined(__APPLE__) && defined(GPU_SUPPORT)
#include "osx/apple_silicon_gpu.hpp"
#endif

using std::array;
using std::atomic;
using std::string_view;

namespace fs = std::filesystem;
namespace rng = std::ranges;

using namespace std::literals;
using namespace Tools;

//* Functions and variables for reading and writing the btop config file
namespace Config {

	atomic<bool> locked (false);
	atomic<bool> writelock (false);
	bool write_new;

	const vector<array<string, 2>> descriptions = {
		{"color_theme", 		"#* Name of a btop++/bpytop/bashtop formatted \".theme\" file, \"Default\" and \"TTY\" for builtin themes.\n"
								"#* Themes should be placed in \"../share/mbtop/themes\" relative to binary or \"$HOME/.config/mbtop/themes\""},

		{"theme_background", 	"#* If the theme set background should be shown, set to False if you want terminal background transparency."},

		{"truecolor", 			"#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false."},

		{"force_tty", 			"#* Set to true to force tty mode regardless if a real tty has been detected or not.\n"
								"#* Will force 16-color mode and TTY theme, set all graph symbols to \"tty\" and swap out other non tty friendly symbols."},

		{"presets",				"#* Define presets for the layout of the boxes. Preset 0 is always all boxes shown with default settings. Max 9 presets.\n"
								"#* Format: \"box_name:P:G,box_name:P:G\" P=(0 or 1) for alternate positions, G=graph symbol to use for box.\n"
								"#* Use whitespace \" \" as separator between different presets.\n"
								"#* Example: \"cpu:0:default,mem:0:tty,proc:1:default cpu:0:braille,proc:0:tty\""},

		{"vim_keys",			"#* Set to True to enable \"h,j,k,l,g,G\" keys for directional control in lists.\n"
								"#* Conflicting keys for h:\"help\" and k:\"kill\" is accessible while holding shift."},

		{"disable_mouse", "#* Disable all mouse events."},
		{"rounded_corners",		"#* Rounded corners on boxes, is ignored if TTY mode is ON."},

		{"terminal_sync", 		"#* Use terminal synchronized output sequences to reduce flickering on supported terminals."},

		{"graph_symbol", 		"#* Default symbols to use for graph creation, \"braille\", \"block\" or \"tty\".\n"
								"#* \"braille\" offers the highest resolution but might not be included in all fonts.\n"
								"#* \"block\" has half the resolution of braille but uses more common characters.\n"
								"#* \"tty\" uses only 3 different symbols but will work with most fonts and should work in a real TTY.\n"
								"#* Note that \"tty\" only has half the horizontal resolution of the other two, so will show a shorter historical view."},

		{"graph_symbol_cpu", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},
#ifdef GPU_SUPPORT
		{"graph_symbol_gpu", 	"# Graph symbol to use for graphs in gpu box, \"default\", \"braille\", \"block\" or \"tty\"."},
		{"graph_symbol_pwr", 	"# Graph symbol to use for graphs in pwr box, \"default\", \"braille\", \"block\" or \"tty\"."},
#endif
		{"graph_symbol_mem", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"graph_symbol_net", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"graph_symbol_proc", 	"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\"."},

		{"shown_boxes", 		"#* Manually set which boxes to show. Available values are \"cpu pwr mem net proc\" and \"gpu0\" through \"gpu5\", separate values with whitespace."},

		{"update_ms", 			"#* Update time in milliseconds, recommended 2000 ms or above for better sample times for graphs."},

		{"proc_sorting",		"#* Processes sorting, \"pid\" \"program\" \"arguments\" \"threads\" \"user\" \"memory\" \"cpu lazy\" \"cpu direct\" \"gpu\",\n"
								"#* \"cpu lazy\" sorts top process over time (easier to follow), \"cpu direct\" updates top process directly.\n"
								"#* \"gpu\" sorts by GPU usage (Apple Silicon only)."},

		{"proc_reversed",		"#* Reverse sorting order, True or False."},

		{"proc_tree",			"#* Show processes as a tree."},

		{"proc_colors", 		"#* Use the cpu graph colors in the process list."},

		{"proc_gradient", 		"#* Use a darkening gradient in the process list."},

		{"proc_per_core", 		"#* If process cpu usage should be of the core it's running on or usage of the total available cpu power."},

		{"proc_gpu", 			"#* Show GPU usage per process (Apple Silicon only)."},

		{"proc_mem_bytes", 		"#* Show process memory as bytes instead of percent."},

		{"proc_cpu_graphs",     "#* Show cpu graph for each process."},

		{"proc_gpu_graphs",     "#* Show gpu graph for each process (when GPU column is enabled)."},

		{"proc_show_cmd",       "#* Show Command column in process list. Toggle with Shift+C."},

		{"proc_show_threads",   "#* Show Threads column in process list."},

		{"proc_show_user",      "#* Show User column in process list."},

		{"proc_show_memory",    "#* Show Memory column in process list."},

		{"proc_show_cpu",       "#* Show CPU% column in process list."},

		{"proc_show_io",        "#* Show combined I/O column in process list (side layout only)."},

		{"proc_show_io_read",   "#* Show I/O Read column in process list (bottom layout only)."},

		{"proc_show_io_write",  "#* Show I/O Write column in process list (bottom layout only)."},

		{"proc_show_state",     "#* Show State column in process list (bottom layout only)."},

		{"proc_show_priority",  "#* Show Priority column in process list (bottom layout only)."},

		{"proc_show_nice",      "#* Show Nice column in process list (bottom layout only)."},

		{"proc_show_ports",     "#* Show Ports column in process list (bottom layout only)."},

		{"proc_show_virt",      "#* Show Virtual Memory column in process list (bottom layout only)."},

		{"proc_show_runtime",   "#* Show Runtime column in process list (bottom layout only)."},

		{"proc_show_cputime",   "#* Show CPU Time column in process list (bottom layout only)."},

		{"proc_show_gputime",   "#* Show GPU Time column in process list (bottom layout only, requires GPU)."},

		{"proc_info_smaps",		"#* Use /proc/[pid]/smaps for memory information in the process info box (very slow but more accurate)"},

		{"proc_left",			"#* Show proc box on left side of screen instead of right."},

		{"proc_filter_kernel",  "#* (Linux) Filter processes tied to the Linux kernel(similar behavior to htop)."},

		{"proc_follow_detailed",	"#* Should the process list follow the selected process when detailed view is open."},

		{"proc_aggregate",		"#* In tree-view, always accumulate child process resources in the parent process."},

		{"keep_dead_proc_usage", "#* Should cpu and memory usage display be preserved for dead processes when paused."},

		{"cpu_graph_upper", 	"#* Sets the CPU stat shown in upper half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},

		{"cpu_graph_lower", 	"#* Sets the CPU stat shown in lower half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},
	#ifdef GPU_SUPPORT
		{"show_gpu_info",		"#* If gpu info should be shown in the cpu box. Available values = \"Auto\", \"On\" and \"Off\"."},
	#endif
		{"cpu_invert_lower", 	"#* Toggles if the lower CPU graph should be inverted."},

		{"cpu_single_graph", 	"#* Set to True to completely disable the lower CPU graph."},

		{"cpu_bottom",			"#* Show cpu box at bottom of screen instead of top."},

		{"gpu_bottom",			"#* Show gpu box at bottom of screen instead of top."},

		{"pwr_bottom",			"#* Show pwr box at bottom of screen instead of top."},

		{"show_uptime", 		"#* Shows the system uptime in the CPU box."},

		{"show_cpu_watts",		"#* Shows the CPU package current power consumption in watts. Requires running `make setcap` or `make setuid` or running with sudo."},

		{"check_temp", 			"#* Show cpu temperature."},

		{"cpu_sensor", 			"#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors."},

		{"show_coretemp", 		"#* Show temperatures for cpu cores also if check_temp is True and sensors has been found."},

		{"cpu_core_map",		"#* Set a custom mapping between core and coretemp, can be needed on certain cpus to get correct temperature for correct core.\n"
								"#* Use lm-sensors or similar to see which cores are reporting temperatures on your machine.\n"
								"#* Format \"x:y\" x=core with wrong temp, y=core with correct temp, use space as separator between multiple entries.\n"
								"#* Example: \"4:0 5:1 6:3\""},

		{"temp_scale", 			"#* Which temperature scale to use, available values: \"celsius\", \"fahrenheit\", \"kelvin\" and \"rankine\"."},

		{"base_10_sizes",		"#* Use base 10 for bits/bytes sizes, KB = 1000 instead of KiB = 1024."},

		{"show_cpu_freq", 		"#* Show CPU frequency."},
	#ifdef __linux__
		{"freq_mode",				"#* How to calculate CPU frequency, available values: \"first\", \"range\", \"lowest\", \"highest\" and \"average\"."},
	#endif
		{"clock_format", 		"#* Clock format using strftime syntax. Empty string to disable clock.\n"
								"#* Examples: \"%H:%M:%S\" (24h), \"%I:%M:%S %p\" (12h with AM/PM), \"%X\" (locale default)"},

		{"clock_12h",			"#* Use 12-hour clock format (HH:MM:SS AM/PM) instead of 24-hour format.\n"
								"#* This overrides clock_format with a 12-hour format."},

		{"show_hostname",		"#* Show hostname in the CPU box header (centered position).\n"
								"#* Strips \".local\" suffix but keeps full FQDN (e.g., host.domain.com)."},

		{"show_uptime_header",	"#* Show system uptime in the CPU box header (left side, after preset)."},

		{"show_username_header","#* Show current username in the CPU box header (left side, after uptime)."},

		{"preset_names",		"#* Custom names for presets 1-9, separated by whitespace. Empty to use default numbers.\n"
								"#* Example: \"Standard LLM Processes Power CPU/MEM\" for presets 1-5."},

		{"preset_0",			"#* Configuration for preset 0 (first preset). Format: box:position:symbol,..."},

		{"background_update", 	"#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort."},

		{"custom_cpu_name", 	"#* Custom cpu model name, empty string to disable."},

		{"disks_filter", 		"#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with whitespace \" \".\n"
									"#* Only disks matching the filter will be shown. Prepend exclude= to only show disks not matching the filter. Examples: disk_filter=\"/boot /home/user\", disks_filter=\"exclude=/boot /home/user\""},

		{"mem_graphs", 			"#* Show graphs instead of meters for memory values."},

		{"mem_bar_mode",		"#* Use meter bars instead of braille graphs when mem panel height <= 18. Toggle with Shift+M (Meter) or Shift+B (Bar/Braille)."},

		{"mem_below_net",		"#* Show mem box below net box instead of above."},

		{"net_beside_mem",		"#* Show net box beside mem box instead of above/below. Pressing '3' cycles: hidden -> stacked -> beside."},

		{"proc_full_width",		"#* When net_beside_mem is active, show proc panel full width under both mem+net. Pressing '4' cycles: hidden -> under net -> full width."},

		{"logs_below_proc",		"#* Position the logs panel below proc panel instead of beside it. Pressing '8' toggles the logs panel."},

		{"log_color_full_line",	"#* Color the full log line based on level, or only the level marker [X]. Colors from theme: log_fault, log_error, log_info, log_default, log_debug."},

		{"log_export_path",		"#* Path for log export files. Default: ~/Desktop. Filename format: <PID>-<Process>-<datetime>.log"},

		{"log_buffer_size",		"#* Maximum number of log entries to keep in buffer. Default: 500. Higher values use more memory."},

		{"stacked_layout",		"#* Force fully stacked vertical layout: MEM full width, NET full width below, PROC full width at bottom."},

		{"zfs_arc_cached",		"#* Count ZFS ARC in cached and available memory."},

		{"show_swap", 			"#* If swap memory should be shown in memory box."},

		{"swap_disk", 			"#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk."},

		{"mem_vram_mode",		"#* DEPRECATED: Use vram_show_used/vram_show_free instead. Kept for backward compatibility."},

		{"show_disks", 			"#* If mem box should be split to also show disks info."},

		{"mem_horizontal", 		"#* Show memory graphs horizontally (side by side) when disks are hidden. Toggle with '2' key."},

		{"mem_show_used",		"#* Show 'Used' memory item in memory box. Toggle visibility with Shift+T."},
		{"mem_show_available",	"#* Show 'Available' memory item in memory box. Toggle visibility with Shift+T."},
		{"mem_show_cached",		"#* Show 'Cached' memory item in memory box. Toggle visibility with Shift+T."},
		{"mem_show_free",		"#* Show 'Free' memory item in memory box. Toggle visibility with Shift+T."},
		{"swap_show_used",		"#* Show 'Swap Used' item in memory box. Toggle visibility with Shift+S."},
		{"swap_show_free",		"#* Show 'Swap Free' item in memory box. Toggle visibility with Shift+S."},
		{"vram_show_used",		"#* Show 'VRAM Used' item in memory box. Toggle visibility with Shift+V."},
		{"vram_show_free",		"#* Show 'VRAM Free' item in memory box. Toggle visibility with Shift+V."},

		{"only_physical", 		"#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar."},

		{"show_network_drives",	"#* Show network drives (NFS, SMB, AFP) in the disks list. The protocol type will be shown in parentheses."},

		{"use_fstab", 			"#* Read disks list from /etc/fstab. This also disables only_physical."},

		{"zfs_hide_datasets",		"#* Setting this to True will hide all datasets, and only show ZFS pools. (IO stats will be calculated per-pool)"},

		{"disk_free_priv",		"#* Set to true to show available disk space for privileged users."},

		{"show_io_stat", 		"#* Toggles if io activity % (disk busy time) should be shown in regular disk usage view."},

		{"io_mode", 			"#* Toggles io mode for disks, showing big graphs for disk read/write speeds."},

		{"io_graph_combined", 	"#* Set to True to show combined read/write io graphs in io mode."},

		{"io_graph_speeds", 	"#* Set the top speed for the io graphs in MiB/s (100 by default), use format \"mountpoint:speed\" separate disks with whitespace \" \".\n"
								"#* Example: \"/mnt/media:100 /:20 /boot:1\"."},

		{"swap_upload_download", "#* Swap the positions of the upload and download speed graphs. When true, upload will be on top."},

		{"net_download", 		"#* Set fixed values for network graphs in Mebibits. Is only used if net_auto is also set to False."},

		{"net_upload", ""},

		{"net_auto", 			"#* Use network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest."},

		{"net_sync", 			"#* Sync the auto scaling for download and upload to whichever currently has the highest scale."},

		{"net_graph_direction", "#* Direction of the network graph. 0 = Right to Left (default, newest data on right), 1 = Left to Right (newest data on left). Cycle with Shift+3."},

		{"net_iface", 			"#* Starts with the Network Interface specified here."},

		{"net_iface_filter",	"#* Filter which network interfaces to show when cycling with 'b' and 'n' keys.\n"
								"#* Uses a space-separated list of interface names. Leave empty to show all interfaces."},

	    {"base_10_bitrate",     "#* \"True\" shows bitrates in base 10 (Kbps, Mbps). \"False\" shows bitrates in binary sizes (Kibps, Mibps, etc.). \"Auto\" uses base_10_sizes."},

		{"show_battery", 		"#* Show battery stats in top right if battery is present."},

		{"selected_battery",	"#* Which battery to use if multiple are present. \"Auto\" for auto detection."},

		{"show_battery_watts",	"#* Show power stats of battery next to charge indicator."},

		{"log_level", 			"#* Set loglevel for \"~/.local/state/mbtop.log\" levels are: \"ERROR\" \"WARNING\" \"INFO\" \"DEBUG\".\n"
								"#* The level set includes all lower levels, i.e. \"DEBUG\" will show all logging info."},
		{"save_config_on_exit",  "#* Automatically save current settings to config file on exit."},

		{"prevent_autosave",	"#* If True, only the primary instance saves config. If False, all instances save (last close wins)."},

		{"show_instance_indicator",	"#* Show P (Primary) or S (Secondary) indicator in header for multi-instance awareness."},

		{"preview_unicode",		"#* Use Unicode box-drawing characters for preset preview. Set to false for ASCII."},
	#ifdef GPU_SUPPORT

		{"nvml_measure_pcie_speeds",
								"#* Measure PCIe throughput on NVIDIA cards, may impact performance on certain cards."},
		{"rsmi_measure_pcie_speeds",
								"#* Measure PCIe throughput on AMD cards, may impact performance on certain cards."},
		{"gpu_mirror_graph",	"#* Horizontally mirror the GPU graph."},
		{"shown_gpus",			"#* Set which GPU vendors to show. Available values are \"apple nvidia amd intel\""},
		{"custom_gpu_name0",	"#* Custom gpu0 model name, empty string to disable."},
		{"custom_gpu_name1",	"#* Custom gpu1 model name, empty string to disable."},
		{"custom_gpu_name2",	"#* Custom gpu2 model name, empty string to disable."},
		{"custom_gpu_name3",	"#* Custom gpu3 model name, empty string to disable."},
		{"custom_gpu_name4",	"#* Custom gpu4 model name, empty string to disable."},
		{"custom_gpu_name5",	"#* Custom gpu5 model name, empty string to disable."},
	#endif
	};

	std::unordered_map<std::string_view, string> strings = {
		{"color_theme", "Default"},
		{"shown_boxes", "cpu pwr mem net proc"},
		{"graph_symbol", "braille"},
		{"presets", "cpu:1:default,pwr:0:default cpu:0:default,pwr:0:default,mem:0:default,net:0:default cpu:0:braille,mem:0:braille,net:0:braille"},
		{"graph_symbol_cpu", "default"},
		{"graph_symbol_gpu", "default"},
		{"graph_symbol_pwr", "default"},
		{"graph_symbol_mem", "default"},
		{"graph_symbol_net", "default"},
		{"graph_symbol_proc", "default"},
		{"proc_sorting", "cpu lazy"},
		{"cpu_graph_upper", "Auto"},
		{"cpu_graph_lower", "Auto"},
		{"cpu_sensor", "Auto"},
		{"selected_battery", "Auto"},
		{"cpu_core_map", ""},
		{"temp_scale", "celsius"},
	#ifdef __linux__
		{"freq_mode", "first"},
	#endif
		{"clock_format", "%X"},
		{"preset_names", "Standard LLM Processes Power CPU/MEM"},
		{"preset_0", ""},
		{"custom_cpu_name", ""},
		{"disks_filter", ""},
		{"io_graph_speeds", ""},
		{"net_iface", ""},
		{"net_iface_filter", ""},
		{"base_10_bitrate", "Auto"},
		{"log_level", "WARNING"},
		{"log_export_path", ""},
		{"proc_filter", ""},
		{"proc_command", ""},
		{"selected_name", ""},
	#ifdef GPU_SUPPORT
		{"custom_gpu_name0", ""},
		{"custom_gpu_name1", ""},
		{"custom_gpu_name2", ""},
		{"custom_gpu_name3", ""},
		{"custom_gpu_name4", ""},
		{"custom_gpu_name5", ""},
		{"show_gpu_info", "Auto"},
	#if defined(__APPLE__)
		{"shown_gpus", "apple"}  //? Default to Apple on macOS (Apple Silicon)
	#else
		{"shown_gpus", "nvidia amd intel"}
	#endif
	#endif
	};
	std::unordered_map<std::string_view, string> stringsTmp;

	std::unordered_map<std::string_view, bool> bools = {
		{"theme_background", true},
		{"truecolor", true},
		{"rounded_corners", true},
		{"proc_reversed", false},
		{"proc_tree", false},
		{"proc_colors", true},
		{"proc_gradient", true},
		{"proc_per_core", false},
		{"proc_gpu", true},
		{"proc_mem_bytes", true},
		{"proc_cpu_graphs", true},
		{"proc_gpu_graphs", true},
		{"proc_show_cmd", true},
		{"proc_show_threads", true},
		{"proc_show_user", true},
		{"proc_show_memory", true},
		{"proc_show_cpu", true},
		{"proc_show_io", true},
		{"proc_show_io_read", true},
		{"proc_show_io_write", true},
		{"proc_show_state", true},
		{"proc_show_priority", true},
		{"proc_show_nice", true},
		{"proc_show_ports", true},
		{"proc_show_virt", true},
		{"proc_show_runtime", true},
		{"proc_show_cputime", true},
		{"proc_show_gputime", true},
		{"proc_info_smaps", false},
		{"proc_left", false},
		{"proc_filter_kernel", false},
		{"cpu_invert_lower", true},
		{"cpu_single_graph", false},
		{"cpu_bottom", false},
		{"gpu_bottom", false},
		{"pwr_bottom", false},
		{"show_uptime", true},
		{"show_cpu_watts", true},
		{"check_temp", true},
		{"show_coretemp", true},
		{"show_cpu_freq", true},
		{"clock_12h", false},
		{"show_hostname", true},
		{"show_uptime_header", false},
		{"show_username_header", false},
		{"background_update", true},
		{"mem_graphs", true},
		{"mem_bar_mode", true},
		{"mem_below_net", false},
		{"net_beside_mem", true},
		{"proc_full_width", false},
		{"logs_below_proc", false},
		{"log_color_full_line", false},
		{"stacked_layout", false},
		{"zfs_arc_cached", true},
		{"show_swap", true},
		{"swap_disk", true},
		{"show_disks", false},
		{"mem_horizontal", true},
		{"mem_show_used", true},
		{"mem_show_available", true},
		{"mem_show_cached", true},
		{"mem_show_free", true},
		{"swap_show_used", true},
		{"swap_show_free", true},
		{"vram_show_used", true},
		{"vram_show_free", true},
		{"only_physical", true},
		{"show_network_drives", false},
		{"use_fstab", true},
		{"zfs_hide_datasets", false},
		{"show_io_stat", true},
		{"io_mode", false},
		{"swap_upload_download", false},
		{"base_10_sizes", false},
		{"io_graph_combined", false},
		{"net_auto", true},
		{"net_sync", true},
		{"show_battery", true},
		{"show_battery_watts", true},
		{"vim_keys", false},
		{"tty_mode", false},
		{"disk_free_priv", false},
		{"force_tty", false},
		{"lowcolor", false},
		{"show_detailed", false},
		{"proc_filtering", false},
		{"proc_aggregate", false},
		{"pause_proc_list", false},
		{"keep_dead_proc_usage", false},
		{"proc_banner_shown", false},
		{"proc_follow_detailed", true},
		{"follow_process", false},
		{"update_following", false},
		{"should_selection_return_to_followed", false},
	#ifdef GPU_SUPPORT
		{"nvml_measure_pcie_speeds", true},
		{"rsmi_measure_pcie_speeds", true},
		{"gpu_mirror_graph", true},
	#endif
		{"terminal_sync", true},
		{"save_config_on_exit", true},
		{"prevent_autosave", true},
		{"show_instance_indicator", true},
		{"preview_unicode", true},
		{"disable_mouse", false},
	};
	std::unordered_map<std::string_view, bool> boolsTmp;

	std::unordered_map<std::string_view, int> ints = {
		{"update_ms", 2000},
		{"net_download", 100},
		{"net_upload", 100},
		{"net_graph_direction", 0},
		{"detailed_pid", 0},
		{"restore_detailed_pid", 0},
		{"selected_pid", 0},
		{"followed_pid", 0},
		{"selected_depth", 0},
		{"proc_start", 0},
		{"proc_selected", 0},
		{"proc_last_selected", 0},
		{"proc_followed", 0},
		{"disk_selected", 0},
		{"disk_start", 0},
		{"mem_vram_mode", 0},
		{"mem_toggle_mode", 0},
		{"swap_toggle_mode", 0},
		{"vram_toggle_mode", 0},
		{"mem_start", 0},
		{"mem_selected", 0},
		{"log_buffer_size", 500}
	};
	std::unordered_map<std::string_view, int> intsTmp;

	// Returns a valid config dir or an empty optional
	// The config dir might be read only, a warning is printed, but a path is returned anyway
	[[nodiscard]] std::optional<fs::path> get_config_dir() noexcept {
		fs::path config_dir;
		{
			std::error_code error;
			if (const auto xdg_config_home = std::getenv("XDG_CONFIG_HOME"); xdg_config_home != nullptr) {
				if (fs::exists(xdg_config_home, error)) {
					config_dir = fs::path(xdg_config_home) / "mbtop";
				}
			} else if (const auto home = std::getenv("HOME"); home != nullptr) {
				error.clear();
				if (fs::exists(home, error)) {
					config_dir = fs::path(home) / ".config" / "mbtop";
				}
				if (error) {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m{} could not be accessed: {}\n", config_dir.string(), error.message());
					config_dir = "";
				}
			}
		}

		// FIXME: This warnings can be noisy if the user deliberately has a non-writable config dir
		//  offer an alternative | disable messages by default | disable messages if config dir is not writable | disable messages with a flag
		// FIXME: Make happy path not branch
		if (not config_dir.empty()) {
			std::error_code error;
			if (fs::exists(config_dir, error)) {
				if (fs::is_directory(config_dir, error)) {
					struct statvfs stats {};
					if ((fs::status(config_dir, error).permissions() & fs::perms::owner_write) == fs::perms::owner_write and
						statvfs(config_dir.c_str(), &stats) == 0 and (stats.f_flag & ST_RDONLY) == 0) {
						return config_dir;
					} else {
						fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` is not writable\n", fs::absolute(config_dir).string());
						// If the config is readable we can still use the provided config, but changes will not be persistent
						if ((fs::status(config_dir, error).permissions() & fs::perms::owner_read) == fs::perms::owner_read) {
							fmt::print(stderr, "\033[0;31mWarning: \033[0mLogging is disabled, config changes are not persistent\n");
							return config_dir;
						}
					}
				} else {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` is not a directory\n", fs::absolute(config_dir).string());
				}
			} else {
				// Doesn't exist
				if (fs::create_directories(config_dir, error)) {
					return config_dir;
				} else {
					fmt::print(stderr, "\033[0;31mWarning: \033[0m`{}` could not be created: {}\n", fs::absolute(config_dir).string(), error.message());
				}
			}
		} else {
			fmt::print(stderr, "\033[0;31mWarning: \033[0mCould not determine config path: Make sure `$XDG_CONFIG_HOME` or `$HOME` is set\n");
		}
		fmt::print(stderr, "\033[0;31mWarning: \033[0mLogging is disabled, config changes are not persistent\n");
		return {};
	}

	bool _locked(const std::string_view name) {
		atomic_wait(writelock, true);
		if (not write_new and rng::find_if(descriptions, [&name](const auto& a) { return a.at(0) == name; }) != descriptions.end())
			write_new = true;
		return locked.load();
	}

	fs::path conf_dir;
	fs::path conf_file;
	fs::path lock_file;

	bool another_instance_running = false;

	vector<string> available_batteries = {"Auto"};

	vector<string> current_boxes;
	vector<string> preset_list = {"cpu:0:default,mem:0:default,net:0:default,proc:0:default"};
	int current_preset = -1;

	bool presetsValid(const string& presets) {
		// Use saved preset_0 if available, otherwise use current preset_list[0]
		string first_preset = getS("preset_0");
		if (first_preset.empty()) {
			first_preset = preset_list.at(0);
		}
		vector<string> new_presets = {first_preset};

		for (int x = 0; const auto& preset : ssplit(presets)) {
			if (++x > 9) {
				validError = "Too many presets entered!";
				return false;
			}
			for (int y = 0; const auto& box : ssplit(preset, ',')) {
				if (++y > 6) {  //? Allow up to 6 boxes: CPU, GPU, PWR, MEM, NET, PROC
					validError = "Too many boxes entered for preset!";
					return false;
				}
				const auto& vals = ssplit(box, ':');
				// mem can have 3-8 fields: type, symbol, meter, disk, mem_vis, swap_vis, vram_vis
				// net can have 3-4 fields: pos, symbol, direction (direction is optional)
				// other boxes have exactly 3 fields
				bool is_mem = (vals.size() > 0 && vals.at(0) == "mem");
				bool is_net = (vals.size() > 0 && vals.at(0) == "net");
				if (is_mem) {
					if (vals.size() < 3 || vals.size() > 8) {
						validError = "Malformatted mem preset in config value presets!";
						return false;
					}
				} else if (is_net) {
					if (vals.size() < 3 || vals.size() > 4) {
						validError = "Malformatted net preset in config value presets!";
						return false;
					}
				} else {
					if (vals.size() != 3) {
						validError = "Malformatted preset in config value presets!";
						return false;
					}
				}
				if (not is_in(vals.at(0), "cpu", "mem", "net", "proc", "gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5", "pwr")) {
					validError = "Invalid box name in config value presets!";
					return false;
				}
				//? Position values: 0, 1 for most boxes; NET can also have 2 (Wide)
				if (not is_in(vals.at(1), "0", "1", "2")) {
					validError = "Invalid position value in config value presets!";
					return false;
				}
				if (not v_contains(valid_graph_symbols_def, vals.at(2))) {
					validError = "Invalid graph name in config value presets!";
					return false;
				}
			}
			new_presets.push_back(preset);
		}

		preset_list = std::move(new_presets);
		return true;
	}

	//* Apply selected preset
	bool apply_preset(const string& preset) {
		//? ==================== New Preset Format (13 Layout Model) ====================
		//? cpu:0:symbol, gpu0:0:symbol, pwr:0:symbol
		//? mem:type:symbol:meter:disk
		//?     type: 0=Horizontal, 1=Vertical
		//?     meter: 0=Bar, 1=Meter (only used if type=V)
		//?     disk: 0=hidden, 1=shown (only used if type=V)
		//? net:pos:symbol
		//?     pos: 0=Left, 1=Right, 2=Wide
		//? proc:pos:symbol
		//?     pos: 0=Right, 1=Wide

		string boxes;
		bool has_mem = false, has_net = false, has_proc = false;
		int cpu_pos = 0;    // 0=Top, 1=Bottom
		int gpu_pos = 0;    // 0=Top, 1=Bottom
		int pwr_pos = 0;    // 0=Top, 1=Bottom
		int mem_type = 0;   // 0=H, 1=V
		int net_pos = 0;    // 0=Left, 1=Right, 2=Wide
		int proc_pos = 0;   // 0=Right, 1=Wide

		// First pass: build boxes string and extract positions
		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			if (vals.empty()) continue;
			boxes += vals.at(0) + ' ';

			if (vals.at(0) == "cpu") {
				cpu_pos = (vals.size() > 1) ? stoi(vals.at(1)) : 0;
			} else if (vals.at(0).starts_with("gpu")) {
				gpu_pos = (vals.size() > 1) ? stoi(vals.at(1)) : 0;
			} else if (vals.at(0) == "pwr") {
				pwr_pos = (vals.size() > 1) ? stoi(vals.at(1)) : 0;
			} else if (vals.at(0) == "mem") {
				has_mem = true;
				mem_type = (vals.size() > 1) ? stoi(vals.at(1)) : 0;
			} else if (vals.at(0) == "net") {
				has_net = true;
				net_pos = (vals.size() > 1) ? stoi(vals.at(1)) : 1;
				// direction (field 4): 0=RTL, 1=LTR, 2=TTB, 3=BTT
				if (vals.size() > 3) {
					int dir = stoi_safe(vals.at(3), 0);
					set("net_graph_direction", (dir >= 0 and dir <= 3) ? dir : 0);
				}
			} else if (vals.at(0) == "proc") {
				has_proc = true;
				proc_pos = (vals.size() > 1) ? stoi(vals.at(1)) : 1;
			}
		}
		if (not boxes.empty()) boxes.pop_back();

		// Check terminal size
		auto min_size = Term::get_min_size(boxes);
		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			return false;
		}

		// Second pass: apply settings
		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			if (vals.empty()) continue;

			// Apply graph symbol
			string symbol = (vals.size() > 2) ? string(vals.at(2)) : "default";
			if (vals.at(0).starts_with("gpu")) {
				set("graph_symbol_gpu", symbol);
			} else {
				auto it = strings.find("graph_symbol_" + vals.at(0));
				if (it != strings.end()) {
					set(it->first, symbol);
				}
			}

			// MEM-specific settings
			if (vals.at(0) == "mem") {
				bool is_vertical = (mem_type == 1);

				// meter: 0=Bar, 1=Meter
				bool use_meter = false;
				if (vals.size() > 3) {
					use_meter = (vals.at(3) == "1");
				}

				// disk: 0=hidden, 1=shown
				bool show_disk = false;
				if (vals.size() > 4) {
					show_disk = (vals.at(4) == "1");
				}

				// mem_vis (field 5): bitmask for mem chart visibility (default 15 = all visible)
				int mem_vis = 15;
				if (vals.size() > 5) {
					mem_vis = stoi_safe(vals.at(5), 15);
				}
				set("mem_show_used", (mem_vis & 1) != 0);
				set("mem_show_available", (mem_vis & 2) != 0);
				set("mem_show_cached", (mem_vis & 4) != 0);
				set("mem_show_free", (mem_vis & 8) != 0);

				// swap_vis (field 6): bitmask for swap chart visibility (default 3 = all visible)
				int swap_vis = 3;
				if (vals.size() > 6) {
					swap_vis = stoi_safe(vals.at(6), 3);
				}
				set("swap_show_used", (swap_vis & 1) != 0);
				set("swap_show_free", (swap_vis & 2) != 0);

				// vram_vis (field 7): bitmask for vram chart visibility (default 3 = all visible)
				int vram_vis = 3;
				if (vals.size() > 7) {
					vram_vis = stoi_safe(vals.at(7), 3);
				}
				set("vram_show_used", (vram_vis & 1) != 0);
				set("vram_show_free", (vram_vis & 2) != 0);

				// Apply MEM settings
				set("mem_horizontal", !is_vertical);  // H=true for horizontal bars
				set("mem_graphs", true);              // Always show graphs
				set("mem_bar_mode", !use_meter);      // true=bars, false=meters
				set("show_disks", show_disk);
			}
		}

		// ============ LAYOUT CONFIGURATION BASED ON 13 LAYOUTS ============
		// Decode positions
		bool net_left = (net_pos == 0);     // Left = under MEM
		bool net_right = (net_pos == 1);    // Right = beside MEM
		bool proc_right_pos = (proc_pos == 0);  // Right = compact on right
		bool proc_wide = (proc_pos == 1);   // Wide = fills width at bottom

		// Default settings for panel positions
		set("cpu_bottom", cpu_pos == 1);  // 0=Top, 1=Bottom
		set("gpu_bottom", gpu_pos == 1);  // 0=Top, 1=Bottom
		set("pwr_bottom", pwr_pos == 1);  // 0=Top, 1=Bottom
		set("mem_below_net", false);

		// ============ APPLY LAYOUT RULES ============

		// Single panel layouts (1-3): No special settings needed, panel fills space

		// MEM + NET layouts (4-5)
		if (has_mem and has_net and not has_proc) {
			if (net_left) {
				// Layout 4: MEM top, NET under (stacked)
				set("net_beside_mem", false);
				set("mem_below_net", false);
			} else {
				// Layout 5: MEM left, NET right (side by side)
				set("net_beside_mem", true);
			}
		}

		// MEM + PROC layouts (6-7)
		if (has_mem and has_proc and not has_net) {
			if (proc_right_pos) {
				// Layout 6: MEM left, PROC right
				set("proc_left", false);
				set("proc_full_width", false);
			} else {
				// Layout 7: MEM top, PROC fills bottom
				set("proc_left", false);
				set("proc_full_width", true);
			}
		}

		// NET + PROC layouts without MEM (12-13)
		if (has_net and has_proc and not has_mem) {
			if (proc_right_pos) {
				// Layout 12: NET left, PROC right
				set("proc_left", false);
				set("proc_full_width", false);
			} else {
				// Layout 13: NET top, PROC fills bottom
				set("proc_left", false);
				set("proc_full_width", true);
			}
		}

		// PROC alone (no MEM, no NET) - always use full width layout
		if (has_proc and not has_mem and not has_net) {
			set("proc_left", false);
			set("proc_full_width", true);  //? Proc alone = always wide/full width
		}

		// Three panel layouts (8-11)
		// Reset stacked_layout by default (only Layout 9 uses it)
		set("stacked_layout", false);

		if (has_mem and has_net and has_proc) {
			if (net_left and proc_right_pos) {
				// Layout 8: MEM+NET stacked left, PROC right
				set("net_beside_mem", false);
				set("proc_left", false);
				set("proc_full_width", false);
			}
			else if (net_left and proc_wide) {
				// Layout 9: ALL STACKED - MEM top, NET under, PROC wide bottom
				// This is a special layout that requires stacked_layout flag
				set("stacked_layout", true);
				set("net_beside_mem", false);
				set("proc_left", false);
				set("proc_full_width", true);
			}
			else if (net_right and proc_right_pos) {
				// Layout 10: MEM left, NET+PROC stacked right
				set("net_beside_mem", true);
				set("proc_left", false);
				set("proc_full_width", false);
			}
			else {
				// Layout 11: MEM+NET side by side top, PROC wide bottom
				set("net_beside_mem", true);
				set("proc_left", false);
				set("proc_full_width", true);
			}
		}

		if (set_boxes(boxes)) {
			set("shown_boxes", boxes);
			return true;
		}
		return false;
	}

	void lock() {
		atomic_wait(writelock);
		locked = true;
	}

	string validError;

	bool intValid(const std::string_view name, const string& value) {
		//? Use from_chars for safe, exception-free conversion
		int i_value{};
		auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), i_value);
		if (ec == std::errc::invalid_argument) {
			validError = "Invalid numerical value!";
			return false;
		}
		if (ec == std::errc::result_out_of_range) {
			validError = "Value out of range!";
			return false;
		}
		if (ec != std::errc{}) {
			validError = "Invalid numerical value!";
			return false;
		}

		if (name == "update_ms" and i_value < 100)
			validError = "Config value update_ms set too low (<100).";

		else if (name == "update_ms" and i_value > ONE_DAY_MILLIS)
			validError = fmt::format("Config value update_ms set too high (>{}).", ONE_DAY_MILLIS);

		else
			return true;

		return false;
	}

	bool validBoxSizes(const string& boxes) {
		auto min_size = Term::get_min_size(boxes);
		return (Term::width >= min_size.at(0) and Term::height >= min_size.at(1));
	}

	bool stringValid(const std::string_view name, const string& value) {
		if (name == "log_level" and not v_contains(Logger::log_levels, value))
			validError = "Invalid log_level: " + value;

		else if (name == "graph_symbol" and not v_contains(valid_graph_symbols, value))
			validError = "Invalid graph symbol identifier: " + value;

		else if (name.starts_with("graph_symbol_") and (value != "default" and not v_contains(valid_graph_symbols, value)))
			validError = fmt::format("Invalid graph symbol identifier for {}: {}", name, value);

		else if (name == "shown_boxes" and not Global::init_conf) {
			if (value.empty())
				validError = "No boxes selected!";
			else if (not validBoxSizes(value))
				validError = "Terminal too small to display entered boxes!";
			else if (not set_boxes(value))
				validError = "Invalid box name(s) in shown_boxes!";
			else
				return true;
		}

	#ifdef GPU_SUPPORT
		else if (name == "show_gpu_info" and not v_contains(show_gpu_values, value))
			validError = "Invalid value for show_gpu_info: " + value;
	#endif

		else if (name == "presets" and not presetsValid(value))
			return false;

		else if (name == "preset_0" and not value.empty()) {
			// Validate preset_0 - same logic as presetsValid but for a single preset
			for (const auto& box : ssplit(value, ',')) {
				const auto& vals = ssplit(box, ':');
				bool is_mem = (vals.size() > 0 && vals.at(0) == "mem");
				bool is_net = (vals.size() > 0 && vals.at(0) == "net");
				if (is_mem) {
					if (vals.size() < 3 || vals.size() > 8) {
						validError = "Malformatted mem preset in preset_0!";
						return false;
					}
				} else if (is_net) {
					if (vals.size() < 3 || vals.size() > 4) {
						validError = "Malformatted net preset in preset_0!";
						return false;
					}
				} else {
					if (vals.size() != 3) {
						validError = "Malformatted preset in preset_0!";
						return false;
					}
				}
				if (not is_in(vals.at(0), "cpu", "mem", "net", "proc", "gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5", "pwr")) {
					validError = "Invalid box name in preset_0!";
					return false;
				}
				if (not is_in(vals.at(1), "0", "1", "2")) {
					validError = "Invalid position value in preset_0!";
					return false;
				}
				if (not v_contains(valid_graph_symbols_def, vals.at(2))) {
					validError = "Invalid graph symbol in preset_0!";
					return false;
				}
			}
			return true;
		}

		else if (name == "cpu_core_map") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (not isint(map_split.at(0)) or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of cpu_core_map!";
					return false;
				}
			}
			return true;
		}
		else if (name == "io_graph_speeds") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (map_split.at(0).empty() or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of io_graph_speeds!";
					return false;
				}
			}
			return true;
		}

		else
			return true;

		return false;
	}

	string getAsString(const std::string_view name) {
		if (auto it = bools.find(name); it != bools.end())
			return it->second ? "True" : "False";
		if (auto it = ints.find(name); it != ints.end())
			return to_string(it->second);
		if (auto it = strings.find(name); it != strings.end())
			return it->second;
		return "";
	}

	void flip(const std::string_view name) {
		//? First find the key in bools to get a stable string_view (pointing to static storage)
		auto it = bools.find(name);
		if (it == bools.end()) return;  // Key not found, do nothing

		const auto& stable_key = it->first;  // Use the stable key from bools map

		if (_locked(name)) {
			if (boolsTmp.contains(stable_key)) boolsTmp.at(stable_key) = not boolsTmp.at(stable_key);
			else boolsTmp.insert_or_assign(stable_key, (not it->second));
		}
		else it->second = not it->second;
	}

	void unlock() {
		if (not locked) return;
		atomic_wait(Runner::active);
		atomic_lock lck(writelock, true);
		try {
			if (Proc::shown) {
				ints.at("selected_pid") = Proc::selected_pid;
				strings.at("selected_name") = Proc::selected_name;
				ints.at("proc_start") = Proc::start;
				ints.at("proc_selected") = Proc::selected;
				ints.at("selected_depth") = Proc::selected_depth;
			}

			for (auto& item : stringsTmp) {
				strings.at(item.first) = item.second;
			}
			stringsTmp.clear();

			for (auto& item : intsTmp) {
				ints.at(item.first) = item.second;
			}
			intsTmp.clear();

			for (auto& item : boolsTmp) {
				bools.at(item.first) = item.second;
			}
			boolsTmp.clear();
		}
		catch (const std::exception& e) {
			Global::exit_error_msg = "Exception during Config::unlock() : " + string{e.what()};
			clean_quit(1);
		}

		locked = false;
	}

	bool set_boxes(const string& boxes) {
		auto new_boxes = ssplit(boxes);
		for (auto& box : new_boxes) {
			if (not v_contains(valid_boxes, box)) return false;
		#ifdef GPU_SUPPORT
			if (box.starts_with("gpu")) {
				int gpu_num = (box.length() > 3) ? stoi_safe(box.substr(3), -1) + 1 : 0;
				if (gpu_num < 1 or gpu_num > Gpu::count) return false;
			}
		#endif
		}
		current_boxes = std::move(new_boxes);
		return true;
	}

	bool toggle_box(const string& box) {
		auto old_boxes = current_boxes;
		auto box_pos = rng::find(current_boxes, box);
		if (box_pos == current_boxes.end())
			current_boxes.push_back(box);
		else
			current_boxes.erase(box_pos);

		string new_boxes;
		if (not current_boxes.empty()) {
			for (const auto& b : current_boxes) new_boxes += b + ' ';
			new_boxes.pop_back();
		}

		auto min_size = Term::get_min_size(new_boxes);

		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			current_boxes = old_boxes;
			return false;
		}

		Config::set("shown_boxes", new_boxes);
		return true;
	}

	void load(const fs::path& conf_file, vector<string>& load_warnings) {
		std::error_code error;
		if (conf_file.empty())
			return;
		else if (not fs::exists(conf_file, error)) {
			write_new = true;
			return;
		}
		if (error) {
			return;
		}

		std::ifstream cread(conf_file);
		if (cread.good()) {
			vector<string> valid_names;
			valid_names.reserve(descriptions.size());
			for (const auto &n : descriptions)
				valid_names.push_back(n[0]);
			if (string v_string; cread.peek() != '#' or (getline(cread, v_string, '\n') and not v_string.contains(Global::Version)))
				write_new = true;
			while (not cread.eof()) {
				cread >> std::ws;
				if (cread.peek() == '#') {
					cread.ignore(SSmax, '\n');
					continue;
				}
				string name, value;
				getline(cread, name, '=');
				if (name.ends_with(' ')) name = trim(name);
				if (not v_contains(valid_names, name)) {
					cread.ignore(SSmax, '\n');
					continue;
				}
				cread >> std::ws;

				if (bools.contains(name)) {
					cread >> value;
					if (not isbool(value))
						load_warnings.push_back("Got an invalid bool value for config name: " + name);
					else
						bools.at(name) = stobool(value);
				}
				else if (ints.contains(name)) {
					cread >> value;
					if (not isint(value))
						load_warnings.push_back("Got an invalid integer value for config name: " + name);
					else if (not intValid(name, value)) {
						load_warnings.push_back(validError);
					}
					else
						ints.at(name) = stoi_safe(value);
				}
				else if (strings.contains(name)) {
					if (cread.peek() == '"') {
						cread.ignore(1);
						getline(cread, value, '"');
					}
					else cread >> value;

					if (not stringValid(name, value))
						load_warnings.push_back(validError);
					else
						strings.at(name) = value;
				}

				cread.ignore(SSmax, '\n');
			}

			if (not load_warnings.empty()) write_new = true;

			//? Auto-correct shown_gpus on Apple Silicon
			#if defined(__APPLE__) && defined(GPU_SUPPORT)
			if (strings.contains("shown_gpus")) {
				string& gpus = strings.at("shown_gpus");
				//? Check if value contains non-Apple vendors on Apple Silicon
				if (Gpu::apple_silicon_gpu.is_available() and
				    (gpus.find("nvidia") != string::npos or
				     gpus.find("amd") != string::npos or
				     gpus.find("intel") != string::npos)) {
					//? Reset to Apple-only on Apple Silicon
					gpus = "apple";
					write_new = true;
					load_warnings.push_back("Reset shown_gpus to 'apple' for Apple Silicon");
				}
			}
			#endif
		}
	}

	bool acquire_lock() {
		if (conf_dir.empty()) return true;  //? No config dir, can't lock

		lock_file = conf_dir / "mbtop.lock";

		//? Check if lock file exists and if owner process is still alive
		if (fs::exists(lock_file)) {
			std::ifstream lock_read(lock_file);
			if (lock_read.good()) {
				pid_t owner_pid = 0;
				lock_read >> owner_pid;
				lock_read.close();

				//? Check if process is still running (kill with signal 0 just checks existence)
				if (owner_pid > 0 and kill(owner_pid, 0) == 0) {
					//? Lock owner is still alive - we're a secondary instance
					another_instance_running = true;
					Logger::info("Another mbtop instance (PID {}) is running", owner_pid);
					return false;
				}
				//? Lock is stale (owner process died), we can take ownership
				Logger::info("Removing stale lock file from PID {}", owner_pid);
			}
		}

		//? Create/update lock file with our PID
		std::ofstream lock_write(lock_file, std::ios::trunc);
		if (lock_write.good()) {
			lock_write << getpid();
			lock_write.close();
			another_instance_running = false;
			Logger::info("Acquired config lock (PID {})", getpid());
			return true;
		}

		//? Couldn't create lock file
		Logger::warning("Could not create lock file");
		return false;
	}

	void release_lock() {
		if (lock_file.empty()) return;

		//? Only remove lock if we own it (check PID matches)
		if (fs::exists(lock_file)) {
			std::ifstream lock_read(lock_file);
			if (lock_read.good()) {
				pid_t owner_pid = 0;
				lock_read >> owner_pid;
				lock_read.close();

				if (owner_pid == getpid()) {
					std::error_code ec;
					fs::remove(lock_file, ec);
					if (not ec) {
						Logger::info("Released config lock");
					}
				}
			}
		}
	}

	void write() {
		if (conf_file.empty() or not write_new) return;
		//? Primary instance (has lock) always saves
		//? Secondary instances check prevent_autosave setting
		if (another_instance_running and getB("prevent_autosave")) {
			Logger::debug("Skipping config write - secondary instance with prevent_autosave enabled");
			return;
		}
		Logger::debug("Writing new config file");
		if (geteuid() != Global::real_uid and seteuid(Global::real_uid) != 0) return;
		std::ofstream cwrite(conf_file, std::ios::trunc);
		if (not cwrite.good()) {
			Logger::error("Failed to open config file for writing: {}", conf_file);
			return;
		}
		cwrite << current_config();
		if (not cwrite.good()) {
			Logger::error("Failed to write config file: {}", conf_file);
		}
	}

	static auto get_xdg_state_dir() -> std::optional<fs::path> {
		std::optional<fs::path> xdg_state_home;

		{
			const auto* xdg_state_home_ptr = std::getenv("XDG_STATE_HOME");
			if (xdg_state_home_ptr != nullptr) {
				xdg_state_home = std::make_optional(fs::path(xdg_state_home_ptr));
			} else {
				const auto* home_ptr = std::getenv("HOME");
				if (home_ptr != nullptr) {
					xdg_state_home = std::make_optional(fs::path(home_ptr) / ".local" / "state");
				}
			}
		}

		if (xdg_state_home.has_value()) {
			std::error_code err;
			fs::create_directories(xdg_state_home.value(), err);
			if (err) {
				return std::nullopt;
			}
			return xdg_state_home;
		}
		return std::nullopt;
	}

	auto get_log_file() -> std::optional<fs::path> {
		return get_xdg_state_dir().transform([](auto&& state_home) -> auto { return state_home / "mbtop.log"; });
	}

	auto current_config() -> std::string {
		auto buffer = std::string {};
		fmt::format_to(std::back_inserter(buffer), "#? Config file for mbtop v.{}\n", Global::Version);

		for (const auto& [name, description] : descriptions) {
			// Write a description comment if available.
			fmt::format_to(std::back_inserter(buffer), "\n");
			if (!description.empty()) {
				fmt::format_to(std::back_inserter(buffer), "{}\n", description);
			}

			fmt::format_to(std::back_inserter(buffer), "{} = ", name);
			// Lookup default value by name and write it out.
			if (strings.contains(name)) {
				fmt::format_to(std::back_inserter(buffer), R"("{}")", strings[name]);
			} else if (ints.contains(name)) {
				fmt::format_to(std::back_inserter(buffer), std::locale::classic(), "{:L}", ints[name]);
			} else if (bools.contains(name)) {
				fmt::format_to(std::back_inserter(buffer), "{}", bools[name] ? "true" : "false");
			}
			fmt::format_to(std::back_inserter(buffer), "\n");
		}
		return buffer;
	}
}
