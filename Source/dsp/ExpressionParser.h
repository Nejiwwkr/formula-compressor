#pragma once

#include "Expression.h"

#include <juce_core/juce_core.h>

namespace formula
{

struct CompileResult
{
    bool ok = false;

    /** 1-based column in the user's text, 0 when ok. */
    int errorColumn = 0;

    juce::String error;
    Program program;
};

/** Compile a user expression into stack bytecode. Message thread only. */
CompileResult compileExpression (const juce::String& text);

juce::String defaultFormula();

/** Multi-line cheat sheet shown by the UI help button. */
juce::String functionReference();

} // namespace formula
