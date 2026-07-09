#include "ArrayRuntime.h"

#include "ArrayValue.h"
#include "Executor.h"

ArrayRuntime::ArrayRuntime(Executor& executor) : executor_(executor) {}

Value ArrayRuntime::create(Expression* sizeExpr) {
    Value size = executor_.evaluate(sizeExpr);
    if (!size.isNumber()) {
        throw ExecutorError("[line {}] array size must be a number.", sizeExpr->getLine());
    }
    auto array = std::make_shared<ArrayValue>();
    array->items.resize(static_cast<size_t>(size.asNumber()));  // 전부 Nil로 채워짐.
    return Value(array);
}

std::pair<std::shared_ptr<ArrayValue>, size_t> ArrayRuntime::resolveIndex(Expression* collectionExpr, Expression* indexExpr) {
    Value collection = executor_.evaluate(collectionExpr);
    if (!collection.isArray()) {
        throw ExecutorError("[line {}] index access is only supported on arrays.", collectionExpr->getLine());
    }
    Value indexValue = executor_.evaluate(indexExpr);
    if (!indexValue.isNumber()) {
        throw ExecutorError("[line {}] array index must be a number.", collectionExpr->getLine());
    }
    auto array = collection.asArray();
    auto i = static_cast<size_t>(indexValue.asNumber());
    if (i >= array->items.size()) {
        throw ExecutorError("[line {}] array index out of bounds.", collectionExpr->getLine());
    }
    return { array, i };
}
