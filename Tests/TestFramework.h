#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace tests
{

extern int checks;
extern int failures;

void report (bool condition, const char* expression, const char* file, int line,
             const juce::String& detail = {});

void reportClose (double actual, double expected, double tolerance,
                  const char* expression, const char* file, int line);

void testExpression();
void testEngine();
void testProcessor();

} // namespace tests

#define CHECK(condition) \
    ::tests::report ((condition), #condition, __FILE__, __LINE__)

#define CHECK_MESSAGE(condition, message) \
    ::tests::report ((condition), #condition, __FILE__, __LINE__, (message))

#define CHECK_CLOSE(actual, expected, tolerance) \
    ::tests::reportClose ((actual), (expected), (tolerance), #actual, __FILE__, __LINE__)
