#include <iostream>
#include <ctime>
#include "Operators.h"
#include "SyntaxAnalysis.h"

template<typename TNumber>
void PrintTree(const Operator<TNumber> &op, int depth) {
	for (int i = 0; i < depth * 4; i++) {
		std::cout << ' ';
	}

	std::cout << typeid(op).name() << std::endl;
	for (auto& operand : op.GetOperands()) {
		PrintTree(*operand, depth + 1);
	}
}

int main() {
	using namespace std;

	while (true) {
		string line;
		cout << "> ";
		getline(cin, line);

		CompilationContext<int> context;
		auto tokens = Lex(line, context);
		auto op = Parse(tokens, context);

		Evaluator<int> eval(&context);
		clock_t start = clock();
		op->Accept(eval);
		clock_t end = clock();
		cout << eval.value << endl
			<< "Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << endl
			<< endl;
	}

#if false
	auto zero = make_shared<ZeroOperator<int>>();
	auto three = make_shared<DecimalOperator<int>>(zero, 3);
	auto four = make_shared<DecimalOperator<int>>(zero, 4);
	auto add = make_shared<BinaryOperator<int>>(three, four, BinaryType::Add);
	auto cond = make_shared<ConditionalOperator<int>>(four, three, add);
	auto body = make_shared<BinaryOperator<int>>(make_shared<ArgumentOperator<int>>(0), make_shared<ArgumentOperator<int>>(1), BinaryType::Mult);

	CompilationContext<int> context;
	OperatorDefinition def("Add", 2);
	OperatorImplement<int> impl(def, body);
	context.AddOperatorImplement(impl);

	auto call = make_shared<UserDefinedOperator<int>>(def, std::vector<std::shared_ptr<Operator<int>>>({ three, four }));

	PrintTree(*cond, 0);
	cout << endl;

	Evaluator<int> eval(&context);
	eval.Visit(*call);
	cout << eval.value << endl;

	auto tokens = Lex("D[add|x,y|x+y] + (1 + 2)", context);
	auto op = Parse(tokens, context);
	PrintTree(*op, 0);
	op->Visit(eval);
	cout << eval.value << endl;
#endif
}
