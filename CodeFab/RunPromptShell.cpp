#include "RunPromptShell.h"

#include <cctype>
#include <string>

#include "ShellErrors.h"

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

}  // namespace

RunPromptShell::RunPromptShell(TokenizeInterface& tokenizer,
                                AssemblerInterface& assembler,
                                CheckerInterface& checker,
                                ExecuteInterface& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), executor_(executor) {
}

void RunPromptShell::run(std::istream& in, std::ostream& out) {
    std::string buffer;
    std::string line;

    out << kPrompt;
    while (std::getline(in, line)) {
        if (buffer.empty() && line == kExitCommand) {
            return;
        }

        if (!buffer.empty()) {
            buffer += "\n";
        }
        buffer += line;

        if (IsBlank(buffer)) {
            buffer.clear();
            out << kPrompt;
            continue;
        }

        try {
            std::vector<Token> tokens = tokenizer_.tokenize(buffer);
            SyntaxTree tree = assembler_.assemble(tokens);
            if (checker_.check(tree)) {
                executor_.execute(tree);
            } else {
                out << "코드 검사에 실패했습니다.\n";
            }
        } catch (const IncompleteInputError&) {
            // 괄호/문자열이 아직 안 닫힌 상태: 버퍼를 비우지 않고 다음 줄을 이어받는다.
            out << kContinuationPrompt;
            continue;
        } catch (const CodeFabError& e) {
            out << "[" << e.line() << "번째 줄] " << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
