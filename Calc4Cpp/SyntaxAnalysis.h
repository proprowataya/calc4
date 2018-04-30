#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <numeric>
#include "Common.h"
#include "Operators.h"

enum class TokenType {
	Argument,
	Define,
	Parenthesis,
	Decimal,
	BinaryOperator,
	ConditionalOperator,
	UserDefinedOperator,
};

struct Token {
	TokenType type;
	std::string supplementaryText;

	std::string name;					// For Argument, Define, UserDefinedOperator
	int index;							// For Argument
	std::vector<std::string> arguments;	// For Define
	std::vector<Token> tokens;			// For Define, Parenthesis
	int value;							// For Decimal
	BinaryType binaryType;				// For BinaryOperator
	int numOperands;					// For UserDefinedOperator
};

int GetNumOperands(const Token &token) {
	switch (token.type) {
	case TokenType::Argument:
	case TokenType::Define:
	case TokenType::Parenthesis:
		return 0;
	case TokenType::Decimal:
		return 1;
	case TokenType::BinaryOperator:
		return 2;
	case TokenType::ConditionalOperator:
		return 3;
	case TokenType::UserDefinedOperator:
		return token.numOperands;
	default:
		return -1;
	}
}

template<typename TNumber>
std::vector<Token> Lex(const std::string &text, CompilationContext<TNumber> &context) {
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
			: text(text), context(context), index(0), arguments(arguments) {}

		std::vector<Token> Lex() {
			std::vector<Token> vec;

			while (index < text.length() && text[index] != ')') {
				if (text[index] == ' ') {
					index++;
					continue;
				}

				vec.push_back(NextToken());
			}

			return vec;
		}

		Token NextToken() {
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

		Token LexDefineToken() {
			assert(text[index] == 'D');
			index++;

			std::string supplementaryText = LexSupplementaryText();
			std::string splitted[3];

			for (size_t i = 0, begin = 0; i < 3; i++) {
				size_t end = supplementaryText.find_first_of('|', begin);
				splitted[i] = supplementaryText.substr(begin, end - begin);
				begin = end + 1;
			}

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

			OperatorDefinition definition(splitted[0], static_cast<int>(arguments.size()));
			OperatorImplement<TNumber> implement(definition, nullptr);
			context.AddOperatorImplement(implement);

			auto tokens = Implement(splitted[2], context, arguments).Lex();
			Token token = { TokenType::Define };
			token.name = splitted[0];
			token.arguments = arguments;
			token.tokens = std::move(tokens);
			token.supplementaryText = supplementaryText;
			return token;
		}

		Token LexDecimalToken() {
			Token token = { TokenType::Decimal };
			token.value = text[index++] - '0';
			token.supplementaryText = LexSupplementaryText();
			return token;
		}

		Token LexUserDefinedOperatorOrArgumentToken() {
			assert(text[index] == '{');
			index++;

			size_t begin = index;
			size_t end = text.find_first_of('}', begin);
			std::string name = text.substr(begin, end - begin);

			assert(end != std::string::npos);
			index = end + 1;

			if (auto implement = context.TryGetOperatorImplement(name)) {
				Token token = { TokenType::UserDefinedOperator };
				token.name = implement->GetDefinition().GetName();
				token.numOperands = implement->GetDefinition().GetNumOperands();
				token.supplementaryText = LexSupplementaryText();
				return token;
			} else {
				auto it = std::find(arguments.begin(), arguments.end(), name);
				if (it != arguments.end()) {
					Token token = { TokenType::Argument };
					token.name = name;
					token.index = static_cast<int>(std::distance(arguments.begin(), it));
					token.supplementaryText = LexSupplementaryText();
					return token;
				} else {
					throw "Error";
				}
			}
		}

		Token LexParenthesisToken() {
			assert(text[index] == '(');
			index++;

			Implement implement(text, context, arguments);
			implement.index = index;
			auto tokens = implement.Lex();

			index = implement.index;
			assert(text[index] == ')');
			index++;

			Token token = { TokenType::Parenthesis };
			token.tokens = tokens;
			token.supplementaryText = LexSupplementaryText();
			return token;
		}

		Token LexSymbolOrArgumentToken() {
			if (index + 1 < text.length()) {
				std::string substr = text.substr(index, 2);
				if (substr == "==") {
					index += 2;
					Token token = { TokenType::BinaryOperator };
					token.binaryType = BinaryType::Equal;
					token.supplementaryText = LexSupplementaryText();
					return token;
				} else if (substr == "!=") {
					index += 2;
					Token token = { TokenType::BinaryOperator };
					token.binaryType = BinaryType::NotEqual;
					token.supplementaryText = LexSupplementaryText();
					return token;
				} else if (substr == ">=") {
					index += 2;
					Token token = { TokenType::BinaryOperator };
					token.binaryType = BinaryType::GreaterThanOrEqual;
					token.supplementaryText = LexSupplementaryText();
					return token;
				} else if (substr == "<=") {
					index += 2;
					Token token = { TokenType::BinaryOperator };
					token.binaryType = BinaryType::LessThanOrEqual;
					token.supplementaryText = LexSupplementaryText();
					return token;
				}
			}

			switch (text[index]) {
			case '+':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::Add;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '-':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::Sub;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '*':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::Mult;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '/':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::Div;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '%':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::Mod;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '<':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::LessThan;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '>':
			{
				index++;
				Token token = { TokenType::BinaryOperator };
				token.binaryType = BinaryType::GreaterThan;
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			case '?':
			{
				index++;
				Token token = { TokenType::ConditionalOperator };
				token.supplementaryText = LexSupplementaryText();
				return token;
			}
			default:
				break;
			}

			if (auto implement = context.TryGetOperatorImplement(text.substr(index, 1))) {
				Token token = { TokenType::UserDefinedOperator };
				token.name = implement->GetDefinition().GetName();
				token.numOperands = implement->GetDefinition().GetNumOperands();
				token.supplementaryText = LexSupplementaryText();
				index++;
				return token;
			} else {
				auto it = std::find(arguments.begin(), arguments.end(), text.substr(index, 1));
				if (it != arguments.end()) {
					Token token = { TokenType::Argument };
					token.name = text.substr(index, 1);
					token.index = static_cast<int>(std::distance(arguments.begin(), it));
					token.supplementaryText = LexSupplementaryText();
					index++;
					return token;
				} else {
					throw "Error";
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
std::shared_ptr<Operator<TNumber>> Parse(const std::vector<Token> &tokens, CompilationContext<TNumber> &context) {
	class Implement {
	private:
		const std::vector<Token> &tokens;
		CompilationContext<TNumber> &context;
		int maxNumOperands;
		size_t index;

	public:
		Implement(const std::vector<Token> &tokens, CompilationContext<TNumber> &context)
			: tokens(tokens), context(context), index(0) {
			auto max = std::max_element(tokens.begin(), tokens.end(), [](const Token &a, const Token &b) {
				return GetNumOperands(a) < GetNumOperands(b);
			});
			maxNumOperands = GetNumOperands(*max);
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
				if (lower.empty() && !tokens.empty() && tokens.begin()->type == TokenType::Decimal) {
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
				throw "Invalid";
			case 1:
				return results[0];
			default:
				return std::make_shared<ParenthesisOperator<TNumber>>(std::move(results));
			}
		}

		void GenerateUserDefinedCode() {
			for (auto& token : tokens) {
				if (token.type == TokenType::Define) {
					auto op = Implement(token.tokens, context).Parse();
					OperatorImplement<TNumber> implement(context.GetOperatorImplement(token.name).GetDefinition(), op);
					context.AddOperatorImplement(implement);
				}
			}
		}

		std::shared_ptr<Operator<TNumber>> CreateOperator(const Token &token, const std::vector<std::shared_ptr<Operator<TNumber>>> &operands) {
			switch (token.type) {
			case TokenType::Argument:
				return std::make_shared<ArgumentOperator<TNumber>>(token.index);
			case TokenType::Define:
				return std::make_shared<DefineOperator<TNumber>>();
			case TokenType::Parenthesis:
				return Implement(token.tokens, context).Parse();
			case TokenType::Decimal:
				return std::make_shared<DecimalOperator<TNumber>>(operands[0], token.value);
			case TokenType::BinaryOperator:
				return std::make_shared<BinaryOperator<TNumber>>(operands[0], operands[1], token.binaryType);
			case TokenType::ConditionalOperator:
				return std::make_shared<ConditionalOperator<TNumber>>(operands[0], operands[1], operands[2]);
			case TokenType::UserDefinedOperator:
				return std::make_shared<UserDefinedOperator<TNumber>>(OperatorDefinition(token.name, token.numOperands), operands);
			default:
				throw "Invalid";
			}
		}

		std::vector<Token> ReadLower() {
			std::vector<Token> vec;

			while (index < tokens.size() && GetNumOperands(tokens[index]) < maxNumOperands) {
				vec.push_back(tokens[index]);
				index++;
			}

			return vec;
		}
	};

	return Implement(tokens, context).Parse();
}
