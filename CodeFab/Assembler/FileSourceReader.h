#pragma once

#include "SourceReaderInterface.h"

// SourceReaderInterface의 실제 파일 시스템 구현체. 운영 환경(main.cpp)에서
// 사용하고, 테스트에서는 대신 인메모리 Fake를 주입한다.
class FileSourceReader : public SourceReaderInterface {
public:
    std::string read(const std::string& path) override;
};
