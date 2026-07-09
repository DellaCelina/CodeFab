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
            // 한 제출(buffer)에 최상위 statement가 여러 개 들어올 수 있다(예: '\'로
            // 이어붙인 여러 줄). assemble()이 돌려주는 tree는 이제 root를 여러 개
            // 담을 수 있으므로(SyntaxTree::getRoots() 참고), 그 순서대로 하나씩
            // check+execute한다 - 새 블록 스코프로 감싸지 않고 그대로 실행해서
            // REPL의 "한 줄 선언 -> 다음 줄에서 사용" 동작을 그대로 유지한다.
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
            // AssemblyError/AssemblerError/CheckerError/ExecutorError 모두 각자의
            // 인터페이스 헤더(TokenizeInterface.h/AssemblerInterface.h/
            // CheckerInterface.h/ExecuteInterface.h)에 정의된, 줄 번호를 별도로
            // 들고 있지 않는 순수 std::exception이다 (필요하면 메시지에 직접
            // 줄 번호를 담는다). 그래서 여기서 한 번에 잡아 메시지만 보고한다.
            out << e.what() << "\n";
        }

        buffer.clear();
        out << kPrompt;
    }
}
