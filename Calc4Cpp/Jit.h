#pragma once

#include "Operators.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LegacyPassManager.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>

constexpr const char *MainFunctionName = "__Main__";

template<typename TNumber>
class IRGenerator : public OperatorVisitor<TNumber> {
private:
    llvm::LLVMContext *context;
    llvm::IRBuilder<> *builder;
    llvm::Function *function;
    std::unordered_map<std::string, llvm::Function *> functionMap;
    static constexpr size_t IntegerBits = sizeof(TNumber) * 8;

public:
    llvm::Value *value;
    
    IRGenerator(llvm::LLVMContext *context, llvm::IRBuilder<> *builder, llvm::Function *function, const std::unordered_map<std::string, llvm::Function *> &functionMap)
        : context(context), builder(builder), function(function), functionMap(functionMap) {}

    virtual void Visit(const ZeroOperator<TNumber> &op) override {
        value = builder->getIntN(IntegerBits, 0);
    }

    virtual void Visit(const PreComputedOperator<TNumber> &op) override {
        value = builder->getIntN(IntegerBits, op.GetValue());
    }

    virtual void Visit(const ArgumentOperator<TNumber> &op) override {
        value = &function->arg_begin()[op.GetIndex()];
    }

    virtual void Visit(const DefineOperator<TNumber> &op) override {
        value = builder->getIntN(IntegerBits, 0);
    }

    virtual void Visit(const ParenthesisOperator<TNumber> &op) override {
        value = builder->getIntN(IntegerBits, 0);
        for (auto& item : op.GetOperands()) {
            item->Accept(*this);
        }
    }

    virtual void Visit(const DecimalOperator<TNumber> &op) override {
        op.GetOperand()->Accept(*this);
        auto operand = value;

        auto multed = builder->CreateMul(operand, builder->getIntN(IntegerBits, 10));
        value = builder->CreateAdd(multed, builder->getIntN(IntegerBits, op.GetValue()));
    }

    virtual void Visit(const BinaryOperator<TNumber> &op) override {
        op.GetLeft()->Accept(*this);
        auto left = value;
        op.GetRight()->Accept(*this);
        auto right = value;

        switch (op.GetType()) {
        case BinaryType::Add:
            value = builder->CreateAdd(left, right);
            break;
        case BinaryType::Sub:
            value = builder->CreateSub(left, right);
            break;
        case BinaryType::Mult:
            value = builder->CreateMul(left, right);
            break;
        case BinaryType::Div:
            value = builder->CreateSDiv(left, right);
            break;
        case BinaryType::Mod:
            value = builder->CreateSRem(left, right);
            break;
        case BinaryType::Equal:
        {
            auto cmp = builder->CreateICmpEQ(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        case BinaryType::NotEqual:
        {
            auto cmp = builder->CreateICmpNE(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        case BinaryType::LessThan:
        {
            auto cmp = builder->CreateICmpSLT(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        case BinaryType::LessThanOrEqual:
        {
            auto cmp = builder->CreateICmpSLE(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        case BinaryType::GreaterThanOrEqual:
        {
            auto cmp = builder->CreateICmpSGE(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        case BinaryType::GreaterThan:
        {
            auto cmp = builder->CreateICmpSGT(left, right);
            value = builder->CreateSelect(cmp, builder->getIntN(IntegerBits, 1), builder->getIntN(IntegerBits, 0));
            break;
        }
        default:
            break;
        }
    }

    virtual void Visit(const ConditionalOperator<TNumber> &op) override {
        op.GetCondition()->Accept(*this);
        auto cond = value;

        llvm::BasicBlock *ifTrue = llvm::BasicBlock::Create(*context, "", function);
        llvm::BasicBlock *ifFalse = llvm::BasicBlock::Create(*context, "", function);
        llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(*context, "", function);

        llvm::IRBuilder<> ifTrueBuilder(ifTrue);
        IRGenerator<TNumber> ifTrueGenerator(context, &ifTrueBuilder, function, functionMap);
        op.GetIfTrue()->Accept(ifTrueGenerator);
        ifTrueBuilder.CreateBr(nextBlock);
        auto ifTrueValue = ifTrueGenerator.value;

        llvm::IRBuilder<> ifFalseBuilder(ifFalse);
        IRGenerator<TNumber> ifFalseGenerator(context, &ifFalseBuilder, function, functionMap);
        op.GetIfFalse()->Accept(ifFalseGenerator);
        ifFalseBuilder.CreateBr(nextBlock);
        auto ifFalseValue = ifFalseGenerator.value;

        builder = new llvm::IRBuilder<>(nextBlock);
        llvm::PHINode *phiNode = builder->CreatePHI(builder->getIntNTy(IntegerBits), 2);
        phiNode->addIncoming(ifTrueValue, ifTrue);
        phiNode->addIncoming(ifFalseValue, ifFalse);

        value = phiNode;
    }

    virtual void Visit(const UserDefinedOperator<TNumber> &op) override {
        std::vector<llvm::Value *> arguments(op.GetOperands().size());
        auto operands = op.GetOperands();
        for (size_t i = 0; i < operands.size(); i++) {
            operands[i]->Accept(*this);
            arguments[i] = value;
        }

        value = builder->CreateCall(functionMap[op.GetDefinition().GetName()]);
    }
};

template<typename TNumber>
void GenerateIR(const CompilationContext<int> &context, const std::shared_ptr<Operator<TNumber>> &op, llvm::LLVMContext *llvmContext, llvm::Module *llvmModule) {
    static constexpr size_t IntegerBits = sizeof(TNumber) * 8;
    llvm::Type *IntegerType = llvm::Type::getIntNTy(*llvmContext, IntegerBits);

    std::unordered_map<std::string, llvm::Function *> functionMap;
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
        auto& definition = it->second.GetDefinition();
        std::vector<llvm::Type *> argumentTypes(definition.GetNumOperands());
        std::generate_n(argumentTypes.begin(), definition.GetNumOperands(), [&]() { return IntegerType; });
        llvm::FunctionType *functionType = llvm::FunctionType::get(IntegerType, argumentTypes, false);
        functionMap[definition.GetName()] = llvm::cast<llvm::Function>(llvmModule->getOrInsertFunction(definition.GetName(), functionType));
    }
    
    llvm::Function *mainFunc = llvm::cast<llvm::Function>(
        llvmModule->getOrInsertFunction(MainFunctionName, IntegerType));
    {
        llvm::BasicBlock *mainBlock = llvm::BasicBlock::Create(*llvmContext, "", mainFunc);
        llvm::IRBuilder<> builder(mainBlock);
        IRGenerator<TNumber> generator(llvmContext, &builder, mainFunc, functionMap);
        op->Accept(generator);
        builder.CreateRet(generator.value);
    }

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
        auto& definition = it->second.GetDefinition();
        llvm::Function *function = functionMap[definition.GetName()];
        llvm::BasicBlock *block = llvm::BasicBlock::Create(*llvmContext, "", function);
        llvm::IRBuilder<> builder(block);

        IRGenerator<TNumber> generator(llvmContext, &builder, function, functionMap);
        context.GetOperatorImplement(definition.GetName()).GetOperator()->Accept(generator);
        builder.CreateRet(generator.value);
    }
}

template<typename TNumber>
TNumber RunByJIT(const CompilationContext<int> &context, const std::shared_ptr<Operator<TNumber>> &op) {
    using namespace llvm;
    
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    LLVMContext Context;

    // Create some module to put our function into it.
    std::unique_ptr<Module> Owner = make_unique<Module>("test", Context);
    Module *M = Owner.get();

    // Generate IR
    GenerateIR(context, op, &Context, M);

    // PrintIR
    outs() << "Before optimized:\n\n" << *M << "\n";
    outs().flush();

    // Optimize
#if false
    legacy::PassManager PM;
    // mpm.add(createIPSCCPPass());
    PM.add(createFunctionInliningPass(OptLevel, SizeLevel, false));
    // mpm.add(createLICMPass());
    // mpm.add(createGVNPass());
    // mpm.add(createGlobalDCEPass());
    PM.run(*M);
#else
    constexpr int OptLevel = 3, SizeLevel = 0;

    legacy::PassManager PM;
    legacy::FunctionPassManager FPM(M);
    PassManagerBuilder PMB;
    PMB.OptLevel = OptLevel;
    PMB.SizeLevel = SizeLevel;
    PMB.Inliner = createFunctionInliningPass(OptLevel, SizeLevel, false);
    PMB.populateFunctionPassManager(FPM);
    PMB.populateModulePassManager(PM);

    for (auto &func : *M) {
        FPM.run(func);
    }

    PM.run(*M);
#endif

    // PrintIR
    outs() << "After optimized:\n\n" << *M << "\n";
    outs().flush();
    
    // JIT
    EngineBuilder ebuilder(std::move(Owner));
    std::string error = "No error";
    ebuilder.setErrorStr(&error).setEngineKind(EngineKind::Kind::JIT);
    ExecutionEngine *EE = ebuilder.create();
    outs() << "Error: " << error << "\n";

    // Call
    auto func = (TNumber(*)())EE->getFunctionAddress(MainFunctionName);
    TNumber result = func();
    
    delete EE;
    llvm_shutdown();
    return result;
}
