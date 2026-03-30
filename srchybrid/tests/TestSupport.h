#pragma once

#include <stdexcept>
#include <string>
#include <Windows.h>

#ifndef ASSERT
/**
 * Signals a test-time ASSERT contract violation from production headers.
 */
class CTestAssertException : public std::runtime_error
{
public:
	CTestAssertException(const char *pszFile, int nLine, const char *pszExpression)
		: std::runtime_error(BuildMessage(pszFile, nLine, pszExpression))
	{
	}

private:
	/**
	 * Formats the failed ASSERT site into a stable exception message.
	 */
	static std::string BuildMessage(const char *pszFile, int nLine, const char *pszExpression)
	{
		return std::string(pszFile) + "(" + std::to_string(nLine) + "): ASSERT(" + pszExpression + ")";
	}
};

#define ASSERT(expression) do { if (!(expression)) throw CTestAssertException(__FILE__, __LINE__, #expression); } while (0)
#endif

#include "../types.h"
