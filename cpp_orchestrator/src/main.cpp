#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- Configuration ---
const std::string AI_SERVICE_URL = "http://127.0.0.1:8000";

// --- Helper Functions ---
// Executes a command and returns its exit code.
int execute_command(const std::string &command)
{
    std::cout << "  Executing: " << command << std::endl;
    return std::system(command.c_str());
}

// Generates a timestamp string for filenames.
std::string get_timestamp_string()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_path> [clip_duration_seconds]" << std::endl;
        return 1;
    }

    // --- Setup ---
    fs::path video_path(argv[1]);
    double clip_duration = (argc > 2) ? std::stod(argv[2]) : 5.0;

    if (!fs::exists(video_path))
    {
        std::cerr << "Error: Video file not found at " << video_path << std::endl;
        return 1;
    }

    std::cout << "Starting video analysis for: " << video_path << std::endl;
    std::cout << "Clip interval: " << clip_duration << "s" << std::endl;

    // Create a temporary directory for video clips
    fs::path temp_dir = fs::temp_directory_path() / "video_analysis_clips";
    fs::create_directory(temp_dir);
    std::cout << "Temporary directory for clips: " << temp_dir << std::endl;

    // --- Main Processing Loop ---
    double current_time = 0.0;
    int clip_count = 1;
    std::string accumulated_context;
    json scene_updates = json::array();

    // NOTE: This is a simplified way to get video duration.
    // A more robust method would use ffprobe to get the exact duration.
    // For this example, we assume a long video and will break when ffmpeg fails.
    while (true)
    {
        std::cout << "\n--- Processing Clip " << clip_count << " at " << std::fixed << std::setprecision(1) << current_time << "s ---" << std::endl;

        // 1. Extract clip using FFmpeg
        fs::path clip_path = temp_dir / ("clip_" + std::to_string(clip_count) + ".mp4");
        std::stringstream ffmpeg_cmd;
        ffmpeg_cmd << "ffmpeg -y -ss " << current_time << " -i \"" << video_path.string()
                   << "\" -t " << clip_duration << " -c copy -avoid_negative_ts make_zero \""
                   << clip_path.string() << "\" 2>nul"; // 2>nul suppresses ffmpeg stderr

        if (execute_command(ffmpeg_cmd.str()) != 0 || !fs::exists(clip_path) || fs::file_size(clip_path) == 0)
        {
            std::cout << "FFmpeg failed to extract clip, or clip is empty. Reached end of video." << std::endl;
            break;
        }

        // 2. Call AI Service to analyze the clip
        std::cout << "  Sending clip to AI service for analysis..." << std::endl;
        json clip_req_body = {
            {"clip_path", clip_path.string()},
            {"prompt", "Describe what is happening in this video clip in detail. Focus on actions, objects, people, and any unusual events."}};
        cpr::Response r_clip = cpr::Post(cpr::Url{AI_SERVICE_URL + "/analyze_clip"},
                                         cpr::Body{clip_req_body.dump()},
                                         cpr::Header{{"Content-Type", "application/json"}},
                                         cpr::Timeout{300000}); // 5 min timeout

        if (r_clip.status_code != 200)
        {
            std::cerr << "  Error from /analyze_clip: " << r_clip.status_code << " - " << r_clip.text << std::endl;
            current_time += clip_duration;
            clip_count++;
            fs::remove(clip_path);
            continue;
        }
        json clip_res_body = json::parse(r_clip.text);
        std::string videollama_caption = clip_res_body["caption"];
        std::cout << "  Video-LLaMA Caption: " << videollama_caption.substr(0, 100) << "..." << std::endl;

        // 3. Call AI service to refine the analysis
        std::cout << "  Sending caption for refinement..." << std::endl;
        json refine_req_body = {
            {"videollama_caption", videollama_caption},
            {"timestamp", current_time},
            {"context", accumulated_context}};
        cpr::Response r_refine = cpr::Post(cpr::Url{AI_SERVICE_URL + "/refine_analysis"},
                                           cpr::Body{refine_req_body.dump()},
                                           cpr::Header{{"Content-Type", "application/json"}},
                                           cpr::Timeout{120000}); // 2 min timeout

        if (r_refine.status_code != 200)
        {
            std::cerr << "  Error from /refine_analysis: " << r_refine.status_code << " - " << r_refine.text << std::endl;
        }
        else
        {
            json refine_res_body = json::parse(r_refine.text);
            json refined_analysis = json::parse(refine_res_body["refined_analysis"].get<std::string>());

            std::cout << "  Refined Description: " << refined_analysis["description"].get<std::string>() << std::endl;
            std::cout << "  Anomaly Score: " << refined_analysis["anomaly_score"].get<double>() << std::endl;

            scene_updates.push_back(refined_analysis);
            accumulated_context += "[" + std::to_string(current_time) + "s] " + refined_analysis.dump() + "\n";
        }

        // Cleanup
        fs::remove(clip_path);
        current_time += clip_duration;
        clip_count++;
    }

    // --- Final Report ---
    std::cout << "\n--- Generating Final Report ---" << std::endl;
    json summary_req_body = {{"full_context", accumulated_context}};
    cpr::Response r_summary = cpr::Post(cpr::Url{AI_SERVICE_URL + "/summarize_report"},
                                        cpr::Body{summary_req_body.dump()},
                                        cpr::Header{{"Content-Type", "application/json"}},
                                        cpr::Timeout{120000});

    json final_summary = "{}";
    if (r_summary.status_code == 200)
    {
        final_summary = json::parse(json::parse(r_summary.text)["final_summary"].get<std::string>());
        std::cout << "  Final summary received from AI service." << std::endl;
    }
    else
    {
        std::cerr << "  Failed to generate final summary: " << r_summary.status_code << " - " << r_summary.text << std::endl;
    }

    json final_report = {
        {"video_name", video_path.filename().string()},
        {"analysis_timestamp_utc", get_timestamp_string()},
        {"scene_updates", scene_updates},
        {"final_summary", final_summary}};

    // Save report to file
    fs::create_directory("reports");
    fs::path report_path = fs::path("reports") / (video_path.stem().string() + "_" + get_timestamp_string() + ".json");
    std::ofstream report_file(report_path);
    report_file << std::setw(4) << final_report << std::endl;
    report_file.close();

    std::cout << "\nAnalysis complete. Report saved to: " << report_path << std::endl;

    // Cleanup temp directory
    fs::remove_all(temp_dir);
    std::cout << "Cleaned up temporary directory." << std::endl;

    return 0;
}