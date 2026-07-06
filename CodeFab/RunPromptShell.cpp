#include "RunPromptShell.h"

#include <cctype>
#include <string>

#include "InputBuffer.h"
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

void RunPromptShell::Run(std::istream& in, std::ostream& out) {
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

        if (!IsInputComplete(buffer)) {
            out << kContinuationPrompt;
            continue;
        }

        try {
            std::vector<Token> tokens = tokenizer_.Tokenize(buffer);
            SyntaxTree tree = assembler_.Assemble(tokens);
            if (checker_.Check(tree)) {
                executor_.Execute(tree);
            } else {
                out << "코드 검사에 실패했습니다.\n";
            }
        } catch (const CodeFabError& e) {
            out << "[" << e.Line() << "번째 줄] " << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
