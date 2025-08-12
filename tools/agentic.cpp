// agentic.cpp
// A self-improving agent that gradually shifts focus from improving itself
// to improving NitrOS as it accumulates consecutive successes.
//
// Build:  g++ -std=gnu++17 -O2 -pthread -o agentic agentic.cpp
// Usage:  ./agentic --max-iterations 200 --seed 42 --dry-run
//
// Integration hints (optional):
//  - Place this file under tools/ or agents/ in your repo.
//  - Provide scripts the agent can call:
//      scripts/build_nitros.sh   -> builds NitrOS; exit 0 on success
//      scripts/test_nitros.sh    -> runs tests;  exit 0 on success
//      scripts/self_tests.sh     -> agent self-checks; exit 0 on success
//  - The agent keeps state in .agentic/state.txt and logs in .agentic/logs/
//
// This program uses only the C++ standard library.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// ----------------------------- Config ----------------------------------
struct Config {
    int maxIterations = 1000;
    unsigned int seed = std::random_device{}();
    bool dryRun = false;            // if true, do not execute external commands
    bool verbose = true;            // extra logs
    int streakToShift = 5;          // after ~5 consecutive successes, mostly focus NitrOS
    double minSelfFocus = 0.10;     // never fully stop self-improvement
    double exploration = 0.10;      // probability to explore the other focus
    std::string stateDir = ".agentic";
    std::string patchDir = "agent/patches";
    std::string logDir = ".agentic/logs";
    std::string nitrosBuild = "scripts/build_nitros.sh";
    std::string nitrosTest  = "scripts/test_nitros.sh";
    std::string selfTest    = "scripts/self_tests.sh";
};

// ----------------------------- State -----------------------------------
struct State {
    int successStreak = 0;      // consecutive successes
    int totalSuccess = 0;       // cumulative successes
    int totalFailure = 0;       // cumulative failures
    int iteration = 0;          // last completed iteration
    // tunables the agent can tweak during self-improvement
    double exploration = 0.10;
    double minSelfFocus = 0.10;
    int streakToShift = 5;
};

static std::atomic<bool> g_stop{false};

void on_sigint(int){ g_stop = true; }

// ----------------------------- Utilities --------------------------------
static inline std::string now_ts(){
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss; oss<<std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

static int run_cmd(const std::string& cmd, bool dryRun, const std::string& logFile){
    std::string final = cmd + " >" + logFile + " 2>&1";
    if (dryRun) {
        std::cerr << "[dry-run] would exec: " << final << "\n";
        return 0;
    }
    std::cerr << "[exec] " << cmd << " (logging to " << logFile << ")\n";
    int rc = std::system(final.c_str());
    if (rc == -1) return 127; // failed to spawn shell
#ifdef _WIN32
    return rc;
#else
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    if (WIFSIGNALED(rc)) return 128 + WTERMSIG(rc);
    return rc;
#endif
}

static void ensure_dirs(const Config& cfg){
    fs::create_directories(cfg.stateDir);
    fs::create_directories(cfg.logDir);
}

static fs::path state_path(const Config& cfg){ return fs::path(cfg.stateDir)/"state.txt"; }

static void save_state(const Config& cfg, const State& st){
    ensure_dirs(cfg);
    std::ofstream f(state_path(cfg));
    f << "successStreak=" << st.successStreak << "\n";
    f << "totalSuccess="  << st.totalSuccess  << "\n";
    f << "totalFailure="  << st.totalFailure  << "\n";
    f << "iteration="     << st.iteration     << "\n";
    f << "exploration="    << st.exploration   << "\n";
    f << "minSelfFocus="   << st.minSelfFocus  << "\n";
    f << "streakToShift="  << st.streakToShift << "\n";
}

static State load_state(const Config& cfg){
    State st{};
    std::ifstream f(state_path(cfg));
    if(!f) return st; // default
    std::string line;
    while(std::getline(f, line)){
        auto pos = line.find('=');
        if (pos==std::string::npos) continue;
        std::string k = line.substr(0,pos);
        std::string v = line.substr(pos+1);
        auto as_int = [&](const std::string& s){ return std::stoi(s); };
        auto as_double = [&](const std::string& s){ return std::stod(s); };
        if(k=="successStreak") st.successStreak = as_int(v);
        else if(k=="totalSuccess") st.totalSuccess = as_int(v);
        else if(k=="totalFailure") st.totalFailure = as_int(v);
        else if(k=="iteration") st.iteration = as_int(v);
        else if(k=="exploration") st.exploration = as_double(v);
        else if(k=="minSelfFocus") st.minSelfFocus = as_double(v);
        else if(k=="streakToShift") st.streakToShift = as_int(v);
    }
    return st;
}

// Weighted coin flip
static bool weighted_pick(double weight, std::mt19937& rng){
    std::uniform_real_distribution<double> U(0.0, 1.0);
    return U(rng) < weight;
}

// ----------------------- Patch Management (optional) ---------------------
static std::optional<fs::path> next_patch(const Config& cfg){
    if(!fs::exists(cfg.patchDir)) return std::nullopt;
    std::vector<fs::path> patches;
    for (auto& p : fs::directory_iterator(cfg.patchDir)){
        if (p.is_regular_file() && p.path().extension()==".patch") patches.push_back(p.path());
    }
    if (patches.empty()) return std::nullopt;
    std::sort(patches.begin(), patches.end());
    return patches.front();
}

static bool apply_patch(const fs::path& patch, bool dryRun, const std::string& logFile){
    std::ostringstream cmd;
    // Try `git apply` then `patch` as fallback
    cmd << "(git apply \"" << patch.string() << "\" || patch -p1 < \"" << patch.string() << "\")";
    return run_cmd(cmd.str(), dryRun, logFile) == 0;
}

// ----------------------------- Tasks ------------------------------------
struct TaskResult { bool success; std::string detail; };

static TaskResult task_self_improve(const Config& cfg, State& st, std::mt19937& rng){
    // Adjust internal tunables based on outcomes to improve future decisions.
    std::uniform_real_distribution<double> J(-0.05, 0.05);

    double newExploration = std::clamp(st.exploration + J(rng), 0.02, 0.25);
    double newMinSelf     = std::clamp(st.minSelfFocus + J(rng), 0.05, 0.30);
    int newShift          = std::clamp(st.streakToShift + (int)std::round(J(rng)*10), 3, 12);

    bool changed = (newExploration!=st.exploration) || (newMinSelf!=st.minSelfFocus) || (newShift!=st.streakToShift);

    // Optionally run a self-test script to validate the new params improve outcomes.
    std::string logfile = (fs::path(cfg.logDir)/("self_"+now_ts()+".log")).string();
    int rc = 0;
    if (fs::exists(cfg.selfTest)) {
        // Provide tunables as env vars to the script so it can score them.
        std::ostringstream cmd;
        cmd << "export AGENT_EXPLORATION=" << newExploration
            << " AGENT_MIN_SELF_FOCUS=" << newMinSelf
            << " AGENT_STREAK_TO_SHIFT=" << newShift
            << "; bash \"" << cfg.selfTest << "\"";
        rc = run_cmd(cmd.str(), cfg.dryRun, logfile);
    }

    bool ok = (rc==0); // treat 0 as improvement or acceptable config
    if (ok && changed) {
        st.exploration = newExploration;
        st.minSelfFocus = newMinSelf;
        st.streakToShift = newShift;
        return {true, "Adjusted tunables and passed self-tests"};
    }
    if (!changed) return {true, "No parameter change needed"};
    return {false, "Parameter tweak not validated by self-tests"};
}

static TaskResult task_nitros_improve(const Config& cfg, State&){
    // Pipeline: optionally apply next patch → build → test
    std::string ts = now_ts();
    std::string logApply = (fs::path(cfg.logDir)/("apply_"+ts+".log")).string();
    std::string logBuild = (fs::path(cfg.logDir)/("build_"+ts+".log")).string();
    std::string logTest  = (fs::path(cfg.logDir)/("test_" +ts+".log")).string();

    if (auto p = next_patch(cfg)) {
        if (!apply_patch(*p, cfg.dryRun, logApply)) {
