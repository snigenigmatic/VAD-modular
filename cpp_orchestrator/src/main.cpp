#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <regex>
#include <thread>

#include <cpr/cpr.h>
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- Configuration ---
struct Config {
    std::string ai_service_url = "http://127.0.0.1:8000";
    int max_retries = 3;
    int retry_delay_ms = 1000;
    size_t max_context_size = 50000;
    int connection_timeout_ms = 30000;
    int analyze_timeout_ms = 300000;
    int refine_timeout_ms = 120000;
};

// --- Helper Functions ---

std::string escape_shell_arg(const std::string& arg) {
    std::string escaped = arg;
    std::regex special_chars(R"([;&|`$(){}[\]<>*?!'\"\\])");
    escaped = std::regex_replace(escaped, special_chars, "\\$&");
    return "\"" + escaped + "\"";
}

bool is_valid_video_path(const fs::path& path) {
    if (!fs::exists(path) || !fs::is_regular_file(path)) return false;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    const std::vector<std::string> valid_extensions = {".mp4", ".avi", ".mov", ".mkv", ".wmv", ".flv", ".webm", ".m4v"};
    return std::find(valid_extensions.begin(), valid_extensions.end(), ext) != valid_extensions.end();
}

bool check_dependencies() {
    if (std::system("ffmpeg -version > /dev/null 2>&1") != 0) {
        std::cerr << "Error: FFmpeg is not installed or not in PATH" << std::endl;
        return false;
    }
    if (std::system("ffprobe -version > /dev/null 2>&1") != 0) {
        std::cerr << "Error: FFprobe is not installed or not in PATH" << std::endl;
        return false;
    }
    return true;
}

bool check_ai_service(const Config& config) {
    try {
        cpr::Response r = cpr::Get(cpr::Url{config.ai_service_url + "/health"}, cpr::Timeout{config.connection_timeout_ms});
        return r.status_code > 0;
    } catch (const std::exception& e) {
        std::cerr << "AI service check failed: " << e.what() << std::endl;
        return false;
    }
}

std::pair<int, std::string> execute_command_with_output(const std::string& command) {
    std::cout << "  Executing: " << command << std::endl;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {-1, "Failed to execute command"};
    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    return {pclose(pipe), result};
}

int execute_command(const std::string& command) {
    return execute_command_with_output(command).first;
}

std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

json safe_json_parse(const std::string& json_string) {
    try {
        return json::parse(json_string);
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        throw std::runtime_error("Failed to parse JSON response");
    }
}

template<typename T>
T safe_json_get(const json& j, const std::string& key, const T& default_value) {
    try {
        return j.contains(key) ? j[key].get<T>() : default_value;
    } catch (const std::exception& e) {
        std::cerr << "JSON access error for key '" << key << "': " << e.what() << std::endl;
        return default_value;
    }
}

cpr::Response make_request_with_retry(const std::function<cpr::Response()>& request_func, const Config& config) {
    cpr::Response response;
    for (int attempt = 0; attempt < config.max_retries; ++attempt) {
        try {
            response = request_func();
            if (response.status_code == 200) return response;
            std::cerr << "  Request failed (attempt " << (attempt + 1) << "/" << config.max_retries << "): " << response.status_code << " - " << response.text << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  Request exception (attempt " << (attempt + 1) << "/" << config.max_retries << "): " << e.what() << std::endl;
        }
        if (attempt < config.max_retries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
        }
    }
    response.status_code = 0;
    response.text = "Max retries exceeded";
    return response;
}

std::string trim_context(const std::string& context, size_t max_size) {
    if (context.length() <= max_size) return context;
    size_t start_pos = context.length() - max_size;
    size_t newline_pos = context.find('\n', start_pos);
    if (newline_pos != std::string::npos && newline_pos < start_pos + 1000) {
        start_pos = newline_pos + 1;
    }
    return "[...context trimmed...]\n" + context.substr(start_pos);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_path> [clip_duration_seconds]" << std::endl;
        return 1;
    }

    Config config;
    fs::path video_path(argv[1]);
    double clip_duration = 5.0;
    
    if (argc > 2) {
        try {
            clip_duration = std::stod(argv[2]);
            if (clip_duration <= 0) throw std::invalid_argument("Clip duration must be positive");
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid clip duration '" << argv[2] << "': " << e.what() << std::endl;
            return 1;
        }
    }

    if (!is_valid_video_path(video_path) || !check_dependencies() || !check_ai_service(config)) {
        return 1;
    }

    std::cout << "Starting video analysis for: " << video_path << std::endl;
    std::cout << "Clip interval: " << clip_duration << "s" << std::endl;

    fs::path temp_dir = fs::temp_directory_path() / ("video_analysis_clips_" + get_timestamp_string());
    if (!fs::create_directories(temp_dir)) {
        std::cerr << "Error creating temporary directory: " << temp_dir << std::endl;
        return 1;
    }
    std::cout << "Temporary directory for clips: " << temp_dir << std::endl;

    double video_duration = 0.0;
    {
        auto [exit_code, output] = execute_command_with_output("ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 " + escape_shell_arg(video_path.string()));
        if (exit_code != 0) { std::cerr << "Failed to get video duration." << std::endl; fs::remove_all(temp_dir); return 1; }
        try { video_duration = std::stod(output); } catch (...) { std::cerr << "Failed to parse video duration." << std::endl; fs::remove_all(temp_dir); return 1; }
    }
    std::cout << "Video duration: " << video_duration << "s" << std::endl;

    double current_time = 0.0;
    int clip_count = 1;
    std::string accumulated_context;
    json scene_updates = json::array();

    while (current_time < video_duration) {
        std::cout << "\n--- Processing Clip " << clip_count << " at " << std::fixed << std::setprecision(1) << current_time << "s ---" << std::endl;
        fs::path clip_path = temp_dir / ("clip_" + std::to_string(clip_count) + ".mp4");
        
        std::stringstream ffmpeg_cmd;
        ffmpeg_cmd << "ffmpeg -y -ss " << current_time << " -i " << escape_shell_arg(video_path.string()) << " -t " << clip_duration << " -c copy -avoid_negative_ts make_zero " << escape_shell_arg(clip_path.string()) << " 2>/dev/null";

        if (execute_command(ffmpeg_cmd.str()) != 0 || !fs::exists(clip_path) || fs::file_size(clip_path) == 0) {
            std::cout << "FFmpeg failed or clip is empty. Reached end of video." << std::endl;
            break;
        }

        std::cout << "  Sending clip to AI service for analysis..." << std::endl;
        cpr::Multipart multipart_data = {
            {"prompt", "Describe what is happening in this video clip in detail. Focus on actions, objects, people, and any unusual events."},
            {"video_file", cpr::File{clip_path.string()}}
        };
        
        cpr::Response r_clip = make_request_with_retry([&]() {
            return cpr::Post(cpr::Url{config.ai_service_url + "/analyze_clip"}, multipart_data, cpr::Timeout{config.analyze_timeout_ms});
        }, config);

        if (r_clip.status_code != 200) {
            std::cerr << "  Error from /analyze_clip after retries: " << r_clip.status_code << " - " << r_clip.text << std::endl;
        } else {
            try {
                json clip_res_body = safe_json_parse(r_clip.text);
                std::string videollama_caption = safe_json_get<std::string>(clip_res_body, "caption", "");
                if (videollama_caption.empty()) { std::cerr << "  No caption received" << std::endl; }
                else {
                    std::cout << "  Video-LLaMA Caption: " << videollama_caption.substr(0, 100) << "..." << std::endl;
                    std::cout << "  Sending caption for refinement..." << std::endl;

                    json refine_req_body = {
                        {"videollama_caption", videollama_caption},
                        {"timestamp", current_time},
                        {"context", trim_context(accumulated_context, config.max_context_size)}
                    };

                    cpr::Response r_refine = make_request_with_retry([&]() {
                        return cpr::Post(cpr::Url{config.ai_service_url + "/refine_analysis"}, cpr::Body{refine_req_body.dump()}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Timeout{config.refine_timeout_ms});
                    }, config);

                    if (r_refine.status_code == 200) {
                        json refine_res_body = safe_json_parse(r_refine.text);
                        json refined_analysis = safe_json_parse(safe_json_get<std::string>(refine_res_body, "refined_analysis", "{}"));
                        
                        std::cout << "  Refined Description: " << safe_json_get<std::string>(refined_analysis, "description", "N/A") << std::endl;
                        std::cout << "  Anomaly Score: " << safe_json_get<double>(refined_analysis, "anomaly_score", 0.0) << std::endl;
                        
                        scene_updates.push_back(refined_analysis);
                        accumulated_context += "[" + std::to_string(current_time) + "s] " + refined_analysis.dump() + "\n";
                    } else {
                        std::cerr << "  Error from /refine_analysis after retries: " << r_refine.status_code << " - " << r_refine.text << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error processing AI service response: " << e.what() << std::endl;
            }
        }

        fs::remove(clip_path);
        current_time += clip_duration;
        clip_count++;
    }

    std::cout << "\n--- Generating Final Report ---" << std::endl;
    json final_summary = {{"error", "No content processed to generate a summary."}};
    if (!accumulated_context.empty()) {
        json summary_req_body = {{"full_context", trim_context(accumulated_context, config.max_context_size)}};
        cpr::Response r_summary = make_request_with_retry([&]() {
            return cpr::Post(cpr::Url{config.ai_service_url + "/summarize_report"}, cpr::Body{summary_req_body.dump()}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Timeout{config.refine_timeout_ms});
        }, config);

        if (r_summary.status_code == 200) {
            try {
                final_summary = safe_json_parse(safe_json_get<std::string>(safe_json_parse(r_summary.text), "final_summary", "{}"));
                std::cout << "  Final summary received." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "  Error parsing final summary: " << e.what() << std::endl;
                final_summary = {{"error", "Failed to parse final summary from AI."}};
            }
        } else {
            std::cerr << "  Failed to generate final summary: " << r_summary.status_code << " - " << r_summary.text << std::endl;
            final_summary = {{"error", "Failed to generate final summary from AI."}};
        }
    }
    
    json final_report = {
        {"video_name", video_path.filename().string()},
        {"analysis_timestamp_utc", get_timestamp_string()},
        {"scene_updates", scene_updates},
        {"final_summary", final_summary}
    };

    fs::create_directories("reports");
    fs::path report_path = fs::path("reports") / (video_path.stem().string() + "_" + get_timestamp_string() + ".json");
    std::ofstream report_file(report_path);
    report_file << std::setw(4) << final_report << std::endl;
    
    std::cout << "\nAnalysis complete. Report saved to: " << report_path << std::endl;
    fs::remove_all(temp_dir);
    std::cout << "Cleaned up temporary directory." << std::endl;

    return 0;
}