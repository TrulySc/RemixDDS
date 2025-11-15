// batch_dds2png.cpp
// Multithreaded DDS → PNG batch converter with a Black Mesa H.E.V theme.
// Calls dds2png_convert() for actual decoding.

#include <filesystem>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

// External converter:
extern "C" int dds2png_convert(const char* inPath, const char* outPath);

// Job entry
struct Job {
    std::string dds;
    std::string png;
};

// Global job queue
std::queue<Job> jobQueue;
std::mutex queueMutex;
std::condition_variable queueCV;

std::atomic<bool> done(false);
std::atomic<int> jobsTotal(0);
std::atomic<int> jobsFinished(0);

// ------------- ANSI COLORS (HEV ORANGE + ACCENTS) -------------
#define ORANGE   "\033[38;2;255;150;30m"
#define YELLOW   "\033[38;2;255;220;0m"
#define GRAY     "\033[38;2;180;180;180m"
#define DIM      "\033[2m"
#define BOLD     "\033[1m"
#define RESET    "\033[0m"
#define CLEARLN  "\33[2K\r"

// ------------- HEV STARTUP SEQUENCE -------------
static void hev_startup()
{
    using namespace std::chrono_literals;
    auto step = [&](const std::string& s) {
        std::cout << ORANGE << "⏻ " << RESET << s << std::flush;
        std::this_thread::sleep_for(200ms);
        std::cout << " " << YELLOW << "OK" << RESET << "\n";
        std::this_thread::sleep_for(130ms);
    };

    std::cout << "\n" << BOLD << ORANGE
    << "═─────────────────────────────────────────────═\n"
    << "       H.E.V MARK IV SUIT SYSTEMS ONLINE\n"
    << "═─────────────────────────────────────────────═"
    << RESET << "\n\n";

    step("INITIALIZING BIOS…");
    step("BOOTING NEURAL INTERFACE…");
    step("CALIBRATING SENSOR ARRAY…");
    step("LOADING TEXTURE DECOMPRESSION MODULES…");
    step("VITAL SIGNS… STABLE");
    step("ENVIRONMENTAL CONTROLS… ONLINE");

    std::cout << "\n" << ORANGE << BOLD
    << "  SYSTEM READY." << RESET << "\n\n";
    std::this_thread::sleep_for(300ms);
}

// ------------- HEV THEMED PROGRESS BAR -------------
static void hev_progress()
{
    int doneCount = jobsFinished.load();
    int total = jobsTotal.load();
    if (total == 0) return;

    float pct = (float)doneCount / (float)total;
    int barWidth = 30;
    int fill = (int)(pct * barWidth);

    std::cout << CLEARLN << ORANGE << "[" << RESET;
    for (int i = 0; i < barWidth; i++) {
        if (i < fill) std::cout << ORANGE << "█" << RESET;
        else          std::cout << GRAY << "░" << RESET;
    }
    std::cout << ORANGE << "] " << RESET;

    std::cout << std::fixed << std::setprecision(1)
    << (pct * 100.0f) << "%  "
    << "(" << doneCount << " / " << total << ")";

    std::cout << "   " << YELLOW << " " << RESET
    << (total - doneCount) << " remaining";

    std::cout << std::flush;
}

// ------------- WORKER THREAD -------------
void workerThread(int id)
{
    for (;;) {
        Job job;

        // fetch job
        {
            std::unique_lock<std::mutex> lk(queueMutex);
            queueCV.wait(lk, [] {
                return !jobQueue.empty() || done.load();
            });

            if (jobQueue.empty()) {
                if (done.load()) return;
                continue;
            }

            job = jobQueue.front();
            jobQueue.pop();
        }

        // process job
        dds2png_convert(job.dds.c_str(), job.png.c_str());
        jobsFinished++;

        hev_progress();
    }
}

// ------------- MAIN -------------
int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << ORANGE
        << " <directory> [threads]\n" << RESET;
        return 1;
    }

    fs::path root = argv[1];
    if (!fs::exists(root)) {
        std::cout << "ERROR: Path does not exist.\n";
        return 1;
    }

    // Optional override thread count
    int threads = (argc >= 3) ? std::stoi(argv[2])
    : (int)std::thread::hardware_concurrency();
    if (threads < 1) threads = 1;

    // HEV boot-up
    hev_startup();

    std::cout << ORANGE << BOLD << " Spawning conversion threads: " << threads
    << RESET << "\n";

    // Scan filesystem
    std::vector<Job> jobs;

    for (auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;

        fs::path p = entry.path();
        if (p.extension() == ".dds" || p.extension() == ".DDS") {
            fs::path out = p;
            out.replace_extension(".png");

            if (fs::exists(out)) continue;

            Job j;
            j.dds = p.string();
            j.png = out.string();
            jobs.push_back(j);
        }
    }

    jobsTotal = (int)jobs.size();

    if (jobsTotal == 0) {
        std::cout << YELLOW << "No DDS files found.\n" << RESET;
        return 0;
    }

    std::cout << ORANGE << " Total DDS files: " << jobsTotal << RESET << "\n\n";

    // Fill queue
    {
        std::lock_guard<std::mutex> lk(queueMutex);
        for (auto& j : jobs)
            jobQueue.push(j);
    }

    // Launch threads
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; i++)
        pool.emplace_back(workerThread, i);

    queueCV.notify_all();

    // Main wait loop
    while (jobsFinished.load() < jobsTotal.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    done = true;
    queueCV.notify_all();

    for (auto& t : pool) t.join();

    std::cout << "\n\n" << BOLD << ORANGE
    << "✔ ALL CONVERSIONS COMPLETE\n"
    << "Thank you for using the H.E.V image conversion subsystem."
    << RESET << "\n";

    return 0;
}
