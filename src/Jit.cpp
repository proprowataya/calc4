#include <algorithm>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "Error.h"
#include "Jit.h"
#include "Operators.h"

/* Explicit instantiation of "EvaluateByJIT" Function */
#define InstantiateEvaluateByJIT(TNumber)                                                          \
    template TNumber EvaluateByJIT<TNumber>(const CompilationContext& context,                     \
                                            const std::shared_ptr<Operator>& op, bool optimize,    \
                                            bool printInfo)
// InstantiateEvaluateByJIT(int8_t);
// InstantiateEvaluateByJIT(int16_t);
InstantiateEvaluateByJIT(int32_t);
InstantiateEvaluateByJIT(int64_t);
InstantiateEvaluateByJIT(__int128_t);

namespace
{
constexpr const char* MainFunctionName = "__[Main]__";
constexpr const char* EntryBlockName = "entry";

template<typename TNumber>
size_t IntegerBits = sizeof(TNumber) * 8;
template<typename TNumber>
void GenerateIR(const CompilationContext& context, const std::shared_ptr<Operator>& op,
                llvm::LLVMContext* llvmContext, llvm::Module* llvmModule);
template<typename TNumber>
class IRGenerator;
}

template<typename TNumber>
TNumber EvaluateByJIT(const CompilationContext& context, const std::shared_ptr<Operator>& op,
                      bool optimize, bool printInfo)
{
    using namespace llvm;
    LLVMContext Context;

    /* ***** Create module ***** */
    std::unique_ptr<Module> Owner = std::make_unique<Module>("calc4-jit-module", Context);
    Module* M = Owner.get();
    M->setTargetTriple(LLVM_HOST_TRIPLE);

    /* ***** Generate LLVM-IR ***** */
    GenerateIR<TNumber>(context, op, &Context, M);

    if (printInfo)
    {
        // PrintIR
        outs() << "LLVM IR (Before optimized):\n---------------------------\n"
               << *M << "---------------------------\n\n";
        outs().flush();
    }

    /* ***** Optimize ***** */
    if (optimize)
    {
        static constexpr int OptLevel = 3, SizeLevel = 0;

        legacy::PassManager PM;
        legacy::FunctionPassManager FPM(M);
        PassManagerBuilder PMB;
        PMB.OptLevel = OptLevel;
        PMB.SizeLevel = SizeLevel;
#if LLVM_VERSION_MAJOR >= 5
        PMB.Inliner = createFunctionInliningPass(OptLevel, SizeLevel, false);
#else
        PMB.Inliner = createFunctionInliningPass(OptLevel, SizeLevel);
#endif //  LLVM_VERSION_MAJOR >= 5
        PMB.populateFunctionPassManager(FPM);
        PMB.populateModulePassManager(PM);

        for (auto& func : *M)
        {
            FPM.run(func);
        }

        PM.run(*M);

        if (printInfo)
        {
            // PrintIR
            outs() << "LLVM IR (After optimized):\n---------------------------\n"
                   << *M << "---------------------------\n\n";
            outs().flush();
        }
    }

    /* ***** Execute JIT compiled code ***** */
    EngineBuilder ebuilder(std::move(Owner));
    std::string error;
    ebuilder.setErrorStr(&error).setEngineKind(EngineKind::Kind::JIT);
    ExecutionEngine* EE = ebuilder.create();

    if (!error.empty())
    {
        throw error;
    }

    auto func = (TNumber(*)())EE->getFunctionAddress(MainFunctionName);

    if (!error.empty())
    {
        throw error;
    }

    TNumber result = func();
    delete EE;
    return result;
}

namespace
{
template<typename TNumber>
void GenerateIR(const CompilationContext& context, const std::shared_ptr<Operator>& op,
                llvm::LLVMContext* llvmContext, llvm::Module* llvmModule)
{
    /* ***** Initialize variables ***** */
    llvm::Type* integerType = llvm::Type::getIntNTy(*llvmContext, IntegerBits<TNumber>);
    llvm::Type* usedDefinedReturnType = integerType;

    /* ***** Make function map (operator's name -> LLVM function) and the functions ***** */
    std::unordered_map<std::string, llvm::Function*> functionMap;
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& definition = it->second.GetDefinition();
        size_t numArguments = definition.GetNumOperands();

        // Make arguments
        std::vector<llvm::Type*> argumentTypes(numArguments);
        std::generate_n(argumentTypes.begin(), numArguments,
                        [integerType]() { return integerType; });

        llvm::FunctionType* functionType =
            llvm::FunctionType::get(usedDefinedReturnType, argumentTypes, false);
        functionMap[definition.GetName()] = llvm::Function::Create(
            functionType, llvm::Function::ExternalLinkage, definition.GetName(), llvmModule);
    }

    /* ***** Make main function ***** */
    llvm::FunctionType* funcType = llvm::FunctionType::get(integerType, {}, false);
    llvm::Function* mainFunction = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                                          MainFunctionName, llvmModule);

    /* ***** Generate IR ****** */
    // Local helper function
    auto Emit = [llvmModule, llvmContext, &functionMap](llvm::Function* function,
                                                        const std::shared_ptr<Operator>& op,
                                                        bool isMainFunction) {
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, function);
        auto builder = std::make_shared<llvm::IRBuilder<>>(block);

        IRGenerator<TNumber> generator(llvmModule, llvmContext, function, builder, functionMap,
                                       isMainFunction);
        generator.BeginFunction();
        op->Accept(generator);
        generator.EndFunction();
    };

    // Main function
    Emit(mainFunction, op, true);

    // User-defined operators
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& definition = it->second.GetDefinition();
        auto& name = definition.GetName();
        Emit(functionMap[name], context.GetOperatorImplement(name).GetOperator(), false);
    }
}

template<typename TNumber>
class IRGeneratorBase : public OperatorVisitor
{
protected:
    llvm::Module* module;
    llvm::LLVMContext* context;
    llvm::Function* function;
    std::shared_ptr<llvm::IRBuilder<>> builder;
    std::unordered_map<std::string, llvm::Function*> functionMap;
    bool isMainFunction;

public:
    IRGeneratorBase(llvm::Module* module, llvm::LLVMContext* context, llvm::Function* function,
                    const std::shared_ptr<llvm::IRBuilder<>>& builder,
                    const std::unordered_map<std::string, llvm::Function*>& functionMap,
                    bool isMainFunction)
        : module(module), context(context), function(function), builder(builder),
          functionMap(functionMap), isMainFunction(isMainFunction)
    {
    }

    virtual void BeginFunction() = 0;
    virtual void EndFunction() = 0;
};

template<typename TNumber>
class IRGenerator : public IRGeneratorBase<TNumber>
{
private:
    llvm::Value* value;

public:
    using IRGeneratorBase<TNumber>::IRGeneratorBase;

    virtual void BeginFunction() override {}

    virtual void EndFunction() override
    {
        this->builder->CreateRet(this->value);
    }

    virtual void Visit(const ZeroOperator& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
    }

    virtual void Visit(const PrecomputedOperator& op) override
    {
        this->value = llvm::ConstantInt::getSigned(GetIntegerType(), op.GetValue<TNumber>());
    }

    virtual void Visit(const OperandOperator& op) override
    {
        auto it = this->function->arg_begin();
        for (int i = 0; i < op.GetIndex(); i++)
        {
            ++it;
        }

        this->value = &*it;
    }

    virtual void Visit(const DefineOperator& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
    }

    virtual void Visit(const LoadVariableOperator& op) override
    {
        throw std::string("Not implemented");
    };

    virtual void Visit(const LoadArrayOperator& op) override
    {
        throw std::string("Not implemented");
    };

    virtual void Visit(const PrintCharOperator& op) override
    {
        throw std::string("Not implemented");
    };

    virtual void Visit(const ParenthesisOperator& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
        for (auto& item : op.GetOperators())
        {
            item->Accept(*this);
        }
    }

    virtual void Visit(const DecimalOperator& op) override
    {
        op.GetOperand()->Accept(*this);
        auto operand = this->value;

        auto multed =
            this->builder->CreateMul(operand, this->builder->getIntN(IntegerBits<TNumber>, 10));
        this->value = this->builder->CreateAdd(
            multed, this->builder->getIntN(IntegerBits<TNumber>, op.GetValue()));
    }

    virtual void Visit(const StoreVariableOperator& op) override
    {
        throw std::string("Not implemented");
    }

    virtual void Visit(const StoreArrayOperator& op) override
    {
        throw std::string("Not implemented");
    }

    virtual void Visit(const BinaryOperator& op) override
    {
        op.GetLeft()->Accept(*this);
        auto left = this->value;
        op.GetRight()->Accept(*this);
        auto right = this->value;

        switch (op.GetType())
        {
        case BinaryType::Add:
            this->value = this->builder->CreateAdd(left, right);
            break;
        case BinaryType::Sub:
            this->value = this->builder->CreateSub(left, right);
            break;
        case BinaryType::Mult:
            this->value = this->builder->CreateMul(left, right);
            break;
        case BinaryType::Div:
            this->value = this->builder->CreateSDiv(left, right);
            break;
        case BinaryType::Mod:
            this->value = this->builder->CreateSRem(left, right);
            break;
        case BinaryType::Equal:
        {
            auto cmp = this->builder->CreateICmpEQ(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        case BinaryType::NotEqual:
        {
            auto cmp = this->builder->CreateICmpNE(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        case BinaryType::LessThan:
        {
            auto cmp = this->builder->CreateICmpSLT(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        case BinaryType::LessThanOrEqual:
        {
            auto cmp = this->builder->CreateICmpSLE(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        case BinaryType::GreaterThanOrEqual:
        {
            auto cmp = this->builder->CreateICmpSGE(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        case BinaryType::GreaterThan:
        {
            auto cmp = this->builder->CreateICmpSGT(left, right);
            this->value =
                this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1),
                                            this->builder->getIntN(IntegerBits<TNumber>, 0));
            break;
        }
        default:
            UNREACHABLE();
            break;
        }
    }

    virtual void Visit(const ConditionalOperator& op) override
    {
        /* ***** Evaluate condition expression ***** */
        llvm::Value* temp =
            this->builder->CreateAlloca(this->builder->getIntNTy(IntegerBits<TNumber>));
        op.GetCondition()->Accept(*this);
        llvm::Value* cond = this->builder->CreateSelect(
            this->builder->CreateICmpNE(this->value,
                                        this->builder->getIntN(IntegerBits<TNumber>, 0)),
            this->builder->getInt1(1), this->builder->getInt1(0));

        /* ***** Generate if-true and if-false codes ***** */
        auto oldBuilder = this->builder;
        auto Core = [this, temp](llvm::BasicBlock* block, const std::shared_ptr<Operator>& op) {
            auto builder = std::make_shared<llvm::IRBuilder<>>(block);
            IRGenerator<TNumber> generator(this->module, this->context, this->function, builder,
                                           this->functionMap, this->isMainFunction);
            op->Accept(generator);
            generator.builder->CreateStore(generator.value, temp);
            return (this->builder = generator.builder);
        };

        llvm::BasicBlock* ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
        auto ifTrueBuilder = Core(ifTrue, op.GetIfTrue());
        llvm::BasicBlock* ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
        auto ifFalseBuilder = Core(ifFalse, op.GetIfFalse());

        /* ***** Emit branch operation ***** */
        llvm::BasicBlock* finalBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
        this->builder = std::make_shared<llvm::IRBuilder<>>(finalBlock);

        oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
        ifTrueBuilder->CreateBr(finalBlock);
        ifFalseBuilder->CreateBr(finalBlock);
        this->value = this->builder->CreateLoad(this->GetIntegerType(), temp);
    }

    virtual void Visit(const UserDefinedOperator& op) override
    {
        std::vector<llvm::Value*> arguments(op.GetOperands().size());
        auto operands = op.GetOperands();
        for (size_t i = 0; i < operands.size(); i++)
        {
            operands[i]->Accept(*this);
            arguments[i] = this->value;
        }

        this->value =
            this->builder->CreateCall(this->functionMap[op.GetDefinition().GetName()], arguments);
    }

private:
    llvm::Type* GetIntegerType() const
    {
        return this->builder->getIntNTy(IntegerBits<TNumber>);
    }
};
}
