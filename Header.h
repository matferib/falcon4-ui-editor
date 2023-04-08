#pragma once

#include <fstream>
#include <vector>
#include <optional>
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

const std::string BMS_REG_KEY{ "SOFTWARE\\WOW6432Node\\Benchmark Sims" };

std::vector<std::string> GetAllBMSInstallations();
std::string InstallDirForInstallation(const std::string& installation);
std::string BaseDirFromInstallationList(const std::vector<std::string>& installation_list);
std::string SimDatafolder(const std::string& base_folder);
std::string ObjectDatafolder(const std::string& base_folder);

// Theaters.
std::vector<std::string> ListTheaters(const std::string& base_folder);
std::string DataDirForTheater(const std::string& theater);

