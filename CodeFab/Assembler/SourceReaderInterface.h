#pragma once

#include <string>

// 소스 파일을 읽어오는 방법을 추상화한다. import 구문(3일차 확장)을 처리할 때
// Assembler가 파일 시스템에 직접 의존하지 않고 이 인터페이스를 통해서만 파일
// 내용을 읽도록 하기 위함이다 - Architecture.md §7.1 참고. 테스트에서는 실제
// 파일 없이 인메모리 Fake로 손쉽게 대체할 수 있다.
class SourceReaderInterface {
public:
    virtual ~SourceReaderInterface() = default;

    // path의 소스 전체를 문자열로 읽어 반환한다. 파일을 열 수 없으면 구현체가
    // std::exception(또는 그 하위 타입)을 던진다. Assembler는 이를 잡아 줄
    // 번호/문맥 정보를 덧붙인 AssemblerError로 다시 던지는 것을 권장한다.
    virtual std::string read(const std::string& path) = 0;
};
