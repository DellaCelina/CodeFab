#include "FileRunMode.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

FileRunMode::FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                          CheckerInterface& checker, ExecuteInterface& executor)
    : tokenizer_(tokenizer), assembler_(assembler), checker_(checker), executor_(executor) {
}

bool FileRunMode::run(const std::string& filePath, std::ostream& out) {
    // filePath는 항상 파일 1개(단일 파일)의 경로여야 한다. 디렉터리 등 파일이
    // 아닌 경로가 오면(예: 실수로 폴더 경로를 넘긴 경우) ifstream이 플랫폼에
    // 따라 애매하게 동작할 수 있으므로, 여기서 명시적으로 걸러 분명한 오류로
    // 보고한다. 경로가 아예 존재하지 않는 경우는 이 검사를 통과시키고 바로
    // 아래 ifstream 오픈 실패로 처리한다(존재 여부와 무관하게 exists()가
    // false를 던지지 않으므로 안전).
    if (std::filesystem::exists(filePath) && !std::filesystem::is_regular_file(filePath)) {
        out << "path는 파일 1개(단일 파일)여야 합니다: " << filePath << "\n";
        return false;
    }

    std::ifstream file(filePath);
    if (!file) {
        // FileSourceReader::read()(Assembler/FileSourceReader.cpp)와 동일한
        // 문구를 사용해 "파일을 열 수 없다"는 오류 메시지의 표현을 통일한다.
        out << "파일을 열 수 없습니다: " << filePath << "\n";
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
        out << e.what() << "\n";
        return false;
    }
}
