#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <numeric>
#include "Common.h"
#include "Operators.h"

/* ********** */

class Token {
public:
    virtual int GetNumOperands() const = 0;
    virtual const std::string &GetSupplementaryText() const = 0;
    virtual ~Token() {}
};

#define MAKE_GET_SUPPLEMENTARY_TEXT virtual const std::string &GetSupplementaryText() const override {\
	return supplementaryText;\
}

#define MAKE_GET_NUM_OPERANDS(NUM_OPERANDS) virtual int GetNumOperands() const override {\
	return NUM_OPERANDS;\
}

class ArgumentToken : public Token {
private:
    std::string name;
    int index;
    std::string supplementaryText;

public:
    ArgumentToken(const std::string &name, int index, const std::string &supplementaryText)
        : name(name), index(index), supplementaryText(supplementaryText) {}

    const std::string GetName() const {
        return name;
    }

    int GetIndex() const {
        return index;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class DefineToken : public Token {
private:
    std::string name;
    std::vector<std::string> arguments;
    std::vector<std::shared_ptr<Token>> tokens;
    std::string supplementaryText;

public:
    DefineToken(
        const std::string &name,
        const std::vector<std::string> &arguments,
        const std::vector<std::shared_ptr<Token>> &tokens,
        const std::string &supplementaryText)
        : name(name), arguments(arguments), tokens(tokens), supplementaryText(supplementaryText) {}

    const std::string GetName() const {
        return name;
    }

    const std::vector<std::string> GetArguments() const {
        return arguments;
    }

    const std::vector<std::shared_ptr<Token>> GetTokens() const {
        return tokens;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class ParenthesisToken : public Token {
private:
    std::vector<std::shared_ptr<Token>> tokens;
    std::string supplementaryText;

public:
    ParenthesisToken(const std::vector<std::shared_ptr<Token>> &tokens, const std::string &supplementaryText)
        : tokens(tokens), supplementaryText(supplementaryText) {}

    const std::vector<std::shared_ptr<Token>> GetTokens() const {
        return tokens;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class DecimalToken : public Token {
private:
    int value;
    std::string supplementaryText;

public:
    DecimalToken(int value, const std::string &supplementaryText)
        : value(value), supplementaryText(supplementaryText) {}

    int GetValue() const {
        return value;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class BinaryOperatorToken : public Token {
private:
    BinaryType type;
    std::string supplementaryText;

public:
    BinaryOperatorToken(BinaryType type, const std::string &supplementaryText)
        : type(type), supplementaryText(supplementaryText) {}

    BinaryType GetType() const {
        return type;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(2)
};

class ConditionalOperatorToken : public Token {
private:
    std::string supplementaryText;

public:
    ConditionalOperatorToken(const std::string &supplementaryText)
        : supplementaryText(supplementaryText) {}

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(3)
};

class UserDefinedOperatorToken : public Token {
private:
    OperatorDefinition definition;
    std::string supplementaryText;

public:
    UserDefinedOperatorToken(const OperatorDefinition &definition, const std::string &supplementaryText)
        : definition(definition), supplementaryText(supplementaryText) {}

    const OperatorDefinition &GetDefinition() const {
        return definition;
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS((definition.GetNumOperands()))
};

template<typename TNumber>
std::vector<std::shared_ptr<Token>> Lex(const std::string &text, CompilationContext<TNumber> &context) {
    class Implement {
    private:
        const std::string &text;
        CompilationContext<TNumber> &context;
        std::vector<std::string> arguments;
        size_t index;

    public:
        Implement(const std::string &text, CompilationContext<TNumber> &context)
            : text(text), context(context), index(0) {}
        Implement(const std::string &text, CompilationContext<TNumber> &context, const std::vector<std::string> &arguments)
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
            std::string splitted[3];

            // Split supplementaryText into three strings
            for (size_t i = 0, begin = 0; i < 3; i++) {
                size_t end = supplementaryText.find_first_of('|', begin);
                splitted[i] = supplementaryText.substr(begin, end - begin);
                begin = end + 1;
            }

            // Split arguments
            std::vector<std::string> arguments;
            for (size_t begin = 0; begin < splitted[1].length();) {
                size_t end = splitted[1].find_first_of(',', begin);
                arguments.push_back(splitted[1].substr(begin, end - begin));

                if (end == std::string::npos) {
                    break;
                } else {
                    begin = end + 1;
                }
            }

            auto &name = splitted[0];

            // Update CompilationContext
            OperatorDefinition definition(name, static_cast<int>(arguments.size()));
            OperatorImplement<TNumber> implement(definition, nullptr);
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
            std::string name = text.substr(begin, end - begin);

            assert(end != std::string::npos);
            index = end + 1;

            if (auto implement = context.TryGetOperatorImplement(name)) {
                return std::make_shared<UserDefinedOperatorToken>(implement->GetDefinition(), LexSupplementaryText());
            } else {
                auto it = std::find(arguments.begin(), arguments.end(), name);
                if (it != arguments.end()) {
                    int index = static_cast<int>(std::distance(arguments.begin(), it));
                    return std::make_shared<ArgumentToken>(name, index, LexSupplementaryText());
                } else {
                    static char buffer[SPRINTF_BUFFER_SIZE];
                    sprintf(buffer, "Argument \"%s\" is not defined", name.c_str());
                    throw std::string(buffer);
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
            assert(text[index] == ')');
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
                    static char buffer[SPRINTF_BUFFER_SIZE];
                    sprintf(buffer, "Variable or operator \"%s\" is not defined", name.c_str());
                    throw std::string(buffer);
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

template<typename TNumber>
std::shared_ptr<Operator<TNumber>> Parse(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext<TNumber> &context) {
    class Implement {
    private:
        const std::vector<std::shared_ptr<Token>> &tokens;
        CompilationContext<TNumber> &context;
        int maxNumOperands;
        size_t index;

    public:
        Implement(const std::vector<std::shared_ptr<Token>> &tokens, CompilationContext<TNumber> &context)
            : tokens(tokens), context(context), index(0) {
            auto maxElement = std::max_element(tokens.begin(), tokens.end(), [](const std::shared_ptr<Token> &a, const std::shared_ptr<Token> &b) {
                return a->GetNumOperands() < b->GetNumOperands();
            });
            maxNumOperands = (*maxElement)->GetNumOperands();
        }

        std::shared_ptr<Operator<TNumber>> Parse() {
            GenerateUserDefinedCode();
            std::vector<std::shared_ptr<Operator<TNumber>>> results;

            if (maxNumOperands == 0) {
                while (index < tokens.size()) {
                    results.push_back(CreateOperator(tokens[index], {}));
                    index++;
                }
            } else {
                std::vector<std::shared_ptr<Operator<TNumber>>> operands;

                auto lower = ReadLower();
                if (lower.empty() && !tokens.empty()
                    && dynamic_cast<const DecimalToken *>(tokens.begin()->get()) != nullptr) {
                    operands.push_back(std::make_shared<ZeroOperator<TNumber>>());
                } else {
                    operands.push_back(Implement(lower, context).Parse());
                }

                while (index < tokens.size()) {
                    auto& token = tokens[index];
                    index++;

                    while (operands.size() < static_cast<size_t>(maxNumOperands)) {
                        operands.push_back(Implement(ReadLower(), context).Parse());
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
                return std::make_shared<ParenthesisOperator<TNumber>>(std::move(results));
            }
        }

        void GenerateUserDefinedCode() {
            for (auto& token : tokens) {
                if (auto define = dynamic_cast<const DefineToken *>(token.get())) {
                    auto op = Implement(define->GetTokens(), context).Parse();
                    OperatorImplement<TNumber> implement(context.GetOperatorImplement(define->GetName()).GetDefinition(), op);
                    context.AddOperatorImplement(implement);
                }
            }
        }

        std::shared_ptr<Operator<TNumber>> CreateOperator(const std::shared_ptr<Token> &token, const std::vector<std::shared_ptr<Operator<TNumber>>> &operands) {
            auto ptr = token.get();

            if (auto argument = dynamic_cast<const ArgumentToken *>(ptr)) {
                return std::make_shared<ArgumentOperator<TNumber>>(argument->GetIndex());
            } else if (auto define = dynamic_cast<const DefineToken *>(ptr)) {
                return std::make_shared<DefineOperator<TNumber>>();
            } else if (auto parentesis = dynamic_cast<const ParenthesisToken *>(ptr)) {
                return Implement(parentesis->GetTokens(), context).Parse();
            } else if (auto decimal = dynamic_cast<const DecimalToken *>(ptr)) {
                return std::make_shared<DecimalOperator<TNumber>>(operands[0], decimal->GetValue());
            } else if (auto binary = dynamic_cast<const BinaryOperatorToken *>(ptr)) {
                return std::make_shared<BinaryOperator<TNumber>>(operands[0], operands[1], binary->GetType());
            } else if (auto conditional = dynamic_cast<const ConditionalOperatorToken *>(ptr)) {
                return std::make_shared<ConditionalOperator<TNumber>>(operands[0], operands[1], operands[2]);
            } else if (auto userDefined = dynamic_cast<const UserDefinedOperatorToken *>(ptr)) {
                return std::make_shared<UserDefinedOperator<TNumber>>(userDefined->GetDefinition(), operands);
            } else {
                UNREACHABLE();
                throw std::string("Assertion error");
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
