#include "IniFile.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <windows.h>

IniFile::IniFile(std::string iniPath) : iniPath(std::move(iniPath)), readOnly(false)
{
	const auto attributes = GetFileAttributes(this->iniPath.c_str());
	if (attributes & FILE_ATTRIBUTE_READONLY)
	{
		readOnly = true;
		SetFileAttributes(this->iniPath.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
	}
}

IniFile::~IniFile()
{
	if (!replacements.empty())
	{
		std::ifstream inFile(iniPath);
		std::stringstream strStream;
		std::string line;
		while (std::getline(inFile, line))
		{
			bool writeUnchanged(true);
			for (auto&& replacement : replacements)
			{
				const auto pos = line.find(replacement);
				if (pos != std::string::npos)
				{
					strStream << line.erase(pos, replacement.size()) << std::endl;
					writeUnchanged = false;
					break;
				}
			}
			if (writeUnchanged)
			{
				strStream << line << std::endl;
			}
		}
		inFile.close();
		std::ofstream outFile(iniPath);
		outFile << strStream.str();
		outFile.close();
	}
	if (readOnly)
	{
		const auto attributes = GetFileAttributes(this->iniPath.c_str());
		SetFileAttributes(iniPath.c_str(), attributes | FILE_ATTRIBUTE_READONLY);
	}
}

void IniFile::writeValue(const std::string& section, const std::string& key, const std::string& value) const
{
	WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), iniPath.c_str());
}

std::string IniFile::readValue(const std::string& section, const std::string& key) const
{
	constexpr DWORD buffSize = 255;
	TCHAR buff[buffSize];
	GetPrivateProfileString(section.c_str(), key.c_str(), "", buff, buffSize, iniPath.c_str());
	return std::string(buff);
}

void IniFile::removeKey(const std::string& section, const std::string& key) const
{
	WritePrivateProfileString(section.c_str(), key.c_str(), nullptr, iniPath.c_str());
}

void IniFile::removeKeys(const std::string& section, const std::string& key) const
{
	while (!readValue(section, key).empty())
	{
		removeKey(section, key);
	}
}

void IniFile::writeNewValue(const std::string& section, const std::string& key, const std::string& value)
{
	if (readValue(section, key).empty())
	{
		writeValue(section, key, value);
	}
	else
	{
		using namespace std::literals;
		const auto uniqueString = std::to_string(replacements.size()) + "af78da8fy"s;
		replacements.push_back(uniqueString);
		writeValue(section, key + uniqueString, value);
	}
}