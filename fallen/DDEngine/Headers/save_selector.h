#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SaveSelector
{
	SaveSelector();
	SaveSelector(const SaveSelector&) = delete;
	void operator=(const SaveSelector&) = delete;

	static SaveSelector& getInstance();

	void RefreshList();
	bool RenameSave(int index, const std::string& newBaseName);


	std::uint8_t mSelectedIndex = 0;
	std::vector<std::string> mSaveNames;
};