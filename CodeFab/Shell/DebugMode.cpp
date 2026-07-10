#include "DebugMode.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Debugger.h"

DebugMode::DebugMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                      CheckerInterface& checker, OptimizerInterface& optimizer, Executor& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), optimizer_(optimizer),
      executor_(executor) {
}

bool DebugMode::run(const std::string& filePath, std::istream& in, std::ostream& out) {
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

    // 래핑 전 원본을 줄 단위로 나눠 Debugger에 넘긴다(줄 번호가 그대로 일치).
    std::vector<std::string> sourceLines;
    {
        std::istringstream lineStream(buffer.str());
        std::string lineText;
        while (std::getline(lineStream, lineText)) {
            if (!lineText.empty() && lineText.back() == '\r') {
                lineText.pop_back();
            }
            sourceLines.push_back(lineText);
        }
    }

    Debugger debugger(executor_, in, out, sourceLines);
    executor_.setStatementHook([&debugger](Statement* stmt, int depth) {
        debugger.onStatement(stmt, depth);
    });
    // debugger가 소멸된 후 executor_가 훅을 호출하는 댕글링을 방지한다.
    struct HookGuard {
        Executor& executor;
        ~HookGuard() { executor.setStatementHook(nullptr); }
    } hookGuard{executor_};

    try {
        std::vector<Token> tokens = tokenizer_.tokenize(source);
        SyntaxTree tree = assembler_.assemble(tokens);
        checker_.check(tree);
        optimizer_.optimize(tree);

        // 래핑 블록 자체는 훅을 거치지 않도록, 안의 문장들을 직접 실행한다.
        // 그래야 각 top-level 문장이 depth 1부터 시작하고 "next"가 올바르게 동작한다.
        auto* root = dynamic_cast<BlockStatement*>(tree.getRoot());
        if (!root) {
            throw std::logic_error("DebugMode::run: wrapped tree root is not a BlockStatement");
        }
        for (Statement* stmt : root->statements) {
            executor_.execute(stmt);
        }
        return true;
    } catch (const std::exception& e) {
        out << e.what() << "\n";
        return false;
    }
}
