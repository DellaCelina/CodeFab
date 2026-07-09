#include "Debugger.h"

#include <algorithm>
#include <sstream>

Debugger::Debugger(const ExecuteInterface& executor, std::istream& in, std::ostream& out)
    : executor_(executor), in_(in), out_(out) {
}

void Debugger::onStatement(Statement* stmt, int depth) {
    if (!shouldStop(stmt, depth)) {
        return;
    }
    out_ << "[DEBUG] " << stmt->getLine() << "번째 줄에서 정지\n";
    printWatches();
    promptAndHandleCommand(stmt, depth);
}

bool Debugger::shouldStop(Statement* stmt, int depth) const {
    if (mode_ == Mode::Step) {
        return true;
    }
    if (mode_ == Mode::Next) {
        // 같거나 더 얕은 깊이로 돌아왔을 때만 멈춘다 - 그래야 "next"를 친
        // 줄이 Block/If/For였어도 그 안의 하위 statement에서는 멈추지 않는다.
        return depth <= nextTargetDepth_;
    }
    // Mode::Continue: breakpoint 줄에 해당할 때만 멈춘다.
    for (int line : breakpoints_) {
        if (stmt->containsLine(line)) {
            return true;
        }
    }
    return false;
}

void Debugger::printWatches() const {
    for (const auto& name : watches_) {
        auto value = executor_.environment().lookup(name);
        out_ << "[WATCH] " << name << " = " << (value ? value->toString() : "undefined") << "\n";
    }
}

void Debugger::printBreakpoints() const {
    if (breakpoints_.empty()) {
        out_ << "[BREAK] 설정된 브레이크포인트가 없습니다.\n";
        return;
    }
    out_ << "[BREAK] ";
    bool first = true;
    for (int line : breakpoints_) {
        if (!first) {
            out_ << ", ";
        }
        out_ << line;
        first = false;
    }
    out_ << "\n";
}

void Debugger::printInspect() const {
    // scopes_.front()가 전역, 그 뒤(back() 방향)가 지역(로컬) 스코프다
    // (Environment.h 참고). 안쪽(가장 최근에 push된) 스코프부터 바깥쪽 순서로
    // 로컬 변수를 먼저 보여준 뒤, 전역 변수를 따로 묶어서 보여준다.
    const auto& scopes = executor_.environment().scopes();

    out_ << "--- 현재 스코프 변수 ---\n";

    out_ << "[로컬]\n";
    for (auto it = scopes.rbegin(); it != scopes.rend() - 1; ++it) {
        for (const auto& [name, value] : it->variables()) {
            out_ << name << " = " << value.toString() << "\n";
        }
    }

    out_ << "[전역]\n";
    for (const auto& [name, value] : scopes.front().variables()) {
        out_ << name << " = " << value.toString() << "\n";
    }
}

void Debugger::promptAndHandleCommand(Statement* stmt, int depth) {
    std::string line;
    while (true) {
        out_ << "> ";
        if (!std::getline(in_, line)) {
            // 입력이 끝나면 더 묻지 않고 나머지를 그냥 실행한다.
            mode_ = Mode::Continue;
            return;
        }

        std::istringstream cmd(line);
        std::string keyword;
        cmd >> keyword;

        if (keyword == "step") {
            mode_ = Mode::Step;
            return;
        }
        if (keyword == "next") {
            mode_ = Mode::Next;
            nextTargetDepth_ = depth;
            return;
        }
        if (keyword == "continue") {
            mode_ = Mode::Continue;
            return;
        }
        if (keyword == "break") {
            int lineNumber;
            if (cmd >> lineNumber) {
                breakpoints_.insert(lineNumber);
                out_ << "[BREAK] " << lineNumber << "번째 줄에 브레이크포인트를 설정했습니다.\n";
            }
            continue;
        }
        if (keyword == "remove") {
            int lineNumber;
            if (cmd >> lineNumber) {
                breakpoints_.erase(lineNumber);
                out_ << "[BREAK] " << lineNumber << "번째 줄의 브레이크포인트를 제거했습니다.\n";
            }
            continue;
        }
        if (keyword == "breakpoints") {
            printBreakpoints();
            continue;
        }
        if (keyword == "watch") {
            std::string name;
            if (cmd >> name) {
                if (std::find(watches_.begin(), watches_.end(), name) == watches_.end()) {
                    watches_.push_back(name);
                }
                // watch를 등록한 즉시 현재 값을 보여준다 - 다음 정지까지
                // 기다리지 않는다.
                auto value = executor_.environment().lookup(name);
                out_ << "[WATCH] " << name << " = " << (value ? value->toString() : "undefined") << "\n";
            }
            continue;
        }
        if (keyword == "unwatch") {
            std::string name;
            if (cmd >> name) {
                watches_.erase(std::remove(watches_.begin(), watches_.end(), name), watches_.end());
            }
            continue;
        }
        if (keyword == "watches") {
            printWatches();
            continue;
        }
        if (keyword == "inspect") {
            printInspect();
            continue;
        }

        out_ << "[DEBUG] 알 수 없는 명령입니다: '" << keyword << "'\n";
    }
}
