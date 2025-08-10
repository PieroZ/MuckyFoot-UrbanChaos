#include "ResourceManager.h"

ResourceManager& ResourceManager::Get()
{
	static ResourceManager instance;
	return instance;
}
