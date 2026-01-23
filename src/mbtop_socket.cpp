/* Copyright 2024 mbtop contributors

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

#include "mbtop_socket.hpp"
#include "mbtop_log.hpp"
#include "mbtop_config.hpp"
#include "mbtop_draw.hpp"
#include "mbtop_shared.hpp"
#include "mbtop_input.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace fs = std::filesystem;

using std::string;
using std::to_string;

namespace Socket {

	string socket_path;
	std::atomic<bool> running{false};
	std::atomic<bool> command_pending{false};
	std::atomic<CommandType> pending_command{CommandType::None};
	CommandParams params;
	string state_response;
	std::atomic<bool> response_ready{false};

	//? Internal state
	static int server_fd = -1;
	static std::thread listener_thread;
	static std::mutex params_mutex;
	static std::mutex response_mutex;
	static std::condition_variable response_cv;

	string get_socket_path() {
		if (socket_path.empty()) {
			socket_path = (fs::path(Config::conf_dir) / "mbtop.sock").string();
		}
		return socket_path;
	}

	//? Simple JSON parsing (no external dependencies)
	//? Format: {"cmd": "show_logs", "name": "...", "command": "...", "filter": "..."}
	static string parse_json_value(const string& json, const string& key) {
		string search = "\"" + key + "\"";
		size_t pos = json.find(search);
		if (pos == string::npos) return "";

		pos = json.find(':', pos);
		if (pos == string::npos) return "";

		pos = json.find('"', pos);
		if (pos == string::npos) return "";

		size_t start = pos + 1;
		size_t end = json.find('"', start);
		if (end == string::npos) return "";

		return json.substr(start, end - start);
	}

	static int parse_json_int(const string& json, const string& key) {
		string search = "\"" + key + "\"";
		size_t pos = json.find(search);
		if (pos == string::npos) return 0;

		pos = json.find(':', pos);
		if (pos == string::npos) return 0;

		// Skip whitespace
		pos++;
		while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

		// Read number
		string num;
		while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) {
			num += json[pos++];
		}

		return num.empty() ? 0 : std::stoi(num);
	}

	//? Generate state JSON response
	static string generate_state_json() {
		string json = "{";
		json += "\"logs_shown\": " + string(Logs::shown ? "true" : "false") + ", ";
		json += "\"logs_below\": " + string(Config::getB("logs_below_proc") ? "true" : "false") + ", ";
		json += "\"proc_shown\": " + string(Proc::shown ? "true" : "false") + ", ";
		json += "\"current_pid\": " + to_string(Logs::current_pid) + ", ";
		json += "\"current_name\": \"" + Logs::current_name + "\", ";
		json += "\"filter_tagged\": " + string(Proc::filter_tagged ? "true" : "false") + ", ";
		json += "\"followed_pid\": " + to_string(Config::getI("followed_pid")) + ", ";
		json += "\"selected_pid\": " + to_string(Config::getI("selected_pid"));
		json += "}";
		return json;
	}

	//? Handle a single client connection
	static void handle_client(int client_fd) {
		char buffer[4096];
		ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
		if (bytes <= 0) {
			close(client_fd);
			return;
		}
		buffer[bytes] = '\0';
		string request(buffer);

		Logger::debug("Socket: Received command: {}", request);

		// Parse JSON command
		string cmd = parse_json_value(request, "cmd");
		string response = "{\"status\": \"error\", \"message\": \"unknown command\"}";

		if (cmd == "show_logs") {
			{
				std::lock_guard<std::mutex> lock(params_mutex);
				params.process_name = parse_json_value(request, "name");
				params.process_command = parse_json_value(request, "command");
				params.pid = parse_json_int(request, "pid");
			}
			pending_command.store(CommandType::ShowLogs);
			command_pending.store(true);
			Input::interrupt();  //? Wake up the main loop
			response = "{\"status\": \"ok\", \"message\": \"show_logs queued\"}";

		} else if (cmd == "show_logs_below") {
			{
				std::lock_guard<std::mutex> lock(params_mutex);
				params.process_name = parse_json_value(request, "name");
				params.process_command = parse_json_value(request, "command");
				params.pid = parse_json_int(request, "pid");
			}
			pending_command.store(CommandType::ShowLogsBelow);
			command_pending.store(true);
			Input::interrupt();
			response = "{\"status\": \"ok\", \"message\": \"show_logs_below queued\"}";

		} else if (cmd == "hide_logs") {
			pending_command.store(CommandType::HideLogs);
			command_pending.store(true);
			Input::interrupt();
			response = "{\"status\": \"ok\", \"message\": \"hide_logs queued\"}";

		} else if (cmd == "select_process") {
			{
				std::lock_guard<std::mutex> lock(params_mutex);
				params.process_name = parse_json_value(request, "name");
				params.process_command = parse_json_value(request, "command");
				params.pid = parse_json_int(request, "pid");
			}
			pending_command.store(CommandType::SelectProcess);
			command_pending.store(true);
			Input::interrupt();
			response = "{\"status\": \"ok\", \"message\": \"select_process queued\"}";

		} else if (cmd == "activate_process_log") {
			{
				std::lock_guard<std::mutex> lock(params_mutex);
				params.process_name = parse_json_value(request, "name");
				params.process_command = parse_json_value(request, "command");
				params.pid = parse_json_int(request, "pid");
			}
			pending_command.store(CommandType::ActivateProcessLog);
			command_pending.store(true);
			Input::interrupt();
			response = "{\"status\": \"ok\", \"message\": \"activate_process_log queued\"}";

		} else if (cmd == "set_log_filter") {
			{
				std::lock_guard<std::mutex> lock(params_mutex);
				params.filter_text = parse_json_value(request, "filter");
			}
			pending_command.store(CommandType::SetLogFilter);
			command_pending.store(true);
			Input::interrupt();
			response = "{\"status\": \"ok\", \"message\": \"set_log_filter queued\"}";

		} else if (cmd == "get_state") {
			// For get_state, we need to wait for main thread to generate response
			response_ready.store(false);
			pending_command.store(CommandType::GetState);
			command_pending.store(true);
			Input::interrupt();

			// Wait for response (timeout 2s)
			{
				std::unique_lock<std::mutex> lock(response_mutex);
				if (response_cv.wait_for(lock, std::chrono::seconds(2), []{ return response_ready.load(); })) {
					response = state_response;
				} else {
					response = "{\"status\": \"error\", \"message\": \"timeout waiting for state\"}";
				}
			}
		}

		// Send response
		write(client_fd, response.c_str(), response.size());
		close(client_fd);
	}

	//? Listener thread function
	static void listener_loop() {
		Logger::info("Socket: Listener started on {}", get_socket_path());

		while (running.load()) {
			struct pollfd pfd;
			pfd.fd = server_fd;
			pfd.events = POLLIN;

			int ret = poll(&pfd, 1, 500);  // 500ms timeout for checking running flag
			if (ret < 0) {
				if (errno == EINTR) continue;
				Logger::error("Socket: poll error: {}", strerror(errno));
				break;
			}

			if (ret == 0) continue;  // Timeout, check running flag

			if (pfd.revents & POLLIN) {
				int client_fd = accept(server_fd, nullptr, nullptr);
				if (client_fd >= 0) {
					// Set client socket to blocking mode for reliable communication
					int flags = fcntl(client_fd, F_GETFL, 0);
					fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);
					handle_client(client_fd);
				}
			}
		}

		Logger::info("Socket: Listener stopped");
	}

	void start() {
		if (running.load()) return;

		string path = get_socket_path();

		// Remove existing socket file
		if (fs::exists(path)) {
			fs::remove(path);
		}

		// Create socket
		server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_fd < 0) {
			Logger::error("Socket: Failed to create socket: {}", strerror(errno));
			return;
		}

		// Set non-blocking (for clean shutdown)
		int flags = fcntl(server_fd, F_GETFL, 0);
		fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

		// Bind
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

		if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			Logger::error("Socket: Failed to bind: {}", strerror(errno));
			close(server_fd);
			server_fd = -1;
			return;
		}

		// Listen
		if (listen(server_fd, 5) < 0) {
			Logger::error("Socket: Failed to listen: {}", strerror(errno));
			close(server_fd);
			server_fd = -1;
			fs::remove(path);
			return;
		}

		// Set permissions (owner only)
		fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);

		// Start listener thread
		running.store(true);
		listener_thread = std::thread(listener_loop);

		Logger::info("Socket: Server started on {}", path);
	}

	void stop() {
		if (not running.load()) return;

		running.store(false);

		// Close server socket to unblock accept
		if (server_fd >= 0) {
			close(server_fd);
			server_fd = -1;
		}

		// Wait for listener thread
		if (listener_thread.joinable()) {
			listener_thread.join();
		}

		// Remove socket file
		string path = get_socket_path();
		if (fs::exists(path)) {
			fs::remove(path);
		}

		Logger::info("Socket: Server stopped");
	}

	bool process_pending_command() {
		if (not command_pending.load()) return false;

		CommandType cmd = pending_command.load();
		CommandParams local_params;
		{
			std::lock_guard<std::mutex> lock(params_mutex);
			local_params = params;
		}

		bool processed = true;

		switch (cmd) {
			case CommandType::ShowLogs:
			case CommandType::ShowLogsBelow:
			case CommandType::ActivateProcessLog: {
				if (not Proc::shown) {
					Logger::warning("Socket: Cannot show logs - Proc panel not visible");
					break;
				}

				// Find and select the process if specified
				if (not local_params.process_name.empty() or local_params.pid > 0) {
					// Search for the process in current_procs
					bool found = false;
					for (const auto& p : Proc::current_procs) {
						bool match = false;
						if (local_params.pid > 0 && p.pid == static_cast<size_t>(local_params.pid)) {
							match = true;
						} else if (not local_params.process_name.empty() && p.name == local_params.process_name) {
							// If command specified, also check command
							if (local_params.process_command.empty() or p.cmd == local_params.process_command) {
								match = true;
							}
						}

						if (match) {
							// Select this process
							Config::set("selected_pid", (int)p.pid);
							Config::set("follow_process", true);
							Config::set("followed_pid", (int)p.pid);
							Config::set("update_following", true);
							Proc::selected_name = p.name;
							Proc::selected_cmd = p.cmd;
							found = true;
							Logger::debug("Socket: Selected process {} ({})", p.name, p.pid);
							break;
						}
					}

					if (not found) {
						Logger::warning("Socket: Process not found: {} (pid={})", local_params.process_name, local_params.pid);
					}
				}

				// Show logs panel
				if (cmd == CommandType::ShowLogs or cmd == CommandType::ActivateProcessLog) {
					Config::set("logs_below_proc", false);
				} else {
					Config::set("logs_below_proc", true);
				}

				Logs::current_pid = Config::getI("followed_pid");
				Logs::current_name = Proc::selected_name;
				Logs::current_cmdline = Proc::selected_cmd;
				Logs::paused = false;
				Logs::clear();
				Logs::shown = true;

				Config::current_preset = -1;
				Draw::calcSizes();
				Runner::run("all", false, true);

				Logger::debug("Socket: Logs panel shown");
				break;
			}

			case CommandType::HideLogs: {
				if (Logs::shown) {
					Logs::shown = false;
					Config::set("logs_below_proc", false);
					Input::mouse_mappings.erase("logs_pause");
					Input::mouse_mappings.erase("logs_export");
					Input::mouse_mappings.erase("logs_filter");
					Input::mouse_mappings.erase("logs_sort");
					Input::mouse_mappings.erase("logs_buffer");

					Config::current_preset = -1;
					Draw::calcSizes();
					Runner::run("all", false, true);

					Logger::debug("Socket: Logs panel hidden");
				}
				break;
			}

			case CommandType::SelectProcess: {
				if (not Proc::shown) {
					Logger::warning("Socket: Cannot select process - Proc panel not visible");
					break;
				}

				bool found = false;
				for (const auto& p : Proc::current_procs) {
					bool match = false;
					if (local_params.pid > 0 && p.pid == static_cast<size_t>(local_params.pid)) {
						match = true;
					} else if (not local_params.process_name.empty() && p.name == local_params.process_name) {
						if (local_params.process_command.empty() or p.cmd == local_params.process_command) {
							match = true;
						}
					}

					if (match) {
						Config::set("selected_pid", (int)p.pid);
						Proc::selected_name = p.name;
						Proc::selected_cmd = p.cmd;
						found = true;
						Logger::debug("Socket: Selected process {} ({})", p.name, p.pid);
						break;
					}
				}

				if (not found) {
					Logger::warning("Socket: Process not found: {} (pid={})", local_params.process_name, local_params.pid);
				}

				Runner::run("all", false, true);
				break;
			}

			case CommandType::SetLogFilter: {
				// Set the level filter based on filter text
				// For now, support: "all", "debug", "info", "error", "fault"
				string filter = local_params.filter_text;
				std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				if (filter == "all") {
					Logs::level_filter = 0x1F;
				} else if (filter == "debug") {
					Logs::level_filter = 0x05;  // Debug + Default
				} else if (filter == "info") {
					Logs::level_filter = 0x02;
				} else if (filter == "error") {
					Logs::level_filter = 0x08;
				} else if (filter == "fault") {
					Logs::level_filter = 0x10;
				}

				Logs::redraw = true;
				Runner::run("all", false, true);

				Logger::debug("Socket: Log filter set to: {}", filter);
				break;
			}

			case CommandType::GetState: {
				{
					std::lock_guard<std::mutex> lock(response_mutex);
					state_response = generate_state_json();
					response_ready.store(true);
				}
				response_cv.notify_one();
				break;
			}

			default:
				processed = false;
				break;
		}

		// Reset flags
		command_pending.store(false);
		pending_command.store(CommandType::None);

		return processed;
	}

}  // namespace Socket
