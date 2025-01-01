/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#ifndef ENABLE_JIT
#error Jit compilation is not enabled. To use Jit feature, compile this file with defining "ENABLE_JIT"
#endif // !ENABLE_JIT

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
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
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"

#include "Exceptions.h"
#include "Jit.h"
#include "Operators.h"

namespace calc4
{
/* Explicit instantiation of "EvaluateByJIT" Function */
#define InstantiateEvaluateByJIT(TNumber, TInputSource, TPrinter)                                  \
    template TNumber EvaluateByJIT<TNumber, DefaultVariableSource<TNumber>,                        \
                                   DefaultGlobalArraySource<TNumber>, TInputSource, TPrinter>(     \
        const CompilationContext& context,                                                         \
        ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>, \
                       TInputSource, TPrinter>& state,                                             \
        const std::shared_ptr<const Operator>& op, const JITCodeGenerationOption& option)

InstantiateEvaluateByJIT(int32_t, DefaultInputSource, DefaultPrinter);
InstantiateEvaluateByJIT(int64_t, DefaultInputSource, DefaultPrinter);
InstantiateEvaluateByJIT(int32_t, BufferedInputSource, BufferedPrinter);
InstantiateEvaluateByJIT(int64_t, BufferedInputSource, BufferedPrinter);
InstantiateEvaluateByJIT(int32_t, StreamInputSource, StreamPrinter);
InstantiateEvaluateByJIT(int64_t, StreamInputSource, StreamPrinter);
#ifdef ENABLE_INT128
InstantiateEvaluateByJIT(__int128_t, DefaultInputSource, DefaultPrinter);
InstantiateEvaluateByJIT(__int128_t, BufferedInputSource, BufferedPrinter);
InstantiateEvaluateByJIT(__int128_t, StreamInputSource, StreamPrinter);
#endif // ENABLE_INT128

namespace
{
constexpr const char* MainFunctionName = "__[Main]__";
constexpr const char* EntryBlockName = "entry";
constexpr const char* GlobalVariableNamePrefix = "variable_";

template<typename TNumber>
size_t IntegerBits = sizeof(TNumber) * 8;

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void GenerateIR(
    const CompilationContext& context, const JITCodeGenerationOption& option,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state,
    const std::shared_ptr<const Operator>& op, llvm::LLVMContext* llvmContext,
    llvm::Module* llvmModule);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
class IRGenerator;

std::unordered_set<std::string> GatherVariableNames(const std::shared_ptr<const Operator>& op,
                                                    const CompilationContext& context);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void ThrowZeroDivisionException(void* state);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
int GetChar(void* state);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void PrintChar(void* state, char c);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber LoadVariable(void* state, const char* variableName);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void StoreVariable(void* state, const char* variableName, TNumber value);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber LoadArray(void* state, TNumber index);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void StoreArray(void* state, TNumber index, TNumber value);

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber EvaluateByJIT(
    const CompilationContext& context,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state,
    const std::shared_ptr<const Operator>& op, const JITCodeGenerationOption& option)
{
    using namespace llvm;
    LLVMContext Context;

    /* ***** Create module ***** */
    std::unique_ptr<Module> Owner = std::make_unique<Module>("calc4-jit-module", Context);
    Module* M = Owner.get();
    M->setTargetTriple(LLVM_HOST_TRIPLE);

    /* ***** Generate LLVM-IR ***** */
    GenerateIR<TNumber>(context, option, state, op, &Context, M);

    /* ***** Optimize ***** */
    if (option.optimize)
    {
        static const OptimizationLevel& OptLevel = OptimizationLevel::O3;
        static constexpr ThinOrFullLTOPhase LTOPhase = ThinOrFullLTOPhase::None;

        // Prepare PassBuilder
        PassBuilder PB;
        LoopAnalysisManager LAM;
        FunctionAnalysisManager FAM;
        CGSCCAnalysisManager CGAM;
        ModuleAnalysisManager MAM;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // We have to make a copy of M->functions because new functions may be added during
        // optimization.
        std::vector<Function*> functions;
        for (auto& func : M->functions())
        {
            functions.emplace_back(&func);
        }

        // Optimize each function
        FunctionPassManager FPM = PB.buildFunctionSimplificationPipeline(OptLevel, LTOPhase);
        for (auto func : functions)
        {
            FPM.run(*func, FAM);
        }

        // Optimize this module
        ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptLevel);
        MPM.run(*M, MAM);
    }

    if (option.dumpProgram)
    {
        // PrintIR
        outs() << "/*\n * LLVM IR\n */\n===============\n" << *M << "===============\n\n";
        outs().flush();
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

    auto func = (TNumber(*)(
        ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>*))
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
template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void GenerateIR(
    const CompilationContext& context, const JITCodeGenerationOption& option,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state,
    const std::shared_ptr<const Operator>& op, llvm::LLVMContext* llvmContext,
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

    /* ***** Gather variable names ***** */
    auto variableNames = GatherVariableNames(op, context);

    /* ***** Make main function ***** */
    llvm::FunctionType* funcType =
        llvm::FunctionType::get(integerType, { executionStateType }, false);
    llvm::Function* mainFunction = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                                          MainFunctionName, llvmModule);

    /* ***** Generate IR ****** */
    // Local helper function
    auto Emit = [llvmModule, llvmContext, &functionMap, &option,
                 &variableNames](llvm::Function* function,
                                 const std::shared_ptr<const Operator>& op, bool isMainFunction) {
        llvm::BasicBlock* block = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, function);
        auto builder = std::make_shared<llvm::IRBuilder<>>(block);

        IRGenerator<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter> generator(
            llvmModule, llvmContext, function, builder, functionMap, option, variableNames,
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

struct InternalFunction
{
    llvm::FunctionType* type;
    llvm::Value* address;
};

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
class IRGeneratorBase : public OperatorVisitor
{
protected:
    llvm::Module* module;
    llvm::LLVMContext* context;
    llvm::Function* function;
    std::shared_ptr<llvm::IRBuilder<>> builder;
    std::unordered_map<std::string, llvm::Function*> functionMap;
    JITCodeGenerationOption option;
    const std::unordered_set<std::string>& variableNames;
    bool isMainFunction;

    InternalFunction throwZeroDivision, getChar, printChar, loadVariable, storeVariable, loadArray,
        storeArray;

public:
    IRGeneratorBase(llvm::Module* module, llvm::LLVMContext* context, llvm::Function* function,
                    const std::shared_ptr<llvm::IRBuilder<>>& builder,
                    const std::unordered_map<std::string, llvm::Function*>& functionMap,
                    const JITCodeGenerationOption& option,
                    const std::unordered_set<std::string>& variableNames, bool isMainFunction)
        : module(module), context(context), function(function), builder(builder),
          functionMap(functionMap), option(option), variableNames(variableNames),
          isMainFunction(isMainFunction)
    {
#define GET_LLVM_FUNCTION_TYPE(RETURN_TYPE, ...)                                                   \
    llvm::FunctionType::get(RETURN_TYPE, { __VA_ARGS__ }, false)

#define GET_LLVM_FUNCTION_ADDRESS(NAME)                                                            \
    this->builder->getIntN(                                                                        \
        IntegerBits<void*>,                                                                        \
        reinterpret_cast<uint64_t>(                                                                \
            &NAME<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>))

#define GET_INTERNAL_FUNCTION(NAME, RETURN_TYPE, ...)                                              \
    { GET_LLVM_FUNCTION_TYPE(RETURN_TYPE, __VA_ARGS__), GET_LLVM_FUNCTION_ADDRESS(NAME) }

        llvm::Type* voidType = llvm::Type::getVoidTy(*this->context);
        llvm::Type* voidPointerType = voidType->getPointerTo(0);
        llvm::Type* integerType = builder->getIntNTy(IntegerBits<TNumber>);
        llvm::Type* stringType = builder->getInt8Ty()->getPointerTo(0);

        throwZeroDivision = GET_INTERNAL_FUNCTION(
            ThrowZeroDivisionException, llvm::Type::getVoidTy(*this->context), { voidPointerType });

        getChar = GET_INTERNAL_FUNCTION(GetChar, this->builder->getInt32Ty(), { voidPointerType });
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

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
class IRGenerator
    : public IRGeneratorBase<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>
{
private:
    llvm::Value* value;

public:
    using IRGeneratorBase<TNumber, TVariableSource, TGlobalArraySource, TInputSource,
                          TPrinter>::IRGeneratorBase;

    virtual void BeginFunction() override
    {
        if (this->isMainFunction)
        {
            // If this is the main function, we need to exchange variables with TVariableSource. We
            // restore all variables at the beginning and pass the modified ones at the end.

            for (auto& variableName : this->variableNames)
            {
                llvm::GlobalVariable* variableNameStr =
                    this->builder->CreateGlobalString(variableName);
                llvm::GlobalVariable* variable = GetGlobalVariable(variableName);

                // Restore the value from TVariableSource
                auto value = CallInternalFunction(
                    this->loadVariable, { &*this->function->arg_begin(), variableNameStr },
                    this->builder.get());

                // Store it in the JITed global variable
                this->builder->CreateStore(value, variable);
            }
        }
    }

    virtual void EndFunction() override
    {
        if (this->isMainFunction)
        {
            for (auto& variableName : this->variableNames)
            {
                llvm::GlobalVariable* variableNameStr =
                    this->builder->CreateGlobalString(variableName);
                llvm::GlobalVariable* variable = GetGlobalVariable(variableName);

                // Load value from the JITed global variable
                auto value = this->builder->CreateLoad(GetIntegerType(), variable);

                // Store it in TVariableSource
                CallInternalFunction(this->storeVariable,
                                     { &*this->function->arg_begin(), variableNameStr, value },
                                     this->builder.get());
            }
        }

        this->builder->CreateRet(this->value);
    }

    virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
    }

    virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
    {
        this->value = llvm::ConstantInt::getSigned(GetIntegerType(), op->GetValue<TNumber>());
    }

    virtual void Visit(const std::shared_ptr<const OperandOperator>& op) override
    {
        auto it = this->function->arg_begin();
        it++; // ExecutionState
        for (int i = 0; i < op->GetIndex(); i++)
        {
            ++it;
        }

        this->value = &*it;
    }

    virtual void Visit(const std::shared_ptr<const DefineOperator>& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
    }

    virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
    {
        auto variable = GetGlobalVariable(op->GetVariableName());
        this->value = this->builder->CreateLoad(GetIntegerType(), variable);
    };

    virtual void Visit(const std::shared_ptr<const InputOperator>& op) override
    {
        auto character = CallInternalFunction(this->getChar, { &*this->function->arg_begin() },
                                              this->builder.get());
        if (this->GetIntegerType()->isIntegerTy(IntegerBits<int>))
        {
            this->value = character;
        }
        else
        {
            this->value = this->builder->CreateSExt(character,
                                                    this->builder->getIntNTy(IntegerBits<TNumber>));
        }
    };

    virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
    {
        op->GetIndex()->Accept(*this);
        auto index = value;
        this->value = CallInternalFunction(
            this->loadArray, { &*this->function->arg_begin(), index }, this->builder.get());
    };

    virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
    {
        op->GetCharacter()->Accept(*this);
        auto casted = this->builder->CreateTrunc(value, llvm::Type::getInt8Ty(*this->context));
        CallInternalFunction(this->printChar, { &*this->function->arg_begin(), casted },
                             this->builder.get());
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
    };

    virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
    {
        this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
        for (auto& item : op->GetOperators())
        {
            item->Accept(*this);
        }
    }

    virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) override
    {
        op->GetOperand()->Accept(*this);
        auto operand = this->value;

        auto multed =
            this->builder->CreateMul(operand, this->builder->getIntN(IntegerBits<TNumber>, 10));
        this->value = this->builder->CreateAdd(
            multed, this->builder->getIntN(IntegerBits<TNumber>, op->GetValue()));
    }

    virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
    {
        op->GetOperand()->Accept(*this);
        auto variable = GetGlobalVariable(op->GetVariableName());
        this->builder->CreateStore(this->value, variable);
    }

    virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
    {
        op->GetValue()->Accept(*this);
        auto valueToBeStored = value;

        op->GetIndex()->Accept(*this);
        auto index = value;

        CallInternalFunction(this->storeArray,
                             { &*this->function->arg_begin(), index, valueToBeStored },
                             this->builder.get());
        this->value = valueToBeStored;
    }

    virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) override
    {
        op->GetLeft()->Accept(*this);
        auto left = this->value;
        op->GetRight()->Accept(*this);
        auto right = this->value;

        switch (op->GetType())
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
        case BinaryType::Mod:
        {
            // We share the same logic among Div and Mod operations

            if (this->option.checkZeroDivision)
            {
                llvm::BasicBlock* whenDivisorIsZero =
                    llvm::BasicBlock::Create(*this->context, "", this->function);

                llvm::BasicBlock* divisionCore =
                    llvm::BasicBlock::Create(*this->context, "", this->function);

                // Branch if divisor is zero
                auto cond = this->builder->CreateICmpEQ(
                    right, this->builder->getIntN(IntegerBits<TNumber>, 0));
                this->builder->CreateCondBr(cond, whenDivisorIsZero, divisionCore);

                // Code generation for when divisor is zero
                {
                    std::unique_ptr<llvm::IRBuilder<>> whenDivisorIsZeroBuilder =
                        std::make_unique<llvm::IRBuilder<>>(whenDivisorIsZero);
                    CallInternalFunction(this->throwZeroDivision, { &*this->function->arg_begin() },
                                         whenDivisorIsZeroBuilder.get());
                    whenDivisorIsZeroBuilder->CreateUnreachable();
                }

                // Code generation for when divisor is not zero
                std::shared_ptr<llvm::IRBuilder<>> divisionCoreBuilder =
                    std::make_shared<llvm::IRBuilder<>>(divisionCore);
                this->builder = std::move(divisionCoreBuilder);
            }

            if (op->GetType() == BinaryType::Div)
            {
                this->value = this->builder->CreateSDiv(left, right);
            }
            else
            {
                this->value = this->builder->CreateSRem(left, right);
            }

            break;
        }
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

    virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
    {
        /* ***** Evaluate condition expression ***** */
        llvm::Value* temp =
            this->builder->CreateAlloca(this->builder->getIntNTy(IntegerBits<TNumber>));
        op->GetCondition()->Accept(*this);
        llvm::Value* cond = this->builder->CreateSelect(
            this->builder->CreateICmpNE(this->value,
                                        this->builder->getIntN(IntegerBits<TNumber>, 0)),
            this->builder->getInt1(1), this->builder->getInt1(0));

        /* ***** Generate if-true and if-false codes ***** */
        auto oldBuilder = this->builder;
        auto Core = [this, temp](llvm::BasicBlock* block,
                                 const std::shared_ptr<const Operator>& op) {
            auto builder = std::make_shared<llvm::IRBuilder<>>(block);
            IRGenerator<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>
                generator(this->module, this->context, this->function, builder, this->functionMap,
                          this->option, this->variableNames, this->isMainFunction);
            op->Accept(generator);
            generator.builder->CreateStore(generator.value, temp);
            return (this->builder = generator.builder);
        };

        llvm::BasicBlock* ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
        auto ifTrueBuilder = Core(ifTrue, op->GetIfTrue());
        llvm::BasicBlock* ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
        auto ifFalseBuilder = Core(ifFalse, op->GetIfFalse());

        /* ***** Emit branch operation ***** */
        llvm::BasicBlock* finalBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
        this->builder = std::make_shared<llvm::IRBuilder<>>(finalBlock);

        oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
        ifTrueBuilder->CreateBr(finalBlock);
        ifFalseBuilder->CreateBr(finalBlock);
        this->value = this->builder->CreateLoad(this->GetIntegerType(), temp);
    }

    virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
    {
        std::vector<llvm::Value*> arguments(op->GetOperands().size() + 1 /* ExecutionState */);
        arguments[0] = &*this->function->arg_begin();

        auto operands = op->GetOperands();
        for (size_t i = 0; i < operands.size(); i++)
        {
            operands[i]->Accept(*this);
            arguments[i + 1] = this->value;
        }

        this->value =
            this->builder->CreateCall(this->functionMap[op->GetDefinition().GetName()], arguments);
    }

private:
    llvm::CallInst* CallInternalFunction(const InternalFunction& func,
                                         llvm::ArrayRef<llvm::Value*> arguments,
                                         llvm::IRBuilder<>* builder)
    {
        auto functionPtr = builder->CreateCast(llvm::Instruction::CastOps::IntToPtr, func.address,
                                               llvm::PointerType::get(func.type, 0));
        return builder->CreateCall(func.type, functionPtr, arguments);
    }

    llvm::GlobalVariable* GetGlobalVariable(const std::string& variableName)
    {
        // First, try to find the global variable from our module
        std::string actualVariableName = GlobalVariableNamePrefix + variableName;
        auto variable = this->module->getGlobalVariable(actualVariableName);
        if (variable != nullptr)
        {
            return variable;
        }

        // If it does not exist, create a new one
        variable = new llvm::GlobalVariable(*this->module, GetIntegerType(), false,
                                            llvm::GlobalVariable::LinkageTypes::CommonLinkage,
                                            llvm::ConstantInt::get(GetIntegerType(), 0),
                                            actualVariableName);
        return variable;
    }

    llvm::Type* GetIntegerType() const
    {
        return this->builder->getIntNTy(IntegerBits<TNumber>);
    }
};

void GatherVariableNamesCore(const std::shared_ptr<const Operator>& op,
                             const CompilationContext& context,
                             std::unordered_set<std::string>& set,
                             std::unordered_set<std::string>& visitedUserDefinedOperators)
{
    if (auto userDefined = dynamic_cast<const UserDefinedOperator*>(op.get()))
    {
        auto& name = userDefined->GetDefinition().GetName();

        if (visitedUserDefinedOperators.find(name) != visitedUserDefinedOperators.end())
        {
            visitedUserDefinedOperators.insert(name);
            auto& implement = context.GetOperatorImplement(name);
            GatherVariableNamesCore(implement.GetOperator(), context, set,
                                    visitedUserDefinedOperators);
        }
    }
    else if (auto parenthesis = dynamic_cast<const ParenthesisOperator*>(op.get()))
    {
        for (auto& op2 : parenthesis->GetOperators())
        {
            GatherVariableNamesCore(op2, context, set, visitedUserDefinedOperators);
        }
    }
    else if (auto loadVariable = dynamic_cast<const LoadVariableOperator*>(op.get()))
    {
        set.insert(loadVariable->GetVariableName());
    }
    else if (auto storeVariable = dynamic_cast<const StoreVariableOperator*>(op.get()))
    {
        set.insert(storeVariable->GetVariableName());
    }

    for (auto& operand : op->GetOperands())
    {
        GatherVariableNamesCore(operand, context, set, visitedUserDefinedOperators);
    }
}

std::unordered_set<std::string> GatherVariableNames(const std::shared_ptr<const Operator>& op,
                                                    const CompilationContext& context)
{
    std::unordered_set<std::string> variableNames;
    std::unordered_set<std::string> visitedUserDefinedOperators;
    GatherVariableNamesCore(op, context, variableNames, visitedUserDefinedOperators);
    return variableNames;
}
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void ThrowZeroDivisionException(void* state)
{
#ifdef _MSC_VER
    // TODO: On Windows systems, there is a problem where exceptions thrown in the Jitted functions
    // will not be properly handled by the caller. For the time being, we terminate process
    // immediately if some error occurs.
    std::cout << "Error: " << Exceptions::ZeroDivisionException(std::nullopt).what() << std::endl
              << "The program will be terminated immediately." << std::endl;
    exit(EXIT_FAILURE);
#else
    throw Exceptions::ZeroDivisionException(std::nullopt);
#endif // _MSC_VER
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
int GetChar(void* state)
{
    return reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource,
                                           TInputSource, TPrinter>*>(state)
        ->GetChar();
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void PrintChar(void* state, char c)
{
    reinterpret_cast<
        ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>*>(
        state)
        ->PrintChar(c);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber LoadVariable(void* state, const char* variableName)
{
    return reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource,
                                           TInputSource, TPrinter>*>(state)
        ->GetVariableSource()
        .Get(variableName);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void StoreVariable(void* state, const char* variableName, TNumber value)
{
    reinterpret_cast<
        ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>*>(
        state)
        ->GetVariableSource()
        .Set(variableName, value);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber LoadArray(void* state, TNumber index)
{
    return reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource,
                                           TInputSource, TPrinter>*>(state)
        ->GetArraySource()
        .Get(index);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
void StoreArray(void* state, TNumber index, TNumber value)
{
    return reinterpret_cast<ExecutionState<TNumber, TVariableSource, TGlobalArraySource,
                                           TInputSource, TPrinter>*>(state)
        ->GetArraySource()
        .Set(index, value);
}
}
