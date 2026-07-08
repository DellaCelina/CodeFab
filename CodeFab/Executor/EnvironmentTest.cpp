#include "gmock/gmock.h"
#include "Environment.h"

TEST(EnvironmentTest, DefineThenLookup_ReturnsValue) {
    Environment env;
    env.define("x", Value(1.0));

    auto result = env.lookup("x");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->asNumber(), 1.0);
}

TEST(EnvironmentTest, Lookup_UndefinedVariable_ReturnsNullopt) {
    Environment env;

    EXPECT_FALSE(env.lookup("missing").has_value());
}

TEST(EnvironmentTest, Assign_UpdatesExistingVariable) {
    Environment env;
    env.define("x", Value(1.0));

    bool assigned = env.assign("x", Value(2.0));

    EXPECT_TRUE(assigned);
    EXPECT_EQ(env.lookup("x")->asNumber(), 2.0);
}

TEST(EnvironmentTest, Assign_UndefinedVariable_ReturnsFalse) {
    Environment env;

    EXPECT_FALSE(env.assign("missing", Value(1.0)));
}

TEST(EnvironmentTest, PushScope_ShadowsOuterVariable) {
    Environment env;
    env.define("x", Value(1.0));

    env.pushScope();
    env.define("x", Value(2.0));

    EXPECT_EQ(env.lookup("x")->asNumber(), 2.0);
}

TEST(EnvironmentTest, PopScope_RemovesInnerVariable) {
    Environment env;
    env.define("x", Value(1.0));

    env.pushScope();
    env.define("y", Value(2.0));
    env.popScope();

    EXPECT_TRUE(env.lookup("x").has_value());
    EXPECT_FALSE(env.lookup("y").has_value());
}

TEST(EnvironmentTest, Assign_WalksUpToOuterScope) {
    // x가 바깥 스코프에만 있어도, 안쪽 스코프에서 assign하면 바깥 것이 바뀌어야 한다.
    Environment env;
    env.define("x", Value(1.0));

    env.pushScope();
    bool assigned = env.assign("x", Value(5.0));
    env.popScope();

    EXPECT_TRUE(assigned);
    EXPECT_EQ(env.lookup("x")->asNumber(), 5.0);
}
