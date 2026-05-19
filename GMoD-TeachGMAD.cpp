/*
* ===================================================================================================|
* MoD - TeachGMAD.cpp                                                                                |
* requires: 7zip/7za.exe ( standalone console version) + gmad.exe (original  bin from gmod)          |
* Source Code by : Sebastian.G.Romanelli      16/5/2026 8.41pm                                       |
* ===================================================================================================|
* Last compilation: 19/5/26 13:22PM | Version : 0.712
* Last public version : 0.712
*/



//=====================includes===========================
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <algorithm>
// #include "zlib.h" 
//=========================================================


namespace fs = std::filesystem;


// =====================utils/utilities=========================

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string getExecutableDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return ".";
    }
    fs::path p(buffer);
    return p.parent_path().string();
}


// extract with 7zip func

bool extractWith7Zip(const fs::path& archive, const fs::path& outDir) {
    fs::create_directories(outDir);

    
    std::string cmdLine = "7za.exe x \"" + archive.string() + "\" -o\"" + outDir.string() + "\" -y";

    // Buffer mutable
    std::vector<char> buffer(cmdLine.begin(), cmdLine.end());
    buffer.push_back('\0');

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessA(
        nullptr,
        buffer.data(),
        nullptr, nullptr, FALSE, 0, nullptr, nullptr,
        &si, &pi)) {
        std::cerr << "[ERROR] 7za.exe could not be executed. Error:" << GetLastError() << "\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // *** CLAVE ***
    // 7-Zip returns an error even though it extracts correctly.
    // If the folder contains content, it is also considered a success.
    if (fs::exists(outDir) && !fs::is_empty(outDir)) {
        std::cout << "[INFO] 7-Zip extracted content (ignoring code)" << exitCode << ")\n";
        return true;
    }

    std::cerr << "[ERROR] 7-Zip did not extract anything. Code: " << exitCode << "\n";
    return false;
}


// Archivo interno 'Inner'
fs::path findInnerFile(const fs::path& dir) {
    for (auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) continue;

        fs::path p = entry.path();
        // archivo sin extensión
        if (p.extension().string().empty()) {
            return p;
        }
    }
    return "";
}




// ========================Lectura de INI===================================== 
// (simple)


//struct
struct Config {
    std::string gmadLs;
    std::string gmodDir;
};

//start

bool loadConfig(const std::string& iniPath, Config& cfg) {
    std::ifstream in(iniPath);
    if (!in.is_open()) {
        std::cerr << "[ERROR] Could not open INI file: " << iniPath << "\n";
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (key == "GMAD-LS") {
            cfg.gmadLs = value;
        }
        else if (key == "GMOD-DIR") {
            cfg.gmodDir = value;
        }
    }

    if (cfg.gmadLs.empty() || cfg.gmodDir.empty()) {
        std::cerr << "[ERROR] GMAD-LS or GMOD-DIR not defined in the INI.\n";
        return false;
    }

    return true;
}


//==============================ejecucion de procesos (gmad.exe)============================

bool runProcessAndWait(const std::string& exePath, const std::string& args) {
    std::string cmdLine = "\"" + exePath + "\" " + args;

    // Buffer mutable 
    std::vector<char> buffer(cmdLine.begin(), cmdLine.end());
    buffer.push_back('\0');

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessA(
        nullptr,
        buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi)) {
        std::cerr << "[ERROR] The process could not be executed: " << exePath
            << "\nCode Error: " << GetLastError() << "\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        std::cerr << "[WARN] Process terminated with code: " << exitCode << "\n";
    }

    return true;
}

/*
// ===========================ZLIB==========================================


bool decompressZlibToFile(const fs::path& inputBin, const fs::path& outputRaw) {
    std::ifstream in(inputBin, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir .bin: " << inputBin << "\n";
        return false;
    }

    std::vector<unsigned char> compressed(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    if (compressed.empty()) {
        std::cerr << "[ERROR] Archivo .bin vacio: " << inputBin << "\n";
        return false;
    }

    // intento de descompresion ZLIB
    uLongf destLen = compressed.size() * 4;
    std::vector<unsigned char> decompressed(destLen);

    int res = uncompress(decompressed.data(), &destLen,
        compressed.data(), (uLong)compressed.size());

    if (res == Z_OK) {
        // exito: escribir archivo descomprimido
        decompressed.resize(destLen);
        std::ofstream out(outputRaw, std::ios::binary);
        out.write((char*)decompressed.data(), decompressed.size());
        return true;
    }

    // si no es ZLIB  copiar tal cual bin = gma disfrazado
    std::cout << "[INFO] .bin no comprimido, copiando directo: " << inputBin << "\n";

    std::error_code ec;
    fs::copy_file(inputBin, outputRaw, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[ERROR] No se pudo copiar .bin como archivo crudo: "
            << ec.message() << "\n";
        return false;
    }

    return true;
}

*/



//==========================inputs (movs de dirs)=======================

bool moveItem(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    ec.clear();
    fs::rename(src, dst, ec);
    if (ec) {
        ec.clear();
        if (fs::is_directory(src)) {
            fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
        else {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        }
        if (ec) {
            std::cerr << "[ERROR] Could not move/copy: " << src << " -> " << dst
                << " (" << ec.message() << ")\n";
            return false;
        }
        ec.clear();
        fs::remove_all(src, ec);
        if (ec) {
            std::cerr << "[WARN] Source could not be removed: " << src
                << " (" << ec.message() << ")\n";
        }
    }
    return true;
}

bool moveDirectoryContents(const fs::path& srcDir,
    const fs::path& dstDir,
    const std::string& skipName = "") {
    std::error_code ec;
    if (!fs::exists(srcDir, ec) || !fs::is_directory(srcDir, ec)) {
        return true; // nada que mover
    }

    for (auto& entry : fs::directory_iterator(srcDir, ec)) {
        if (ec) break;
        fs::path name = entry.path().filename();
        if (!skipName.empty() && toLower(name.string()) == toLower(skipName)) {
            continue;
        }
        fs::path target = dstDir / name;
        if (!moveItem(entry.path(), target)) {
            return false;
        }
    }
    return true;
}


//=====================processing folder extracted from a .gma=======================


bool processExtractedFolder(const fs::path& extractedDir,
    const fs::path& gmodDir) {
    std::error_code ec;

    if (!fs::exists(extractedDir, ec) || !fs::is_directory(extractedDir, ec)) {
        std::cerr << "[WARN] Extracted folder not found: " << extractedDir << "\n";
        return false;
    }

    fs::path mapsDir = extractedDir / "maps";
    fs::path gmodMaps = gmodDir / "maps";
    fs::path gmodAddons = gmodDir / "addons";

    bool hasMaps = fs::exists(mapsDir, ec) && fs::is_directory(mapsDir, ec);

    if (hasMaps) {
        std::cout << "[INFO] Folder contains 'maps': " << extractedDir << "\n";

        //  move maps a GMOD-DIR/maps/
        if (!moveDirectoryContents(mapsDir, gmodMaps)) {
            std::cerr << "[ERROR] Maps could not be moved from: " << mapsDir << "\n";
        }
        // clean maps folder if is empty
        ec.clear();
        fs::remove_all(mapsDir, ec);

        // the rest of the folder to a GMOD-DIR/addons/
        fs::path targetAddonFolder = gmodAddons / extractedDir.filename();
        fs::create_directories(targetAddonFolder, ec);
        if (!moveDirectoryContents(extractedDir, targetAddonFolder, "maps")) {
            std::cerr << "[ERROR] Could not move the rest of the folder to addons: "
                << extractedDir << "\n";
        }

        // clean folder original if is empty
        ec.clear();
        fs::remove_all(extractedDir, ec);
    }
    else {
        std::cout << "[INFO] Folder without 'maps', goes entirely to addons: " << extractedDir << "\n";

        fs::path targetAddonFolder = gmodAddons / extractedDir.filename();
        if (!moveItem(extractedDir, targetAddonFolder)) {
            std::cerr << "[ERROR] Could not move folder to addons: " << extractedDir << "\n";
            return false;
        }
    }

    return true;
}


//=============================processing a .gma file============================


bool processGmaFile(const fs::path& gmaPath,
    const fs::path& gmadExe,
    const fs::path& gmodDir) {
    std::cout << "[INFO] Processing .gma: " << gmaPath << "\n";

    if (!fs::exists(gmadExe)) {
        std::cerr << "[ERROR] gmad.exe not found in: " << gmadExe << "\n";
        return false;
    }

    // command de gmad: extract -file "archivo.gma"
    std::stringstream args;
    args << "extract -file \"" << gmaPath.string() << "\"";

    if (!runProcessAndWait(gmadExe.string(), args.str())) {
        std::cerr << "[ERROR] gmad.exe failed to execute for: " << gmaPath << "\n";
        return false;
    }

    // Extracted folder: same name as the .gma file without the extension
    fs::path extractedDir = gmaPath.parent_path() / gmaPath.stem();
    return processExtractedFolder(extractedDir, gmodDir);
}


//======================================Processing .bin======================================


bool processBinFile(const fs::path& binPath,
    const fs::path& gmadExe,
    const fs::path& gmodDir) {

    std::cout << "[INFO] Processing .bin: " << binPath << "\n";

    fs::path tempDir = binPath.parent_path() / (binPath.stem().string() + "_extracted");
    fs::create_directories(tempDir);

    /*
    //  Intentar ZLIB (por si acaso)
    fs::path rawPath = binPath;
    rawPath.replace_extension("");

    if (decompressZlibToFile(binPath, rawPath)) {
        fs::path gmaPath = rawPath;
        gmaPath.replace_extension(".gma");

        std::error_code ec;
        fs::rename(rawPath, gmaPath, ec);

        if (!ec) {
            return processGmaFile(gmaPath, gmadExe, gmodDir);
        }
    }
    */

    //  EXTRAct with 7-Zip SIEMPRE
    std::cout << "[INFO] Extracting with 7-Zip (forced): " << binPath << "\n";
    extractWith7Zip(binPath, tempDir);

    //  Get internal file without extension
    fs::path inner = findInnerFile(tempDir);

    if (!inner.empty()) {
        fs::path gmaPath = inner;
        gmaPath.replace_extension(".gma");

        std::error_code ec;
        fs::rename(inner, gmaPath, ec);

        if (!ec) {
            std::cout << "[INFO] Internal file found and renamed to .gma: " << gmaPath << "\n";
            return processGmaFile(gmaPath, gmadExe, gmodDir);
        }
    }

    std::cerr << "[ERROR] No internal file found after extracting the .bin.\n";
    return false;
}






//==================================welcome mensaje===================================


void printWelcome() {
    // ANSI escape codes: \033[34m = Blue, \033[0m = Reset
    // ANSI escape codes: \033[36m = Cyan (Lighter Blue), \033[0m = Reset
    std::cout
        << "=============================================================\n"
        << "                 \033[36mGMoD-TeachGMAD - Auto GMA Install\033[0m\n"
        << "=============================================================\n"
        << "  This utility automates the extraction and installation of  \n"
        << "             .GMA's/.BIN's of Garry s Mod                    \n"
        << "                                                             \n"
        << "_________________________                                    \n"
        << "S.G.Roman      |v0.712  |                                    \n"
        << "=============================================================\n\n";
}


//=================================main=======================================


int main() {
    SetConsoleOutputCP(CP_UTF8);

    printWelcome();
    std::cout << "Press ENTER to start the installation process...\n";
    std::cin.get();

    std::string exeDir = getExecutableDirectory();
    fs::path exePath(exeDir);

    fs::path iniPath = exePath / "config.ini";
    Config cfg;
    if (!loadConfig(iniPath.string(), cfg)) {
        std::cerr << "\n[ERROR] Configuration could not be loaded. Press ENTER to exit.\n";
        std::cin.get();
        return 1;
    }

    fs::path gmadLsPath = cfg.gmadLs;
    fs::path gmodDirPath = cfg.gmodDir;
    fs::path gmadExePath = exePath / "gmad.exe";

    std::cout << "[INFO] GMAD-LS  = " << gmadLsPath << "\n";
    std::cout << "[INFO] GMOD-DIR = " << gmodDirPath << "\n";
    std::cout << "[INFO] gmad.exe = " << gmadExePath << "\n\n";

    std::error_code ec;
    if (!fs::exists(gmadLsPath, ec) || !fs::is_directory(gmadLsPath, ec)) {
        std::cerr << "[ERROR] GMAD-LS is not a valid directory: " << gmadLsPath << "\n";
        std::cerr << "Press ENTER to exit.\n";
        std::cin.get();
        return 1;
    }


    //nelson mandela(osea nop)
    if (!fs::exists(gmodDirPath, ec) || !fs::is_directory(gmodDirPath, ec)) {
        std::cerr << "[ERROR] GMOD-DIR is not a valid directory: " << gmodDirPath << "\n";
        std::cerr << "Press ENTER to exit.\n";
        std::cin.get();
        return 1;
    }

    // Initial file get
    std::vector<fs::path> binFiles;
    std::vector<fs::path> gmaFiles;

    for (auto& entry : fs::recursive_directory_iterator(gmadLsPath, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        fs::path p = entry.path();
        std::string ext = toLower(p.extension().string());

        if (ext == ".bin") binFiles.push_back(p);
        else if (ext == ".gma") gmaFiles.push_back(p);
    }

    std::cout << "[INFO] Found " << binFiles.size() << " files .bin and "
        << gmaFiles.size() << " original .gma files.\n\n";

    // Process .bin files (these can generate new .gma files)
    for (const auto& bin : binFiles) {
        if (!processBinFile(bin, gmadExePath, gmodDirPath)) {
            std::cerr << "[WARN] bin process fail: " << bin << "\n";
        }
    }

    // rescan for new GMAS after bin process
    if (!binFiles.empty()) {
        gmaFiles.clear();
        for (auto& entry : fs::recursive_directory_iterator(gmadLsPath, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            if (toLower(entry.path().extension().string()) == ".gma") {
                gmaFiles.push_back(entry.path());
            }
        }
    }

    // Process all .gma files (both the originals and those extracted from .bin files)
    for (const auto& gma : gmaFiles) {
        if (!processGmaFile(gma, gmadExePath, gmodDirPath)) {
            std::cerr << "[WARN] Processing of .gma failed: " << gma << "\n";
        }
    }

    std::cout << "Process completed. Check your Garry's Mod folder.\n";
    std::cout << "Press ENTER to exit.\n";
    std::cin.get();
    return 0;

    //nomamei

/*
 _____________________________________________________________________________|
                                    LICENSE                                   |
 =============================================================================|
 GMOD Teach Gmad v1.0 - Developed by Sebastian G. Romanelli (2026)            |
 This software is Open-Source under the CC BY-NC 4.0 License.                 |
 FREE FOR THE COMMUNITY. COMMERCIAL SALE OR PAID REDISTRIBUTION IS PROHIBITED.|
 =============================================================================|
*/

}
