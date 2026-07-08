#pragma once

#include <string>
#include <vector>

#include "../Tokenizer/Token.h"

// 소스 파일을 읽어 토큰화까지 마친 결과를 돌려주는 것을 추상화한다. import
// 구문(3일차 확장)을 처리할 때 Assembler가 파일 시스템/Tokenizer에 직접
// 의존하지 않고 이 인터페이스 하나만 통해 "파일 경로 -> 토큰 목록"을 얻도록
// 하기 위함이다 - Architecture.md §7.1 참고. 테스트에서는 실제 파일과 실제
// Tokenizer 없이 인메모리 Fake로 손쉽게 대체할 수 있다.
class SourceReaderInterface {
public:
    virtual ~SourceReaderInterface() = default;

    // path의 소스를 읽어 토큰화까지 완료한 결과를 반환한다. 파일을 열 수
    // 없거나 토큰화에 실패하면 구현체가 std::exception(또는 그 하위 타입)을
    // 던진다. Assembler는 이를 잡아 줄 번호/문맥 정보를 덧붙인
    // AssemblerError로 다시 던지는 것을 권장한다.
    virtual std::vector<Token> read(const std::string& path) = 0;
};
