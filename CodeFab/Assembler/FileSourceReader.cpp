#include "FileSourceReader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

std::string FileSourceReader::read(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("파일을 열 수 없습니다: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
