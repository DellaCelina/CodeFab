#include "DebugMode.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "Debugger.h"

DebugMode::DebugMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                      CheckerInterface& checker, Executor& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), executor_(executor) {
}

bool DebugMode::run(const std::string& filePath, std::istream& in, std::ostream& out) {
    // FileRunMode.cpp와 동일한 검증: filePath는 항상 파일 1개(단일 파일)여야
    // 한다.
    if (std::filesystem::exists(filePath) && !std::filesystem::is_regular_file(filePath)) {
        out << "path는 파일 1개(단일 파일)여야 합니다: " << filePath << "\n";
        return false;
    }

    std::ifstream file(filePath);
    if (!file) {
        out << "파일을 열 수 없습니다: " << filePath << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    // FileRunMode.cpp와 동일한 이유로 파일 전체를 "{ }"로 감싸 하나의
    // BlockStatement로 만든다 - Assembler::assemble()이 문장 하나만
    // 파싱하기 때문이다.
    std::string source = "{" + buffer.str() + "}";

    Debugger debugger(executor_, in, out);
    executor_.setStatementHook([&debugger](Statement* stmt, int depth) {
        debugger.onStatement(stmt, depth);
    });
    // debugger는 이 함수가 끝나면 사라지는 지역 변수라, 위 훅을 실행이
    // 끝나거나 예외가 나거나 상관없이 반드시 해제해야 한다 - 그러지 않으면
    // executor_(이 함수보다 오래 사는 참조)가 나중에 다시 실행될 때 이미
    // 소멸된 debugger를 참조하는 댕글링 콜백이 남는다.
    struct HookGuard {
        Executor& executor;
        ~HookGuard() { executor.setStatementHook(nullptr); }
    } hookGuard{executor_};

    try {
        std::vector<Token> tokens = tokenizer_.tokenize(source);
        SyntaxTree tree = assembler_.assemble(tokens);
        if (!checker_.check(tree)) {
            out << "코드 검사에 실패했습니다.\n";
            return false;
        }
        executor_.execute(tree);
        return true;
    } catch (const std::exception& e) {
        // AssemblyError/AssemblerError/CheckerError/ExecutorError/
        // IncompleteInputError 모두 각자의 인터페이스 헤더에 정의된 순수
        // std::exception이다 (FileRunMode.cpp와 동일).
        out << e.what() << "\n";
        return false;
    }
}
