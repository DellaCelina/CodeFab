#include "InputBuffer.h"

namespace {

bool IsOpenBracket(char c) {
    return c == '(' || c == '{' || c == '[';
}

bool IsCloseBracket(char c) {
    return c == ')' || c == '}' || c == ']';
}

}  // namespace

bool IsInputComplete(const std::string& source) {
    int depth = 0;
    bool inString = false;

    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];

        if (inString) {
            if (c == '\\' && i + 1 < source.size()) {
                ++i;  // 이스케이프 시퀀스는 다음 문자를 그대로 건너뜀
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
        } else if (IsOpenBracket(c)) {
            ++depth;
        } else if (IsCloseBracket(c)) {
            if (depth > 0) {
                --depth;
            }
        }
    }

    return depth == 0 && !inString;
}
