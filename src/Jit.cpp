#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
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
#include "Error.h"

/* Explicit instantiation of "EvaluateByJIT" Function */
#define InstantiateEvaluateByJIT(TNumber) template TNumber EvaluateByJIT<TNumber>(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo)
//InstantiateEvaluateByJIT(int8_t);
//InstantiateEvaluateByJIT(int16_t);
InstantiateEvaluateByJIT(int32_t);
InstantiateEvaluateByJIT(int64_t);
InstantiateEvaluateByJIT(__int128_t);

namespace {
    constexpr const char *MainFunctionName = "__[Main]__";
    constexpr const char *EntryBlockName = "entry";

    template<typename TNumber> size_t IntegerBits = sizeof(TNumber) * 8;
    template<typename TNumber> void GenerateIR(const CompilationContext &context, const std::shared_ptr<Operator> &op, llvm::LLVMContext *llvmContext, llvm::Module *llvmModule);
    template<typename TNumber> class IRGenerator;
}

template<>
mpz_class EvaluateByJIT<mpz_class>(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo) {
    __mpz_struct *returned = EvaluateByJIT<__mpz_struct *>(context, op, optimize, printInfo);
    mpz_class result(returned);
    free(returned);
    return result;
}

template<typename TNumber>
TNumber EvaluateByJIT(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo) {
    using namespace llvm;
    LLVMContext Context;

    /* ***** Create module ***** */
    std::unique_ptr<Module> Owner = make_unique<Module>("calc4-jit-module", Context);
    Module *M = Owner.get();
    M->setTargetTriple(LLVM_HOST_TRIPLE);

    /* ***** Generate LLVM-IR ***** */
    GenerateIR<TNumber>(context, op, &Context, M);

    if (printInfo) {
        // PrintIR
        outs() << "LLVM IR (Before optimized):\n---------------------------\n" << *M << "---------------------------\n\n";
        outs().flush();
    }

    /* ***** Optimize ***** */
    if (optimize) {
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

    /* ***** Execute JIT compiled code ***** */
    EngineBuilder ebuilder(std::move(Owner));
    std::string error;
    ebuilder.setErrorStr(&error).setEngineKind(EngineKind::Kind::JIT);
    ExecutionEngine *EE = ebuilder.create();

    if (!error.empty()) {
        throw error;
    }

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
        llvm::Type *voidType = llvm::Type::getVoidTy(*this->context);
        llvm::Type *int32Type = llvm::Type::getInt32Ty(*this->context);

        GMPFunctions() {}

        GMPFunctions(llvm::LLVMContext *context, llvm::Module *module)
            : GMPFunctionsBase(context, module) {}

#define STR(X) #X
#define DECLARE_FUNCTION(NAME, RETURN_TYPE, ...) \
        llvm::Function *llvm_ ## NAME = llvm::Function::Create( \
            llvm::FunctionType::get(RETURN_TYPE, { __VA_ARGS__ }, false), \
            llvm::Function::ExternalLinkage, STR(__g ## NAME), this->module)

        DECLARE_FUNCTION(mpz_init, voidType, mpzPtrType);
        DECLARE_FUNCTION(mpz_set, voidType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_init_set, voidType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_set_si, voidType, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_set_str, int32Type, mpzPtrType, llvm::PointerType::get(llvm::Type::getInt8Ty(*this->context), 0), int32Type);
        DECLARE_FUNCTION(mpz_clear, voidType, mpzPtrType);

        DECLARE_FUNCTION(mpz_add, voidType, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_add_ui, voidType, mpzPtrType, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_sub, voidType, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_mul, voidType, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_mul_si, voidType, mpzPtrType, mpzPtrType, llvm::Type::getInt32Ty(*this->context));
        DECLARE_FUNCTION(mpz_tdiv_q, voidType, mpzPtrType, mpzPtrType, mpzPtrType);
        DECLARE_FUNCTION(mpz_tdiv_r, voidType, mpzPtrType, mpzPtrType, mpzPtrType);

        DECLARE_FUNCTION(mpz_cmp, int32Type, mpzPtrType, mpzPtrType);

        llvm::Function *mallocfunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt64PtrTy(*this->context), { llvm::Type::getInt64Ty(*this->context) }, false),
            llvm::Function::ExternalLinkage, "malloc", this->module);
    };

    template<typename TNumber>
    void GenerateIR(const CompilationContext &context, const std::shared_ptr<Operator> &op, llvm::LLVMContext *llvmContext, llvm::Module *llvmModule) {
        /* ***** Initialize variables ***** */
        std::unique_ptr<GMPFunctions> gmp = nullptr;    // Used when integer precision is infinite
        size_t numAdditionalArguments;                  // 1 if integer precision is infinite, 0 otherwise
        llvm::Type *integerType;
        llvm::Type *usedDefinedReturnType;

        if (std::is_same<TNumber, __mpz_struct *>::value) {
            // Integer precision is infinite
            gmp.reset(new GMPFunctions(llvmContext, llvmModule));
            numAdditionalArguments = 1;
            integerType = gmp->mpzPtrType;
            usedDefinedReturnType = gmp->voidType;
        } else {
            // Integer precision is fixed
            numAdditionalArguments = 0;
            integerType = llvm::Type::getIntNTy(*llvmContext, IntegerBits<TNumber>);
            usedDefinedReturnType = integerType;
        }

        /* ***** Make function map (operator's name -> LLVM function) and the functions ***** */
        std::unordered_map<std::string, llvm::Function *> functionMap;
        for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
            auto& definition = it->second.GetDefinition();
            size_t numArguments = definition.GetNumOperands() + numAdditionalArguments;

            // Make arguments
            std::vector<llvm::Type *> argumentTypes(numArguments);
            std::generate_n(argumentTypes.begin(), numArguments, [integerType]() { return integerType; });

            llvm::FunctionType *functionType = llvm::FunctionType::get(usedDefinedReturnType, argumentTypes, false);
            functionMap[definition.GetName()] = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, definition.GetName(), llvmModule);
        }

        /* ***** Make main function ***** */
        llvm::FunctionType *funcType = llvm::FunctionType::get(integerType, {}, false);
        llvm::Function *mainFunction = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, MainFunctionName, llvmModule);

        /* ***** Generate IR ****** */
        // Local helper function
        auto Emit = [llvmModule, llvmContext, &functionMap, &gmp](llvm::Function *function, const std::shared_ptr<Operator> &op, bool isMainFunction) {
            llvm::BasicBlock *block = llvm::BasicBlock::Create(*llvmContext, EntryBlockName, function);
            auto builder = std::make_shared<llvm::IRBuilder<>>(block);

            IRGenerator<TNumber> generator(llvmModule, llvmContext, function, builder, functionMap, gmp.get(), isMainFunction);
            generator.BeginFunction();
            op->Accept(generator);
            generator.EndFunction();
        };

        // Main function
        Emit(mainFunction, op, true);

        // User-defined operators
        for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
            auto &definition = it->second.GetDefinition();
            auto &name = definition.GetName();
            Emit(functionMap[name], context.GetOperatorImplement(name).GetOperator(), false);
        }
    }

    template<typename TNumber>
    class IRGeneratorBase : public OperatorVisitor {
    protected:
        llvm::Module *module;
        llvm::LLVMContext *context;
        llvm::Function *function;
        std::shared_ptr<llvm::IRBuilder<>> builder;
        std::unordered_map<std::string, llvm::Function *> functionMap;
        GMPFunctions *gmp;
        bool isMainFunction;

    public:
        IRGeneratorBase(llvm::Module *module,
                        llvm::LLVMContext *context,
                        llvm::Function *function,
                        const std::shared_ptr<llvm::IRBuilder<>> &builder,
                        const std::unordered_map<std::string, llvm::Function *> &functionMap,
                        GMPFunctions *gmp,
                        bool isMainFunction) :
            module(module), context(context), function(function), builder(builder),
            functionMap(functionMap), gmp(gmp), isMainFunction(isMainFunction) {}

        virtual void BeginFunction() = 0;
        virtual void EndFunction() = 0;
    };

    template<typename TNumber>
    class IRGenerator : public IRGeneratorBase<TNumber> {
    private:
        llvm::Value *value;

    public:
        using IRGeneratorBase<TNumber>::IRGeneratorBase;

        virtual void BeginFunction() override {}

        virtual void EndFunction() override {
            this->builder->CreateRet(this->value);
        }

        virtual void Visit(const ZeroOperator &op) override {
            this->value = this->builder->getIntN(IntegerBits<TNumber>, 0);
        }

        virtual void Visit(const PrecomputedOperator &op) override {
            this->value = this->builder->getIntN(IntegerBits<TNumber>, op.GetValue<TNumber>());
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
            /* ***** Evaluate condition expression ***** */
            llvm::Value *temp = this->builder->CreateAlloca(this->builder->getIntNTy(IntegerBits<TNumber>));
            op.GetCondition()->Accept(*this);
            llvm::Value *cond = this->builder->CreateSelect(this->builder->CreateICmpNE(this->value, this->builder->getIntN(IntegerBits<TNumber>, 0)), this->builder->getInt1(1), this->builder->getInt1(0));

            /* ***** Generate if-true and if-false codes ***** */
            auto oldBuilder = this->builder;
            auto Core = [this, temp](llvm::BasicBlock *block, const std::shared_ptr<Operator> &op) {
                auto builder = std::make_shared<llvm::IRBuilder<>>(block);
                IRGenerator<TNumber> generator(this->module, this->context, this->function, builder, this->functionMap, this->gmp, this->isMainFunction);
                op->Accept(generator);
                generator.builder->CreateStore(generator.value, temp);
                return (this->builder = generator.builder);
            };

            llvm::BasicBlock *ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
            auto ifTrueBuilder = Core(ifTrue, op.GetIfTrue());
            llvm::BasicBlock *ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
            auto ifFalseBuilder = Core(ifFalse, op.GetIfFalse());

            /* ***** Emit branch operation ***** */
            llvm::BasicBlock *finalBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
            this->builder = std::make_shared<llvm::IRBuilder<>>(finalBlock);

            oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
            ifTrueBuilder->CreateBr(finalBlock);
            ifFalseBuilder->CreateBr(finalBlock);
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
        llvm::Value *value;

    public:
        using IRGeneratorBase<__mpz_struct *>::IRGeneratorBase;

        virtual void BeginFunction() override {
            if (this->isMainFunction) {
                value = this->builder->CreateAlloca(gmp->mpzIntType);
                this->builder->CreateCall(gmp->llvm_mpz_init, { GetValuePtr() });
            } else {
                value = &*this->function->arg_begin();
            }
        }

        virtual void EndFunction() override {
            if (this->isMainFunction) {
                llvm::Value *alloced = this->builder->CreateCall(gmp->mallocfunc, { builder->getInt64(sizeof(__mpz_struct)) });
                llvm::Value *casted = this->builder->CreateBitCast(alloced, gmp->mpzPtrType);
                this->builder->CreateCall(gmp->llvm_mpz_init_set, { casted, GetValuePtr() });
                this->builder->CreateCall(gmp->llvm_mpz_clear, { GetValuePtr() });
                this->builder->CreateRet(casted);
            } else {
                this->builder->CreateRet(nullptr);
            }
        }

        llvm::Value *GetValuePtr(llvm::Value *val = nullptr) const {
            if (val == nullptr) {
                val = this->value;
            }

            if (val == &*function->arg_begin()) {
                assert(!isMainFunction);
                // This is an user-defined operator and the specified 'val' is
                // the variable that is used to pass return value. It is already casted.
                return val;
            }

            return this->builder->CreateInBoundsGEP(val, { this->builder->getInt64(0), this->builder->getInt64(0) });
        }

        virtual void Visit(const ZeroOperator &op) override {
            this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), this->builder->getInt32(0) });
        }

        virtual void Visit(const PrecomputedOperator &op) override {
            mpz_class precomputed = op.GetValue<mpz_class>();
            if (precomputed.fits_slong_p()) {
                this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), this->builder->getInt32(precomputed.get_si()) });
            } else {
                std::stringstream ss;
                ss << precomputed;
                llvm::GlobalVariable *str = this->builder->CreateGlobalString(ss.str());
                llvm::Value *strPtr = this->builder->CreateInBoundsGEP(str, { this->builder->getInt64(0) , this->builder->getInt64(0) });
                this->builder->CreateCall(gmp->llvm_mpz_set_str, { GetValuePtr(), strPtr, this->builder->getInt32(10) });
            }
        }

        virtual void Visit(const OperandOperator &op) override {
            auto it = this->function->arg_begin();
            ++it;
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
            /* ***** Allocate temporal variable ***** */
            llvm::Value *temp = this->builder->CreateAlloca(gmp->mpzIntType);

            /* ***** First, evaluate left operand and store its result to the temporal variable ***** */
            op.GetLeft()->Accept(*this);
            this->builder->CreateCall(gmp->llvm_mpz_init_set, { GetValuePtr(temp), GetValuePtr() });

            /* ***** Next, evaluate right operand ***** */
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
            case BinaryType::Equal:
            case BinaryType::NotEqual:
            case BinaryType::LessThan:
            case BinaryType::LessThanOrEqual:
            case BinaryType::GreaterThanOrEqual:
            case BinaryType::GreaterThan:
            {
                llvm::Value *gmpcmp = this->builder->CreateCall(gmp->llvm_mpz_cmp, { GetValuePtr(temp), GetValuePtr() });

                llvm::Value *cmp;
                switch (op.GetType()) {
                case BinaryType::Equal:
                    cmp = this->builder->CreateICmpEQ(gmpcmp, this->builder->getInt32(0));
                    break;
                case BinaryType::NotEqual:
                    cmp = this->builder->CreateICmpNE(gmpcmp, this->builder->getInt32(0));
                    break;
                case BinaryType::LessThan:
                    cmp = this->builder->CreateICmpSLT(gmpcmp, this->builder->getInt32(0));
                    break;
                case BinaryType::LessThanOrEqual:
                    cmp = this->builder->CreateICmpSLE(gmpcmp, this->builder->getInt32(0));
                    break;
                case BinaryType::GreaterThanOrEqual:
                    cmp = this->builder->CreateICmpSGE(gmpcmp, this->builder->getInt32(0));
                    break;
                case BinaryType::GreaterThan:
                    cmp = this->builder->CreateICmpSGT(gmpcmp, this->builder->getInt32(0));
                    break;
                default:
                    UNREACHABLE();
                    break;
                }

                llvm::Value *result = this->builder->CreateSelect(cmp, this->builder->getInt32(1), this->builder->getInt32(0));
                this->builder->CreateCall(gmp->llvm_mpz_set_si, { GetValuePtr(), result });
                break;
            }
            default:
                UNREACHABLE();
                break;
            }

            /* ***** Free temporal variable ***** */
            this->builder->CreateCall(gmp->llvm_mpz_clear, { GetValuePtr(temp) });
        }

        virtual void Visit(const ConditionalOperator &op) override {
            /* ***** Evaluate condition expression ***** */
            /*
                The structure of __mpz_struct is as follows:
                    typedef struct {
                        int _mp_alloc;
                        int _mp_size;
                        mp_limb_t *_mp_d;
                    } __mpz_struct;
                We can determine whether the value is zero by looking at '_mp_size' member.
            */
            op.GetCondition()->Accept(*this);
            llvm::Value *casted = this->builder->CreateBitCast(GetValuePtr(), llvm::PointerType::get(llvm::Type::getInt32Ty(*this->context), 0));
            llvm::Value *ptr = this->builder->CreateInBoundsGEP(casted, this->builder->getInt32(1));
            llvm::Value *sign = this->builder->CreateLoad(ptr);
            llvm::Value *cond = this->builder->CreateSelect(
                this->builder->CreateICmpNE(sign, this->builder->getInt32(0)),
                this->builder->getInt1(1), this->builder->getInt1(0));

            /* ***** Generate if-true and if-false codes ***** */
            auto oldBuilder = this->builder;
            auto Core = [this](llvm::BasicBlock *block, const std::shared_ptr<Operator> &op) {
                auto builder = std::make_shared<llvm::IRBuilder<>>(block);
                IRGenerator<__mpz_struct *> generator(this->module, this->context, this->function, builder, this->functionMap, this->gmp, this->isMainFunction);
                generator.value = value;
                op->Accept(generator);
                return (this->builder = generator.builder);
            };

            llvm::BasicBlock *ifTrue = llvm::BasicBlock::Create(*this->context, "", this->function);
            auto ifTrueBuilder = Core(ifTrue, op.GetIfTrue());
            llvm::BasicBlock *ifFalse = llvm::BasicBlock::Create(*this->context, "", this->function);
            auto ifFalseBuilder = Core(ifFalse, op.GetIfFalse());

            /* ***** Emit branch operation ***** */
            llvm::BasicBlock *finalBlock = llvm::BasicBlock::Create(*this->context, "", this->function);
            this->builder = std::make_shared<llvm::IRBuilder<>>(finalBlock);

            oldBuilder->CreateCondBr(cond, ifTrue, ifFalse);
            ifTrueBuilder->CreateBr(finalBlock);
            ifFalseBuilder->CreateBr(finalBlock);
        }

        virtual void Visit(const UserDefinedOperator &op) override {
            size_t numParams = op.GetDefinition().GetNumOperands() + 1;
            std::vector<llvm::Value *> params(numParams);

            /* ***** Allocate operands ***** */
            params[0] = this->value;
            for (size_t i = 1 /* not 0 */; i < numParams; i++) {
                llvm::Value *alloced = this->builder->CreateAlloca(gmp->mpzIntType);
                params[i] = GetValuePtr(alloced);
                this->builder->CreateCall(gmp->llvm_mpz_init, { params[i] });
            }

            /* ***** Evaluate operands ***** */
            auto operands = op.GetOperands();
            for (size_t i = 0; i < operands.size(); i++) {
                operands[i]->Accept(*this);
                this->builder->CreateCall(gmp->llvm_mpz_set, { params[i + 1], GetValuePtr() });
            }

            /* ***** Call user-defined operator ***** */
            this->builder->CreateCall(this->functionMap[op.GetDefinition().GetName()], params);

            /* ***** Free operands ***** */
            for (size_t i = 1 /* not 0 */; i < numParams; i++) {
                this->builder->CreateCall(gmp->llvm_mpz_clear, { params[i] });
            }
        }
    };
}
