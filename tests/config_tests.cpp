// CameraUnlock.ini and what the conversion moved into code: the committed file is the table's
// fresh render, the defaults the dev build ran on map to the defaults, a first start creates the
// committed file, the mode hotkey saves only its own lines and leaves Defaults.ini and
// ArxFatalisHeadTracking.ini alone, End's row cannot be saved, the field of view refuses the values
// no focal answers, and a folder the ANSI code page cannot name imports as the dev build read it.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "config.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ArxHeadTracking;

namespace {

namespace cfg = cameraunlock::config;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// The file a first start creates.
std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

std::string Committed() {
    return ReadBytes(fs::path(ARX_COMMITTED_CONFIG));
}

void TestCommittedConfigIsRendered() {
    Check(Committed() == Rendered(), "config/CameraUnlock.ini is the table's fresh render (pixi run render-config)");
}

// A scratch game folder, and a Defaults.ini of its own beside it that the first load creates.
struct Scratch {
    fs::path root;
    fs::path game;
    fs::path defaults;

    explicit Scratch(const std::wstring& gameFolder) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = fs::path(temp) / (L"arx-config-tests-" + std::to_wstring(GetCurrentProcessId()));
        game = root / gameFolder;
        fs::remove_all(game);
        fs::create_directories(game);
        fs::create_directories(root / L"user");
        defaults = root / L"user" / L"CameraUnlock" / L"Defaults.ini";
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    cfg::ConfigOwnerOptions<Config> Options() const {
        return MakeConfigOwnerOptions(game.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
    }
    fs::path ConfigPath() const { return game / kConfigFileName; }
    fs::path LegacyPath() const { return game / kLegacyConfigFileName; }
};

std::vector<std::string> Listing(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

// With neither file there, the first start creates CameraUnlock.ini as the committed file and
// Defaults.ini with the built-in values, and no ArxFatalisHeadTracking.ini.
void TestFirstStartCreatesTheCommittedFile() {
    const Scratch s(L"first-start");
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Created, "a first start with no file is Created");
    Check(ReadBytes(s.ConfigPath()) == Committed(), "a first start creates config/CameraUnlock.ini's bytes");
    Check(Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini"}, "a first start creates CameraUnlock.ini and nothing else");
    Check(fs::exists(s.defaults), "a first start creates Defaults.ini");
    Check(AllValues(loaded.config) == AllValues(MakeConfigTable().defaults()), "a first start runs on the built-in values");
}

// A fresh install and an upgrade from the dev build's defaults start the same: the map of the
// frozen defaults holds every row at the table's default, leaves every global row the table does
// not mark PerGame to Defaults.ini, drops nothing, and every sensitivity and inversion is the
// shipped identity.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const fs::path missing = fs::path(temp) / L"arx-no-such-folder" / kLegacyConfigFileName;
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const cfg::ImportResult result =
        MakeLegacyImport().run(cfg::LegacyInput{missing.wstring(), missing.string(), false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent, "no file imports as Absent");
    Check(result.dropped.empty(), "the dev build's defaults drop nothing");
    Check(result.pose_shaping.size() == 9, "every sensitivity and inversion is recorded");
    Check(result.follows_defaults_ini.size() == 14,
          "the 14 rows that are global and not PerGame follow Defaults.ini");
    for (const cfg::PoseShapingValue& value : result.pose_shaping) {
        Check(value.folded, "[" + value.section + "] " + value.key + " at its shipped value is folded");
    }
    Check(AllValues(mapped) == AllValues(table.defaults()), "the dev build's defaults map to the defaults");
    Check(mapped.toggle_key_name == "End, Ctrl+Shift+Y" && mapped.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+J",
          "the old hotkeys and their chords become key lists");
    Check(legacy::Config{}.collision_radius == kDefaultCollisionMargin, "the collision margin is the one the dev build shipped");
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A save changes the lines of its rows and no other byte, writes a value over default, and
// touches neither Defaults.ini nor ArxFatalisHeadTracking.ini; the tracking mode persists, and
// End's row cannot be saved at all.
void TestModeHotkeySaves() {
    const Scratch s(L"save");
    const std::string committed = Committed();
    const std::string legacyBytes = "[General]\r\nEnableOnStartup=0\r\n";
    WriteBytes(s.ConfigPath(), committed);
    WriteBytes(s.LegacyPath(), legacyBytes);

    {
        cfg::ConfigOwner<Config> owner(s.Options());
        const auto loaded = owner.Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
        Check(loaded.config.enable_on_startup, "ArxFatalisHeadTracking.ini is not read while CameraUnlock.ini exists");
        Check(Contains(loaded.log, "is left as it was and is not read"),
              "the log says ArxFatalisHeadTracking.ini is not read while CameraUnlock.ini exists");
        const std::string defaultsBefore = ReadBytes(s.defaults);

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        const cfg::ConfigSaveResult first = owner.Save([rotationOnly](Config& c) {
            c.rotation_enabled = rotationOnly.rotation_enabled;
            c.position_enabled = rotationOnly.position_enabled;
        });
        Check(first.status == cfg::ConfigSaveStatus::Saved, "the tracking mode saves");
        Check(Contains(first.log, "no longer follows Defaults.ini"),
              "the save says the mode pair stopped following Defaults.ini");
        const std::string afterRotationOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(committed, afterRotationOnly) == std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes the mode pair over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotation_enabled = positionOnly.rotation_enabled;
                  c.position_enabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        const std::string afterPositionOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterRotationOnly, afterPositionOnly) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enable_on_startup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");
        Check(ReadBytes(s.ConfigPath()) == afterPositionOnly, "a refused save writes nothing");

        Check(ReadBytes(s.defaults) == defaultsBefore, "saving leaves Defaults.ini as it was");
        Check(ReadBytes(s.LegacyPath()) == legacyBytes, "saving leaves ArxFatalisHeadTracking.ini as it was");
    }

    const auto again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() &&
              !again.config.rotation_enabled && again.config.position_enabled && again.config.enable_on_startup,
          "the saved tracking mode comes back at the next start");
    Check((Listing(s.game) == std::vector<std::string>{"ArxFatalisHeadTracking.ini", "CameraUnlock.ini"}),
          "the game folder holds CameraUnlock.ini and ArxFatalisHeadTracking.ini and nothing else");
}

// FieldOfView takes 0 or 40 to 110. A value between 0 and 40, where no focal answers, and one that
// is not a number keep the default with a diagnostic naming the line.
void TestFieldOfViewRefusesWhatNoFocalAnswers() {
    const Scratch s(L"fov");
    const std::string committed = Committed();
    for (const char* value : {"90.0", "20.0", "110.5", "nan", "75,95"}) {
        std::string text = committed;
        const std::string line = "\r\nFieldOfView=0.0\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error("the committed file has no FieldOfView=0.0 line");
        text.replace(at + 2, line.size() - 4, std::string("FieldOfView=") + value);
        WriteBytes(s.ConfigPath(), text);
        const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
        const bool accepted = std::strcmp(value, "90.0") == 0;
        Check(loaded.config.field_of_view == (accepted ? 90.0f : 0.0f),
              std::string("FieldOfView=") + value + (accepted ? " is read" : " keeps the game's own field of view"));
        Check(loaded.diagnostics.empty() == accepted,
              std::string("FieldOfView=") + value + (accepted ? " draws no diagnostic" : " draws a diagnostic"));
    }
}

// The dev build named its folder in the ANSI code page, and where the code page could not hold
// the name, by the folder's 8.3 short name, which is ASCII. The import opens the legacy file by
// the same path, so a folder the code page cannot name imports the file where the volume keeps
// short names, and imports nothing where it does not, since the dev build did not start there.
// ArxFatalisHeadTracking.ini stays as it was either way.
void TestAFolderTheCodepageCannotNameImportsAsTheDevBuildReadIt() {
    const Scratch s(L"arx-\x4E2D");
    const std::string legacyBytes = "[Network]\r\nPort=5000\r\n";
    WriteBytes(s.LegacyPath(), legacyBytes);
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();

    wchar_t shortDir[MAX_PATH];
    const DWORD written = GetShortPathNameW(s.game.c_str(), shortDir, MAX_PATH);
    bool asciiShortName = written > 0 && written < MAX_PATH;
    for (DWORD i = 0; asciiShortName && i < written; ++i) asciiShortName = shortDir[i] < 0x80;
    const bool devBuildRead = GetACP() == CP_UTF8 || asciiShortName;
    std::printf("note: the dev build %s this folder's legacy file (%s)\n", devBuildRead ? "read" : "could not read",
                GetACP() == CP_UTF8 ? "the ANSI code page is UTF-8" : asciiShortName ? "by its short name" : "no short name");

    Check(loaded.status == cfg::ConfigLoadStatus::Migrated, "a folder the code page cannot name migrates");
    Check(loaded.config.udp_port == (devBuildRead ? 5000 : 4242), "the port is what the dev build read there");
    Check(ReadBytes(s.LegacyPath()) == legacyBytes, "ArxFatalisHeadTracking.ini stays as it was");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            WriteBytes(argv[2], Rendered());
            return 0;
        }
        if (argc != 1) {
            std::printf("usage: %s [--render-config <path>]\n", argv[0]);
            return 2;
        }

        TestCommittedConfigIsRendered();
        TestLegacyDefaultsMapToTheDefaults();
        TestFirstStartCreatesTheCommittedFile();
        TestModeHotkeySaves();
        TestFieldOfViewRefusesWhatNoFocalAnswers();
        TestAFolderTheCodepageCannotNameImportsAsTheDevBuildReadIt();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
