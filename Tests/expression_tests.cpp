#include "TestFramework.h"

#include "dsp/ExpressionParser.h"

#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979323846f;

float eval (const juce::String& text, float x = 0.0f,
            float p1 = 0.0f, float p2 = 0.0f, float p3 = 0.0f, float p4 = 0.0f)
{
    const auto result = formula::compileExpression (text);

    if (! result.ok)
    {
        tests::report (false, "compile", __FILE__, __LINE__,
                       text + " -> " + result.error);
        return std::nanf ("");
    }

    return result.program.eval (x, p1, p2, p3, p4);
}

float evalS (const juce::String& text, float x, float s,
             float p1 = 0.0f, float p2 = 0.0f, float p3 = 0.0f, float p4 = 0.0f)
{
    const auto result = formula::compileExpression (text);

    if (! result.ok)
    {
        tests::report (false, "compile", __FILE__, __LINE__,
                       text + " -> " + result.error);
        return std::nanf ("");
    }

    return result.program.eval (x, s, p1, p2, p3, p4);
}

void expectError (const juce::String& text, int expectedColumn = -1)
{
    const auto result = formula::compileExpression (text);
    CHECK_MESSAGE (! result.ok, "expected an error for: " + text);
    CHECK_MESSAGE (result.errorColumn >= 1, "error column should be valid: " + text);

    if (expectedColumn >= 0)
        CHECK_MESSAGE (result.errorColumn == expectedColumn,
                       "column for \"" + text + "\" was "
                           + juce::String (result.errorColumn)
                           + ", expected " + juce::String (expectedColumn));
}
} // namespace

void tests::testExpression()
{
    std::printf ("Expression tests\n");

    // Arithmetic and precedence.
    CHECK_CLOSE (eval ("2 + 3 * 4"), 14.0, 1.0e-5);
    CHECK_CLOSE (eval ("(2 + 3) * 4"), 20.0, 1.0e-5);
    CHECK_CLOSE (eval ("10 - 4 - 3"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("12 / 4 / 3"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("5 % 3"), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("2 ^ 3 ^ 2"), 512.0, 1.0e-4);
    CHECK_CLOSE (eval ("-2 ^ 2"), -4.0, 1.0e-5);
    CHECK_CLOSE (eval ("2 ^ -1"), 0.5, 1.0e-5);
    CHECK_CLOSE (eval ("1e-2 * 100"), 1.0, 1.0e-5);

    // Default formula (dBFS): min(x, -12 + (x + 12) / 4).
    CHECK_CLOSE (eval (formula::defaultFormula(), -6.0206f), -10.5052, 1.0e-3);
    CHECK_CLOSE (eval (formula::defaultFormula(), -20.0f), -20.0, 1.0e-5);

    // Variables, parameters and constants.
    CHECK_CLOSE (eval ("P1 * 10 + p2", 0.0f, 0.25f, 0.5f), 3.0, 1.0e-5);

    // Sidechain variable s and its usesSidechain flag.
    {
        const auto withS = formula::compileExpression ("x - s / 10");
        CHECK (withS.ok);
        CHECK (withS.program.usesSidechain);
        CHECK_CLOSE (withS.program.eval (0.5f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f),
                     0.46, 1.0e-5);
        CHECK_CLOSE (withS.program.eval (0.5f, 0.0f, 0.0f, 0.0f, 0.0f),
                     0.5, 1.0e-5);

        const auto withoutS = formula::compileExpression ("x * 2");
        CHECK (withoutS.ok);
        CHECK (! withoutS.program.usesSidechain);

        CHECK_CLOSE (evalS ("s", 0.2f, 0.7f), 0.7, 1.0e-5);
    }
    CHECK_CLOSE (eval ("x * 3", 0.25f), 0.75, 1.0e-5);
    CHECK_CLOSE (eval ("pi"), kPi, 1.0e-5);
    CHECK_CLOSE (eval ("e"), 2.718281828, 1.0e-5);
    CHECK_CLOSE (eval ("P3 - P4", 0.0f, 0.0f, 0.0f, 0.75f, 0.25f), 0.5, 1.0e-5);

    // Comparisons, logic and ternary.
    CHECK_CLOSE (eval ("x < 0.5 ? 1 : 2", 0.25f), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("x < 0.5 ? 1 : 2", 0.75f), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("(x > 0.5) + 1", 0.75f), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("x > 0 && x < 1", 0.5f), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("x > 0 && x < 1", 1.5f), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("x <= 0 || x >= 1", 1.0f), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("x == 1", 1.0f), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("x != 1", 1.0f), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("!0"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("!5"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("1 ? 2 : 3"), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("0 ? 2 : 3"), 3.0, 1.0e-5);

    // Functions.
    CHECK_CLOSE (eval ("abs(-2)"), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("min(3, 4)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("max(3, 4)"), 4.0, 1.0e-5);
    CHECK_CLOSE (eval ("clamp(5, 0, 1)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("clamp(-1, 0, 1)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("sqrt(9)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("cbrt(27)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("pow(2, 5)"), 32.0, 1.0e-5);
    CHECK_CLOSE (eval ("exp(0)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("log(2.718281828)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("log10(1000)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("log2(8)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("floor(-1.2)"), -2.0, 1.0e-5);
    CHECK_CLOSE (eval ("ceil(-1.2)"), -1.0, 1.0e-5);
    CHECK_CLOSE (eval ("round(2.6)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("trunc(-2.7)"), -2.0, 1.0e-5);
    CHECK_CLOSE (eval ("sign(-3)"), -1.0, 1.0e-5);
    CHECK_CLOSE (eval ("sign(0)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("sin(0)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("cos(0)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("tanh(0)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("asin(1)"), kPi * 0.5, 1.0e-5);
    CHECK_CLOSE (eval ("atan2(1, 1)"), kPi * 0.25, 1.0e-5);
    CHECK_CLOSE (eval ("step(0.5, 0.3)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("step(0.5, 0.7)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("smoothstep(0, 1, 0.5)"), 0.5, 1.0e-5);
    CHECK_CLOSE (eval ("mix(0, 10, 0.25)"), 2.5, 1.0e-5);
    CHECK_CLOSE (eval ("db(1)"), 0.0, 1.0e-5);
    CHECK_CLOSE (eval ("lin(0)"), 1.0, 1.0e-5);
    CHECK_CLOSE (eval ("lin(-6.0206)"), 0.5, 1.0e-4);
    CHECK_CLOSE (eval ("if(1, 2, 3)"), 2.0, 1.0e-5);
    CHECK_CLOSE (eval ("if(0, 2, 3)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("MIN(3, 4)"), 3.0, 1.0e-5);
    CHECK_CLOSE (eval ("TANH(0)"), 0.0, 1.0e-5);

    // Optional prefix.
    CHECK_CLOSE (eval ("f(x) = 2 * x", 3.0f), 6.0, 1.0e-5);
    CHECK_CLOSE (evalS ("f(x,s) = x + s", 2.0f, 0.5f), 2.5, 1.0e-5);
    CHECK_CLOSE (evalS ("f(x, s) = x + s", 2.0f, 0.5f), 2.5, 1.0e-5);
    CHECK_CLOSE (eval ("y = x + 1", 3.0f), 4.0, 1.0e-5);
    CHECK_CLOSE (eval ("f(x)=min(x, 0.25)", 0.5f), 0.25, 1.0e-5);

    // Division by zero is allowed to be non-finite; the engine clamps it.
    CHECK (std::isinf (eval ("1 / 0")));

    // rand() stays in [0,1) and actually varies.
    {
        const auto result = formula::compileExpression ("rand()");
        CHECK (result.ok);

        float minimum = 2.0f;
        float maximum = -1.0f;

        for (int i = 0; i < 200; ++i)
        {
            const float value = result.program.eval (0.0f, 0, 0, 0, 0);
            CHECK (value >= 0.0f && value < 1.0f);
            minimum = juce::jmin (minimum, value);
            maximum = juce::jmax (maximum, value);
        }

        CHECK (maximum - minimum > 0.5f);
    }

    // Invalid programs evaluate as identity.
    {
        formula::Program invalid;
        invalid.valid = false;
        CHECK_CLOSE (invalid.eval (0.37f, 0, 0, 0, 0), 0.37, 1.0e-6);
    }

    // Errors, with rough column checks.
    expectError ("x + q", 5);
    expectError ("foo(x)", 1);
    expectError ("min(x)", 1);
    expectError ("min(1, 2, 3)", 1);
    expectError ("x = 2", 3);
    expectError ("(x + 1");
    expectError ("1 +");
    expectError ("x @ 2", 3);
    expectError ("");
    expectError ("    ");
    expectError ("f(x) = ");
    expectError ("min(,)");
    expectError ("x ^^ 2");
    expectError ("1e");
    expectError ("1e+");
    expectError (".");
    expectError ("1..2");
    expectError (")(");

    // Compile limits.
    {
        juce::String complex = "x";

        for (int i = 0; i < 300; ++i)
            complex += "+x";

        const auto result = formula::compileExpression (complex);
        CHECK (! result.ok);
        CHECK (result.error.containsIgnoreCase ("complex"));
    }

    {
        juce::String longText;

        for (int i = 0; i < 1100; ++i)
            longText += "1";

        const auto result = formula::compileExpression (longText);
        CHECK (! result.ok);
        CHECK (result.error.containsIgnoreCase ("long"));
    }

    // Junk must never crash.
    const char* junk[] = { ")(*&^", "1..2", "min(,)", "x^^2", "1e", "1e+", ".",
                           "[]", "x ? : 1", "min((1,2))", "P5", "x +* 2" };

    for (const auto* text : junk)
    {
        const auto result = formula::compileExpression (text);
        CHECK_MESSAGE (! result.ok, juce::String ("junk compiled: ") + text);
    }
}
