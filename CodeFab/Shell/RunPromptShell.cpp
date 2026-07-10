#include "RunPromptShell.h"

#include <cctype>
#include <string>

namespace {

const char* kPrompt = ">>> ";
const char* kContinuationPrompt = "... ";
const char* kExitCommand = "exit";

bool IsBlank(const std::string& s) {
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

// 입력한 한 줄이 '\'로 끝나면 아직 입력이 끝나지 않은 것으로 보고,
// 다음 줄을 이어받기 위해 true를 반환한다. 이때 줄 끝의 '\'는 버퍼에서 제거한다.
bool ConsumeLineContinuation(std::string& line) {
    if (!line.empty() && line.back() == '\\') {
        line.pop_back();
        return true;
    }
    return false;
}

}  // namespace

RunPromptShell::RunPromptShell(TokenizeInterface& tokenizer,
                                AssemblerInterface& assembler,
                                CheckerInterface& checker,
                                OptimizerInterface& optimizer,
                                ExecuteInterface& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), optimizer_(optimizer),
      executor_(executor) {
}

void RunPromptShell::run(std::istream& in, std::ostream& out) {
    std::string buffer;
    std::string line;
    std::vector<SyntaxTree> sessionTrees; // 선언 노드 수명을 세션 전체로 유지

    out << kPrompt;
    while (std::getline(in, line)) {
        if (buffer.empty() && line == kExitCommand) {
            return;
        }

        bool lineContinues = ConsumeLineContinuation(line);

        if (!buffer.empty()) {
            buffer += "\n";
        }
        buffer += line;

        if (lineContinues) {
            // '\'로 끝난 줄: 아직 입력이 끝나지 않았으므로 실행하지 않고 다음 줄을 이어받는다.
            out << kContinuationPrompt;
            continue;
        }

        if (IsBlank(buffer)) {
            buffer.clear();
            out << kPrompt;
            continue;
        }

        try {
            std::vector<Token> tokens = tokenizer_.tokenize(buffer);
            SyntaxTree tree = assembler_.assemble(tokens);
            std::vector<SyntaxNode*> statements = tree.getRoots();
            for (SyntaxNode* statement : statements) {
                tree.setRoot(statement);
                checker_.check(tree);
                executor_.execute(tree);
            }
            sessionTrees.push_back(std::move(tree));
        } catch (const std::exception& e) {
            out << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
