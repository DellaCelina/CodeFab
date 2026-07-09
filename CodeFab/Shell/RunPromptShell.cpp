#include "RunPromptShell.h"

#include <cctype>
#include <string>
#include <string_view>

#include "../Assembler/AssemblerInterface.h"

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

// Assembler.cpp의 popExpectedToken()/parsePrimary()는 다음 토큰을 더 기대했는데
// 토큰이 하나도 남지 않은 경우(EOF) 줄 번호 없이 메시지만 담아 AssemblerError를
// 던진다("Expect '}' after block.", "unexpected end of expression." 등). 반면
// 실제 문법 오류(토큰은 있는데 종류가 틀림)는 항상 makeParseError()를 거쳐
// "[line N] ... (near '...')" 형식으로 던진다. 이 차이를 "아직 문장이 끝나지
// 않아서 다음 줄을 더 받아야 한다"는 신호로 그대로 재사용한다 - 별도의 괄호
// 깊이 카운터를 셀 필요 없이, if(...) 처럼 블록이 없는 문장이나 여러 줄에 걸친
// for/if/블록 전부 동일한 방식으로 감지된다.
bool IsIncompleteInputError(const AssemblerError& e) {
    return !std::string_view(e.what()).starts_with("[line");
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
    std::vector<SyntaxTree> sessionTrees; // Func/Class 선언 노드 수명을 세션 전체로 유지

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
            checker_.check(tree);
            executor_.execute(tree);
            sessionTrees.push_back(std::move(tree));
        } catch (const AssemblerError& e) {
            if (IsIncompleteInputError(e)) {
                // 아직 문장이 끝나지 않았다(예: if/for 조건 뒤 본문이 아직 없거나,
                // 블록을 닫는 '}'가 아직 안 옴) - buffer를 비우지 않고 다음 줄을
                // 이어받는다.
                out << kContinuationPrompt;
                continue;
            }
            out << e.what() << "\n";
        } catch (const std::exception& e) {
            // AssemblyError/CheckerError/ExecutorError 모두 각자의 인터페이스
            // 헤더(TokenizeInterface.h/CheckerInterface.h/ExecuteInterface.h)에
            // 정의된, 줄 번호를 별도로 들고 있지 않는 순수 std::exception이다
            // (필요하면 메시지에 직접 줄 번호를 담는다). 그래서 여기서 한 번에
            // 잡아 메시지만 보고한다.
            out << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
