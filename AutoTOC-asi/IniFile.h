#pragma once
#include <string>
#include <vector>

class IniFile
{
	std::wstring iniPath;
	std::vector<std::wstring> replacements;
	bool readOnly;

public:
	explicit IniFile(std::wstring iniPath);

	~IniFile();

	void writeValue(const std::wstring& section, const std::wstring& key, const std::wstring& value) const;

	[[nodiscard]] std::wstring readValue(const std::wstring& section, const std::wstring& key) const;

	void removeKey(const std::wstring& section, const std::wstring& key) const;

	void removeKeys(const std::wstring& section, const std::wstring& key) const;

	void writeNewValue(const std::wstring& section, const std::wstring& key, const std::wstring& value);
};