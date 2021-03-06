﻿#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <numeric>
#include "Common.h"
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Error.h"

std::vector<std::shared_ptr<Token>> Lex(const std::string &text, CompilationContext &context) {
    class Implement {
    public:
        const std::string &text;
        CompilationContext &context;
        const std::vector<std::string> &arguments;
        size_t index;

        Implement(const std::string &text, CompilationContext &context, const std::vector<std::string> &arguments)
            : text(text), context(context), arguments(arguments), index(0) {}

        std::vector<std::shared_ptr<Token>> Lex() {
            std::vector<std::shared_ptr<Token>> vec;

            while (index < text.length() && text[index] != ')') {
                if (text[index] == ' ') {
                    // Skip whitespace
                    index++;
                    continue;
                }

                vec.push_back(NextToken());
            }

            return vec;
        }

        std::shared_ptr<Token> NextToken() {
            switch (text[index]) {
            case 'D':
                return LexDefineToken();
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

        std::shared_ptr<DefineToken> LexDefineToken() {
            assert(text[index] == 'D');
            index++;

            std::string supplementaryText = LexSupplementaryText();

            /* ***** Split supplementaryText into three strings ***** */
            std::vector<std::string> splitted = Split(supplementaryText, '|');
            if (splitted.size() != 3) {
                throw ErrorMessage::DefinitionTextNotSplittedProperly(supplementaryText);
            }

            /* ***** Split arguments ***** */
            std::vector<std::string> arguments = Split(splitted[1], ',');

            /* ***** Trim arguments (e.g. "  x " => "x") ***** */
            for (auto& arg : arguments) {
                arg = TrimWhiteSpaces(arg);
            }

            /* ***** Operator name ***** */
            auto &name = splitted[0];

            /* ***** Update CompilationContext ***** */
            OperatorDefinition definition(name, static_cast<int>(arguments.size()));
            OperatorImplement implement(definition, nullptr);
            context.AddOperatorImplement(implement);

            /* ***** Lex internal text ***** */
            auto tokens = Implement(splitted[2], context, arguments).Lex();

            /* ***** Construct token ***** */
            return std::make_shared<DefineToken>(name, arguments, tokens, LexSupplementaryText());
        }

        std::shared_ptr<DecimalToken> LexDecimalToken() {
            int value = text[index++] - '0';
            return std::make_shared<DecimalToken>(value, LexSupplementaryText());
        }

        std::shared_ptr<Token> LexUserDefinedOperatorOrArgumentToken() {
            assert(text[index] == '{');
            index++;

            /* ***** Find begin/end indexes of an identifier ***** */
            size_t begin = index;
            size_t end = text.find_first_of('}', begin);
            if (end == std::string::npos) {
                throw ErrorMessage::TokenExpected("}");
            }

            /* ***** Get identifier's name ***** */
            std::string name = text.substr(begin, end - begin);
            index = end + 1;
            return LexTokenFromGivenName(name);
        }

        std::shared_ptr<ParenthesisToken> LexParenthesisToken() {
            assert(text[index] == '(');
            index++;

            /* ***** Lex internal text ***** */
            Implement implement(text, context, arguments);
            implement.index = index;
            auto tokens = implement.Lex();
            index = implement.index;

            /* ***** Ensure that text[index] == ')' ***** */
            if (index >= text.length() || text[index] != ')') {
                throw ErrorMessage::TokenExpected(")");
            }

            index++;    // ')'
            return std::make_shared<ParenthesisToken>(tokens, LexSupplementaryText());
        }

        std::shared_ptr<Token> LexSymbolOrArgumentToken() {
            /* ***** Try lex as two-letter symbol ***** */
            if (index + 1 < text.length()) {
                std::string substr = text.substr(index, 2);
                if (substr == "==") {
                    index += 2;
                    return std::make_shared<BinaryOperatorToken>(BinaryType::Equal, LexSupplementaryText());
                } else if (substr == "!=") {
                    index += 2;
                    return std::make_shared<BinaryOperatorToken>(BinaryType::NotEqual, LexSupplementaryText());
                } else if (substr == ">=") {
                    index += 2;
                    return std::make_shared<BinaryOperatorToken>(BinaryType::GreaterThanOrEqual, LexSupplementaryText());
                } else if (substr == "<=") {
                    index += 2;
                    return std::make_shared<BinaryOperatorToken>(BinaryType::LessThanOrEqual, LexSupplementaryText());
                }
            }

            /* ***** Try lex as one-letter symbol ***** */
            switch (text[index]) {
            case '+':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::Add, LexSupplementaryText());
            case '-':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::Sub, LexSupplementaryText());
            case '*':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::Mult, LexSupplementaryText());
            case '/':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::Div, LexSupplementaryText());
            case '%':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::Mod, LexSupplementaryText());
            case '<':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::LessThan, LexSupplementaryText());
            case '>':
                index++;
                return std::make_shared<BinaryOperatorToken>(BinaryType::GreaterThan, LexSupplementaryText());
            case '?':
                index++;
                return std::make_shared<ConditionalOperatorToken>(LexSupplementaryText());
            default:
                break;
            }

            /* ***** Get identifier's name ***** */
            std::string name = text.substr(index, 1);
            index++;
            return LexTokenFromGivenName(name);
        }

        std::shared_ptr<Token> LexTokenFromGivenName(const std::string &name) {
            /* ***** Try lex as user-defined operator ***** */
            if (auto implement = context.TryGetOperatorImplement(name)) {
                return std::make_shared<UserDefinedOperatorToken>(implement->GetDefinition(), LexSupplementaryText());
            } else {
                /* ***** If failed, try lex as argument operator ***** */

                /* ***** Find name in arguments list ***** */
                auto it = std::find(arguments.begin(), arguments.end(), name);

                if (it != arguments.end()) {
                    /* It has been found so it is an argument ***** */
                    int index = static_cast<int>(std::distance(arguments.begin(), it));
                    return std::make_shared<ArgumentToken>(name, index, LexSupplementaryText());
                } else {
                    throw ErrorMessage::OperatorOrOperandNotDefined(name);
                }
            }
        }

        std::string LexSupplementaryText() {
            if (index >= text.length() || text[index] != '[') {
                return std::string();
            }

            index++;
            size_t begin = index;
            size_t end = text.find_first_of(']', begin);
            if (end == std::string::npos) {
                throw ErrorMessage::TokenExpected("]");
            }

            index = end + 1;
            return text.substr(begin, end - begin);
        }
    };

    std::vector<std::string> emptyVector;
    Implement implement(text, context, emptyVector);
    auto result = implement.Lex();
    if (implement.index < text.length()) {
        throw ErrorMessage::UnexpectedToken(text[implement.index]);
    }

    return result;
}

std::shared_ptr<Operator> Parse(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext &context) {
    class Implement {
    public:
        const std::vector<std::shared_ptr<Token>> &tokens;
        CompilationContext &context;
        int maxNumOperands;
        size_t index;

        Implement(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext &context)
            : tokens(tokens), context(context), index(0) {
            maxNumOperands = 0;
            for (size_t i = 0; i < tokens.size(); i++) {
                maxNumOperands = std::max(maxNumOperands, tokens[i]->GetNumOperands());
            }
        }

        std::shared_ptr<Operator> Parse() {
            /* ***** Parse user-defined operators (DefineToken) ***** */
            for (auto& token : tokens) {
                if (auto define = dynamic_cast<const DefineToken *>(token.get())) {
                    auto op = Implement(define->GetTokens(), context).Parse();
                    OperatorImplement implement(context.GetOperatorImplement(define->GetName()).GetDefinition(), op);
                    context.AddOperatorImplement(implement);
                }
            }

            /* ***** Parse core ***** */
            std::vector<std::shared_ptr<Operator>> results;

            if (maxNumOperands == 0) {
                while (index < tokens.size()) {
                    results.push_back(tokens[index]->CreateOperator({}, context));
                    index++;
                }
            } else {
                std::vector<std::shared_ptr<Operator>> operands;

                /* ***** Extract tokens that take a few number of operands than current operator ***** */
                auto lower = ReadLower();

                if (lower.empty()) {
                    /* ***** First operand is missing ***** */
                    if (!tokens.empty() && dynamic_cast<const DecimalToken *>(tokens[0].get())) {
                        // If the operand missing its operand is DecimalOperator,
                        // we implicitly set ZeroOperator as its operand
                        operands.push_back(std::make_shared<ZeroOperator>());
                    } else {
                        // Otherwise, it is a syntax error
                        throw ErrorMessage::SomeOperandsMissing();
                    }
                } else {
                    operands.push_back(Implement(lower, context).Parse());
                }

                while (index < tokens.size()) {
                    auto& token = tokens[index];
                    index++;

                    /* ***** Repeat parsing until the number of operands is sufficient ***** */
                    while (operands.size() < static_cast<size_t>(maxNumOperands)) {
                        auto lower = ReadLower();
                        if (lower.empty()) {
                            throw ErrorMessage::SomeOperandsMissing();
                        }

                        operands.push_back(Implement(lower, context).Parse());
                        if (operands.size() < static_cast<size_t>(maxNumOperands)) {
                            index++;
                        }
                    }

                    auto op = token->CreateOperator(operands, context);
                    operands.clear();
                    operands.push_back(op);
                }

                results.insert(results.end(), operands.begin(), operands.end());
            }

            switch (results.size()) {
            case 0:
                throw ErrorMessage::CodeIsEmpty();
            case 1:
                return results[0];
            default:
                return std::make_shared<ParenthesisOperator>(std::move(results));
            }
        }

        std::vector<std::shared_ptr<Token>> ReadLower() {
            std::vector<std::shared_ptr<Token>> vec;

            while (index < tokens.size() && tokens[index]->GetNumOperands() < maxNumOperands) {
                vec.push_back(tokens[index]);
                index++;
            }

            return vec;
        }
    };

    return Implement(tokens, context).Parse();
}
