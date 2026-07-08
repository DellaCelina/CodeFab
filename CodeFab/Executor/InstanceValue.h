#pragma once

#include <memory>

#include "Scope.h"

struct ClassDeclareStatement;

// 클래스 인스턴스 하나를 나타낸다(Architecture.md §4.4). 필드 저장소로 기존
// Scope를 그대로 재사용한다 - "없는 필드에 쓰면 새로 생성, 있으면 읽기/쓰기"라는
// 요구사항이 Scope::define/assign/get의 기존 동작과 정확히 일치하기 때문에
// 별도 자료구조를 만들지 않는다. klass는 SyntaxTree가 소유한 선언 노드를
// 가리키는 비소유 참조 포인터다(SyntaxTree가 프로그램 실행 내내 살아있음을
// 전제로 한다). fields를 shared_ptr로 두는 이유는 같은 인스턴스를 가리키는 여러
// Value가 필드 변경을 공유해야 하기 때문이다(참조 의미론).
struct InstanceValue {
    const ClassDeclareStatement* klass;
    std::shared_ptr<Scope> fields;
};
