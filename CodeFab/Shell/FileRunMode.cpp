#include "FileRunMode.h"

#include <fstream>
#include <sstream>
#include <vector>

FileRunMode::FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                          CheckerInterface& checker, ExecuteInterface& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), executor_(executor) {
}

bool FileRunMode::run(const std::string& path, std::ostream& out) {
    std::ifstream file(path);
    if (!file) {
        // FileSourceReader::read()(Assembler/FileSourceReader.cpp)와 동일한
        // 문구를 사용해 "파일을 열 수 없다"는 오류 메시지의 표현을 통일한다.
        out << "파일을 열 수 없습니다: " << path << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    // Assembler::assemble()은 REPL 한 줄(= 문장 하나)만 파싱하도록 설계되어 있어서
    // (parseStatement()를 한 번만 호출), 문장이 여러 개인 파일을 그대로 넘기면 첫
    // 문장만 파싱되고 나머지는 버려진다. 파일 전체를 "{ ... }"로 감싸 하나의
    // BlockStatement로 만들면 Assembler/Checker/Executor 어느 쪽도 새로 손대지
    // 않고 여러 top-level 문장을 그대로 실행할 수 있다 - 이미 블록 스코프
    // (BlockStatement)가 진입/종료 시 새 변수 저장소를 만들었다 없애는 것과 동일한
    // 방식이므로 파일 하나를 "하나의 블록"으로 취급해도 의미가 달라지지 않는다.
    std::string source = "{" + buffer.str() + "}";

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
        // std::exception이다. RunPromptShell과 달리 파일 모드는 "입력을 더
        // 받는다"는 개념이 없으므로(파일 전체를 이미 다 읽었다), 여기서는
        // IncompleteInputError도 다른 예외와 동일하게 실패로 처리한다 -
        // 파일 끝까지 괄호/문자열이 닫히지 않았다는 뜻이므로 실제 오류가 맞다.
        out << e.what() << "\n";
        return false;
    }
}
