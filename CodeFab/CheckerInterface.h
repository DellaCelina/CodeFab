#pragma once

#include "SyntaxTree.h"

// 문법 트리를 실행 전 검사한다. (담당: Checker)
// 의미 오류(변수 중복 선언, 자기 참조 등) 발견 시 상세 메시지가 필요한 경우
// CheckError(line, message) 를 throw 할 수 있다.
// throw 없이 단순 실패만 알릴 경우 false 를 반환해도 되며,
// Shell은 두 가지 방식 모두를 처리한다.
class CheckerInterface {
public:
    virtual ~CheckerInterface() = default;

    virtual bool Check(SyntaxTree& tree) = 0;
};
