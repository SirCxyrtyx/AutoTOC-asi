#pragma once
#include <windows.h>
#include <fstream>
#include <map>

std::map<std::string, LPVOID> fileToPtrMap;
class TOCUpdater
{
	char filePath[MAX_PATH]{};
	int filePathLen;
	LPVOID address = nullptr;
	bool isDLC;
	int32_t originalFileSize{};
	std::string fullRelativePath;

public:
	TOCUpdater()
	{
		std::ifstream in;
		in.open("tocupdate");
		in.read(reinterpret_cast<char*>(&originalFileSize), 4);
		char tempPath[MAX_PATH];
		in.read(tempPath, MAX_PATH);
		in.close();

		fullRelativePath = std::string(tempPath);

		const auto search = fileToPtrMap.find(fullRelativePath);
		if (search != fileToPtrMap.end())
		{
			address = search->second;
			return;
		}

		if (tempPath[8] == 'D')
		{
			strcpy_s(filePath, strstr(tempPath, "CookedPCConsole"));
			isDLC = true;
		}
		else
		{
			strcpy_s(filePath, tempPath);
		}
		filePathLen = strlen(filePath);
	}

	bool TryUpdate()
	{
		if (address == nullptr)
		{
			GetAddress();
			if (address == nullptr)
			{
				//can't find it ¯\_(ツ)_/¯
				return false;
			}
			fileToPtrMap.insert_or_assign(fullRelativePath, address);
		}

		char path[MAX_PATH];
		GetModuleFileNameA(nullptr, path, MAX_PATH);
		path[strlen(path) - 30] = 0;
		strcat_s(path, fullRelativePath.c_str());
		WIN32_FILE_ATTRIBUTE_DATA fileData;
		GetFileAttributesExA(path, GetFileExInfoStandard, &fileData);
		const auto fileSize = fileData.nFileSizeLow;

		auto* fileSizePtr = static_cast<DWORD*>(address);
		*fileSizePtr = fileSize;

#ifdef LOGGING
  		std::ofstream out;
		out.open("tocupdatelog.txt", std::ios::app);
		out << path << " | " << fileSize << " at " << address << std::endl;
		out.close();
#endif


		return true;
	}
private:
	void GetAddress()
	{
		SIZE_T bytesRead;

		while (true)
		{
			MEMORY_BASIC_INFORMATION info;
			if (!VirtualQuery(address, &info, sizeof(info)))
			{
				break;
			}

			auto* addr = static_cast<char*>(info.BaseAddress);
			if (info.State == MEM_COMMIT && (info.Protect & PAGE_GUARD) == 0 && (info.Protect & PAGE_NOACCESS) == 0)
			{
				const auto len = info.RegionSize - filePathLen;
				for (auto i = 24, j = 0; i < len; ++i)
				{
					if (addr[i] == filePath[0])
					{
						for (j = 1; j < filePathLen; j++)
						{
							if (addr[i + j] != filePath[j])
								break;
						}
						if (j >= filePathLen)
						{
							auto* fileSizeAddr = reinterpret_cast<int32_t*>(addr + i - 24);
							if (*fileSizeAddr == originalFileSize)
							{
								address = fileSizeAddr;
								return;
							}
						}
					}
				}
			}
			address = addr + info.RegionSize;
		}
		address = nullptr;
	}
};
