#include "save_selector.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

SaveSelector::SaveSelector()
{
    RefreshList();
}

SaveSelector& SaveSelector::getInstance()
{
    static SaveSelector instance;
    return instance;
}

static std::filesystem::path SaveDir()
{
    return std::filesystem::path("data") / "saves";
}

static std::filesystem::path IndexFilePath()
{
    return SaveDir() / "saves_order.txt";
}

static void LoadIndexFile(std::vector<std::string>& out)
{
    out.clear();
    auto idx = IndexFilePath();
    if (!std::filesystem::exists(idx)) return;

    std::ifstream ifs(idx.string(), std::ios::in);
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line))
    {
        // trim CR/LF and spaces
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        out.push_back(line);
    }
}

static void SaveIndexFile(const std::vector<std::string>& order)
{
    auto idx = IndexFilePath();
    std::error_code ec;
    std::filesystem::create_directories(idx.parent_path(), ec);
    std::ofstream ofs(idx.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return;
    for (const auto& name : order)
    {
        ofs << name << "\n";
    }
}

void SaveSelector::RefreshList()
{
    namespace fs = std::filesystem;

    mSaveNames.clear();

    const fs::path saveDir = SaveDir();
    std::error_code ec;
    fs::create_directories(saveDir, ec); // ignore error; we'll handle gracefully

    // collect files in directory with extension .wag
    std::unordered_set<std::string> filesSet;
    std::unordered_map<std::string, fs::file_time_type> timeMap;

    if (fs::exists(saveDir))
    {
        for (auto& entry : fs::directory_iterator(saveDir, ec))
        {
            if (ec) break;
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                // case-insensitive check for .wag
                if (!ext.empty())
                {
                    std::string lext = ext;
                    std::transform(lext.begin(), lext.end(), lext.begin(), ::tolower);
                    if (lext == ".wag")
                    {
                        auto filename = entry.path().filename().string(); // with extension
                        filesSet.insert(filename);
                        timeMap[filename] = entry.last_write_time(ec);
                    }
                }
            }
        }
    }

    // load index file if present
    std::vector<std::string> indexOrder;
    LoadIndexFile(indexOrder);

    // First, add files that are present and listed in the index in that order.
    for (const auto& name : indexOrder)
    {
        if (filesSet.find(name) != filesSet.end())
        {
            mSaveNames.push_back(name);
            filesSet.erase(name);
        }
        // if not found (file removed), skip it
    }

    // For any remaining files (new ones), append them sorted by last_write_time (oldest first)
    std::vector<std::string> remaining;
    remaining.reserve(filesSet.size());
    for (auto& n : filesSet) remaining.push_back(n);

    std::sort(remaining.begin(), remaining.end(), [&](const std::string& a, const std::string& b) {
        auto ita = timeMap.find(a);
        auto itb = timeMap.find(b);
        if (ita == timeMap.end() || itb == timeMap.end()) return a < b;
        return ita->second < itb->second; // older first
        });

    for (auto& name : remaining) mSaveNames.push_back(name);

    // Update index file so it reflects the new canonical order (index = existing order + new appended)
    std::vector<std::string> newIndex = mSaveNames; // copy
    SaveIndexFile(newIndex);

    // pad/truncate to 5 slots for UI
    while (mSaveNames.size() < 5) mSaveNames.emplace_back("EMPTY");
    if (mSaveNames.size() > 5) mSaveNames.resize(5);
}

bool SaveSelector::RenameSave(int index, const std::string& newBaseName)
{
    namespace fs = std::filesystem;

    if (index < 0) return false;
    RefreshList(); // make sure list up-to-date

    const std::string oldname = (index < (int)mSaveNames.size()) ? mSaveNames[index] : std::string();

    // sanitize newBaseName
    std::string newname = newBaseName;
    for (char& c : newname) if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|') c = '_';
    if (newname.empty()) return false;

    // Determine extension to keep (from old file if present) or default .wag
    std::string ext = ".wag";
    if (!oldname.empty() && oldname != "EMPTY")
    {
        size_t dot = oldname.find_last_of('.');
        if (dot != std::string::npos)
            ext = oldname.substr(dot);
    }

    if (newname.find('.') == std::string::npos)
        newname += ext;

    fs::path saveDir = SaveDir();
    std::string newFilename = newname;
    std::string oldFilename = oldname;

    std::error_code ec;

    // If oldname is empty or equals "EMPTY", create new file
    if (oldname.empty() || oldname == "EMPTY")
    {
        fs::path newpath = saveDir / newFilename;
        // create empty file if not exist
        if (!fs::exists(newpath, ec))
        {
            std::ofstream ofs(newpath.string(), std::ios::binary);
            if (!ofs.good()) return false;
            ofs.close();
        }

        // Update index: append new filename at end
        std::vector<std::string> index;
        LoadIndexFile(index);
        // remove any previous occurrence (unlikely)
        index.erase(std::remove(index.begin(), index.end(), newFilename), index.end());
        index.push_back(newFilename);
        SaveIndexFile(index);

        RefreshList();
        return true;
    }

    // Build full paths
    std::string oldpath = (saveDir / oldFilename).string();
    std::string newpath = (saveDir / newFilename).string();

    // If target exists already and is different, fail
    if (fs::exists(newpath, ec) && _stricmp(oldFilename.c_str(), newFilename.c_str()) != 0)
    {
        // target exists and is different name -> fail
        return false;
    }

    // Perform rename
    try
    {
        fs::rename(oldpath, newpath);
    }
    catch (const fs::filesystem_error&)
    {
        return false;
    }

    // Update index file: replace oldname with newFilename if present, otherwise append at end
    std::vector<std::string> indexList;
    LoadIndexFile(indexList);

    bool replaced = false;
    for (auto& s : indexList)
    {
        if (s == oldFilename)
        {
            s = newFilename;
            replaced = true;
            break;
        }
    }
    if (!replaced)
    {
        indexList.push_back(newFilename);
    }
    // Remove duplicates while preserving order
    std::vector<std::string> dedup;
    std::unordered_set<std::string> seen;
    for (auto& s : indexList)
    {
        if (seen.insert(s).second) dedup.push_back(s);
    }
    SaveIndexFile(dedup);

    // refresh cache
    RefreshList();
    return true;
}