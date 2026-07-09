#include "Debugger.h"

#include <algorithm>
#include <sstream>

Debugger::Debugger(const ExecuteInterface& executor, std::istream& in, std::ostream& out,
                    std::vector<std::string> sourceLines)
    : executor_(executor), in_(in), out_(out), sourceLines_(std::move(sourceLines)) {
}

void Debugger::onStatement(Statement* stmt, int depth) {
    if (!shouldStop(stmt, depth)) {
        return;
    }
    int line = stmt->getLine();
    out_ << "[DEBUG] paused at line " << line;
    printCurrentSourceLine(line);
    out_ << "\n";
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

void Debugger::printCurrentSourceLine(int line) const {
    // sourceLines_가 비어있으면(Debugger 생성 시 넘기지 않은 경우) 아무것도
    // 표시하지 않는다 - line 정보 자체는 유효해도 보여줄 원본 텍스트가 없다.
    if (line < 1 || static_cast<size_t>(line) > sourceLines_.size()) {
        return;
    }
    out_ << " -> " << sourceLines_[line - 1];
}

void Debugger::printWatches() const {
    for (const auto& name : watches_) {
        auto value = executor_.environment().lookup(name);
        out_ << "[WATCH] " << name << " = " << (value ? value->toString() : "undefined") << "\n";
    }
}

void Debugger::printBreakpoints() const {
    if (breakpoints_.empty()) {
        out_ << "[BREAK] no breakpoints set.\n";
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

    out_ << "--- current scope variables ---\n";

    bool anyLocal = false;
    for (auto it = scopes.rbegin(); it != scopes.rend() - 1; ++it) {
        for (const auto& [name, value] : it->variables()) {
            out_ << "[LOCAL] " << name << " = " << value.toString() << " (" << value.typeName()
                 << ")\n";
            anyLocal = true;
        }
    }
    if (!anyLocal) {
        out_ << "[LOCAL]\n";
    }

    bool anyGlobal = false;
    for (const auto& [name, value] : scopes.front().variables()) {
        out_ << "[GLOBAL] " << name << " = " << value.toString() << " (" << value.typeName()
             << ")\n";
        anyGlobal = true;
    }
    if (!anyGlobal) {
        out_ << "[GLOBAL]\n";
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
                out_ << "[BREAK] breakpoint set at line " << lineNumber << ".\n";
            }
            continue;
        }
        if (keyword == "remove") {
            int lineNumber;
            if (cmd >> lineNumber) {
                breakpoints_.erase(lineNumber);
                out_ << "[BREAK] breakpoint removed at line " << lineNumber << ".\n";
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

        out_ << "[DEBUG] unknown command: '" << keyword << "'\n";
    }
}
