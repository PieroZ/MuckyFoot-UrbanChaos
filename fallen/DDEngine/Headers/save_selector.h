
#pragma once

#include <vector>
#include <string>

class SaveSelector
{
public:
    static SaveSelector& getInstance();
    SaveSelector();

    // Refresh the internal list of save filenames (with extension).
    void RefreshList();

    // Rename (or create) a save at slot `index` (0-based). `newBaseName` is the new base name
    // (without extension preferred). Returns true on success.
    bool RenameSave(int index, const std::string& newBaseName);

    // Public cache of save filenames (with extension), sized/padded by RefreshList().
    std::vector<std::string> mSaveNames;
	bool mTypingSaveName = false;

    // When true, the SAVES menu is in "load" mode (selecting a slot loads).
    bool mLoadMode = false;
};