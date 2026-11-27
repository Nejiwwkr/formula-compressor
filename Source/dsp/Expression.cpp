#include "Expression.h"

#include <cmath>

namespace formula
{

namespace
{
struct RandomState
{
    std::uint32_t state = 0x9e3779b9u;

    float next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state >> 8) * (1.0f / 16777216.0f);
    }
};

float smoothstep (float e0, float e1, float v) noexcept
{
    if (e0 == e1)
        return v >= e1 ? 1.0f : 0.0f;

    float t = (v - e0) / (e1 - e0);
    t = std::fmin (1.0f, std::fmax (0.0f, t));
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

float Program::eval (float x, float p1, float p2, float p3, float p4) const noexcept
{
    if (! valid || count <= 0)
        return x;

    float stack[maxStack];
    int sp = 0;

    const auto push = [&] (float value) noexcept
    {
        if (sp < maxStack)
            stack[sp++] = value;
    };

    const auto pop = [&]() noexcept
    {
        return sp > 0 ? stack[--sp] : 0.0f;
    };

    thread_local RandomState random;

    for (int i = 0; i < count && i < maxOps; ++i)
    {
        const auto op = ops[static_cast<std::size_t> (i)];

        switch (op)
        {
            case Op::PushConst:  push (args[static_cast<std::size_t> (i)]); break;
            case Op::PushX:      push (x);  break;
            case Op::PushP1:     push (p1); break;
            case Op::PushP2:     push (p2); break;
            case Op::PushP3:     push (p3); break;
            case Op::PushP4:     push (p4); break;

            case Op::Add: { const float b = pop(), a = pop(); push (a + b); break; }
            case Op::Sub: { const float b = pop(), a = pop(); push (a - b); break; }
            case Op::Mul: { const float b = pop(), a = pop(); push (a * b); break; }
            case Op::Div: { const float b = pop(), a = pop(); push (a / b); break; }
            case Op::Mod: { const float b = pop(), a = pop(); push (std::fmod (a, b)); break; }
            case Op::Pow: { const float b = pop(), a = pop(); push (std::pow (a, b)); break; }
            case Op::Neg: { const float a = pop(); push (-a); break; }
            case Op::Not: { const float a = pop(); push (a == 0.0f ? 1.0f : 0.0f); break; }
            case Op::LogicalAnd:
            {
                const float b = pop(), a = pop();
                push (a != 0.0f && b != 0.0f ? 1.0f : 0.0f);
                break;
            }
            case Op::LogicalOr:
            {
                const float b = pop(), a = pop();
                push (a != 0.0f || b != 0.0f ? 1.0f : 0.0f);
                break;
            }

            case Op::LessThan:     { const float b = pop(), a = pop(); push (a <  b ? 1.0f : 0.0f); break; }
            case Op::LessEqual:    { const float b = pop(), a = pop(); push (a <= b ? 1.0f : 0.0f); break; }
            case Op::GreaterThan:  { const float b = pop(), a = pop(); push (a >  b ? 1.0f : 0.0f); break; }
            case Op::GreaterEqual: { const float b = pop(), a = pop(); push (a >= b ? 1.0f : 0.0f); break; }
            case Op::Equal:        { const float b = pop(), a = pop(); push (a == b ? 1.0f : 0.0f); break; }
            case Op::NotEqual:     { const float b = pop(), a = pop(); push (a != b ? 1.0f : 0.0f); break; }

            case Op::Abs:   { const float a = pop(); push (std::fabs (a)); break; }
            case Op::Min:   { const float b = pop(), a = pop(); push (std::fmin (a, b)); break; }
            case Op::Max:   { const float b = pop(), a = pop(); push (std::fmax (a, b)); break; }
            case Op::Clamp:
            {
                const float hi = pop(), lo = pop(), v = pop();
                push (std::fmin (hi, std::fmax (lo, v)));
                break;
            }
            case Op::Sqrt:  { const float a = pop(); push (std::sqrt (a)); break; }
            case Op::Cbrt:  { const float a = pop(); push (std::cbrt (a)); break; }
            case Op::Exp:   { const float a = pop(); push (std::exp (a)); break; }
            case Op::Log:   { const float a = pop(); push (std::log (a)); break; }
            case Op::Log10: { const float a = pop(); push (std::log10 (a)); break; }
            case Op::Log2:  { const float a = pop(); push (std::log2 (a)); break; }
            case Op::Floor: { const float a = pop(); push (std::floor (a)); break; }
            case Op::Ceil:  { const float a = pop(); push (std::ceil (a)); break; }
            case Op::Round: { const float a = pop(); push (std::round (a)); break; }
            case Op::Trunc: { const float a = pop(); push (std::trunc (a)); break; }
            case Op::Sign:
            {
                const float a = pop();
                push (a > 0.0f ? 1.0f : (a < 0.0f ? -1.0f : 0.0f));
                break;
            }

            case Op::Sin:  { const float a = pop(); push (std::sin (a)); break; }
            case Op::Cos:  { const float a = pop(); push (std::cos (a)); break; }
            case Op::Tan:  { const float a = pop(); push (std::tan (a)); break; }
            case Op::Asin: { const float a = pop(); push (std::asin (a)); break; }
            case Op::Acos: { const float a = pop(); push (std::acos (a)); break; }
            case Op::Atan: { const float a = pop(); push (std::atan (a)); break; }
            case Op::Atan2: { const float b = pop(), a = pop(); push (std::atan2 (a, b)); break; }
            case Op::Sinh: { const float a = pop(); push (std::sinh (a)); break; }
            case Op::Cosh: { const float a = pop(); push (std::cosh (a)); break; }
            case Op::Tanh: { const float a = pop(); push (std::tanh (a)); break; }

            case Op::Step:
            {
                const float v = pop(), edge = pop();
                push (v >= edge ? 1.0f : 0.0f);
                break;
            }
            case Op::Smoothstep:
            {
                const float v = pop(), e1 = pop(), e0 = pop();
                push (smoothstep (e0, e1, v));
                break;
            }
            case Op::Mix:
            {
                const float t = pop(), b = pop(), a = pop();
                push (a + (b - a) * t);
                break;
            }
            case Op::Db:
            {
                const float a = pop();
                push (20.0f * std::log10 (std::fmax (1.0e-9f, std::fabs (a))));
                break;
            }
            case Op::Lin:
            {
                const float a = pop();
                push (std::pow (10.0f, a / 20.0f));
                break;
            }
            case Op::Rand:
                push (random.next());
                break;

            case Op::Select:
            {
                const float falseValue = pop();
                const float trueValue = pop();
                const float condition = pop();
                push (condition != 0.0f ? trueValue : falseValue);
                break;
            }
        }
    }

    return sp > 0 ? stack[sp - 1] : x;
}

} // namespace formula
