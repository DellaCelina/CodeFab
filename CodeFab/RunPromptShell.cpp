#include "RunPromptShell.h"

#include <cctype>
#include <stdexcept>
#include <string>

#include "CheckerInterface.h"
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
        } catch (const CheckerError& e) {
            // CheckerError는 CodeFabError를 상속하지 않는 독립적인 예외라서
            // (CheckerInterface.h 참고, ExecutorError와 같은 방식) 별도로 잡아야 한다.
            out << "[" << e.line() << "번째 줄] " << e.what() << "\n";
        } catch (const CodeFabError& e) {
            out << "[" << e.line() << "번째 줄] " << e.what() << "\n";
        } catch (const std::invalid_argument& e) {
            // Assembler가 문법 오류를 std::invalid_argument로 던진다 (line() 정보 없음).
            out << e.what() << "\n";
        } catch (const std::exception& e) {
            // Executor가 타입 불일치 등을 만나면 ExecutorError를 던진다. ExecutorError는
            // line() 정보가 없는 순수 std::exception이라 여기서 마지막 안전망으로 잡아
            // 메시지만 보고한다 (RunPromptShell.cpp 밖에서 줄 번호를 붙일 방법이 없다).
            out << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
