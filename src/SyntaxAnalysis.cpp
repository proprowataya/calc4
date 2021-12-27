#include "SyntaxAnalysis.h"
#include "Common.h"
#include "Error.h"
#include "Operators.h"
#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

public:
    StringReader(std::string_view text) : text(text), index(0), lineNo(0), charNo(0) {}

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
        return { index, lineNo, charNo };
    }
};

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
        reader.Read();

        std::string supplementaryText = LexSupplementaryText();

        /* ***** Split supplementaryText into three strings ***** */
        std::vector<std::string> splitted = Split(supplementaryText, '|');
        if (splitted.size() != 3)
        {
            throw ErrorMessage::DefinitionTextNotSplittedProperly(supplementaryText);
        }

        /* ***** Split arguments ***** */
        std::vector<std::string> arguments = Split(splitted[1], ',');

        /* ***** Trim arguments (e.g. "  x " => "x") ***** */
        for (auto& arg : arguments)
        {
            arg = TrimWhiteSpaces(arg);
        }

        /* ***** Operator name ***** */
        auto& name = splitted[0];

        /* ***** Update CompilationContext ***** */
        OperatorDefinition definition(name, static_cast<int>(arguments.size()));
        OperatorImplement implement(definition, nullptr);
        context.AddOperatorImplement(implement);

        /* ***** Lex internal text ***** */
        auto tokens = LexerImplement(splitted[2], context, arguments).Lex();

        /* ***** Construct token ***** */
        return std::make_shared<DefineToken>(name, arguments, tokens, std::move(supplementaryText));
    }

    std::shared_ptr<LoadVariableToken> LexLoadVariableToken()
    {
        reader.Read();
        return std::make_shared<LoadVariableToken>(LexSupplementaryText());
    }

    std::shared_ptr<StoreVariableToken> LexStoreVariableToken()
    {
        reader.Read();
        return std::make_shared<StoreVariableToken>(LexSupplementaryText());
    }

    std::shared_ptr<PrintCharToken> LexPrintCharToken()
    {
        reader.Read();
        return std::make_shared<PrintCharToken>(LexSupplementaryText());
    }

    std::shared_ptr<LoadArrayToken> LexLoadArrayToken()
    {
        reader.Read();
        return std::make_shared<LoadArrayToken>(LexSupplementaryText());
    }

    std::shared_ptr<DecimalToken> LexDecimalToken()
    {
        int value = reader.Read() - '0';
        return std::make_shared<DecimalToken>(value, LexSupplementaryText());
    }

    std::shared_ptr<Token> LexUserDefinedOperatorOrArgumentToken()
    {
        assert(reader.Peek() == '{');
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
            throw ErrorMessage::TokenExpected("}");
        }

        reader.Read(); // '}'
        return LexTokenFromGivenName(name);
    }

    std::shared_ptr<ParenthesisToken> LexParenthesisToken()
    {
        assert(reader.Peek() == '(');
        reader.Read();

        /* ***** Lex internal text ***** */
        LexerImplement implement(reader, context, arguments);
        auto tokens = implement.Lex();
        reader = implement.reader;

        /* ***** Ensure that reader.Peek() == ')' ***** */
        if (reader.Eof() || reader.Peek() != ')')
        {
            throw ErrorMessage::TokenExpected(")");
        }

        reader.Read(); // ')'
        return std::make_shared<ParenthesisToken>(tokens, LexSupplementaryText());
    }

    std::shared_ptr<Token> LexSymbolOrArgumentToken()
    {
        /* ***** Try lex as two-letter symbol ***** */
        std::string_view substr = reader.TryPeek(2);
        if (substr.length() == 2)
        {
            if (substr == "==")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(BinaryType::Equal,
                                                             LexSupplementaryText());
            }
            else if (substr == "!=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(BinaryType::NotEqual,
                                                             LexSupplementaryText());
            }
            else if (substr == ">=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(BinaryType::GreaterThanOrEqual,
                                                             LexSupplementaryText());
            }
            else if (substr == "<=")
            {
                reader.Read(2);
                return std::make_shared<BinaryOperatorToken>(BinaryType::LessThanOrEqual,
                                                             LexSupplementaryText());
            }
            else if (substr == "->")
            {
                reader.Read(2);
                return std::make_shared<StoreArrayToken>(LexSupplementaryText());
            }
        }

        /* ***** Try lex as one-letter symbol ***** */
        switch (reader.Peek())
        {
        case '+':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::Add, LexSupplementaryText());
        case '-':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::Sub, LexSupplementaryText());
        case '*':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::Mult, LexSupplementaryText());
        case '/':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::Div, LexSupplementaryText());
        case '%':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::Mod, LexSupplementaryText());
        case '<':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::LessThan,
                                                         LexSupplementaryText());
        case '>':
            reader.Read();
            return std::make_shared<BinaryOperatorToken>(BinaryType::GreaterThan,
                                                         LexSupplementaryText());
        case '?':
            reader.Read();
            return std::make_shared<ConditionalOperatorToken>(LexSupplementaryText());
        default:
            break;
        }

        /* ***** Get identifier's name ***** */
        std::string_view name = reader.Read(1);
        return LexTokenFromGivenName(name);
    }

    std::shared_ptr<Token> LexTokenFromGivenName(std::string_view name)
    {
        /* ***** Try lex as user-defined operator ***** */
        if (auto implement = context.TryGetOperatorImplement(name))
        {
            return std::make_shared<UserDefinedOperatorToken>(implement->GetDefinition(),
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
                return std::make_shared<ArgumentToken>(std::string(name), index,
                                                       LexSupplementaryText());
            }
            else
            {
                throw ErrorMessage::OperatorOrOperandNotDefined(std::string(name));
            }
        }
    }

    std::string LexSupplementaryText()
    {
        if (reader.Eof() || reader.Peek() != '[')
        {
            return std::string();
        }

        reader.Read(); // '['
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
            throw ErrorMessage::TokenExpected("]");
        }

        reader.Read(); // ']'
        return std::string(supplementaryText);
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
                throw ErrorMessage::SomeOperandsMissing();
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
                    throw ErrorMessage::SomeOperandsMissing();
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
        throw ErrorMessage::CodeIsEmpty();
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
        throw ErrorMessage::UnexpectedToken(implement.reader.Peek());
    }

    return result;
}

std::shared_ptr<const Operator> Parse(const std::vector<std::shared_ptr<Token>>& tokens,
                                      CompilationContext& context)
{
    GenerateUserDefinedCodes(tokens, context);
    return ParseCore(tokens, context);
}
