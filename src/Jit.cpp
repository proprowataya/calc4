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
#define InstantiateEvaluateByJIT(TNumber, TPrinter)                                                \
    template TNumber EvaluateByJIT<TNumber, DefaultVariableSource<TNumber>,                        \
                                   DefaultGlobalArraySource<TNumber>, TPrinter>(                   \
        const CompilationContext& context,                                                         \
        ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>, \
                       TPrinter>& state,                                                           \
        const std::shared_ptr<Operator>& op, bool optimize, bool printInfo)

InstantiateEvaluateByJIT(int32_t, DefaultPrinter);
InstantiateEvaluateByJIT(int64_t, DefaultPrinter);
InstantiateEvaluateByJIT(__int128_t, DefaultPrinter);
InstantiateEvaluateByJIT(int32_t, BufferedPrinter);
InstantiateEvaluateByJIT(int64_t, BufferedPrinter);
InstantiateEvaluateByJIT(__int128_t, BufferedPrinter);

namespace
{
constexpr const char* MainFunctionName = "__[Main]__";
constexpr const char* EntryBlockName = "entry";

template<typename TNumber>
size_t IntegerBits = sizeof(TNumber) * 8;

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void GenerateIR(const CompilationContext& context,
                ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                const std::shared_ptr<Operator>& op, llvm::LLVMContext* llvmContext,
                llvm::Module* llvmModule);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
class IRGenerator;
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void PrintChar(void* state, char c);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber LoadVariable(void* state, const char* variableName);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void StoreVariable(void* state, const char* variableName, TNumber value);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber LoadArray(void* state, TNumber index);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void StoreArray(void* state, TNumber index, TNumber value);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber EvaluateByJIT(const CompilationContext& context,
                      ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                      const std::shared_ptr<Operator>& op, bool optimize, bool printInfo)
{
    using namespace llvm;
    LLVMContext Context;

    /* ***** Create module ***** */
    std::unique_ptr<Module> Owner = std::make_unique<Module>("calc4-jit-module", Context);
    Module* M = Owner.get();
    M->setTargetTriple(LLVM_HOST_TRIPLE);

    /* ***** Generate LLVM-IR ***** */
    GenerateIR<TNumber>(context, state, op, &Context, M);

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

    auto func =
        (TNumber(*)(ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*))
            EE->getFunctionAddress(MainFunctionName);

    if (!error.empty())
    {
        throw error;
    }

    TNumber result = func(&state);
    delete EE;
    return result;
}

namespace
{
template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void GenerateIR(const CompilationContext& context,
                ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                const std::shared_ptr<Operator>& op, llvm::LLVMContext* llvmContext,
                llvm::Module* llvmModule)
{
    /* ***** Initialize variables ***** */
    llvm::Type* integerType = llvm::Type::getIntNTy(*llvmContext, IntegerBits<TNumber>);
    llvm::Type* usedDefinedReturnType = integerType;
    llvm::Type* executionStateType = llvm::Type::getVoidTy(*llvmContext)->getPointerTo(0);

    /* ***** Make function map (operator's name -> LLVM function) and the functions ***** */
    std::unordered_map<std::string, llvm::Function*> functionMap;
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& definition = it->second.GetDefinition();

        // Make arguments
        //  - The first argument is ExecutionState, and the rests are integers
        std::vector<llvm::Type*> argumentTypes(definition.GetNumOperands() + 1);
        argumentTypes[0] = executionStateType;
        std::fill(argumentTypes.begin() + 1, argumentTypes.end(), integerType);

        llvm::FunctionType* functionType =
            llvm::FunctionType::get(usedDefinedReturnType, argumentTypes, false);
        functionMap[definition.GetName()] = llvm::Function::Create(
            functionType, llvm::Function::ExternalLinkage, definition.GetName(), llvmModule);
    }

    /* ***** Make main function ***** */
    llvm::FunctionType* funcType =
        llvm::FunctionType::get(integerType, { executionStateType }, false);
    llvm::Function* mainFunction = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                                          MainFunctionName, llvmModule);

    /* ***** Generate IR ****** */
    // Local helper function
    auto Emit = [llvmModule, llvmContext, &functionMap](llvm::Function* function,
                                                        const std::shared_ptr<Operator>& op,
                                                        bool isMainFunction) {
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, function);
        auto builder = std::make_shared<llvm::IRBuilder<>>(block);

        IRGenerator<TNumber, TVariableSource, TGlobalArraySource, TPrinter> generator(
            llvmModule, llvmContext, function, builder, functionMap, isMainFunction);
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

struct InternalFunction
{
    llvm::FunctionType* type;
    llvm::Value* address;
};

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
class IRGeneratorBase : public OperatorVisitor
{
protected:
    llvm::Module* module;
    llvm::LLVMContext* context;
    llvm::Function* function;
    std::shared_ptr<llvm::IRBuilder<>> builder;
    std::unordered_map<std::string, llvm::Function*> functionMap;
    bool isMainFunction;

    InternalFunction printChar, loadVariable, storeVariable, loadArray, storeArray;

public:
    IRGeneratorBase(llvm::Module* module, llvm::LLVMContext* context, llvm::Function* function,
                    const std::shared_ptr<llvm::IRBuilder<>>& builder,
                    const std::unordered_map<std::string, llvm::Function*>& functionMap,
                    bool isMainFunction)
        : module(module), context(context), function(function), builder(builder),
          functionMap(functionMap), isMainFunction(isMainFunction)
    {
#define GET_LLVM_FUNCTION_TYPE(RETURN_TYPE, ...)                                                   \
    llvm::FunctionType::get(RETURN_TYPE, { __VA_ARGS__ }, false)

#define GET_LLVM_FUNCTION_ADDRESS(NAME)                                                            \
    this->builder->getIntN(                                                                        \
        IntegerBits<void*>,                                                                        \
        reinterpret_cast<uint64_t>(&NAME<TNumber, TVariableSource, TGlobalArraySource, TPrinter>))

#define GET_INTERNAL_FUNCTION(NAME, RETURN_TYPE, ...)                                              \
    { GET_LLVM_FUNCTION_TYPE(RETURN_TYPE, __VA_ARGS__), GET_LLVM_FUNCTION_ADDRESS(NAME) }

        llvm::Type* voidType = llvm::Type::getVoidTy(*this->context);
        llvm::Type* voidPointerType = voidType->getPointerTo(0);
        llvm::Type* integerType = builder->getIntNTy(IntegerBits<TNumber>);
        llvm::Type* stringType = builder->getInt8Ty()->getPointerTo(0);

        printChar = GET_INTERNAL_FUNCTION(PrintChar, llvm::Type::getVoidTy(*this->context),
                                          { voidPointerType, this->builder->getInt8Ty() });
        loadVariable =
            GET_INTERNAL_FUNCTION(LoadVariable, integerType, { voidPointerType, stringType });
        storeVariable = GET_INTERNAL_FUNCTION(StoreVariable, voidType,
                                              { voidPointerType, stringType, integerType });
        loadArray = GET_INTERNAL_FUNCTION(LoadArray, integerType, { voidPointerType, integerType });
        storeArray = GET_INTERNAL_FUNCTION(StoreArray, voidType,
                                           { voidPointerType, integerType, integerType });
    }

    virtual void BeginFunction() = 0;
    virtual void EndFunction() = 0;
};

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
class IRGenerator : public IRGeneratorBase<TNumber, TVariableSource, TGlobalArraySource, TPrinter>
{
private:
    llvm::Value* value;

public:
    using IRGeneratorBase<TNumber, TVariableSource, TGlobalArraySource, TPrinter>::IRGeneratorBase;

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
        it++; // ExecutionState
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
        llvm::GlobalVariable* variableName =
            this->builder->CreateGlobalString(op.GetVariableName());
        this->value = this->builder->CreateCall(this->loadVariable.type, this->loadVariable.address,
                                                { &*this->function->arg_begin(), variableName });
    };

    virtual void Visit(const LoadArrayOperator& op) override
    {
        op.GetIndex()->Accept(*this);
        auto index = value;
        this->value = this->builder->CreateCall(this->loadArray.type, this->loadArray.address,
                                                { &*this->function->arg_begin(), index });
    };

    virtual void Visit(const PrintCharOperator& op) override
    {
        op.GetCharacter()->Accept(*this);
        auto casted = this->builder->CreateTrunc(value, llvm::Type::getInt8Ty(*this->context));
        this->builder->CreateCall(this->printChar.type, this->printChar.address,
                                  { &*this->function->arg_begin(), casted });
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
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
        op.GetOperand()->Accept(*this);
        llvm::GlobalVariable* variableName =
            this->builder->CreateGlobalString(op.GetVariableName());
        this->builder->CreateCall(this->storeVariable.type, this->storeVariable.address,
                                  { &*this->function->arg_begin(), variableName, value });
    }

    virtual void Visit(const StoreArrayOperator& op) override
    {
        op.GetValue()->Accept(*this);
        auto valueToBeStored = value;

        op.GetIndex()->Accept(*this);
        auto index = value;

        this->builder->CreateCall(this->storeArray.type, this->storeArray.address,
                                  { &*this->function->arg_begin(), index, valueToBeStored });
        this->value = valueToBeStored;
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
            IRGenerator<TNumber, TVariableSource, TGlobalArraySource, TPrinter> generator(
                this->module, this->context, this->function, builder, this->functionMap,
                this->isMainFunction);
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
        std::vector<llvm::Value*> arguments(op.GetOperands().size() + 1 /* ExecutionState */);
        arguments[0] = &*this->function->arg_begin();

        auto operands = op.GetOperands();
        for (size_t i = 0; i < operands.size(); i++)
        {
            operands[i]->Accept(*this);
            arguments[i + 1] = this->value;
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

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void PrintChar(void* state, char c)
{
    reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*>(state)
        ->PrintChar(c);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber LoadVariable(void* state, const char* variableName)
{
    return reinterpret_cast<
               ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*>(state)
        ->GetVariableSource()
        .Get(variableName);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void StoreVariable(void* state, const char* variableName, TNumber value)
{
    reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*>(state)
        ->GetVariableSource()
        .Set(variableName, value);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber LoadArray(void* state, TNumber index)
{
    return reinterpret_cast<
               ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*>(state)
        ->GetArraySource()
        .Get(index);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
void StoreArray(void* state, TNumber index, TNumber value)
{
    return reinterpret_cast<
               ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>*>(state)
        ->GetArraySource()
        .Set(index, value);
}
