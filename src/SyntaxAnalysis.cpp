/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2024 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "SyntaxAnalysis.h"
#include "Common.h"
#include "Exceptions.h"
#include "Operators.h"
#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace calc4
{
namespace
{
std::shared_ptr<const Operator> ParseCore(const std::vector<std::shared_ptr<Token>>& tokens,
                                          CompilationContext& context);
void GenerateUserDefinedCodes(const std::vector<std::shared_ptr<Token>>& tokens,
                              CompilationContext& context);

// TODO: TryPeek(length) and Read(length) do not always return the same substring due to inadequate
// handling of Windows style line endings.
class StringReader
{
private:
    std::string_view text;
    size_t index;
    int lineNo;
    int charNo;
    size_t originalIndex;

public:
    StringReader(std::string_view text)
        : text(text), index(0), lineNo(0), charNo(0), originalIndex(0)
    {
    }

    StringReader(std::string_view text, const CharPosition& originalPosition)
        : text(text), index(0), lineNo(originalPosition.lineNo), charNo(originalPosition.charNo),
          originalIndex(originalPosition.index)
    {
    }

    char Peek() const
    {
        assert(!Eof());
        return text[index];
    }

    char TryPeek() const
    {
        if (Eof())
        {
            return static_cast<char>(-1);
        }

        return text[index];
    }

    std::string_view TryPeek(size_t length) const
    {
        if (Eof())
        {
            return std::string_view{};
        }

        return text.substr(index, length);
    }

    char Read()
    {
        assert(!Eof());

        char c = text[index++];
        charNo++;
        if (c == '\n' || c == '\r')
        {
            // Reached end of line
            lineNo++;
            charNo = 0;

            if (c == '\r' && Peek() == '\n')
            {
                // This line ending is Windows style
                index++;
            }
        }

        return c;
    }

    std::string_view Read(size_t length)
    {
        size_t startIndex = index;

        for (size_t i = 0; i < length; i++)
        {
            Read();
        }

        return text.substr(startIndex, index - startIndex);
    }

    template<typename TPred>
    std::string_view ReadWhile(TPred pred)
    {
        size_t startIndex = index;

        while (!Eof() && pred(Peek()))
        {
            Read();
        }

        return text.substr(startIndex, index - startIndex);
    }

    bool Eof() const
    {
        return index >= text.length();
    }

    CharPosition GetCurrentPosition() const
    {
        return { originalIndex + index, lineNo, charNo };
    }
};

std::vector<std::pair<std::string_view, CharPosition>> Split(StringReader& reader, char separator)
{
    if (reader.Eof())
    {
        return {};
    }

    std::vector<std::pair<std::string_view, CharPosition>> result;

    while (true)
    {
        assert(!reader.Eof());
        auto position = reader.GetCurrentPosition();
        std::string_view substr = reader.ReadWhile([separator](char c) { return c != separator; });
        result.emplace_back(substr, position);

        if (reader.Eof())
        {
            break;
        }

        assert(reader.Peek() == separator);
        reader.Read();

        if (reader.Eof())
        {
            result.emplace_back("", reader.GetCurrentPosition());
            break;
        }
    }

    return result;
}

class LexerImplement
{
public:
    StringReader reader;
    CompilationContext& context;
    const std::vector<std::string>& arguments;

    LexerImplement(std::string_view text, CompilationContext& context,
                   const std::vector<std::string>& arguments)
        : reader(text), context(context), arguments(arguments)
    {
    }

    LexerImplement(const StringReader& reader, CompilationContext& context,
                   const std::vector<std::string>& arguments)
        : reader(reader), context(context), arguments(arguments)
    {
    }

    std::vector<std::shared_ptr<Token>> Lex()
    {
        std::vector<std::shared_ptr<Token>> vec;

        while (!reader.Eof() && reader.Peek() != ')')
        {
            char c = reader.Peek();
            if (c == ' ' || c == '\n' || c == '\r')
            {
                // Skip whitespaces and line separators
                reader.Read();
                continue;
            }
            else if (reader.TryPeek(2) == "/*")
            {
                // C style comment
                reader.Read(2);

                // Skip characters to the end of this comment
                char last = '\0';
                reader.ReadWhile([&last](char c) {
                    bool stopReading = (last == '*' && c == '/');
                    last = c;
                    return !stopReading;
                });

                assert(reader.Peek() == '/');
                reader.Read();
                continue;
            }
            else if (reader.TryPeek(2) == "//")
            {
                // C++ style comment
                reader.Read(2);

                // Skip characters to the end of this comment
                reader.ReadWhile([](char c) { return c != '\n' && c != '\r'; });

                assert(reader.Eof() || reader.Peek() == '\n' || reader.Peek() == '\r');
                reader.Read();
                continue;
            }

            vec.push_back(NextToken());
        }

        return vec;
    }

    std::shared_ptr<Token> NextToken()
    {
        switch (reader.Peek())
        {
        case 'D':
            return LexDefineToken();
        case 'L':
            return LexLoadVariableToken();
        case 'S':
            return LexStoreVariableToken();
        case 'P':
            return LexPrintCharToken();
        case '@':
            return LexLoadArrayToken();
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return LexDecimalToken();
        case '{':
            return LexUserDefinedOperatorOrArgumentToken();
        case '(':
            return LexParenthesisToken();
        default:
            return LexSymbolOrArgumentToken();
        }
    }

    std::shared_ptr<DefineToken> LexDefineToken()
    {
        assert(reader.Peek() == 'D');
        auto position = reader.GetCurrentPosition();
        reader.Read();

        auto [supplementaryText, supplementaryTextPosition] = LexSupplementaryTextWithPosition();

        /* ***** Split supplementaryText into three strings ***** */
        StringReader supplementaryTextReader(supplementaryText, supplementaryTextPosition);
        auto splitted = Split(supplementaryTextReader, '|');
        if (splitted.size() != 3)
        {
            throw Exceptions::DefinitionTextNotSplittedProperlyException(position,
                                                                         supplementaryText);
        }

        /* ***** Split arguments ***** */
        StringReader argumentReader(splitted[1].first, splitted[1].second);
        auto splittedArguments = Split(argumentReader, ',');

        /* ***** Trim arguments (e.g. "  x " => "x") ***** */
        std::vector<std::string> arguments;
        for (auto& arg : splittedArguments)
        {
            arguments.emplace_back(TrimWhiteSpaces(arg.first));
        }

        /* ***** Operator name ***** */
        auto& name = splitted[0];

        /* ***** Update CompilationContext ***** */
        OperatorDefinition definition(std::string(name.first), static_cast<int>(arguments.size()));
        OperatorImplement implement(definition, nullptr);
        context.AddOperatorImplement(implement);

        /* ***** Lex internal text ***** */
        StringReader internalTextReader(splitted[2].first, splitted[2].second);
        auto tokens = LexerImplement(internalTextReader, context, arguments).Lex();

        /* ***** Construct token ***** */
        return std::make_shared<DefineToken>(position, std::string(name.first), arguments, tokens,
                                             std::move(supplementaryText));
    }

    std::shared_ptr<LoadVariableToken> LexLoadVariableToken()
    {
        auto position = reader.GetCurrentPosition();
        reader.Read();
        return std::make_shared<LoadVariableToken>(position, LexSupplementaryText());
    }

    std::shared_ptr<StoreVariableToken> LexStoreVariableToken()
    {
        auto position = reader.GetCurrentPosition();
        reader.Read();
        return std::make_shared<StoreVariableToken>(position, LexSupplementaryText());
    }

    std::shared_ptr<PrintCharToken> LexPrintCharToken()
    {
        auto position = reader.GetCurrentPosition();
        reader.Read();
        return std::make_shared<PrintCharToken>(position, LexSupplementaryText());
    }

    std::shared_ptr<LoadArrayToken> LexLoadArrayToken()
    {
        auto position = reader.GetCurrentPosition();
        reader.Read();
        return std::make_shared<LoadArrayToken>(position, LexSupplementaryText());
    }

    std::shared_ptr<DecimalToken> LexDecimalToken()
    {
        auto position = reader.GetCurrentPosition();
        int value = reader.Read() - '0';
        return std::make_shared<DecimalToken>(position, value, LexSupplementaryText());
    }

    std::shared_ptr<Token> LexUserDefinedOperatorOrArgumentToken()
    {
        assert(reader.Peek() == '{');
        auto position = reader.GetCurrentPosition();
        reader.Read();

        /* ***** Get identifier's name ***** */
        bool success = false;
        std::string_view name = reader.ReadWhile([&success](char c) {
            if (c == '}')
            {
                // Successfully reached end of bracket
                success = true;
                return false;
            }

            // Continue to scan next
            return true;
        });

        if (!success)
        {
            throw Exceptions::TokenExpectedException(reader.GetCurrentPosition(), "}");
        }

        reader.Read(); // '}'
        return LexTokenFromGivenName(position, name);
    }

    std::shared_ptr<ParenthesisToken> LexParenthesisToken()
    {
        assert(reader.Peek() == '(');
        auto position = reader.GetCurrentPosition();
        reader.Read();

        /* ***** Lex internal text ***** */
        LexerImplement implement(reader, context, arguments);
        auto tokens = implement.Lex();
        reader = implement.reader;

        /* ***** Ensure that reader.Peek() == ')' ***** */
        if (reader.Eof() || reader.Peek() != ')')
        {
            throw Exceptions::TokenExpectedException(reader.GetCurrentPosition(), ")");
        }

        reader.Read(); // ')'
        return std::make_shared<ParenthesisToken>(position, tokens, LexSupplementaryText());
    }

    std::shared_ptr<Token> LexSymbolOrArgumentToken()
    {
        auto position = reader.GetCurrentPosition();

        /* ***** Try lex as two-letter symbol ***** */
        std::string_view substr = reader.TryPeek(2);
        if (substr.length() == 2)
        {
            if (substr == "==")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(position, BinaryType::Equal,
                                                             LexSupplementaryText());
            }
            else if (substr == "!=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(position, BinaryType::NotEqual,
                                                             LexSupplementaryText());
            }
            else if (substr == ">=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(
                    position, BinaryType::GreaterThanOrEqual, LexSupplementaryText());
            }
            else if (substr == "<=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(position, BinaryType::LessThanOrEqual,
                                                             LexSupplementaryText());
            }
            else if (substr == "->")
            {
                reader.Read(2);
                return std::make_shared<StoreArrayToken>(position, LexSupplementaryText());
            }
        }

        /* ***** Try lex as one-letter symbol ***** */
        switch (reader.Peek())
        {
        case '+':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::Add,
                                                         LexSupplementaryText());
        case '-':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::Sub,
                                                         LexSupplementaryText());
        case '*':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::Mult,
                                                         LexSupplementaryText());
        case '/':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::Div,
                                                         LexSupplementaryText());
        case '%':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::Mod,
                                                         LexSupplementaryText());
        case '<':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::LessThan,
                                                         LexSupplementaryText());
        case '>':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(position, BinaryType::GreaterThan,
                                                         LexSupplementaryText());
        case '?':
            reader.Read();
            return std::make_shared<ConditionalOperatorToken>(position, LexSupplementaryText());
        default:
            break;
        }

        /* ***** Get identifier's name ***** */
        std::string_view name = reader.Read(1);
        return LexTokenFromGivenName(position, name);
    }

    std::shared_ptr<Token> LexTokenFromGivenName(const CharPosition& position,
                                                 std::string_view name)
    {
        /* ***** Try lex as user-defined operator ***** */
        if (auto implement = context.TryGetOperatorImplement(name))
        {
            return std::make_shared<UserDefinedOperatorToken>(position, implement->GetDefinition(),
                                                              LexSupplementaryText());
        }
        else
        {
            /* ***** If failed, try lex as argument operator ***** */

            /* ***** Find name in arguments list ***** */
            auto it = std::find(arguments.begin(), arguments.end(), name);

            if (it != arguments.end())
            {
                /* It has been found so it is an argument ***** */
                int index = static_cast<int>(std::distance(arguments.begin(), it));
                return std::make_shared<ArgumentToken>(position, std::string(name), index,
                                                       LexSupplementaryText());
            }
            else
            {
                throw Exceptions::OperatorOrOperandNotDefinedException(position, std::string(name));
            }
        }
    }

    std::string LexSupplementaryText()
    {
        return LexSupplementaryTextWithPosition().first;
    }

    std::pair<std::string, CharPosition> LexSupplementaryTextWithPosition()
    {
        if (reader.Eof() || reader.Peek() != '[')
        {
            // TODO: Inappropriate returning value
            return std::make_pair(std::string(), CharPosition{});
        }

        reader.Read(); // '['
        auto position = reader.GetCurrentPosition();
        int depth = 1;

        std::string_view supplementaryText = reader.ReadWhile([&depth](char c) {
            if (c == '[')
            {
                depth++;
            }
            else if (c == ']')
            {
                depth--;
            }

            return depth > 0;
        });

        if (depth != 0)
        {
            throw Exceptions::TokenExpectedException(reader.GetCurrentPosition(), "]");
        }

        reader.Read(); // ']'
        return std::make_pair(std::string(supplementaryText), position);
    }
};

class ParserImplement
{
public:
    const std::vector<std::shared_ptr<Token>>& tokens;
    CompilationContext& context;
    int maxNumOperands;
    size_t index;

    ParserImplement(const std::vector<std::shared_ptr<Token>>& tokens, CompilationContext& context,
                    size_t index)
        : tokens(tokens), context(context), index(index)
    {
        maxNumOperands = 0;
        for (size_t i = index; i < tokens.size(); i++)
        {
            maxNumOperands = std::max(maxNumOperands, tokens[i]->GetNumOperands());
        }
    }

    std::pair<std::shared_ptr<const Operator>, size_t> ParseOne()
    {
        if (maxNumOperands == 0)
        {
            auto op = tokens[index]->CreateOperator({}, context);
            index++;
            return std::make_pair(std::move(op), index);
        }

        std::vector<std::shared_ptr<const Operator>> operands;

        /***** Extract tokens that take a few number of operands than current operator *****/
        auto lower = ReadLower();

        if (lower.empty())
        {
            /* ***** First operand is missing ***** */
            if (!tokens.empty() && dynamic_cast<const DecimalToken*>(tokens[0].get()))
            {
                // If the operand missing its operand is DecimalOperator,
                // we implicitly set ZeroOperator as its operand
                operands.push_back(ZeroOperator::Create());
            }
            else
            {
                // Otherwise, it is a syntax error
                assert(index < tokens.size());
                throw Exceptions::SomeOperandsMissingException(tokens[index]->GetPosition());
            }
        }
        else
        {
            operands.push_back(ParseCore(lower, context));
        }

        std::shared_ptr<const Operator> result = nullptr;
        while (index < tokens.size())
        {
            auto& token = tokens[index];
            if (token->GetNumOperands() < maxNumOperands)
            {
                break;
            }

            index++;

            /* ***** Repeat parsing until the number of operands is sufficient ***** */
            while (operands.size() < static_cast<size_t>(maxNumOperands))
            {
                auto lower = ReadLower();
                if (lower.empty())
                {
                    throw Exceptions::SomeOperandsMissingException(token->GetPosition());
                }

                operands.push_back(ParseCore(lower, context));
                if (operands.size() < static_cast<size_t>(maxNumOperands))
                {
                    index++;
                }
            }

            auto op = token->CreateOperator(operands, context);
            operands.clear();
            operands.push_back(op);
            result = op;
        }

        assert(result != nullptr);
        return std::make_pair(std::move(result), index);
    }

    std::vector<std::shared_ptr<Token>> ReadLower()
    {
        std::vector<std::shared_ptr<Token>> vec;

        while (index < tokens.size() && tokens[index]->GetNumOperands() < maxNumOperands)
        {
            vec.push_back(tokens[index]);
            index++;
        }

        return vec;
    }
};

std::shared_ptr<const Operator> ParseCore(const std::vector<std::shared_ptr<Token>>& tokens,
                                          CompilationContext& context)
{
    std::vector<std::shared_ptr<const Operator>> operators;

    size_t index = 0;
    while (index < tokens.size())
    {
        auto pair = ParserImplement(tokens, context, index).ParseOne();
        operators.emplace_back(std::move(pair.first));
        index = pair.second;
    }

    switch (operators.size())
    {
    case 0:
        throw Exceptions::CodeIsEmptyException(std::nullopt);
    case 1:
        return operators[0];
    default:
        return ParenthesisOperator::Create(std::move(operators));
    }
}

void GenerateUserDefinedCodes(const std::vector<std::shared_ptr<Token>>& tokens,
                              CompilationContext& context)
{
    for (auto& token : tokens)
    {
        if (auto define = dynamic_cast<const DefineToken*>(token.get()))
        {
            auto op = ParseCore(define->GetTokens(), context);
            OperatorImplement implement(
                context.GetOperatorImplement(define->GetName()).GetDefinition(), op);
            context.AddOperatorImplement(implement);
        }
    }
}
}

std::vector<std::shared_ptr<Token>> Lex(std::string_view text, CompilationContext& context)
{
    std::vector<std::string> emptyVector;
    LexerImplement implement(text, context, emptyVector);
    auto result = implement.Lex();
    if (!implement.reader.Eof())
    {
        throw Exceptions::UnexpectedTokenException(implement.reader.GetCurrentPosition(),
                                                   implement.reader.Peek());
    }

    return result;
}

std::shared_ptr<const Operator> Parse(const std::vector<std::shared_ptr<Token>>& tokens,
                                      CompilationContext& context)
{
    GenerateUserDefinedCodes(tokens, context);
    return ParseCore(tokens, context);
}
}
