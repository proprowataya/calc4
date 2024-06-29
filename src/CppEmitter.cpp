/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2024 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "CppEmitter.h"
#include "Common.h"
#include <algorithm>
#include <malloc.h>
#include <set>
#include <stack>
#include <string_view>
#include <vector>

#ifdef ENABLE_GMP
#include <gmpxx.h>
#endif // ENABLE_GMP

using namespace std::string_view_literals;

namespace calc4
{
#define InstantiateEmitCppCode(TNumber)                                                            \
    template void EmitCppCode<TNumber>(const std::shared_ptr<const Operator>& op,                  \
                                       const CompilationContext& context, std::ostream& ostream)

InstantiateEmitCppCode(int32_t);
InstantiateEmitCppCode(int64_t);

#ifdef ENABLE_INT128
InstantiateEmitCppCode(__int128_t);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
InstantiateEmitCppCode(mpz_class);
#endif // ENABLE_GMP

namespace
{
constexpr std::string_view IndentText = "    "sv;
constexpr std::string_view MainOperatorName = "main_operator"sv;

/*****/

struct OperatorInformation
{
    OperatorDefinition definition;
    std::shared_ptr<const Operator> op;
    bool isMain;

    OperatorInformation(const OperatorDefinition& definition,
                        const std::shared_ptr<const Operator>& op, bool isMain)
        : definition(definition), op(op), isMain(isMain)
    {
    }
};

/*****/

constexpr std::string_view OperatorEntryLabel()
{
    return "Entry"sv;
}

constexpr std::string_view MemoryFieldName()
{
    return "Memory"sv;
}

constexpr std::string_view PrintFunctionName()
{
    return "Print"sv;
}

template<typename TNumber>
constexpr std::string_view TypeName()
{
    if constexpr (std::is_same_v<TNumber, int32_t>)
    {
        return "int32_t"sv;
    }
    else if constexpr (std::is_same_v<TNumber, int64_t>)
    {
        return "int64_t"sv;
    }
#ifdef ENABLE_INT128
    else if constexpr (std::is_same_v<TNumber, __int128_t>)
    {
        return "__int128_t"sv;
    }
#endif // ENABLE_INT128
#ifdef ENABLE_GMP
    else if constexpr (std::is_same_v<TNumber, mpz_class>)
    {
        return "mpz_class"sv;
    }
#endif // ENABLE_GMP
    else
    {
        static_assert([] { return false; }(), "Unsupported integer type");
    }
}

struct VariableName
{
    int no;
    VariableName(int no) : no(no) {}
};

std::ostream& operator<<(std::ostream& ostream, VariableName name)
{
    return ostream << "var_" << name.no;
}

struct UserDefinedVariableName
{
    std::string_view name;
    UserDefinedVariableName(std::string_view name) : name(name) {}
};

std::ostream& operator<<(std::ostream& ostream, UserDefinedVariableName name)
{
    return ostream << "user_defined_var_" << name.name;
}

struct ArgumentName
{
    int no;
    ArgumentName(int no) : no(no) {}
};

std::ostream& operator<<(std::ostream& ostream, ArgumentName name)
{
    return ostream << "arg_" << name.no;
}

struct UserDefinedOperatorName
{
    const OperatorDefinition& definition;
    UserDefinedOperatorName(const OperatorDefinition& definition) : definition(definition) {}
};

std::ostream& operator<<(std::ostream& ostream, const UserDefinedOperatorName& name)
{
    return ostream << "user_defined_operator_" << name.definition.GetName();
}

template<typename TNumber>
class Visitor : public OperatorVisitor
{
private:
    const OperatorDefinition& definition;
    const CompilationContext& context;
    std::ostream& os;
    int indent;
    int lastVariableNo = -1;
    std::stack<int> stack;

    std::ostream& Append()
    {
        for (int i = 0; i < indent; i++)
        {
            os << IndentText;
        }

        return os;
    }

    void AppendVariableDeclarationBegin()
    {
        Append() << TypeName<TNumber>() << " " << VariableName(++lastVariableNo) << " = ";
    }

    void AppendVariableDeclarationEnd()
    {
        os << ';' << std::endl;
    }

    void Return(std::optional<int> result = std::nullopt)
    {
        stack.push(result.value_or(lastVariableNo));
    }

    int ProcessOperator(const std::shared_ptr<const Operator>& op)
    {
        op->Accept(*this);
        int result = stack.top();
        stack.pop();
        return result;
    }

public:
    Visitor(const OperatorDefinition& definition, const CompilationContext& context,
            std::ostream& os, int indent)
        : definition(definition), context(context), os(os), indent(indent)
    {
    }

    void AppendReturn()
    {
        int variableNo = stack.top();
        stack.pop();
        assert(stack.empty());

        if (variableNo >= 0)
        {
            Append() << "return " << VariableName(variableNo) << ';' << std::endl;
        }
    }

    virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) override
    {
        AppendVariableDeclarationBegin();
        os << '0';
        AppendVariableDeclarationEnd();
        Return();
    }

    virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
    {
        AppendVariableDeclarationBegin();
        os << op->GetValue<TNumber>();
        AppendVariableDeclarationEnd();
        Return();
    }

    virtual void Visit(const std::shared_ptr<const OperandOperator>& op) override
    {
        AppendVariableDeclarationBegin();
        os << ArgumentName(op->GetIndex());
        AppendVariableDeclarationEnd();
        Return();
    };

    virtual void Visit(const std::shared_ptr<const DefineOperator>& op) override
    {
        AppendVariableDeclarationBegin();
        os << '0';
        AppendVariableDeclarationEnd();
        Return();
    };

    virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
    {
        AppendVariableDeclarationBegin();
        os << UserDefinedVariableName(op->GetVariableName());
        AppendVariableDeclarationEnd();
        Return();
    };

    virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
    {
        int index = ProcessOperator(op->GetIndex());
        AppendVariableDeclarationBegin();
        os << MemoryFieldName() << '[' << VariableName(index) << ']';
        AppendVariableDeclarationEnd();
        Return();
    };

    virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
    {
        int character = ProcessOperator(op->GetCharacter());
        Append() << PrintFunctionName() << '(' << VariableName(character) << ");" << std::endl;
        AppendVariableDeclarationBegin();
        os << '0';
        AppendVariableDeclarationEnd();
        Return();
    };

    virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
    {
        if (op->GetOperators().empty())
        {
            AppendVariableDeclarationBegin();
            os << '0';
            AppendVariableDeclarationEnd();
            Return();
        }
        else
        {
            int variableNo;

            for (auto& item : op->GetOperators())
            {
                variableNo = ProcessOperator(item);
            }

            if (variableNo >= 0)
            {
                AppendVariableDeclarationBegin();
                os << VariableName(variableNo);
                AppendVariableDeclarationEnd();
            }

            Return(variableNo);
        }
    }

    virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) override
    {
        int operand = ProcessOperator(op->GetOperand());
        AppendVariableDeclarationBegin();
        os << VariableName(operand) << " * 10 + " << op->GetValue();
        AppendVariableDeclarationEnd();
        Return();
    }

    virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
    {
        int value = ProcessOperator(op->GetOperand());
        Append() << UserDefinedVariableName(op->GetVariableName()) << " = " << VariableName(value)
                 << ';' << std::endl;
        Return(value);
    }

    virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
    {
        int valueToBeStored = ProcessOperator(op->GetValue());
        int index = ProcessOperator(op->GetIndex());

        Append() << MemoryFieldName() << '[' << VariableName(index)
                 << "] = " << VariableName(valueToBeStored) << ';' << std::endl;
        Return(valueToBeStored);
    }

    virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) override
    {
        int left = ProcessOperator(op->GetLeft());
        int right = ProcessOperator(op->GetRight());

        const char* operatorChar;
        switch (op->GetType())
        {
        case BinaryType::Add:
            operatorChar = "+";
            break;
        case BinaryType::Sub:
            operatorChar = "-";
            break;
        case BinaryType::Mult:
            operatorChar = "*";
            break;
        case BinaryType::Div:
            operatorChar = "/";
            break;
        case BinaryType::Mod:
            operatorChar = "%";
            break;
        case BinaryType::Equal:
            operatorChar = "==";
            break;
        case BinaryType::NotEqual:
            operatorChar = "!=";
            break;
        case BinaryType::LessThan:
            operatorChar = "<";
            break;
        case BinaryType::LessThanOrEqual:
            operatorChar = "<=";
            break;
        case BinaryType::GreaterThanOrEqual:
            operatorChar = ">=";
            break;
        case BinaryType::GreaterThan:
            operatorChar = ">";
            break;
        default:
            UNREACHABLE();
            break;
        }

        AppendVariableDeclarationBegin();
        os << VariableName(left) << ' ' << operatorChar << ' ' << VariableName(right);
        switch (op->GetType())
        {
        case BinaryType::Equal:
        case BinaryType::NotEqual:
        case BinaryType::LessThan:
        case BinaryType::LessThanOrEqual:
        case BinaryType::GreaterThanOrEqual:
        case BinaryType::GreaterThan:
            os << " ? 1 : 0";
            break;
        default:
            break;
        }
        AppendVariableDeclarationEnd();
        Return();
    }

    virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
    {
        int condition = ProcessOperator(op->GetCondition());
        int result = ++lastVariableNo;

        auto ProcessBranch = [this, result](const std::shared_ptr<const Operator>& op) {
            indent++;
            int childResult = ProcessOperator(op);
            if (childResult >= 0)
            {
                Append() << VariableName(result) << " = " << VariableName(childResult) << ';'
                         << std::endl;
            }
            indent--;
        };

        Append() << TypeName<TNumber>() << ' ' << VariableName(result) << ';' << std::endl;
        Append() << "if (" << VariableName(condition) << " != 0)" << std::endl;
        Append() << '{' << std::endl;
        ProcessBranch(op->GetIfTrue());
        Append() << '}' << std::endl;
        Append() << "else" << std::endl;
        Append() << '{' << std::endl;
        ProcessBranch(op->GetIfFalse());
        Append() << '}' << std::endl;

        Return(result);
    };

    virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
    {
        int* operandVariableNos =
            reinterpret_cast<int*>(alloca(sizeof(int) * op->GetDefinition().GetNumOperands()));

        auto operands = op->GetOperands();
        for (size_t i = 0; i < operands.size(); i++)
        {
            operandVariableNos[i] = ProcessOperator(operands[i]);
        }

        if (op->IsTailCall().value_or(false) && op->GetDefinition() == this->definition)
        {
            for (int i = 0; i < static_cast<int>(operands.size()); i++)
            {
                Append() << ArgumentName(i) << " = " << VariableName(operandVariableNos[i]) << ';'
                         << std::endl;
            }

            Append() << "goto " << OperatorEntryLabel() << ';' << std::endl;
            Return(-1);
        }
        else
        {
            AppendVariableDeclarationBegin();
            os << UserDefinedOperatorName(op->GetDefinition()) << '(';
            for (size_t i = 0; i < operands.size(); i++)
            {
                if (i > 0)
                {
                    os << ", ";
                }

                os << VariableName(operandVariableNos[i]);
            }
            os << ')';
            AppendVariableDeclarationEnd();
            Return();
        }
    }
};

void GatherVariableNamesCore(const std::shared_ptr<const Operator>& op,
                             std::set<std::string_view>& result)
{
    if (auto loadVariable = dynamic_cast<const LoadVariableOperator*>(op.get()))
    {
        result.emplace(loadVariable->GetVariableName());
    }
    else if (auto storeVariable = dynamic_cast<const StoreVariableOperator*>(op.get()))
    {
        result.emplace(storeVariable->GetVariableName());
    }
    else if (auto parenthesis = dynamic_cast<const ParenthesisOperator*>(op.get()))
    {
        for (auto& child : parenthesis->GetOperators())
        {
            GatherVariableNamesCore(child, result);
        }
    }

    for (auto& operand : op->GetOperands())
    {
        GatherVariableNamesCore(operand, result);
    }
}

std::set<std::string_view> GatherVariableNames(const std::shared_ptr<const Operator>& op,
                                               const CompilationContext& context)
{
    std::set<std::string_view> result;

    GatherVariableNamesCore(op, result);
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        GatherVariableNamesCore(it->second.GetOperator(), result);
    }

    return result;
}

template<typename TNumber>
void EmitDeclaration(const OperatorInformation& info, std::ostream& ostream)
{
    ostream << TypeName<TNumber>() << ' ';

    if (info.isMain)
    {
        ostream << MainOperatorName;
    }
    else
    {
        ostream << UserDefinedOperatorName(info.definition);
    }

    ostream << '(';

    for (int i = 0; i < info.definition.GetNumOperands(); i++)
    {
        if (i > 0)
        {
            ostream << ", ";
        }

        ostream << TypeName<TNumber>() << ' ' << ArgumentName(i);
    }

    ostream << ')';
}

template<typename TNumber>
void EmitOperator(const OperatorInformation& info, const CompilationContext& context,
                  std::ostream& ostream)
{
    EmitDeclaration<TNumber>(info, ostream);
    ostream << std::endl;
    ostream << '{' << std::endl;
    ostream << OperatorEntryLabel() << ':' << std::endl;

    Visitor<TNumber> visitor(info.definition, context, ostream, 1);
    info.op->Accept(visitor);
    visitor.AppendReturn();

    ostream << '}' << std::endl;
};
}

template<typename TNumber>
void EmitCppCode(const std::shared_ptr<const Operator>& op, const CompilationContext& context,
                 std::ostream& ostream)
{
    /* Emit header */
    ostream << "#include <cstdint>" << std::endl;

#ifdef ENABLE_GMP
    if constexpr (std::is_same_v<TNumber, mpz_class>)
    {
        ostream << "#include <gmpxx.h>" << std::endl;
    }
#endif // ENABLE_GMP

    ostream << "#include <iostream>" << std::endl;
    ostream << "#include <unordered_map>" << std::endl;
    ostream << std::endl;
    ostream << "std::unordered_map<" << TypeName<TNumber>() << ", " << TypeName<TNumber>() << "> "
            << MemoryFieldName() << ';' << std::endl;
    ostream << std::endl;
    ostream << "void " << PrintFunctionName() << "(" << TypeName<TNumber>() << " value)"
            << std::endl;
    ostream << '{' << std::endl;
    ostream << IndentText << "std::cout << static_cast<char>(value);" << std::endl;
    ostream << '}' << std::endl;
    ostream << std::endl;

    /* Gather information of operators and sort */
    std::vector<OperatorInformation> infos;
    infos.emplace_back(OperatorDefinition(std::string(MainOperatorName), 0), op, true);

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        infos.emplace_back(it->second.GetDefinition(), it->second.GetOperator(), false);
    }

    std::sort(infos.begin() + 1, infos.end(), [](auto& left, auto& right) {
        return left.definition.GetName() < right.definition.GetName();
    });

    /* Emit forward declarations */
    for (auto& info : infos)
    {
        EmitDeclaration<TNumber>(info, ostream);
        ostream << ';' << std::endl;
    }

    ostream << std::endl;

    /* Gather variable names */
    std::set<std::string_view> variableNames = GatherVariableNames(op, context);

    /* Emit variable definitions */
    if (!variableNames.empty())
    {
        for (auto& variableName : variableNames)
        {
            ostream << TypeName<TNumber>() << ' ' << UserDefinedVariableName(variableName)
                    << " = 0;" << std::endl;
        }

        ostream << std::endl;
    }

    /* Emit main */
    ostream << "int main()" << std::endl;
    ostream << '{' << std::endl;
    ostream << IndentText << "std::cout << " << MainOperatorName << "() << std::endl;" << std::endl;
    ostream << '}' << std::endl << std::endl;

    /* Emit definitions */
    for (auto& info : infos)
    {
        if (!info.isMain)
        {
            ostream << std::endl;
        }

        EmitOperator<TNumber>(info, context, ostream);
    }
}
}
