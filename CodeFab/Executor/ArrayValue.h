#pragma once

#include <vector>

#include "Value.h"

// 고정 크기 배열 하나를 나타낸다(Architecture.md §5). Value.h는 이 타입을
// shared_ptr로만 가리키므로 전방 선언만으로 충분하지만, vector<Value>의 원소
// 타입인 Value는 여기서는 완전한 타입이어야 하므로 Value.h를 직접 include한다
// (Value.h -> ArrayValue.h 방향의 include는 없으므로 순환이 아니다).
struct ArrayValue {
    std::vector<Value> items;
};
