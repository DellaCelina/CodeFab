#pragma once

#include <memory>

#include "Scope.h"

struct ClassDeclareStatement;

// 클래스 인스턴스. klass는 비소유 포인터(SyntaxTree가 소유),
// fields는 참조 의미론을 위해 shared_ptr로 공유된다.
struct InstanceValue {
    const ClassDeclareStatement* klass;
    std::shared_ptr<Scope> fields;
};
