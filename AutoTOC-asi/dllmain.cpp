// dllmain.cpp : Defines the entry point for the DLL application.

//Change the solution platform to either LE or ME3 to change which game this will work with
#ifdef _WIN64
#define LEGENDARY_EDITION
#endif

#include <fstream>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>
#include "Shlwapi.h"
#include "Strsafe.h"

#ifndef LEGENDARY_EDITION
#include "../detours/detours.h"
#endif

#define _CRT_SECURE_NO_WARNINGS

#ifndef LEGENDARY_EDITION
#pragma comment(lib, "detours.lib") //Library needed for Hooking part.
#endif

#pragma pack(1)

//#define LOGGING

#ifdef LEGENDARY_EDITION
#include "IniFile.h"
#else
#include "TOCUpdater.h"
#endif

using namespace std;

enum class MEGame
{
	ME3,
	LE1,
	LE2,
	LE3
};

MEGame Game;

typedef unsigned short ushort;

struct fileData {
	wstring fileName;
	int fileSize;

	fileData(const wstring fileName, const int fileSize) : fileName(fileName), fileSize(fileSize) {}
};

void write(ofstream &file, void* data, streamsize size);

void writeTOC(TCHAR  tocPath[MAX_PATH], TCHAR  baseDir[MAX_PATH], bool isDLC);

void write(ofstream &file, byte data);

void write(ofstream &file, ushort data);

void write(ofstream &file, int data);

void getFiles(vector<fileData> &files, TCHAR* basepath, TCHAR* searchPath);

#ifdef LEGENDARY_EDITION
  void getLE1Files(vector<fileData>& files, TCHAR* basepath);
#endif


void AutoToc(TCHAR path[MAX_PATH]);

#ifdef LOGGING
wofstream logFile;
#endif // LOGGING

#ifndef LEGENDARY_EDITION

using tProcessEvent = void(__thiscall* )(void*, void*, void*, void*);
tProcessEvent ProcessEvent = reinterpret_cast<tProcessEvent>(0x00453120);

static HHOOK msgHook = nullptr;
LRESULT CALLBACK wndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= HC_ACTION)
	{
		auto* const msgStruct = reinterpret_cast<LPCWPSTRUCT>(lParam);
		if (msgStruct->message == WM_APP + 'T' + 'O' + 'C')
		{
#ifdef LOGGING
			logFile << "Recieved Message From ME3Explorer..." << endl;
#endif
			TOCUpdater tocUpdater;
			const bool success = tocUpdater.TryUpdate();
#ifdef LOGGING
			logFile << (success ? "TOC Updated!" : "TOC Update FAILED!") << endl;
#endif
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool hookedMessages = false;
void __fastcall HookedPE(void* pObject, void* edx, void* pFunction, void* pParms, void* pResult)
{
	if (!hookedMessages)
	{
		hookedMessages = true;
		msgHook = SetWindowsHookEx(WH_CALLWNDPROC, wndProc, nullptr, GetCurrentThreadId());
	}
	ProcessEvent(pObject, pFunction, pParms, pResult);
}

void onAttach()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread()); //This command set the current working thread to the game current thread.
	DetourAttach(&(PVOID&)ProcessEvent, HookedPE); //This command will start your Hook.
	DetourTransactionCommit();
}
#endif

#ifdef _USRDLL 
  BOOL APIENTRY DllMain( HMODULE hModule, DWORD  reason, LPVOID)
{
	TCHAR path[MAX_PATH];
	GetModuleFileName(hModule, path, MAX_PATH);
#ifndef LEGENDARY_EDITION
	if (reason == DLL_PROCESS_ATTACH)
	{
		AutoToc(path);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)onAttach, NULL, 0, NULL);
	}
	if (reason == DLL_PROCESS_DETACH)
	{
		UnhookWindowsHookEx(msgHook);
#ifdef LOGGING
		logFile.close();
#endif
		
	}
#else
	if (reason == DLL_PROCESS_ATTACH)
	{
		AutoToc(path);
#ifdef LOGGING
		logFile.close();
#endif
		
	}
#endif

	return TRUE;
}
#else
int main()
{
	TCHAR path[MAX_PATH];
	//exe version is only for testing purposes right now, so just hardcode the path to where the asi ought to go
	StringCchCopy(path, MAX_PATH, L"F:\\SteamLibrary\\steamapps\\common\\Mass Effect Legendary Edition\\Game\\ME1\\Binaries\\Win64\\ASI\\AutoTOCasiLE.asi");
	AutoToc(path);
#ifdef LOGGING
	logFile.close();
#endif
}
#endif



void AutoToc(TCHAR path[MAX_PATH])
{
	PathRemoveFileSpec(path);
#ifdef LOGGING
	StringCchCat(path, MAX_PATH, L"\\AutoTOC.log");
	logFile.open(path);
	logFile << "AutoTOC log:\n";
	PathRemoveFileSpec(path);
#endif // LOGGING

	//get us to the game base folder
	PathRemoveFileSpec(path);
	PathRemoveFileSpec(path);
	PathRemoveFileSpec(path);
#ifdef LEGENDARY_EDITION 
  size_t strLen;
	StringCchLength(path, MAX_PATH, &strLen);

	switch (path[strLen - 1])
	{
	case '1':
		Game = MEGame::LE1;
		break;
	case '2':
		Game = MEGame::LE2;
		break;
	case '3':
		Game = MEGame::LE3;
		break;
	default:
		const std::wstring message = L"AutoTOC asi is unable to determine which Mass Effect game it is running in! This game path is unexpected:\n\n";
		MessageBox(nullptr,
			(message + path).c_str(),
			L"Mass Effect - ",
			MB_OK | MB_ICONERROR);
		exit(1);
	}
#else
	Game = MEGame::ME3;
#endif

	
	StringCchCat(path, MAX_PATH, L"\\");


	TCHAR baseDir[MAX_PATH];
	StringCchCopy(baseDir, MAX_PATH, path);
	TCHAR tocPath[MAX_PATH];
	StringCchCopy(tocPath, MAX_PATH, path);
#ifdef LEGENDARY_EDITION 
	StringCchCat(tocPath, MAX_PATH, L"BioGame\\PCConsoleTOC.bin");
#else
	StringCchCat(tocPath, MAX_PATH, L"BIOGame\\PCConsoleTOC.bin");
#endif
#ifdef LOGGING
	logFile << "\nwriting basegame toc..\n";
#endif // LOGGING
	writeTOC(tocPath, baseDir, false);

	if (Game != MEGame::LE1)
	{
#ifdef LOGGING
		logFile << "\nTOCing dlc:\n";
#endif // LOGGING
#ifdef LEGENDARY_EDITION 
		StringCchCat(path, MAX_PATH, L"BioGame\\DLC\\");
#else
		StringCchCat(path, MAX_PATH, L"BIOGame\\DLC\\");
#endif

		if (PathIsDirectory(path))
		{
			StringCchCopy(baseDir, MAX_PATH, path);

			WIN32_FIND_DATA fd;
			HANDLE hFind = INVALID_HANDLE_VALUE;
			TCHAR enumeratePath[MAX_PATH];
			StringCchCopy(enumeratePath, MAX_PATH, path);
			StringCchCat(enumeratePath, MAX_PATH, L"*");
			hFind = FindFirstFile(enumeratePath, &fd);
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					// 0 means true for lstrcmp
					if (wcslen(fd.cFileName) > 3 && fd.cFileName[0] == L'D' && fd.cFileName[1] == L'L' && fd.cFileName[2] == L'C')
					{
#ifdef LOGGING
						logFile << "writing toc for " << fd.cFileName << "\n";
#endif // LOGGING
						StringCchCopy(baseDir, MAX_PATH, path);
						StringCchCat(baseDir, MAX_PATH, fd.cFileName);
						StringCchCat(baseDir, MAX_PATH, L"\\");
						StringCchCopy(tocPath, MAX_PATH, baseDir);
						StringCchCat(tocPath, MAX_PATH, L"PCConsoleTOC.bin");
						writeTOC(tocPath, baseDir, true);
					}
				}
			} while (FindNextFile(hFind, &fd) != 0);
			FindClose(hFind);
		}
#ifdef LOGGING
		else
		{
			logFile << "DLC directory not found!\n";
		}
#endif // LOGGING
	}
#ifdef LOGGING
	logFile << "\ndone\n";
	logFile.flush();
#endif // LOGGING
}

void writeTOC(TCHAR tocPath[MAX_PATH], TCHAR baseDir[MAX_PATH], const bool isDLC)
{
	ofstream toc;
	toc.open(tocPath, ios::out | ios::binary | ios::trunc);
	//header
	write(toc, 0x3AB70C13);
	write(toc, 0x0);
	write(toc, 0x1);
	write(toc, 0x8);
	vector<fileData> files;
#ifdef LOGGING
	logFile << "getting files..\n";
#endif // LOGGING
	if (isDLC)
	{
		getFiles(files, baseDir, L"");
	}
	else
	{
#ifdef LEGENDARY_EDITION
		if (Game == MEGame::LE1)
		{
			getLE1Files(files, baseDir);
		}
		else
		{
			getFiles(files, baseDir, L"BioGame\\");
			getFiles(files, baseDir, L"Engine\\");
			if (Game == MEGame::LE3)
			{
				getFiles(files, baseDir, L"BioGame\\DLC\\");
			}
		}
#else
		getFiles(files, baseDir, L"BIOGame\\");
#endif

	}
#ifdef LOGGING
	logFile << "\nwriting file data..\n";
#endif // LOGGING
	CHAR filepath[MAX_PATH];
	const int numFiles = files.size();
	write(toc, numFiles);
	for (vector<fileData>::size_type i = 0; i != numFiles; i++)
	{
		size_t _;
		wcstombs_s(&_, filepath, files[i].fileName.c_str(), MAX_PATH);
		const int stringLength = strlen(filepath);
		//size of entry: everything before the string, the stringlength, and the null character
		ushort entryLength = ushort(0x1D + stringLength);
#ifdef LEGENDARY_EDITION
		auto pad = (4 - entryLength % 4) % 4;
		entryLength += pad;
#endif
		if (i == files.size() - 1)
		{
			//last entry doesn't have to have a size for some reason
			write(toc, ushort(0));
		}
		else
		{	
			write(toc, entryLength);
		}
		write(toc, static_cast<ushort>(0));
		write(toc, files[i].fileSize);
		write(toc, int(0));
		write(toc, int(0));
		write(toc, int(0));
		write(toc, int(0));
		write(toc, int(0));
		toc.write(filepath, stringLength);
		write(toc, byte(0));
#ifdef LEGENDARY_EDITION
		//align
		for (;pad > 0; --pad)
		{
			write(toc, byte(0));
		}
#endif	
	}
	toc.close();
}


void write(ofstream &file, void* data, const streamsize size) {
	file.write(static_cast<char*>(data), size);
}

void write(ofstream &file, byte data) {
	file.write(reinterpret_cast<char*>(&data), 1);
}

void write(ofstream &file, ushort data) {
	file.write(reinterpret_cast<char*>(&data), 2);
}

void write(ofstream &file, int data) {
	file.write(reinterpret_cast<char*>(&data), 4);
}

void getFiles(vector<fileData> &files, TCHAR* basepath, TCHAR* searchPath) {
	WIN32_FIND_DATA fd;
	LARGE_INTEGER fileSize;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR tmpPath[MAX_PATH];
	TCHAR* ext;
	StringCchCopy(enumeratePath, MAX_PATH, basepath);
	StringCchCat(enumeratePath, MAX_PATH, searchPath);
	StringCchCat(enumeratePath, MAX_PATH, L"*");

#ifdef LOGGING
	logFile << "\nenumerating files..\n";
#endif // LOGGING
	hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		StringCchCopy(tmpPath, MAX_PATH, searchPath);
		StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// 0 means true for lstrcmp
			if (lstrcmp(PathFindFileName(fd.cFileName), L"DLC") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"Patches") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"Splash") != 0 &&
#ifdef LEGENDARY_EDITION 
  				lstrcmp(PathFindFileName(fd.cFileName), L"Config") != 0 &&
#endif
				lstrcmp(PathFindFileName(fd.cFileName), L".") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"..") != 0)
			{
#ifdef LOGGING
				logFile << "getting files from Directory:" << fd.cFileName << "\n";
#endif // LOGGING
				StringCchCat(tmpPath, MAX_PATH, L"\\");
				getFiles(files, basepath, tmpPath);
			}
			
		}
		else
		{
			//allowed = { "*.pcc", "*.afc", "*.bik", "*.bin", "*.tlk", "*.txt", "*.cnd", "*.upk", "*.tfc", "*.isb", "*.usf" }
			ext = PathFindExtension(tmpPath);
			if (lstrcmp(ext, L".pcc") == 0 
				|| lstrcmp(ext, L".afc") == 0
				|| lstrcmp(ext, L".bik") == 0
				|| lstrcmp(ext, L".bin") == 0
				|| lstrcmp(ext, L".tlk") == 0
				|| lstrcmp(ext, L".txt") == 0
				|| lstrcmp(ext, L".cnd") == 0
				|| lstrcmp(ext, L".upk") == 0
				|| lstrcmp(ext, L".tfc") == 0
#ifdef LEGENDARY_EDITION
  				|| lstrcmp(ext, L".isb") == 0
				|| lstrcmp(ext, L".usf") == 0
				|| lstrcmp(ext, L".ini") == 0
				|| lstrcmp(ext, L".dlc") == 0
#endif

				)
			{
				fileSize.HighPart = fd.nFileSizeHigh;
				fileSize.LowPart = fd.nFileSizeLow;
				wstring fileName = tmpPath;
#ifdef LOGGING
				logFile << "found file: ";
				logFile.flush();
				logFile.write(fileName.c_str(), fileName.size() + 1);
				logFile << "\n";
#endif // LOGGING
				files.emplace_back(fileName, int(fileSize.QuadPart));
			}
			
		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);
}

#ifdef LEGENDARY_EDITION
struct caseInsensitiveCmp {
	bool operator() (const std::wstring& lhs, const std::wstring& rhs) const {
		return lstrcmpi(lhs.c_str(), rhs.c_str()) < 0;
	}
};

void addToMap(std::map<std::wstring, std::pair<std::wstring, int>, caseInsensitiveCmp>& fileMap, TCHAR* basepath, TCHAR* searchPath)
{
	WIN32_FIND_DATA fd;
	LARGE_INTEGER fileSize;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR tmpPath[MAX_PATH];
	TCHAR* ext;
	StringCchCopy(enumeratePath, MAX_PATH, basepath);
	StringCchCat(enumeratePath, MAX_PATH, searchPath);
	StringCchCat(enumeratePath, MAX_PATH, L"*");

#ifdef LOGGING
	logFile << "enumerating files: " << enumeratePath << "\n";
#endif // LOGGING
	HANDLE hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		StringCchCopy(tmpPath, MAX_PATH, searchPath);
		StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// 0 means true for lstrcmp
			if (lstrcmp(PathFindFileName(fd.cFileName), L"DLC") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"Patches") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"Splash") != 0 &&
#ifdef LEGENDARY_EDITION 
				lstrcmp(PathFindFileName(fd.cFileName), L"Config") != 0 &&
#endif
				lstrcmp(PathFindFileName(fd.cFileName), L".") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), L"..") != 0)
			{
#ifdef LOGGING
				logFile << "\ngetting files from Directory:" << fd.cFileName << "\n";
#endif // LOGGING
				StringCchCat(tmpPath, MAX_PATH, L"\\");
				addToMap(fileMap, basepath, tmpPath);
			}

		}
		else
		{
			//allowed = { "*.pcc", "*.afc", "*.bik", "*.bin", "*.tlk", "*.txt", "*.cnd", "*.upk", "*.tfc", "*.isb", "*.usf" }
			ext = PathFindExtension(tmpPath);
			if (lstrcmp(ext, L".pcc") == 0
				|| lstrcmp(ext, L".afc") == 0
				|| lstrcmp(ext, L".bik") == 0
				|| lstrcmp(ext, L".bin") == 0
				|| lstrcmp(ext, L".tlk") == 0
				|| lstrcmp(ext, L".txt") == 0
				|| lstrcmp(ext, L".cnd") == 0
				|| lstrcmp(ext, L".upk") == 0
				|| lstrcmp(ext, L".tfc") == 0
#ifdef LEGENDARY_EDITION
				|| lstrcmp(ext, L".isb") == 0
				|| lstrcmp(ext, L".usf") == 0
				|| lstrcmp(ext, L".ini") == 0
				|| lstrcmp(ext, L".dlc") == 0
#endif

				)
			{
				fileSize.HighPart = fd.nFileSizeHigh;
				fileSize.LowPart = fd.nFileSizeLow;
				wstring fileName = tmpPath;
#ifdef LOGGING
				logFile << "found file: ";
				//logFile.flush();
				logFile.write(fileName.c_str(), fileName.size());
				logFile << "\n";
#endif // LOGGING
				fileMap[fd.cFileName] = std::make_pair(fileName, int(fileSize.QuadPart));
			}

		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);
}

void getLE1Files(vector<fileData>& files, TCHAR* basepath)
{
	std::map<int, std::wstring> dlcMount;
	map<int, std::wstring> dlcFriendlyNames;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR dlcPath[MAX_PATH];
	WIN32_FIND_DATA fd;
	TCHAR tmpPath[MAX_PATH];
	std::map<std::wstring, std::pair<std::wstring, int>, caseInsensitiveCmp> fileMap;
	bool dlcFound = false;
	
	StringCchCopy(dlcPath, MAX_PATH, basepath);
	StringCchCat(dlcPath, MAX_PATH, L"BioGame\\DLC\\");
	StringCchCopy(enumeratePath, MAX_PATH, dlcPath);
	StringCchCat(enumeratePath, MAX_PATH, L"*");

#ifdef LOGGING
	logFile << "\nfinding LE1 DLCs... \n";
#endif // LOGGING
	HANDLE hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && wcslen(fd.cFileName) > 4 
			&& fd.cFileName[0] == L'D' && fd.cFileName[1] == L'L' && fd.cFileName[2] == L'C' && fd.cFileName[3] == L'_')
		{
			StringCchCopy(tmpPath, MAX_PATH, dlcPath);
			StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
			StringCchCat(tmpPath, MAX_PATH, L"\\AutoLoad.ini");
			const auto fileAttributes = GetFileAttributes(tmpPath);
			if (fileAttributes != INVALID_FILE_ATTRIBUTES)
			{
				const wstring iniPath(tmpPath);
				IniFile autoLoad(iniPath);
				auto strMount = autoLoad.readValue(L"ME1DLCMOUNT", L"ModMount");
				if (!strMount.empty())
				{
					int mount = 0;
					try
					{
						mount = std::stoi(strMount);
					}
					catch (...)
					{
						mount = 0;
					}
					if (mount > 0)
					{
						dlcMount[mount] = fd.cFileName;
						dlcFriendlyNames[mount] = autoLoad.readValue(L"ME1DLCMOUNT", L"ModName");
						dlcFound = true;
#ifdef LOGGING
						logFile << "Registered " << fd.cFileName << " (" << dlcFriendlyNames[mount] << ")  at mount: " << mount << "\n";
#endif // LOGGING
						continue;
					}
				}
#ifdef LOGGING
				logFile << fd.cFileName << " has an invalid AutoLoad.ini! exiting...\n";
#endif // LOGGING
				const std::wstring message = L" has an improperly formatted AutoLoad.ini! Try re-installing the mod. If that doesn't fix the problem, contact the mod author.\n\nMass Effect will now close.";
				MessageBox(nullptr,
					(fd.cFileName + message).c_str(),
					L"Mass Effect LE - Broken Mod Warning",
					MB_OK | MB_ICONERROR);
				exit(1);
			}
#ifdef LOGGING
			logFile << fd.cFileName << " does not have an AutoLoad.ini, skipping...\n";
#endif // LOGGING
		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);

#ifdef LOGGING
	if (!dlcFound)
	{
		logFile << "\nNo DLC found\n";
	}
	logFile << "\nbuilding file map... \n";
#endif // LOGGING
	
	addToMap(fileMap, basepath, L"BioGame\\");
	addToMap(fileMap, basepath, L"Engine\\");

	std::wofstream loadOrderTxt(wstring(dlcPath) + L"LoadOrder.Txt");
	loadOrderTxt << L"This is an auto-generated file for informational purposes only. Editing it will not change the load order.\n\n";
	if (dlcFound)
	{
		for (auto&& pair : dlcMount)
		{
			std::wstring dlcName = pair.second;
			StringCchCopy(tmpPath, MAX_PATH, L"BioGame\\DLC\\");
			StringCchCat(tmpPath, MAX_PATH, dlcName.c_str());
			StringCchCat(tmpPath, MAX_PATH, L"\\");
			addToMap(fileMap, basepath, tmpPath);

			loadOrderTxt << pair.first << L": " << dlcName << L" (" << dlcFriendlyNames[pair.first] << L")" << std::endl;
		}
	}
	else
	{
		loadOrderTxt << "No valid dlc found!";
	}
	loadOrderTxt.close();

#ifdef LOGGING
	logFile << "\nLE1 TOC in text form: \n\n";
#endif // LOGGING
	for (auto && pair : fileMap)
	{
#ifdef LOGGING
		logFile << pair.second.first << "    " << pair.second.second << "\n";
#endif // LOGGING
		files.emplace_back(pair.second.first, pair.second.second);
	}
}
#endif

