#include "DebugMode.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
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

    // FileRunMode.cpp와 동일한 이유로 파일 전체를 "{ }"로 감싸 하나의
    // BlockStatement로 만든다 - Assembler::assemble()이 문장 하나만
    // 파싱하기 때문이다.
    std::string source = "{" + buffer.str() + "}";

    // Debugger가 정지 시 "-> <원본 소스 줄>"을 보여줄 수 있도록, 원본 파일
    // 내용(래핑하기 전)을 줄 단위로 나눠 넘긴다. "{"를 줄바꿈 없이 맨 앞에
    // 붙였을 뿐이라(§ 위 주석), 원본 파일의 줄 번호와 토큰의 line은 항상
    // 그대로 일치한다.
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
        checker_.check(tree);

        // 여기서 만든 "{ }" 래핑 블록은 실제 소스에는 없는 합성 statement라,
        // executor_.execute(tree)로 그대로 실행하면 Executor::execute(Statement*)가
        // 이 블록 자체에도 statementHook_을 호출한다 - Debugger가 Step 모드에서는
        // 이걸 실제 문장과 구분하지 못해 파일의 첫 줄에서 한 번 더(가짜로) 멈추고,
        // depth도 한 칸씩 밀려 "next"가 최상위 문장을 하나 더 깊은 것으로
        // 취급하게 된다. 그래서 래핑 블록 자체는 실행하지 않고, 그 안의
        // top-level 문장들을 직접 하나씩 실행한다 - 각 top-level 문장이 depth
        // 1부터 시작하고, 래핑 블록은 디버거에 전혀 보이지 않는다.
        auto* root = dynamic_cast<BlockStatement*>(tree.getRoot());
        if (!root) {
            throw std::logic_error("DebugMode::run: wrapped tree root is not a BlockStatement");
        }
        for (Statement* stmt : root->statements) {
            executor_.execute(stmt);
        }
        return true;
    } catch (const std::exception& e) {
        // AssemblyError/AssemblerError/CheckerError/ExecutorError/
        // IncompleteInputError 모두 각자의 인터페이스 헤더에 정의된 순수
        // std::exception이다 (FileRunMode.cpp와 동일).
        out << e.what() << "\n";
        return false;
    }
}
