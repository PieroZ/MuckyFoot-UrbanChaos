#include "save_selector.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>

SaveSelector::SaveSelector()
{
	RefreshList();
}

SaveSelector& SaveSelector::getInstance()
{
	static SaveSelector instance;
	return instance;	
}

void SaveSelector::RefreshList()
{
    namespace fs = std::filesystem;

    mSaveNames.clear();

    const char* saveDir = "data\\saves";
    CreateDirectoryA(saveDir, NULL);

    for (auto& entry : fs::directory_iterator(saveDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wag")
        {
            //mSaveNames.emplace_back(entry.path().stem().string());
            mSaveNames.emplace_back(entry.path().filename().string());
        }
    }

    std::sort(mSaveNames.begin(), mSaveNames.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
        });

    while (mSaveNames.size() < 5) mSaveNames.emplace_back("EMPTY");
    if (mSaveNames.size() > 5) mSaveNames.resize(5);
}

bool SaveSelector::RenameSave(int index, const std::string& newBaseName)
{
    if (index < 0) return false;
    RefreshList(); // make sure list up-to-date

    if (index >= (int)mSaveNames.size())
    {
        // Trying to rename a non-existent slot -> create new file name with extension .wag
        std::string newname = newBaseName;
        // sanitize and append extension
        for (char& c : newname) if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|') c = '_';
        if (newname.find('.') == std::string::npos) newname += ".wag";
        // create empty file
        std::string path = std::string("data\\saves\\") + newname;
        HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return false;
        CloseHandle(h);
        RefreshList();
        return true;
    }

    const std::string oldname = mSaveNames[index];

    // Build paths
    std::string oldpath = std::string("data\\saves\\") + oldname;

    // sanitize newBaseName
    std::string newname = newBaseName;
    for (char& c : newname) if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|') c = '_';

    // Keep same extension as old file (if any), otherwise .wag
    std::string ext = ".wag";
    size_t dot = oldname.find_last_of('.');
    if (dot != std::string::npos)
        ext = oldname.substr(dot);

    if (newname.find('.') == std::string::npos)
        newname += ext;

    std::string newpath = std::string("data\\saves\\") + newname;

    // Move (rename) file
    if (!MoveFileA(oldpath.c_str(), newpath.c_str()))
    {
        // failed
        return false;
    }

    // update cached list
    RefreshList();
    return true;
}