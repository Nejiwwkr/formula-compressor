#include "ExpressionParser.h"

#include <vector>

namespace formula
{

namespace
{
enum class TokType
{
    Number,
    Identifier,
    Plus, Minus, Star, Slash, Percent, Caret,
    LParen, RParen, Comma, Question, Colon,
    Less, LessEqual, Greater, GreaterEqual, EqualEqual, NotEqual,
    AndAnd, OrOr, Not,
    End
};

struct Token
{
    TokType type = TokType::End;
    juce::String text;
    double number = 0.0;
    int pos = 0;
};

struct FunctionDef
{
    const char* name;
    int argc;
    Op op;
};

constexpr FunctionDef functions[] = {
    { "abs", 1, Op::Abs },
    { "min", 2, Op::Min },
    { "max", 2, Op::Max },
    { "clamp", 3, Op::Clamp },
    { "pow", 2, Op::Pow },
    { "sqrt", 1, Op::Sqrt },
    { "cbrt", 1, Op::Cbrt },
    { "exp", 1, Op::Exp },
    { "log", 1, Op::Log },
    { "ln", 1, Op::Log },
    { "log10", 1, Op::Log10 },
    { "log2", 1, Op::Log2 },
    { "floor", 1, Op::Floor },
    { "ceil", 1, Op::Ceil },
    { "round", 1, Op::Round },
    { "trunc", 1, Op::Trunc },
    { "sign", 1, Op::Sign },
    { "sin", 1, Op::Sin },
    { "cos", 1, Op::Cos },
    { "tan", 1, Op::Tan },
    { "asin", 1, Op::Asin },
    { "acos", 1, Op::Acos },
    { "atan", 1, Op::Atan },
    { "atan2", 2, Op::Atan2 },
    { "sinh", 1, Op::Sinh },
    { "cosh", 1, Op::Cosh },
    { "tanh", 1, Op::Tanh },
    { "step", 2, Op::Step },
    { "smoothstep", 3, Op::Smoothstep },
    { "mix", 3, Op::Mix },
    { "db", 1, Op::Db },
    { "lin", 1, Op::Lin },
    { "rand", 0, Op::Rand },
    { "if", 3, Op::Select }
};

const FunctionDef* findFunction (const juce::String& name) noexcept
{
    for (const auto& fn : functions)
        if (name == fn.name)
            return &fn;

    return nullptr;
}

bool isDigit (juce::juce_wchar c) noexcept
{
    return c >= '0' && c <= '9';
}

bool isIdentifierStart (juce::juce_wchar c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isIdentifierBody (juce::juce_wchar c) noexcept
{
    return isIdentifierStart (c) || isDigit (c);
}

bool tokenise (const juce::String& source,
               std::vector<Token>& tokens,
               juce::String& error,
               int& errorPos)
{
    const int length = source.length();
    int i = 0;

    while (i < length)
    {
        const auto c = source[i];

        if (juce::CharacterFunctions::isWhitespace (c))
        {
            ++i;
            continue;
        }

        if (isDigit (c) || (c == '.' && i + 1 < length && isDigit (source[i + 1])))
        {
            const int start = i;

            while (i < length && isDigit (source[i]))
                ++i;

            if (i < length && source[i] == '.')
            {
                ++i;

                while (i < length && isDigit (source[i]))
                    ++i;
            }

            if (i < length && (source[i] == 'e' || source[i] == 'E'))
            {
                const int exponentPos = i;
                ++i;

                if (i < length && (source[i] == '+' || source[i] == '-'))
                    ++i;

                if (i >= length || ! isDigit (source[i]))
                {
                    error = "malformed exponent";
                    errorPos = exponentPos;
                    return false;
                }

                while (i < length && isDigit (source[i]))
                    ++i;
            }

            const auto text = source.substring (start, i);
            tokens.push_back ({ TokType::Number, text, text.getDoubleValue(), start });
            continue;
        }

        if (isIdentifierStart (c))
        {
            const int start = i;
            ++i;

            while (i < length && isIdentifierBody (source[i]))
                ++i;

            tokens.push_back ({ TokType::Identifier, source.substring (start, i), 0.0, start });
            continue;
        }

        const auto pushOperator = [&] (TokType type, int width)
        {
            tokens.push_back ({ type, source.substring (i, i + width), 0.0, i });
            i += width;
        };

        switch (c)
        {
            case '+': pushOperator (TokType::Plus, 1); break;
            case '-': pushOperator (TokType::Minus, 1); break;
            case '*': pushOperator (TokType::Star, 1); break;
            case '/': pushOperator (TokType::Slash, 1); break;
            case '%': pushOperator (TokType::Percent, 1); break;
            case '^': pushOperator (TokType::Caret, 1); break;
            case '(': pushOperator (TokType::LParen, 1); break;
            case ')': pushOperator (TokType::RParen, 1); break;
            case ',': pushOperator (TokType::Comma, 1); break;
            case '?': pushOperator (TokType::Question, 1); break;
            case ':': pushOperator (TokType::Colon, 1); break;

            case '<':
                if (i + 1 < length && source[i + 1] == '=') pushOperator (TokType::LessEqual, 2);
                else pushOperator (TokType::Less, 1);
                break;

            case '>':
                if (i + 1 < length && source[i + 1] == '=') pushOperator (TokType::GreaterEqual, 2);
                else pushOperator (TokType::Greater, 1);
                break;

            case '=':
                if (i + 1 < length && source[i + 1] == '=')
                {
                    pushOperator (TokType::EqualEqual, 2);
                }
                else
                {
                    error = "unexpected '=' (use '==' to compare)";
                    errorPos = i;
                    return false;
                }
                break;

            case '!':
                if (i + 1 < length && source[i + 1] == '=') pushOperator (TokType::NotEqual, 2);
                else pushOperator (TokType::Not, 1);
                break;

            case '&':
                if (i + 1 < length && source[i + 1] == '&')
                {
                    pushOperator (TokType::AndAnd, 2);
                }
                else
                {
                    error = "unexpected '&' (use '&&')";
                    errorPos = i;
                    return false;
                }
                break;

            case '|':
                if (i + 1 < length && source[i + 1] == '|')
                {
                    pushOperator (TokType::OrOr, 2);
                }
                else
                {
                    error = "unexpected '|' (use '||')";
                    errorPos = i;
                    return false;
                }
                break;

            default:
                error = "unexpected character '" + juce::String::charToString (c) + "'";
                errorPos = i;
                return false;
        }
    }

    tokens.push_back ({ TokType::End, {}, 0.0, length });
    return true;
}

class Parser
{
public:
    Parser (const std::vector<Token>& tokensIn, Program& programIn, int offsetIn)
        : tokens (tokensIn), program (programIn), offset (offsetIn)
    {
    }

    bool run()
    {
        if (! parseTernary())
            return false;

        if (peek().type != TokType::End)
            return fail (peek().pos, "unexpected content after the expression");

        if (program.count == 0)
            return fail (0, "expression is empty");

        program.valid = true;
        return true;
    }

    const juce::String& getError() const noexcept { return error; }
    int getErrorPos() const noexcept { return errorPos; }

private:
    const Token& peek() const noexcept
    {
        return tokens[static_cast<std::size_t> (juce::jmin (index, static_cast<int> (tokens.size()) - 1))];
    }

    bool check (TokType type) const noexcept { return peek().type == type; }

    bool match (TokType type)
    {
        if (! check (type))
            return false;

        if (index < static_cast<int> (tokens.size()) - 1)
            ++index;

        return true;
    }

    const Token& advance() noexcept
    {
        const auto& token = peek();

        if (index < static_cast<int> (tokens.size()) - 1)
            ++index;

        return token;
    }

    bool fail (int pos, const juce::String& message)
    {
        if (! failed)
        {
            failed = true;
            error = message;
            errorPos = offset + pos + 1;
        }

        return false;
    }

    bool emit (Op op, float arg = 0.0f)
    {
        if (program.count >= Program::maxOps)
            return fail (peek().pos, "expression is too complex (more than 256 operations)");

        program.ops[static_cast<std::size_t> (program.count)] = op;
        program.args[static_cast<std::size_t> (program.count)] = arg;
        ++program.count;
        return true;
    }

    bool parseTernary()
    {
        if (! parseOr())
            return false;

        if (match (TokType::Question))
        {
            if (! parseTernary())
                return false;

            if (! match (TokType::Colon))
                return fail (peek().pos, "expected ':' in the conditional expression");

            if (! parseTernary())
                return false;

            return emit (Op::Select);
        }

        return true;
    }

    bool parseOr()
    {
        if (! parseAnd())
            return false;

        while (match (TokType::OrOr))
        {
            if (! parseAnd())
                return false;

            if (! emit (Op::LogicalOr))
                return false;
        }

        return true;
    }

    bool parseAnd()
    {
        if (! parseComparison())
            return false;

        while (match (TokType::AndAnd))
        {
            if (! parseComparison())
                return false;

            if (! emit (Op::LogicalAnd))
                return false;
        }

        return true;
    }

    bool parseComparison()
    {
        if (! parseAdditive())
            return false;

        for (;;)
        {
            Op op;

            if (match (TokType::Less))             op = Op::LessThan;
            else if (match (TokType::LessEqual))    op = Op::LessEqual;
            else if (match (TokType::Greater))      op = Op::GreaterThan;
            else if (match (TokType::GreaterEqual)) op = Op::GreaterEqual;
            else if (match (TokType::EqualEqual))   op = Op::Equal;
            else if (match (TokType::NotEqual))     op = Op::NotEqual;
            else break;

            if (! parseAdditive())
                return false;

            if (! emit (op))
                return false;
        }

        return true;
    }

    bool parseAdditive()
    {
        if (! parseMultiplicative())
            return false;

        for (;;)
        {
            Op op;

            if (match (TokType::Plus))       op = Op::Add;
            else if (match (TokType::Minus)) op = Op::Sub;
            else break;

            if (! parseMultiplicative())
                return false;

            if (! emit (op))
                return false;
        }

        return true;
    }

    bool parseMultiplicative()
    {
        if (! parseUnary())
            return false;

        for (;;)
        {
            Op op;

            if (match (TokType::Star))         op = Op::Mul;
            else if (match (TokType::Slash))   op = Op::Div;
            else if (match (TokType::Percent)) op = Op::Mod;
            else break;

            if (! parseUnary())
                return false;

            if (! emit (op))
                return false;
        }

        return true;
    }

    bool parseUnary()
    {
        if (match (TokType::Minus))
        {
            if (! parseUnary())
                return false;

            return emit (Op::Neg);
        }

        if (match (TokType::Plus))
            return parseUnary();

        if (match (TokType::Not))
        {
            if (! parseUnary())
                return false;

            return emit (Op::Not);
        }

        return parsePower();
    }

    bool parsePower()
    {
        if (! parsePrimary())
            return false;

        if (match (TokType::Caret))
        {
            if (! parseUnary())
                return false;

            return emit (Op::Pow);
        }

        return true;
    }

    bool parsePrimary()
    {
        const auto token = advance();

        switch (token.type)
        {
            case TokType::Number:
                return emit (Op::PushConst, static_cast<float> (token.number));

            case TokType::LParen:
                if (! parseTernary())
                    return false;

                if (! match (TokType::RParen))
                    return fail (peek().pos, "expected ')'");

                return true;

            case TokType::Identifier:
            {
                const auto name = token.text.toLowerCase();

                if (check (TokType::LParen))
                    return parseFunctionCall (name, token);

                if (name == "x")  return emit (Op::PushX);
                if (name == "p1") return emit (Op::PushP1);
                if (name == "p2") return emit (Op::PushP2);
                if (name == "p3") return emit (Op::PushP3);
                if (name == "p4") return emit (Op::PushP4);

                if (name == "pi")
                    return emit (Op::PushConst, 3.14159265358979323846f);

                if (name == "e")
                    return emit (Op::PushConst, 2.71828182845904523536f);

                if (findFunction (name) != nullptr)
                    return fail (token.pos, "expected '(' after function '" + token.text + "'");

                return fail (token.pos, "unknown variable '" + token.text + "'");
            }

            default:
                return fail (token.pos, "expected a number, variable or '('");
        }
    }

    bool parseFunctionCall (const juce::String& name, const Token& nameToken)
    {
        match (TokType::LParen);

        int argumentCount = 0;

        if (! check (TokType::RParen))
        {
            for (;;)
            {
                if (! parseTernary())
                    return false;

                ++argumentCount;

                if (match (TokType::Comma))
                    continue;

                break;
            }
        }

        if (! match (TokType::RParen))
            return fail (peek().pos, "expected ')' after the arguments of '" + nameToken.text + "'");

        const auto* fn = findFunction (name);

        if (fn == nullptr)
            return fail (nameToken.pos, "unknown function '" + nameToken.text + "'");

        if (argumentCount != fn->argc)
        {
            return fail (nameToken.pos,
                         "'" + nameToken.text + "' expects "
                             + juce::String (fn->argc) + " argument"
                             + (fn->argc == 1 ? "" : "s") + ", got "
                             + juce::String (argumentCount));
        }

        return emit (fn->op);
    }

    const std::vector<Token>& tokens;
    Program& program;
    const int offset;
    int index = 0;
    bool failed = false;
    juce::String error;
    int errorPos = 1;
};

int firstNonSpace (const juce::String& text) noexcept
{
    int i = 0;

    while (i < text.length() && juce::CharacterFunctions::isWhitespace (text[i]))
        ++i;

    return i;
}
} // namespace

CompileResult compileExpression (const juce::String& text)
{
    CompileResult result;

    if (text.length() > 1024)
    {
        result.error = "expression is too long (max 1024 characters)";
        result.errorColumn = 1;
        return result;
    }

    juce::String expression = text;
    int offset = 0;

    const int leading = firstNonSpace (expression);
    expression = expression.substring (leading);
    offset += leading;

    // Accept the optional "f(x) = " or "y = " prefix.
    const auto lower = expression.toLowerCase();
    int prefixLength = -1;

    if (lower.startsWith ("f(x)"))
        prefixLength = 4;
    else if (lower.startsWith ("y"))
        prefixLength = 1;

    if (prefixLength >= 0)
    {
        int cursor = prefixLength;

        while (cursor < expression.length()
               && juce::CharacterFunctions::isWhitespace (expression[cursor]))
            ++cursor;

        const bool isAssignment = cursor < expression.length()
                                  && expression[cursor] == '='
                                  && ! (cursor + 1 < expression.length()
                                        && expression[cursor + 1] == '=');

        if (isAssignment)
        {
            ++cursor;
            expression = expression.substring (cursor);
            offset += cursor;
        }
    }

    while (expression.endsWithChar (' ') || expression.endsWithChar ('\t')
           || expression.endsWithChar ('\r') || expression.endsWithChar ('\n'))
        expression = expression.dropLastCharacters (1);

    if (expression.isEmpty())
    {
        result.error = "expression is empty";
        result.errorColumn = 1;
        return result;
    }

    std::vector<Token> tokens;
    juce::String lexError;
    int lexErrorPos = 0;

    if (! tokenise (expression, tokens, lexError, lexErrorPos))
    {
        result.error = lexError;
        result.errorColumn = offset + lexErrorPos + 1;
        return result;
    }

    Parser parser (tokens, result.program, offset);

    if (! parser.run())
    {
        result.error = parser.getError();
        result.errorColumn = parser.getErrorPos();
        result.program = Program {};
        return result;
    }

    result.ok = true;
    return result;
}

juce::String defaultFormula()
{
    return "min(x, -12 + (x + 12) / 4)";
}

juce::String functionReference()
{
    return juce::String (
        "VARIABLES\n"
        "  x             input level, linear amplitude (0..1; may exceed 1)\n"
        "  P1..P4        the four P knobs (-1..+1)\n"
        "  pi, e         constants\n\n"
        "OPERATORS\n"
        "  + - * / % ^   arithmetic (^ is power, right associative)\n"
        "  < <= > >= == !=  comparisons, yield 0 or 1\n"
        "  && || !       logic\n"
        "  cond ? a : b  conditional\n\n"
        "FUNCTIONS\n"
        "  abs min max clamp(v,lo,hi) sqrt cbrt pow(a,b)\n"
        "  exp log/ln log10 log2 floor ceil round trunc sign\n"
        "  sin cos tan asin acos atan atan2 sinh cosh tanh\n"
        "  step(edge,x) smoothstep(e0,e1,x) mix(a,b,t)\n"
        "  db(v)         20*log10(|v|)\n"
        "  lin(d)        10^(d/20), dB back to amplitude\n"
        "  rand()        uniform random in [0,1)\n"
        "  if(c,a,b)     same as c ? a : b\n\n"
        "X DOMAIN (the AMP / dBFS switch)\n"
        "  AMP           x is linear amplitude, f(x) returns linear amplitude\n"
        "  dBFS          x is level in dBFS, f(x) returns dBFS; gain dB = f(x) - x\n"
        "  in dBFS mode  f(x) = 0 means 0 dB (unity), not silence; use -120 for mute\n\n"
        "POLARITY AND SILENCE (AMP mode)\n"
        "  f(x) < 0      negative gain: both channels are polarity inverted (dashed)\n"
        "  f(x) = 0      true silence; gain magnitude is clamped to +24 dB\n"
        "  mix < 100%    an inverted gain partially cancels the dry signal\n\n"
        "VIEWS\n"
        "  LEVEL         input/output level in dB, equal scale, unity is 45 deg\n"
        "  GAIN          gain in dB; a constant gain is a flat line\n"
        "  LINEAR        linear amplitude, ordinary axes; slope is the gain factor\n\n"
        "EXAMPLES\n"
        "  min(x, P1 + (x - P1) * P2)     classic compression, x in AMP\n"
        "  min(x, -12 + (x + 12) / 4)     classic 4:1, x in dBFS\n"
        "  x < -30 ? x : mix(x, -24 + (x + 24) / 4, smoothstep(-30, -24, x))\n"
        "  -0.5 * x                       -6 dB and polarity inverted (AMP)\n");
}

} // namespace formula
