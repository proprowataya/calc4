#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <numeric>
#include "Common.h"
#include "Operators.h"
#include "SyntaxAnalysis.h"

std::vector<std::shared_ptr<Token>> Lex(const std::string &text, CompilationContext &context) {
    class Implement {
    private:
        const std::string &text;
        CompilationContext &context;
        std::vector<std::string> arguments;
        size_t index;

    public:
        Implement(const std::string &text, CompilationContext &context)
            : text(text), context(context), index(0) {}
        Implement(const std::string &text, CompilationContext &context, const std::vector<std::string> &arguments)
            : text(text), context(context), arguments(arguments), index(0) {}

        std::vector<std::shared_ptr<Token>> Lex() {
            std::vector<std::shared_ptr<Token>> vec;

            while (index < text.length() && text[index] != ')') {
                if (text[index] == ' ') {
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

            // Split supplementaryText into three strings
            std::vector<std::string> splitted = Split(supplementaryText, '|');
            if (splitted.size() != 3) {
                sprintf(sprintfBuffer, ErrorMessage::DefinitionTextNotSplittedProperly, supplementaryText.c_str());
                throw std::string(sprintfBuffer);
            }

            // Split arguments
            std::vector<std::string> arguments = Split(splitted[1], ',');

            // Trim arguments (e.g. "  x " => "x")
            for (auto& arg : arguments) {
                arg = TrimWhiteSpaces(arg);
            }

            // Operator name
            auto &name = splitted[0];

            // Update CompilationContext
            OperatorDefinition definition(name, static_cast<int>(arguments.size()));
            OperatorImplement implement(definition, nullptr);
            context.AddOperatorImplement(implement);

            auto tokens = Implement(splitted[2], context, arguments).Lex();
            return std::make_shared<DefineToken>(name, arguments, tokens, LexSupplementaryText());
        }

        std::shared_ptr<DecimalToken> LexDecimalToken() {
            int value = text[index++] - '0';
            return std::make_shared<DecimalToken>(value, LexSupplementaryText());
        }

        std::shared_ptr<Token> LexUserDefinedOperatorOrArgumentToken() {
            assert(text[index] == '{');
            index++;

            size_t begin = index;
            size_t end = text.find_first_of('}', begin);
            if (end == std::string::npos) {
                sprintf(sprintfBuffer, ErrorMessage::TokenExpected, "}");
                throw std::string(sprintfBuffer);
            }

            std::string name = text.substr(begin, end - begin);
            index = end + 1;

            if (auto implement = context.TryGetOperatorImplement(name)) {
                return std::make_shared<UserDefinedOperatorToken>(implement->GetDefinition(), LexSupplementaryText());
            } else {
                auto it = std::find(arguments.begin(), arguments.end(), name);
                if (it != arguments.end()) {
                    int index = static_cast<int>(std::distance(arguments.begin(), it));
                    return std::make_shared<ArgumentToken>(name, index, LexSupplementaryText());
                } else {
                    sprintf(sprintfBuffer, ErrorMessage::OperatorOrOperandNotDefined, name.c_str());
                    throw std::string(sprintfBuffer);
                }
            }
        }

        std::shared_ptr<ParenthesisToken> LexParenthesisToken() {
            assert(text[index] == '(');
            index++;

            Implement implement(text, context, arguments);
            implement.index = index;
            auto tokens = implement.Lex();

            index = implement.index;

            if (index >= text.length() || text[index] != ')') {
                sprintf(sprintfBuffer, ErrorMessage::TokenExpected, ")");
                throw std::string(sprintfBuffer);
            }

            index++;
            return std::make_shared<ParenthesisToken>(tokens, LexSupplementaryText());
        }

        std::shared_ptr<Token> LexSymbolOrArgumentToken() {
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

            std::string name = text.substr(index, 1);

            if (auto implement = context.TryGetOperatorImplement(name)) {
                index++;
                return std::make_shared<UserDefinedOperatorToken>(implement->GetDefinition(), LexSupplementaryText());
            } else {
                index++;
                auto it = std::find(arguments.begin(), arguments.end(), name);
                if (it != arguments.end()) {
                    int index = static_cast<int>(std::distance(arguments.begin(), it));
                    return std::make_shared<ArgumentToken>(name, index, LexSupplementaryText());
                } else {
                    sprintf(sprintfBuffer, ErrorMessage::OperatorOrOperandNotDefined, name.c_str());
                    throw std::string(sprintfBuffer);
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
            index = end + 1;

            return text.substr(begin, end - begin);
        }
    };

    return Implement(text, context).Lex();
}

std::shared_ptr<Operator> Parse(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext &context) {
    class Implement {
    private:
        const std::vector<std::shared_ptr<Token>> &tokens;
        CompilationContext &context;
        int maxNumOperands;
        size_t index;

    public:
        Implement(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext &context)
            : tokens(tokens), context(context), index(0) {
            auto maxElement = std::max_element(tokens.begin(), tokens.end(), [](const std::shared_ptr<Token> &a, const std::shared_ptr<Token> &b) {
                return a->GetNumOperands() < b->GetNumOperands();
            });
            maxNumOperands = (*maxElement)->GetNumOperands();
        }

        std::shared_ptr<Operator> Parse() {
            GenerateUserDefinedCode();
            std::vector<std::shared_ptr<Operator>> results;

            if (maxNumOperands == 0) {
                while (index < tokens.size()) {
                    results.push_back(CreateOperator(tokens[index], {}));
                    index++;
                }
            } else {
                std::vector<std::shared_ptr<Operator>> operands;

                auto lower = ReadLower();
                if (lower.empty()) {
                    if (!tokens.empty() && dynamic_cast<const DecimalToken *>(tokens.begin()->get()) != nullptr) {
                        operands.push_back(std::make_shared<ZeroOperator>());
                    } else {
                        sprintf(sprintfBuffer, ErrorMessage::SomeOperandsMissing);
                        throw std::string(sprintfBuffer);
                    }
                } else {
                    operands.push_back(Implement(lower, context).Parse());
                }

                while (index < tokens.size()) {
                    auto& token = tokens[index];
                    index++;

                    while (operands.size() < static_cast<size_t>(maxNumOperands)) {
                        auto lower = ReadLower();
                        if (lower.empty()) {
                            sprintf(sprintfBuffer, ErrorMessage::SomeOperandsMissing);
                            throw std::string(sprintfBuffer);
                        }

                        operands.push_back(Implement(lower, context).Parse());
                        if (operands.size() < static_cast<size_t>(maxNumOperands)) {
                            index++;
                        }
                    }

                    auto op = CreateOperator(token, operands);
                    operands.clear();
                    operands.push_back(op);
                }

                results.insert(results.end(), operands.begin(), operands.end());
            }

            switch (results.size()) {
            case 0:
                throw std::string("Invalid code");
            case 1:
                return results[0];
            default:
                return std::make_shared<ParenthesisOperator>(std::move(results));
            }
        }

        void GenerateUserDefinedCode() {
            for (auto& token : tokens) {
                if (auto define = dynamic_cast<const DefineToken *>(token.get())) {
                    auto op = Implement(define->GetTokens(), context).Parse();
                    OperatorImplement implement(context.GetOperatorImplement(define->GetName()).GetDefinition(), op);
                    context.AddOperatorImplement(implement);
                }
            }
        }

        std::shared_ptr<Operator> CreateOperator(const std::shared_ptr<Token> &token, const std::vector<std::shared_ptr<Operator>> &operands) {
            auto ptr = token.get();

            if (auto argument = dynamic_cast<const ArgumentToken *>(ptr)) {
                return std::make_shared<ArgumentOperator>(argument->GetIndex());
            } else if (dynamic_cast<const DefineToken *>(ptr)) {
                return std::make_shared<DefineOperator>();
            } else if (auto parentesis = dynamic_cast<const ParenthesisToken *>(ptr)) {
                return Implement(parentesis->GetTokens(), context).Parse();
            } else if (auto decimal = dynamic_cast<const DecimalToken *>(ptr)) {
                return std::make_shared<DecimalOperator>(operands[0], decimal->GetValue());
            } else if (auto binary = dynamic_cast<const BinaryOperatorToken *>(ptr)) {
                return std::make_shared<BinaryOperator>(operands[0], operands[1], binary->GetType());
            } else if (dynamic_cast<const ConditionalOperatorToken *>(ptr)) {
                return std::make_shared<ConditionalOperator>(operands[0], operands[1], operands[2]);
            } else if (auto userDefined = dynamic_cast<const UserDefinedOperatorToken *>(ptr)) {
                return std::make_shared<UserDefinedOperator>(userDefined->GetDefinition(), operands);
            } else {
                UNREACHABLE();
                throw std::string(ErrorMessage::AssertionError);
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
