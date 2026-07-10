#include "FileSourceReader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

FileSourceReader::FileSourceReader(TokenizeInterface& tokenizer) : tokenizer_(tokenizer) {
}

std::vector<Token> FileSourceReader::read(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return tokenizer_.tokenize(buffer.str());
}
