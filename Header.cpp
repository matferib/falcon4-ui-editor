#include "Header.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>


namespace {
bool IsEqualCaseInsensitiveString(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
}

std::string::const_iterator FindStringInStringCaseInsensitive(const std::string& input_string, const std::string& string_to_find) {
    return std::search(
        input_string.begin(), input_string.end(),
        string_to_find.begin(), string_to_find.end(),
        [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
}

std::vector<std::string> GetAllBMSInstallations(HKEY hKey) {
    constexpr int kMaxCharLength{ 512 };

    char achKey[kMaxCharLength];         // buffer for subkey name
    DWORD cbName;                        // size of name string 
    char achClass[kMaxCharLength] = "";  // buffer for class name 
    DWORD cchClassName = kMaxCharLength; // size of class string 
    DWORD cSubKeys = 0;                  // number of subkeys 
    DWORD cbMaxSubKey;                   // longest subkey size 
    DWORD cchMaxClass;                   // longest class string 
    DWORD cValues;                       // number of values for key 
    DWORD cchMaxValue;                   // longest value name 
    DWORD cbMaxValueData;                // longest value data 
    DWORD cbSecurityDescriptor;          // size of security descriptor 
    FILETIME ftLastWriteTime;            // last write time 

    DWORD cchValue = kMaxCharLength;

    // Get the class name and the value count. 
    DWORD retCode = RegQueryInfoKeyA(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        nullptr,                 // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 

    // Enumerate the subkeys, until RegEnumKeyEx fails.
    if (cSubKeys == 0) {
        std::cerr << "  No values found\n";
        return{};
    }

    std::vector<std::string> installation_list;
    if (cSubKeys > 0) {
        std::cout << "\n  Number of found BMS installations: " << cSubKeys << std::endl;

        for (DWORD i = 0; i < cSubKeys; i++) {
            cbName = kMaxCharLength;
            retCode = RegEnumKeyExA(hKey, i,
                achKey,
                &cbName,
                nullptr,
                nullptr,
                nullptr,
                &ftLastWriteTime);
            if (retCode == ERROR_SUCCESS) {
                installation_list.emplace_back(achKey);
            }
        }
    }
    return installation_list;
}
}  // namespace

std::string GetAndShowChoice(const std::string& default_choice) {
    std::string str;
    std::getline(std::cin, str);
    if (str.empty() && !default_choice.empty()) { str = default_choice; }
    if (str.empty()) { str = "'no input'"; }
    std::cout << "  You have chosen " << str << std::endl;
    return str;
}

namespace {
std::string InstallDirForRegKey(const std::string& reg_key) {
    HKEY hkey;
    RegOpenKeyExA(HKEY_LOCAL_MACHINE, reg_key.c_str(), 0, KEY_READ, &hkey);

    constexpr int kMaxCharLength{ 512 };
    BYTE byteArray[kMaxCharLength];
    DWORD dataSize = sizeof(byteArray);
    DWORD type = 0;
    if (RegQueryValueExA(
        hkey,
        "baseDir",
        nullptr,
        &type,
        byteArray,
        &dataSize) == ERROR_SUCCESS) {
        return std::string(reinterpret_cast<char const*>(byteArray));
    }
    std::cerr << "  Error reading reg key " << reg_key << ", exiting...\n";
    return {};
}
}  // namespace

std::string InstallDirForInstallation(const std::string& installation) {
    return InstallDirForRegKey(BMS_REG_KEY + "\\" + installation);
}

namespace {
std::optional<std::string> PickInstallationFromList(const std::vector<std::string>& installation_list) {
    for (int i = 0; i < installation_list.size(); ++i) {
        std::cout << "  (" << i << ") " << installation_list[i] << std::endl;
    }
    std::cout << "  (.) Any other key to exit\n";
    std::cout << "\n  Which BMS installation shall be used? [0-" << installation_list.size() - 1 << "]?\n";
    std::cout << "  > ";
    const auto nr = std::stoi(GetAndShowChoice("-1"));

    if (nr < 0 || nr >= installation_list.size()) {
        std::cerr << "  Invalid choice, exiting...\n";
        return std::nullopt;
    }

    return BMS_REG_KEY + "\\" + installation_list[nr];
}
}  // namespace  

std::string BaseDirFromInstallationList(const std::vector<std::string>& installation_list) {
    const auto reg_key = PickInstallationFromList(installation_list);
    if (!reg_key.has_value()) { return{}; }
    return InstallDirForRegKey(*reg_key);
}

std::vector<std::string> GetAllBMSInstallations() {
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, BMS_REG_KEY.c_str(), 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        return GetAllBMSInstallations(hkey);
    }
    return {};
}

std::string SimDatafolder(const std::string& base_folder) {
    if (base_folder.empty()) { return {}; }
    // tvt uses base data, so point sim folder to base
    const auto tvt_delimiter = base_folder.find("Add-On Korea TvT");
    if (tvt_delimiter != std::string::npos) { return base_folder.substr(0, tvt_delimiter) + "Sim"; }
    return base_folder + "\\Sim";
}

std::string ObjectDatafolder(const std::string& base_folder) {
    if (base_folder.empty()) { return {}; }
    return base_folder + "\\TerrData\\Objects";
}

namespace {
const char kDefaultTheaterName[] = "Default";
}  // namespace

std::vector<std::string> ListTheaters(const std::string& base_folder) {
    std::fstream theater_list_file{ base_folder + "\\Data\\TerrData\\TheaterDefinition\\theater.lst" };
    std::vector<std::string> theater_list;

    if (theater_list_file.good()) {
        for (std::string line; theater_list_file.good(); getline(theater_list_file, line)) {
            const auto it = FindStringInStringCaseInsensitive(line, "TerrData");
            if (it != line.cend()) {
                // default KTO will result in empty string, let's replace with kDefaultTheaterName
                // '-1' to strip the trailing '//'
                const auto entry = it == line.cbegin() ? kDefaultTheaterName : std::string(line.cbegin(), it - 1);
                theater_list.emplace_back(entry);
            }
        }
    }
    theater_list_file.close();
    return theater_list;
}

std::string DataDirForTheater(const std::string& theater) {
    return theater == kDefaultTheaterName ? std::string{ "\\Data" } : "\\Data\\" + theater;
}
