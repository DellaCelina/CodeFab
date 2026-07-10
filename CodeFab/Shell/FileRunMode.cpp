#include "FileRunMode.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

FileRunMode::FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                          CheckerInterface& checker, OptimizerInterface& optimizer,
                          ExecuteInterface& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), optimizer_(optimizer),
      executor_(executor) {
}

bool FileRunMode::run(const std::string& filePath, std::ostream& out) {
    if (std::filesystem::exists(filePath) && !std::filesystem::is_regular_file(filePath)) {
        out << "Error: path must be a single file: " << filePath << "\n";
        return false;
    }

    std::ifstream file(filePath);
    if (!file) {
        out << "Error: cannot open file: " << filePath << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    // assemble()이 문장 하나만 파싱하므로, 파일 전체를 BlockStatement 하나로 감싼다.
    std::string source = "{" + buffer.str() + "}";

    try {
        std::vector<Token> tokens = tokenizer_.tokenize(source);
        SyntaxTree tree = assembler_.assemble(tokens);
        checker_.check(tree);
        optimizer_.optimize(tree);
        executor_.execute(tree);
        return true;
    } catch (const std::exception& e) {
        out << e.what() << "\n";
        return false;
    }
}
