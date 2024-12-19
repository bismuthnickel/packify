#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <sys/stat.h>
#include <filesystem>
#include <cstring>
#include <cstdlib>

bool exists(const std::string &path) {
    std::filesystem::path p(path);
    return std::filesystem::exists(p);
}

void create_directory(const std::string &path) {
    if (!exists(path)) {
        std::filesystem::create_directories(path);
    }
}

size_t write_data(void *ptr, size_t size, size_t nmemb, void *data) {
    FILE *write_here = (FILE *)data;
    return fwrite(ptr, size, nmemb, write_here);
}

void download_package(const std::string &url, const std::string &filename) {
    CURL *curl;
    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Error: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }

    fclose(fp);
    curl_global_cleanup();
}

void parse_pkfy(const std::string &filename, std::string &name, std::string &version, std::vector<std::string> &dependencies) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    std::getline(file, name);
    std::getline(file, version);

    std::string line;
    while (std::getline(file, line)) {
        dependencies.push_back(line);
    }

    file.close();
}

void install_package(const std::string &package_path, const std::string &install_path) {
    create_directory("/usr/local/bin");

    std::ifstream src(package_path, std::ios::binary);
    if (!src.is_open()) {
        std::cerr << "Error: Unable to open source file " << package_path << std::endl;
        return;
    }

    std::ofstream dest(install_path, std::ios::binary);
    if (!dest.is_open()) {
        std::cerr << "Error: Unable to create destination file " << install_path << std::endl;
        return;
    }

    dest << src.rdbuf();

    src.close();
    dest.close();

    std::string command = "chmod +x " + install_path;
    system(command.c_str());

    std::cout << "Installed package to " << install_path << std::endl;
}

void delete_temp_file(const std::string &file_path) {
    if (std::filesystem::exists(file_path)) {
        std::filesystem::remove(file_path);
        std::cout << "Deleted temporary file: " << file_path << std::endl;
    }
}

void install_command(const std::string &package_name) {
    std::string package_url = "https://raw.githubusercontent.com/bismuthnickel/pkfy/refs/heads/main/" + package_name;
    std::string temp_package_file = "/tmp/" + package_name + ".pkfy";
    std::string temp_package = "/tmp/" + package_name;

    std::cout << "Downloading package " << package_name << " from " << package_url << "...\n";
    download_package(package_url + ".pkfy", temp_package_file);
    download_package(package_url, temp_package);

    std::string name, version;
    std::vector<std::string> dependencies;
    parse_pkfy(temp_package_file, name, version, dependencies);

    std::cout << "Installing " << name << " version " << version << std::endl;

    std::string install_path = "/usr/local/bin/" + name;
    install_package(temp_package, install_path);

    for (const auto &dep : dependencies) {
        std::cout << "Installing dependency: " << dep << std::endl;
        install_command(dep);
    }

    delete_temp_file(temp_package_file);
    delete_temp_file(temp_package);
}

void uninstall_command(const std::string &package_name) {
    std::string packages_file = "/usr/local/share/.packages";

    if (!exists(packages_file)) {
        std::cerr << "Error: No packages installed yet.\n";
        return;
    }

    std::ifstream file(packages_file);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open .packages file\n";
        return;
    }

    std::vector<std::string> packages;
    std::string line;
    while (std::getline(file, line)) {
        if (line != package_name) {
            packages.push_back(line);
        }
    }
    file.close();

    std::ofstream out(packages_file);
    if (!out.is_open()) {
        std::cerr << "Error: Unable to write to .packages file\n";
        return;
    }

    for (const auto &pkg : packages) {
        out << pkg << std::endl;
    }

    out.close();

    std::string install_path = "/usr/local/bin/" + package_name;
    if (std::filesystem::exists(install_path)) {
        std::filesystem::remove(install_path);
        std::cout << "Uninstalled package: " << package_name << std::endl;
    } else {
        std::cerr << "Error: Package binary not found\n";
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: packify <command> <package_name>\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "install" && argc < 3) {
        std::cerr << "Error: Package name required for install.\n";
        return 1;
    }

    if (command == "uninstall" && argc < 3) {
        std::cerr << "Error: Package name required for uninstall.\n";
        return 1;
    }

    std::string package_name = argc > 2 ? argv[2] : "";

    if (command == "install" && !package_name.empty()) {
        install_command(package_name);
    } else if (command == "uninstall" && !package_name.empty()) {
        uninstall_command(package_name);
    } else {
        std::cerr << "Unknown command or missing arguments.\n";
        return 1;
    }

    return 0;
}
