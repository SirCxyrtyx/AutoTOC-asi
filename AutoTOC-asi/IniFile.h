#pragma once
#include <string>
#include <vector>

class IniFile
{
	std::string iniPath;
	std::vector<std::string> replacements;
	bool readOnly;

public:
	explicit IniFile(std::string iniPath);

	~IniFile();

	void writeValue(const std::string& section, const std::string& key, const std::string& value) const;

	[[nodiscard]] std::string readValue(const std::string& section, const std::string& key) const;

	void removeKey(const std::string& section, const std::string& key) const;

	void removeKeys(const std::string& section, const std::string& key) const;

	void writeNewValue(const std::string& section, const std::string& key, const std::string& value);
};