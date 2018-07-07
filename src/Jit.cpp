#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <gmpxx.h>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/MCJIT.h"
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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"

#include "Jit.h"
#include "Operators.h"

/* Explicit instantiation of "RunByJIT" Function */
#define InstantiateRunByJIT(TNumber) template TNumber RunByJIT<TNumber>(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo)
//InstantiateRunByJIT(int8_t);
//InstantiateRunByJIT(int16_t);
InstantiateRunByJIT(int32_t);
InstantiateRunByJIT(int64_t);
InstantiateRunByJIT(__int128_t);

namespace {
    constexpr const char *MainFunctionName = "__Main__";
    constexpr const char *EntryBlockName = "entry";

    template<typename TNumber>
    size_t IntegerBits = sizeof(TNumber) * 8;

    template<typename TNumber> void GenerateIR(const CompilationContext &context, const std::shared_ptr<Operator> &op, llvm::LLVMContext *llvmContext, llvm::Module *llvmModule);
    struct GMPFunctions;
    template<typename TNumber> class IRGenerator;
}

template<>
mpz_class RunByJIT<mpz_class>(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo) {
    __mpz_struct *result = RunByJIT<__mpz_struct *>(context, op, optimize, printInfo);
    return mpz_class(result);
}

template<typename TNumber>
TNumber RunByJIT(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo) {
    using namespace llvm;
    LLVMContext Context;

    // Create some module to put our function into it.
    std::unique_ptr<Module> Owner = make_unique<Module>("calc4-jit-module", Context);
    Module *M = Owner.get();
    M->setTargetTriple(LLVM_HOST_TRIPLE);

    // Generate IR
    GenerateIR<TNumber>(context, op, &Context, M);

    if (printInfo) {
        // PrintIR
        outs() << "LLVM IR (Before optimized):\n---------------------------\n" << *M << "---------------------------\n\n";
        outs().flush();
    }

    if (optimize) {
        // Optimize
        static constexpr int OptLevel = 3, SizeLevel = 0;

        legacy::PassManager PM;
        legacy::FunctionPassManager FPM(M);
        PassManagerBuilder PMB;
        PMB.OptLevel = OptLevel;
        PMB.SizeLevel = SizeLevel;
#if  LLVM_VERSION_MAJOR >= 5
        PMB.Inliner = createFunctionInliningPass(OptLevel, SizeLevel, false);
#else
        PMB.Inliner = createFunctionInliningPass(OptLevel, SizeLevel);
#endif //  LLVM_VERSION_MAJOR >= 5
        PMB.populateFunctionPassManager(FPM);
        PMB.populateModulePassManager(PM);

        for (auto &func : *M) {
            FPM.run(func);
        }

        PM.run(*M);

        if (printInfo) {
            // PrintIR
            outs() << "LLVM IR (After optimized):\n---------------------------\n" << *M << "---------------------------\n\n";
            outs().flush();
        }
    }

    // JIT
    EngineBuilder ebuilder(std::move(Owner));
    std::string error;
    ebuilder.setErrorStr(&error).setEngineKind(EngineKind::Kind::JIT);
    ExecutionEngine *EE = ebuilder.create();

    if (!error.empty()) {
        throw error;
    }

    // Call
    auto func = (TNumber(*)())EE->getFunctionAddress(MainFunctionName);

    if (!error.empty()) {
        throw error;
    }

    TNumber result = func();
    delete EE;
    return result;
}

namespace {
    struct GMPFunctionsBase {
    protected:
        llvm::LLVMContext *context;
        llvm::Module *module;

        GMPFunctionsBase() {}

        GMPFunctionsBase(llvm::LLVMContext *context, llvm::Module *module)
            : context(context), module(module) {}
    };

    struct GMPFunctions : protected GMPFunctionsBase {
        /* GMP Types */
        llvm::Type *mpzStructType =
            llvm::StructType::create(
                { llvm::ArrayType::get(llvm::Type::getInt1Ty(*context), sizeof(__mpz_struct)) },
                "__mpz_struct");
        llvm::Type *mpzIntType = llvm::ArrayType::get(mpzStructType, 1);
        llvm::Type *mpzPtrType = llvm::PointerType::getUnqual(mpzStructType);

        GMPFunctions() {}

        GMPFunctions(llvm::LLVMContext *context, llvm::Module *module)
            : GMPFunctionsBase(context, module) {}

#define STR(X) #X
#define DECLARE_FUNCTION(NAME, ...) \
        llvm::Function *llvm_ ## NAME = llvm::Function::Create( \
            llvm::FunctionType::get(llvm::Type::getVoidTy(*this->context), { __VA_ARGS__ }, false), \
            llvm::Function::ExternalLinkage, STR(__g ## NAME)  , this->module)

        DECLARE_FUNCTION(mpz_init, mpzPtrType);
        DECLARE_FUNCTION(mpz_set, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_init_set, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_set_si, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_clear, mpzPtrType);

        DECLARE_FUNCTION(mpz_add, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_add_ui, mpzPtrType, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_sub, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_mul, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_mul_si, mpzPtrType, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_tdiv_q, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_tdiv_r, mpzPtrType, mpzPtrType, mpzPtrType);

        llvm::Function *mallocfunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt64PtrTy(*this->context), { llvm::Type::getInt64Ty(*this->context) }, false),
            llvm::Function::ExternalLinkage, "malloc", this->module);
    };

    template<typename TNumber>
    void GenerateIR(const CompilationContext &context, const std::shared_ptr<Operator> &op, llvm::LLVMContext *llvmContext, llvm::Module *llvmModule) {
        std::unique_ptr<GMPFunctions> gmp = nullptr;
        llvm::Type *IntegerType;

        if (std::is_same<TNumber, __mpz_struct *>::value) {
            gmp.reset(new GMPFunctions(llvmContext, llvmModule));
            IntegerType = gmp->mpzPtrType;
        } else {
            IntegerType = llvm::Type::getIntNTy(*llvmContext, IntegerBits<TNumber>);
        }

        std::unordered_map<std::string, llvm::Function *> functionMap;
        for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
            auto& definition = it->second.GetDefinition();
            std::vector<llvm::Type *> argumentTypes(definition.GetNumOperands());
            std::generate_n(argumentTypes.begin(), definition.GetNumOperands(), [&]() { return IntegerType; });
            llvm::FunctionType *functionType = llvm::FunctionType::get(IntegerType, argumentTypes, false);
            functionMap[definition.GetName()] = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, definition.GetName(), llvmModule);
        }

        llvm::FunctionType *funcType = llvm::FunctionType::get(IntegerType, {}, false);
        llvm::Function *mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, MainFunctionName, llvmModule);
        {
            llvm::BasicBlock *mainBlock = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, mainFunc);
            llvm::IRBuilder<> builder(mainBlock);
            IRGenerator<TNumber> generator(llvmModule, llvmContext, &builder, mainFunc, functionMap, gmp.get());
            generator.BeginFunction();
            op->Accept(generator);
            generator.EndFunction();
        }

        for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
            auto& definition = it->second.GetDefinition();
            llvm::Function *function = functionMap[definition.GetName()];
            llvm::BasicBlock *block = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, function);
            llvm::IRBuilder<> builder(block);

            IRGenerator<TNumber> generator(llvmModule, llvmContext, &builder, function, functionMap, gmp.get());
            generator.BeginFunction();
            context.GetOperatorImplement(definition.GetName()).GetOperator()->Accept(generator);
            generator.EndFunction();
        }
    }

    template<typename TNumber>
    class IRGeneratorBase : public OperatorVisitor {
    public:
        llvm::Module *module;
        llvm::LLVMContext *context;
        llvm::IRBuilder<> *builder;
        llvm::Function *function;
        std::unordered_map<std::string, llvm::Function *> functionMap;
        llvm::Value *value;

        /* GMP Types */
        GMPFunctions *gmp;

        IRGeneratorBase(
            llvm::Module *module,
            llvm::LLVMContext *context,
            llvm::IRBuilder<> *builder,
            llvm::Function *function,
            const std::unordered_map<std::string, llvm::Function *> &functionMap,
            GMPFunctions *gmp)
            :
            module(module), context(context), builder(builder), function(function), functionMap(functionMap), gmp(gmp) {}

        virtual void BeginFunction() = 0;
        virtual void EndFunction() = 0;
    };

    template<typename TNumber>
    class IRGenerator : public IRGeneratorBase<TNumber> {
    public:
        using IRGeneratorBase<TNumber>::IRGeneratorBase;

        virtual void BeginFunction() override {}

        virtual void EndFunction() override {
            this->builder->CreateRet(this->value);
        }

        virtual void Visit(const ZeroOperator &op) override {
            this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
        }

        virtual void Visit(const OperandOperator &op) override {
            auto it = this->function->arg_begin();
            for (int i = 0; i < op.GetIndex(); i++) {
                ++it;
            }

            this->value = &*it;
        }

        virtual void Visit(const DefineOperator &op) override {
            this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
        }

        virtual void Visit(const ParenthesisOperator &op) override {
            this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
            for (auto& item : op.GetOperators()) {
                item->Accept(*this);
            }
        }

        virtual void Visit(const DecimalOperator &op) override {
            op.GetOperand()->Accept(*this);
            auto operand = this->value;

            auto multed = this->builder->CreateMul(operand, this->builder->getIntN(IntegerBits<TNumber>, 10));
            this->value = this->builder->CreateAdd(multed, this->builder->getIntN(IntegerBits<TNumber>, op.GetValue()));
        }

        virtual void Visit(const BinaryOperator &op) override {
            op.GetLeft()->Accept(*this);
            auto left = this->value;
            op.GetRight()->Accept(*this);
            auto right = this->value;

            switch (op.GetType()) {
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
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            case BinaryType::NotEqual:
            {
                auto cmp = this->builder->CreateICmpNE(left, right);
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            case BinaryType::LessThan:
            {
                auto cmp = this->builder->CreateICmpSLT(left, right);
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            case BinaryType::LessThanOrEqual:
            {
                auto cmp = this->builder->CreateICmpSLE(left, right);
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            case BinaryType::GreaterThanOrEqual:
            {
                auto cmp = this->builder->CreateICmpSGE(left, right);
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            case BinaryType::GreaterThan:
            {
                auto cmp = this->builder->CreateICmpSGT(left, right);
                this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                break;
            }
            default:
                UNREACHABLE();
                break;
            }
        }

        virtual void Visit(const ConditionalOperator &op) override {
            llvm::Value *temp = this->builder->CreateAlloca(this->builder->getIntNTy(IntegerBits<TNumber>));
            op.GetCondition()->Accept(*this);
            auto cond = this->builder->CreateSelect(this->builder->CreateICmpNE(this->value, this->builder->getIntN(IntegerBits<TNumber>, 0)), this->builder->getInt1(1), this->builder->getInt1(0));
            auto oldBuilder = this->builder;

            llvm::BasicBlock *ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
            llvm::IRBuilder<> ifTrueBuilder(ifTrue);
            IRGenerator<TNumber> ifTrueGenerator(this->module, this->context, &ifTrueBuilder, this->function, this->functionMap, this->gmp);
            op.GetIfTrue()->Accept(ifTrueGenerator);
            this->builder = ifTrueGenerator.builder;
            this->builder->CreateStore(ifTrueGenerator.value, temp);

            llvm::BasicBlock *ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
            llvm::IRBuilder<> ifFalseBuilder(ifFalse);
            IRGenerator<TNumber> ifFalseGenerator(this->module, this->context, &ifFalseBuilder, this->function, this->functionMap, this->gmp);
            op.GetIfFalse()->Accept(ifFalseGenerator);
            this->builder = ifFalseGenerator.builder;
            this->builder->CreateStore(ifFalseGenerator.value, temp);

            llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
            this->builder = new llvm::IRBuilder<>(nextBlock);

            oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
            ifTrueGenerator.builder->CreateBr(nextBlock);
            ifFalseGenerator.builder->CreateBr(nextBlock);
            this->value = this->builder->CreateLoad(temp);
        }

        virtual void Visit(const UserDefinedOperator &op) override {
            std::vector<llvm::Value *> arguments(op.GetOperands().size());
            auto operands = op.GetOperands();
            for (size_t i = 0; i < operands.size(); i++) {
                operands[i]->Accept(*this);
                arguments[i] = this->value;
            }

            this->value = this->builder->CreateCall(this->functionMap[op.GetDefinition().GetName()], arguments);
        }
    };

    template<>
    class IRGenerator<__mpz_struct *> : public IRGeneratorBase<__mpz_struct *> {
    private:
        /* GMP Variable */
        llvm::Value *localValue;

    public:
        using IRGeneratorBase<__mpz_struct *>::IRGeneratorBase;

        virtual void BeginFunction() override {
            localValue = this->builder->CreateAlloca(gmp->mpzIntType);
            this->builder->CreateCall(gmp->llvm_mpz_init, { GetValuePtr() });
        }

        virtual void EndFunction() override {
            llvm::Value *alloced = this->builder->CreateCall(gmp->mallocfunc, { builder->getInt64(sizeof(__mpz_struct)) });
            llvm::Value *casted = this->builder->CreateBitCast(alloced, gmp->mpzPtrType);
            this->builder->CreateCall(gmp->llvm_mpz_init_set, { casted, GetValuePtr() });
            this->builder->CreateCall(gmp->llvm_mpz_clear, { GetValuePtr() });
            this->builder->CreateRet(casted);
        }

        llvm::Value *GetValuePtr(llvm::Value *value = nullptr) const {
            if (value == nullptr) {
                value = localValue;
            }

            return this->builder->CreateInBoundsGEP(
                value, { this->builder->getInt64(0), this->builder->getInt64(0) });
        }

        virtual void Visit(const ZeroOperator &op) override {
            this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), this->builder->getInt32(0) });
        }

        virtual void Visit(const OperandOperator &op) override {
            auto it = this->function->arg_begin();
            for (int i = 0; i < op.GetIndex(); i++) {
                ++it;
            }

            this->builder->CreateCall(gmp->llvm_mpz_set, { GetValuePtr(), &*it });
        }

        virtual void Visit(const DefineOperator &op) override {
            this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), this->builder->getInt32(0) });
        }

        virtual void Visit(const ParenthesisOperator &op) override {
            this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), this->builder->getInt32(0) });
            for (auto& item : op.GetOperators()) {
                item->Accept(*this);
            }
        }

        virtual void Visit(const DecimalOperator &op) override {
            op.GetOperand()->Accept(*this);
            this->builder->CreateCall(gmp->llvm_mpz_mul_si, { GetValuePtr(), GetValuePtr(), this->builder->getInt32(10) });
            this->builder->CreateCall(gmp->llvm_mpz_add_ui, { GetValuePtr(), GetValuePtr(), this->builder->getInt32(op.GetValue()) });
        }

        virtual void Visit(const BinaryOperator &op) override {
            // Allocate temporal variable
            llvm::Value *temp = this->builder->CreateAlloca(gmp->mpzIntType);

            // First, evaluate left operand and store its result to the temporal variable
            op.GetLeft()->Accept(*this);
            this->builder->CreateCall(gmp->llvm_mpz_init_set, { GetValuePtr(temp), GetValuePtr() });

            // Next, evaluate right operand
            op.GetRight()->Accept(*this);

            switch (op.GetType()) {
            case BinaryType::Add:
                this->builder->CreateCall(gmp->llvm_mpz_add, { GetValuePtr(), GetValuePtr(temp), GetValuePtr() });
                break;
            case BinaryType::Sub:
                this->builder->CreateCall(gmp->llvm_mpz_sub, { GetValuePtr(), GetValuePtr(temp), GetValuePtr() });
                break;
            case BinaryType::Mult:
                this->builder->CreateCall(gmp->llvm_mpz_mul, { GetValuePtr(), GetValuePtr(temp), GetValuePtr() });
                break;
            case BinaryType::Div:
                this->builder->CreateCall(gmp->llvm_mpz_tdiv_q, { GetValuePtr(), GetValuePtr(temp), GetValuePtr() });
                break;
            case BinaryType::Mod:
                this->builder->CreateCall(gmp->llvm_mpz_tdiv_r, { GetValuePtr(), GetValuePtr(temp), GetValuePtr() });
                break;
                //case BinaryType::Equal:
                //{
                //    auto cmp = this->builder->CreateICmpEQ(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
                //case BinaryType::NotEqual:
                //{
                //    auto cmp = this->builder->CreateICmpNE(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
                //case BinaryType::LessThan:
                //{
                //    auto cmp = this->builder->CreateICmpSLT(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
                //case BinaryType::LessThanOrEqual:
                //{
                //    auto cmp = this->builder->CreateICmpSLE(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
                //case BinaryType::GreaterThanOrEqual:
                //{
                //    auto cmp = this->builder->CreateICmpSGE(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
                //case BinaryType::GreaterThan:
                //{
                //    auto cmp = this->builder->CreateICmpSGT(left, right);
                //    this->value = this->builder->CreateSelect(cmp, this->builder->getIntN(IntegerBits<TNumber>, 1), this->builder->getIntN(IntegerBits<TNumber>, 0));
                //    break;
                //}
            default:
                UNREACHABLE();
                break;
            }

            // Free temporal variable
            this->builder->CreateCall(gmp->llvm_mpz_clear, { GetValuePtr(temp) });
        }

        virtual void Visit(const ConditionalOperator &op) override {
            //llvm::Value *temp = this->builder->CreateAlloca(this->builder->getIntNTy(IntegerBits<TNumber>));
            //op.GetCondition()->Accept(*this);
            //auto cond = this->builder->CreateSelect(this->builder->CreateICmpNE(this->value, this->builder->getIntN(IntegerBits<TNumber>, 0)), this->builder->getInt1(1), this->builder->getInt1(0));
            //auto oldBuilder = this->builder;

            //llvm::BasicBlock *ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
            //llvm::IRBuilder<> ifTrueBuilder(ifTrue);
            //IRGenerator<TNumber> ifTrueGenerator(this->context, &ifTrueBuilder, this->function, this->functionMap);
            //op.GetIfTrue()->Accept(ifTrueGenerator);
            //this->builder = ifTrueGenerator.builder;
            //this->builder->CreateStore(ifTrueGenerator.value, temp);

            //llvm::BasicBlock *ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
            //llvm::IRBuilder<> ifFalseBuilder(ifFalse);
            //IRGenerator<TNumber> ifFalseGenerator(this->context, &ifFalseBuilder, this->function, this->functionMap);
            //op.GetIfFalse()->Accept(ifFalseGenerator);
            //this->builder = ifFalseGenerator.builder;
            //this->builder->CreateStore(ifFalseGenerator.value, temp);

            //llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
            //this->builder = new llvm::IRBuilder<>(nextBlock);

            //oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
            //ifTrueGenerator.builder->CreateBr(nextBlock);
            //ifFalseGenerator.builder->CreateBr(nextBlock);
            //this->value = this->builder->CreateLoad(temp);
        }

        virtual void Visit(const UserDefinedOperator &op) override {
            //std::vector<llvm::Value *> arguments(op.GetOperands().size());
            //auto operands = op.GetOperands();
            //for (size_t i = 0; i < operands.size(); i++) {
            //    operands[i]->Accept(*this);
            //    arguments[i] = this->value;
            //}

            //this->value = this->builder->CreateCall(this->functionMap[op.GetDefinition().GetName()], arguments);
        }
    };
}
