#pragma once

#include <array>
#include <cstdint>

namespace formula
{

/**
    Opcodes for the stack based expression bytecode.

    The program is a trivially copyable value: it can be built on the message
    thread and read by the audio thread through a plain pointer.
*/
enum class Op : std::uint8_t
{
    PushConst = 0,
    PushX,
    PushS,
    PushP1,
    PushP2,
    PushP3,
    PushP4,

    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,
    Neg,
    Not,
    LogicalAnd,
    LogicalOr,

    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,
    Equal,
    NotEqual,

    Abs,
    Min,
    Max,
    Clamp,
    Sqrt,
    Cbrt,
    Exp,
    Log,
    Log10,
    Log2,
    Floor,
    Ceil,
    Round,
    Trunc,
    Sign,

    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Atan2,
    Sinh,
    Cosh,
    Tanh,

    Step,
    Smoothstep,
    Mix,
    Db,
    Lin,
    Rand,

    Select
};

/** Fixed-size expression program. Evaluate with Program::eval(). */
struct Program
{
    static constexpr int maxOps = 256;
    static constexpr int maxStack = 128;

    int count = 0;
    std::array<Op, maxOps> ops {};
    std::array<float, maxOps> args {};
    bool valid = false;
    bool usesSidechain = false;

    /** Stack machine evaluation. Invalid or empty programs behave as x. */
    float eval (float x, float s, float p1, float p2, float p3, float p4) const noexcept;

    /** Backwards-compatible evaluation with s = 0. */
    float eval (float x, float p1, float p2, float p3, float p4) const noexcept
    {
        return eval (x, 0.0f, p1, p2, p3, p4);
    }
};

} // namespace formula
