/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "WasmTextEmitter.h"
#include "Operators.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace calc4
{
namespace
{
/* =============================================================================
 * Small IR
 * ============================================================================= */

enum class ValType
{
    I32,
    I64,
};

constexpr std::string_view ToWatTypeName(ValType t)
{
    switch (t)
    {
    case ValType::I32:
        return "i32";
    case ValType::I64:
        return "i64";
    default:
        UNREACHABLE();
        return ""; // Keep compiler silence
    }
}

struct ImportFunc
{
    std::string moduleName;   // e.g. "env"
    std::string fieldName;    // e.g. "getchar"
    std::string internalName; // e.g. "$getchar"
    std::vector<ValType> params;
    std::optional<ValType> result;
};

struct MemoryDef
{
    std::string internalName = "$mem";
    uint32_t minPages = 16; // 16 * 64KiB = 1MiB
    bool exportMemory = true;
    std::string exportName = "memory";
};

struct GlobalDef
{
    std::string internalName; // e.g. "$g_x"
    ValType type = ValType::I64;
    bool isMutable = true;
    int64_t initValue = 0;
};

struct LocalDef
{
    std::string name; // e.g. "$tmp"
    ValType type;
};

struct ParamDef
{
    std::string name; // e.g. "$arg0"
    ValType type;
};

struct Instr
{
    enum class Kind
    {
        Simple,
        Block,
        Loop,
        If,
    };

    Kind kind = Kind::Simple;

    // --- Simple instruction: "opcode arg0 arg1 ..."
    std::string opcode;
    std::vector<std::string> args;

    // --- Structured instruction fields
    std::string label;                 // for block/loop (e.g. "$ret", "$entry")
    std::optional<ValType> resultType; // for block/if when it yields a value
    std::vector<Instr> body;           // body (for block/loop) or then-body (for if)
    std::vector<Instr> elseBody;       // else-body (for if only)

    static Instr Simple(std::string opcode, std::vector<std::string> args = {})
    {
        Instr i;
        i.kind = Kind::Simple;
        i.opcode = std::move(opcode);
        i.args = std::move(args);
        return i;
    }

    static Instr Block(std::string label, std::optional<ValType> resultType,
                       std::vector<Instr> body)
    {
        Instr i;
        i.kind = Kind::Block;
        i.label = std::move(label);
        i.resultType = resultType;
        i.body = std::move(body);
        return i;
    }

    static Instr Loop(std::string label, std::vector<Instr> body)
    {
        Instr i;
        i.kind = Kind::Loop;
        i.label = std::move(label);
        i.body = std::move(body);
        return i;
    }

    static Instr If(std::optional<ValType> resultType, std::vector<Instr> thenBody,
                    std::vector<Instr> elseBody)
    {
        Instr i;
        i.kind = Kind::If;
        i.resultType = resultType;
        i.body = std::move(thenBody);
        i.elseBody = std::move(elseBody);
        return i;
    }
};

struct FuncDef
{
    std::string internalName;              // e.g. "$main"
    std::optional<std::string> exportName; // e.g. "main"
    std::vector<ParamDef> params;
    std::optional<ValType> result;
    std::vector<LocalDef> locals;
    std::vector<Instr> body;
};

struct ModuleDef
{
    std::vector<ImportFunc> imports;
    MemoryDef memory;
    std::vector<GlobalDef> globals;
    std::vector<FuncDef> functions;
};

/* =============================================================================
 * WAT writer
 * ============================================================================= */

class WatWriter
{
private:
    std::ostream& os;
    int indent = 0;

    void Indent()
    {
        for (int i = 0; i < indent; i++)
        {
            os << "  ";
        }
    }

    void WriteSimpleInstr(const Instr& ins)
    {
        Indent();
        os << ins.opcode;
        for (auto& a : ins.args)
        {
            os << ' ' << a;
        }
        os << '\n';
    }

    void WriteInstr(const Instr& ins)
    {
        switch (ins.kind)
        {
        case Instr::Kind::Simple:
            WriteSimpleInstr(ins);
            break;
        case Instr::Kind::Block:
        {
            Indent();
            os << "(block";
            if (!ins.label.empty())
            {
                os << ' ' << ins.label;
            }
            if (ins.resultType)
            {
                os << " (result " << ToWatTypeName(*ins.resultType) << ")";
            }
            os << '\n';

            indent++;
            for (auto& c : ins.body)
            {
                WriteInstr(c);
            }
            indent--;

            Indent();
            os << ")\n";
            break;
        }
        case Instr::Kind::Loop:
        {
            Indent();
            os << "(loop";
            if (!ins.label.empty())
            {
                os << ' ' << ins.label;
            }
            os << '\n';

            indent++;
            for (auto& c : ins.body)
            {
                WriteInstr(c);
            }
            indent--;

            Indent();
            os << ")\n";
            break;
        }
        case Instr::Kind::If:
        {
            Indent();
            os << "(if";
            if (ins.resultType)
            {
                os << " (result " << ToWatTypeName(*ins.resultType) << ")";
            }
            os << '\n';

            indent++;
            Indent();
            os << "(then\n";
            indent++;
            for (auto& c : ins.body)
            {
                WriteInstr(c);
            }
            indent--;
            Indent();
            os << ")\n";

            const bool needElse = ins.resultType.has_value() || !ins.elseBody.empty();
            if (needElse)
            {
                Indent();
                os << "(else\n";
                indent++;

                if (ins.elseBody.empty())
                {
                    // Result-typed if requires an else branch. Emit an explicit unreachable to keep
                    // the output valid even if the IR forgot to provide else.
                    Indent();
                    os << "unreachable\n";
                }
                else
                {
                    for (auto& c : ins.elseBody)
                    {
                        WriteInstr(c);
                    }
                }

                indent--;
                Indent();
                os << ")\n";
            }
            indent--;

            Indent();
            os << ")\n";
            break;
        }
        }
    }

public:
    explicit WatWriter(std::ostream& os) : os(os) {}

    void WriteModule(const ModuleDef& m)
    {
        os << "(module\n";
        indent++;

        // Imports
        for (auto& im : m.imports)
        {
            Indent();
            os << "(import \"" << im.moduleName << "\" \"" << im.fieldName << "\" (func "
               << im.internalName;
            for (auto& p : im.params)
            {
                os << " (param " << ToWatTypeName(p) << ")";
            }
            if (im.result)
            {
                os << " (result " << ToWatTypeName(*im.result) << ")";
            }
            os << "))\n";
        }

        // Memory
        Indent();
        os << "(memory " << m.memory.internalName;
        if (m.memory.exportMemory)
        {
            os << " (export \"" << m.memory.exportName << "\")";
        }
        os << ' ' << m.memory.minPages << ")\n";

        // Globals
        for (auto& g : m.globals)
        {
            Indent();
            os << "(global " << g.internalName << ' ';
            if (g.isMutable)
            {
                os << "(mut " << ToWatTypeName(g.type) << ")";
            }
            else
            {
                os << ToWatTypeName(g.type);
            }
            os << " (" << ToWatTypeName(g.type) << ".const " << g.initValue << "))\n";
        }

        // Functions
        for (auto& f : m.functions)
        {
            Indent();
            os << "(func " << f.internalName;
            if (f.exportName)
            {
                os << " (export \"" << *f.exportName << "\")";
            }
            for (auto& p : f.params)
            {
                os << " (param " << p.name << ' ' << ToWatTypeName(p.type) << ")";
            }
            if (f.result)
            {
                os << " (result " << ToWatTypeName(*f.result) << ")";
            }
            os << '\n';

            indent++;

            for (auto& l : f.locals)
            {
                Indent();
                os << "(local " << l.name << ' ' << ToWatTypeName(l.type) << ")\n";
            }

            for (auto& ins : f.body)
            {
                WriteInstr(ins);
            }

            indent--;
            Indent();
            os << ")\n";
        }

        indent--;
        os << ")\n";
    }
};

/* =============================================================================
 * AST -> IR lowering
 * ============================================================================= */

std::string ToHex2(unsigned char v)
{
    static constexpr char Hex[] = "0123456789ABCDEF";
    std::string s;
    s.push_back(Hex[(v >> 4) & 0xF]);
    s.push_back(Hex[v & 0xF]);
    return s;
}

bool IsAsciiAlnum(unsigned char c)
{
    return (c >= static_cast<unsigned char>('0') && c <= static_cast<unsigned char>('9')) ||
        (c >= static_cast<unsigned char>('A') && c <= static_cast<unsigned char>('Z')) ||
        (c >= static_cast<unsigned char>('a') && c <= static_cast<unsigned char>('z'));
}

// Make a safe identifier suffix for WAT `$name` (conservative + injective).
// - Keeps [A-Za-z0-9].
// - Escapes '_' as '__'.
// - Escapes other bytes as `_XX` where XX is hex.
// This encoding is injective: different raw names will never collide.
std::string SanitizeId(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size() + 8);

    for (unsigned char c : raw)
    {
        if (IsAsciiAlnum(c))
        {
            out.push_back(static_cast<char>(c));
        }
        else if (c == '_')
        {
            out += "__";
        }
        else
        {
            out.push_back('_');
            out += ToHex2(c);
        }
    }

    // Keep the result non-empty for readability/debugging.
    // NOTE: "_" cannot be produced by the encoding of any non-empty raw string.
    if (out.empty())
    {
        out = "_";
    }

    return out;
}

template<typename TNumber>
struct TypeTraits;

template<>
struct TypeTraits<int32_t>
{
    static constexpr ValType numType = ValType::I32;
    static constexpr int32_t byteSize = 4;

    static constexpr std::string_view WatTypeName()
    {
        return "i32";
    }

    static constexpr std::string_view ConstOp()
    {
        return "i32.const";
    }

    static constexpr std::string_view AddOp()
    {
        return "i32.add";
    }
    static constexpr std::string_view SubOp()
    {
        return "i32.sub";
    }
    static constexpr std::string_view MulOp()
    {
        return "i32.mul";
    }
    static constexpr std::string_view DivSOp()
    {
        return "i32.div_s";
    }
    static constexpr std::string_view RemSOp()
    {
        return "i32.rem_s";
    }

    static constexpr std::string_view EqOp()
    {
        return "i32.eq";
    }
    static constexpr std::string_view NeOp()
    {
        return "i32.ne";
    }
    static constexpr std::string_view LtSOp()
    {
        return "i32.lt_s";
    }

    static constexpr std::string_view LtUOp()
    {
        return "i32.lt_u";
    }
    static constexpr std::string_view LeSOp()
    {
        return "i32.le_s";
    }
    static constexpr std::string_view GtSOp()
    {
        return "i32.gt_s";
    }
    static constexpr std::string_view GeSOp()
    {
        return "i32.ge_s";
    }

    static constexpr std::string_view LoadOp()
    {
        return "i32.load";
    }
    static constexpr std::string_view StoreOp()
    {
        return "i32.store";
    }

    // boolean i32 -> i32: nop
    static void EmitBoolToNumber(std::vector<Instr>& /*out*/) {}
    // i32 result of getchar -> i32: nop
    static void EmitGetCharToNumber(std::vector<Instr>& /*out*/) {}
    // i32 -> i32 for putchar: nop
    static void EmitNumberToPutChar(std::vector<Instr>& /*out*/) {}
    // i32 address: nop
    static void EmitIndexToAddress(std::vector<Instr>& out)
    {
        // index(i32) * 4
        out.emplace_back(Instr::Simple(std::string(ConstOp()), { std::to_string(byteSize) }));
        out.emplace_back(Instr::Simple("i32.mul"));
        // already i32
    }
};

template<>
struct TypeTraits<int64_t>
{
    static constexpr ValType numType = ValType::I64;
    static constexpr int32_t byteSize = 8;

    static constexpr std::string_view WatTypeName()
    {
        return "i64";
    }

    static constexpr std::string_view ConstOp()
    {
        return "i64.const";
    }

    static constexpr std::string_view AddOp()
    {
        return "i64.add";
    }
    static constexpr std::string_view SubOp()
    {
        return "i64.sub";
    }
    static constexpr std::string_view MulOp()
    {
        return "i64.mul";
    }
    static constexpr std::string_view DivSOp()
    {
        return "i64.div_s";
    }
    static constexpr std::string_view RemSOp()
    {
        return "i64.rem_s";
    }

    static constexpr std::string_view EqOp()
    {
        return "i64.eq";
    }
    static constexpr std::string_view NeOp()
    {
        return "i64.ne";
    }
    static constexpr std::string_view LtSOp()
    {
        return "i64.lt_s";
    }

    static constexpr std::string_view LtUOp()
    {
        return "i64.lt_u";
    }
    static constexpr std::string_view LeSOp()
    {
        return "i64.le_s";
    }
    static constexpr std::string_view GtSOp()
    {
        return "i64.gt_s";
    }
    static constexpr std::string_view GeSOp()
    {
        return "i64.ge_s";
    }

    static constexpr std::string_view LoadOp()
    {
        return "i64.load";
    }
    static constexpr std::string_view StoreOp()
    {
        return "i64.store";
    }

    // boolean i32 -> i64: zero-extend (0/1)
    static void EmitBoolToNumber(std::vector<Instr>& out)
    {
        out.emplace_back(Instr::Simple("i64.extend_i32_u"));
    }

    // i32 result of getchar -> i64: sign-extend (keep -1)
    static void EmitGetCharToNumber(std::vector<Instr>& out)
    {
        out.emplace_back(Instr::Simple("i64.extend_i32_s"));
    }

    // i64 -> i32 for putchar
    static void EmitNumberToPutChar(std::vector<Instr>& out)
    {
        out.emplace_back(Instr::Simple("i32.wrap_i64"));
    }

    // index(i64) -> address(i32)
    static void EmitIndexToAddress(std::vector<Instr>& out)
    {
        out.emplace_back(Instr::Simple(std::string(ConstOp()), { std::to_string(byteSize) }));
        out.emplace_back(Instr::Simple("i64.mul"));
        out.emplace_back(Instr::Simple("i32.wrap_i64"));
    }
};

struct NameResolver
{
    // Variable name (Calc4) -> wasm global id (e.g. "$g_user_defined_var_x")
    std::map<std::string, std::string> globalsByVar;

    // OperatorDefinition(name/arity) -> wasm func id (e.g. "$user_defined_operator_fib")
    // (We use a stable key string to avoid depending on pointer identity.)
    std::unordered_map<std::string, std::string> funcsByDefKey;

    struct OperatorDefinitionHash
    {
        std::size_t operator()(const OperatorDefinition& def) const noexcept
        {
            std::size_t h1 = std::hash<std::string>()(def.GetName());
            std::size_t h2 = std::hash<int>()(def.GetNumOperands());
            // Similar to boost::hash_combine
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    // Cache for KeyOf(): massive ASTs may contain many call nodes.
    // We cache the "name#arity" string per distinct OperatorDefinition value to avoid
    // repeated string concatenations.
    mutable std::unordered_map<OperatorDefinition, std::string, OperatorDefinitionHash> defKeyCache;

    const std::string& KeyOf(const OperatorDefinition& def) const
    {
        auto it = defKeyCache.find(def);
        if (it != defKeyCache.end())
        {
            return it->second;
        }

        std::string key;
        key.reserve(def.GetName().size() + 1 + 16);
        key += def.GetName();
        key.push_back('#');
        key += std::to_string(def.GetNumOperands());

        auto p = defKeyCache.emplace(def, std::move(key));
        return p.first->second;
    }

    const std::string& GetFuncName(const OperatorDefinition& def) const
    {
        return funcsByDefKey.at(KeyOf(def));
    }

    const std::string& GetGlobalName(const std::string& varName) const
    {
        return globalsByVar.at(varName);
    }
};

// TODO: Remove duplicate code in CppEmitter, Jit, and WasmTextEmitter
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

// TODO: Remove duplicate code in CppEmitter, Jit, and WasmTextEmitter
std::set<std::string_view> GatherVariableNames(const std::shared_ptr<const Operator>& mainOp,
                                               const CompilationContext& context)
{
    std::set<std::string_view> result;
    GatherVariableNamesCore(mainOp, result);

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        GatherVariableNamesCore(it->second.GetOperator(), result);
    }

    return result;
}

template<typename TNumber>
class ValueEmitter : public OperatorVisitor
{
private:
    using TT = TypeTraits<TNumber>;

    const CompilationContext& context;
    const NameResolver& names;
    const WasmTextOptions& opt;

    // Current output list for visitor
    std::vector<Instr>* out = nullptr;

    // Locals (names inside current function)
    std::vector<std::string> paramNames;
    std::string tmpLocal; // used for store operations
    std::string idxLocal; // used for array index caching (fast/fallback selection)

    static void EmitConstNumber(std::vector<Instr>& out, int64_t v)
    {
        out.emplace_back(Instr::Simple(std::string(TT::ConstOp()), { std::to_string(v) }));
    }

    static void EmitZero(std::vector<Instr>& out)
    {
        EmitConstNumber(out, 0);
    }

    static void EmitOne(std::vector<Instr>& out)
    {
        EmitConstNumber(out, 1);
    }

    // Convert (number != 0) to i32 condition (for if/br_if)
    static void EmitNonZeroAsI32(std::vector<Instr>& out)
    {
        EmitConstNumber(out, 0);
        out.emplace_back(Instr::Simple(std::string(TT::NeOp()))); // yields i32
    }

    // Convert i32 boolean (on stack) to number type (if needed).
    static void EmitBoolToNumber(std::vector<Instr>& out)
    {
        TT::EmitBoolToNumber(out);
    }

    void EmitFastIndexCondition(std::vector<Instr>& out) const
    {
        // cond(i32) = (idxLocal < fastMemoryLimitElements) using unsigned comparison.
        out.emplace_back(Instr::Simple("local.get", { idxLocal }));
        EmitConstNumber(out, static_cast<int64_t>(opt.fastMemoryLimitElements));
        out.emplace_back(Instr::Simple(std::string(TT::LtUOp())));
    }

    void EmitFastAddressFromIdxLocal(std::vector<Instr>& out) const
    {
        // address(i32) = base + idxLocal * sizeof(T)
        out.emplace_back(Instr::Simple("local.get", { idxLocal }));
        TT::EmitIndexToAddress(out);

        if (opt.fastMemoryBaseOffsetBytes != 0)
        {
            out.emplace_back(
                Instr::Simple("i32.const", { std::to_string(opt.fastMemoryBaseOffsetBytes) }));
            out.emplace_back(Instr::Simple("i32.add"));
        }
    }

public:
    ValueEmitter(const CompilationContext& context, const NameResolver& names,
                 const WasmTextOptions& opt)
        : context(context), names(names), opt(opt)
    {
    }

    void SetCurrentFunctionLocals(std::vector<std::string> paramNames, std::string tmpLocal,
                                  std::string idxLocal)
    {
        this->paramNames = std::move(paramNames);
        this->tmpLocal = std::move(tmpLocal);
        this->idxLocal = std::move(idxLocal);
    }

    void EmitValue(const std::shared_ptr<const Operator>& op, std::vector<Instr>& out)
    {
        auto* prev = this->out;
        this->out = &out;
        op->Accept(*this);
        this->out = prev;
    }

    /* ----- Visitors (value mode only) ----- */

    void Visit(const std::shared_ptr<const ZeroOperator>& /*op*/) override
    {
        EmitZero(*out);
    }

    void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
    {
        // Cast to int64 for printing; WAT constant is signed.
        int64_t v = static_cast<int64_t>(op->GetValue<TNumber>());
        out->emplace_back(Instr::Simple(std::string(TT::ConstOp()), { std::to_string(v) }));
    }

    void Visit(const std::shared_ptr<const OperandOperator>& op) override
    {
        int idx = op->GetIndex();
        assert(idx >= 0 && static_cast<size_t>(idx) < paramNames.size());
        out->emplace_back(Instr::Simple("local.get", { paramNames[static_cast<size_t>(idx)] }));
    }

    void Visit(const std::shared_ptr<const DefineOperator>& /*op*/) override
    {
        EmitZero(*out);
    }

    void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
    {
        const std::string& g = names.GetGlobalName(op->GetVariableName());
        out->emplace_back(Instr::Simple("global.get", { g }));
    }

    void Visit(const std::shared_ptr<const InputOperator>& /*op*/) override
    {
        out->emplace_back(Instr::Simple("call", { "$getchar" }));
        TT::EmitGetCharToNumber(*out);
    }

    void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
    {
        // Hybrid global array:
        // - Fast path: indices in [0, fastMemoryLimitElements) are stored in linear memory.
        // - Fallback: other indices (including negative) are handled by imported functions.

        // Evaluate index once
        EmitValue(op->GetIndex(), *out);
        out->emplace_back(Instr::Simple("local.set", { idxLocal }));

        // if (idx < fastMemoryLimitElements) then load from memory else call mem_get
        EmitFastIndexCondition(*out);

        std::vector<Instr> thenBody;
        std::vector<Instr> elseBody;

        // then: direct load
        EmitFastAddressFromIdxLocal(thenBody);
        thenBody.emplace_back(Instr::Simple(std::string(TT::LoadOp())));

        // else: fallback mem_get
        elseBody.emplace_back(Instr::Simple("local.get", { idxLocal }));
        elseBody.emplace_back(Instr::Simple("call", { "$mem_get" }));

        out->emplace_back(Instr::If(TT::numType, std::move(thenBody), std::move(elseBody)));
    }

    void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
    {
        EmitValue(op->GetCharacter(), *out);
        TT::EmitNumberToPutChar(*out);
        out->emplace_back(Instr::Simple("call", { "$putchar" }));
        EmitZero(*out);
    }

    void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
    {
        auto& ops = op->GetOperators();
        if (ops.empty())
        {
            EmitZero(*out);
            return;
        }

        for (size_t i = 0; i < ops.size(); i++)
        {
            EmitValue(ops[i], *out);
            if (i + 1 < ops.size())
            {
                out->emplace_back(Instr::Simple("drop"));
            }
        }
    }

    void Visit(const std::shared_ptr<const DecimalOperator>& op) override
    {
        EmitValue(op->GetOperand(), *out);
        EmitConstNumber(*out, 10);
        out->emplace_back(Instr::Simple(std::string(TT::MulOp())));
        EmitConstNumber(*out, op->GetValue());
        out->emplace_back(Instr::Simple(std::string(TT::AddOp())));
    }

    void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
    {
        // value -> tmp, set global, return tmp
        EmitValue(op->GetOperand(), *out);
        out->emplace_back(Instr::Simple("local.tee", { tmpLocal }));
        out->emplace_back(
            Instr::Simple("global.set", { names.GetGlobalName(op->GetVariableName()) }));
        out->emplace_back(Instr::Simple("local.get", { tmpLocal }));
    }

    void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
    {
        // Hybrid global array store returns stored value.
        // Evaluation order is value then index to match other backends.
        // This avoids clobbering idxLocal while evaluating value.
        EmitValue(op->GetValue(), *out);
        EmitValue(op->GetIndex(), *out);
        out->emplace_back(Instr::Simple("local.set", { idxLocal }));
        out->emplace_back(Instr::Simple("local.set", { tmpLocal }));

        // if (idx < fastMemoryLimitElements) then store to memory else call mem_set
        EmitFastIndexCondition(*out);

        std::vector<Instr> thenBody;
        std::vector<Instr> elseBody;

        // then: direct store
        EmitFastAddressFromIdxLocal(thenBody);
        thenBody.emplace_back(Instr::Simple("local.get", { tmpLocal }));
        thenBody.emplace_back(Instr::Simple(std::string(TT::StoreOp())));

        // else: fallback mem_set
        elseBody.emplace_back(Instr::Simple("local.get", { idxLocal }));
        elseBody.emplace_back(Instr::Simple("local.get", { tmpLocal }));
        elseBody.emplace_back(Instr::Simple("call", { "$mem_set" }));

        out->emplace_back(Instr::If(std::nullopt, std::move(thenBody), std::move(elseBody)));

        // store returns stored value
        out->emplace_back(Instr::Simple("local.get", { tmpLocal }));
    }

    void Visit(const std::shared_ptr<const BinaryOperator>& op) override
    {
        auto type = op->GetType();

        // Short-circuit logical ops
        if (type == BinaryType::LogicalAnd || type == BinaryType::LogicalOr)
        {
            // cond = (left != 0)
            EmitValue(op->GetLeft(), *out);
            EmitNonZeroAsI32(*out);

            std::vector<Instr> thenBody;
            std::vector<Instr> elseBody;

            if (type == BinaryType::LogicalAnd)
            {
                // then: (right != 0) ? 1 : 0
                EmitValue(op->GetRight(), thenBody);
                EmitNonZeroAsI32(thenBody); // i32
                EmitBoolToNumber(thenBody); // -> number
                // else: 0
                EmitZero(elseBody);
            }
            else
            {
                // LogicalOr
                // then: 1
                EmitOne(thenBody);
                // else: (right != 0) ? 1 : 0
                EmitValue(op->GetRight(), elseBody);
                EmitNonZeroAsI32(elseBody);
                EmitBoolToNumber(elseBody);
            }

            out->emplace_back(Instr::If(TT::numType, std::move(thenBody), std::move(elseBody)));
            return;
        }

        // Regular binary ops
        EmitValue(op->GetLeft(), *out);
        EmitValue(op->GetRight(), *out);

        auto EmitCompareAndConvert = [this]() {
            // comparison yields i32; convert to number if needed
            TT::EmitBoolToNumber(*out);
        };

        switch (type)
        {
        case BinaryType::Add:
            out->emplace_back(Instr::Simple(std::string(TT::AddOp())));
            break;
        case BinaryType::Sub:
            out->emplace_back(Instr::Simple(std::string(TT::SubOp())));
            break;
        case BinaryType::Mult:
            out->emplace_back(Instr::Simple(std::string(TT::MulOp())));
            break;
        case BinaryType::Div:
            out->emplace_back(Instr::Simple(std::string(TT::DivSOp())));
            break;
        case BinaryType::Mod:
            out->emplace_back(Instr::Simple(std::string(TT::RemSOp())));
            break;
        case BinaryType::Equal:
            out->emplace_back(Instr::Simple(std::string(TT::EqOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::NotEqual:
            out->emplace_back(Instr::Simple(std::string(TT::NeOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::LessThan:
            out->emplace_back(Instr::Simple(std::string(TT::LtSOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::LessThanOrEqual:
            out->emplace_back(Instr::Simple(std::string(TT::LeSOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::GreaterThanOrEqual:
            out->emplace_back(Instr::Simple(std::string(TT::GeSOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::GreaterThan:
            out->emplace_back(Instr::Simple(std::string(TT::GtSOp())));
            EmitCompareAndConvert();
            break;
        case BinaryType::LogicalAnd:
        case BinaryType::LogicalOr:
            // handled above
            UNREACHABLE();
            break;
        default:
            UNREACHABLE();
            break;
        }
    }

    void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
    {
        // if (cond != 0) then ifTrue else ifFalse
        EmitValue(op->GetCondition(), *out);
        EmitNonZeroAsI32(*out);

        std::vector<Instr> thenBody;
        std::vector<Instr> elseBody;

        EmitValue(op->GetIfTrue(), thenBody);
        EmitValue(op->GetIfFalse(), elseBody);

        out->emplace_back(Instr::If(TT::numType, std::move(thenBody), std::move(elseBody)));
    }

    void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
    {
        // Normal call (tail-call optimization is implemented in tail-mode lowering).
        for (auto& arg : op->GetOperands())
        {
            EmitValue(arg, *out);
        }

        out->emplace_back(Instr::Simple("call", { names.GetFuncName(op->GetDefinition()) }));
    }
};

template<typename TNumber>
struct FuncLoweringContext
{
    using TT = TypeTraits<TNumber>;

    const OperatorDefinition* currentDefinition = nullptr; // for self-tail-call check
    std::string retLabel = "$ret";
    std::string entryLabel = "$entry";

    // Locals
    std::vector<std::string> paramNames;  // "$arg0", "$arg1", ...
    std::vector<std::string> argTmpNames; // "$argtmp0", ...
    std::string tmpLocal = "$tmp";        // scratch local for stores
    std::string idxLocal = "$idx";        // scratch local for array indices
};

template<typename TNumber>
void EmitTailExpression(const std::shared_ptr<const Operator>& op,
                        ValueEmitter<TNumber>& valueEmitter, const NameResolver& names,
                        const WasmTextOptions& opt, FuncLoweringContext<TNumber>& fctx,
                        std::vector<Instr>& out)
{
    using TT = TypeTraits<TNumber>;

    // Special case: ( ... ) in tail position
    if (auto parenthesis = std::dynamic_pointer_cast<const ParenthesisOperator>(op))
    {
        auto& ops = parenthesis->GetOperators();
        if (ops.empty())
        {
            out.emplace_back(Instr::Simple(std::string(TT::ConstOp()), { "0" }));
            out.emplace_back(Instr::Simple("br", { fctx.retLabel }));
            return;
        }

        for (size_t i = 0; i < ops.size(); i++)
        {
            if (i + 1 < ops.size())
            {
                valueEmitter.EmitValue(ops[i], out);
                out.emplace_back(Instr::Simple("drop"));
            }
            else
            {
                EmitTailExpression<TNumber>(ops[i], valueEmitter, names, opt, fctx, out);
            }
        }
        return;
    }

    // Special case: conditional in tail position
    if (auto conditional = std::dynamic_pointer_cast<const ConditionalOperator>(op))
    {
        // condition (i32)
        valueEmitter.EmitValue(conditional->GetCondition(), out);
        // (cond != 0) -> i32
        out.emplace_back(Instr::Simple(std::string(TT::ConstOp()), { "0" }));
        out.emplace_back(Instr::Simple(std::string(TT::NeOp())));

        std::vector<Instr> thenBody;
        std::vector<Instr> elseBody;

        EmitTailExpression<TNumber>(conditional->GetIfTrue(), valueEmitter, names, opt, fctx,
                                    thenBody);
        EmitTailExpression<TNumber>(conditional->GetIfFalse(), valueEmitter, names, opt, fctx,
                                    elseBody);

        out.emplace_back(Instr::If(std::nullopt, std::move(thenBody), std::move(elseBody)));

        // Both branches should terminate by br/br, so reaching here is unexpected.
        out.emplace_back(Instr::Simple("unreachable"));
        return;
    }

    // Special case: self tail call => convert to loop
    if (auto call = std::dynamic_pointer_cast<const UserDefinedOperator>(op))
    {
        if (call->IsTailCall().value_or(false) && fctx.currentDefinition != nullptr &&
            call->GetDefinition() == *fctx.currentDefinition)
        {
            auto operands = call->GetOperands();
            assert(operands.size() == fctx.paramNames.size());
            assert(operands.size() == fctx.argTmpNames.size());

            // Evaluate all arguments into temporaries (to preserve original parameter values).
            for (size_t i = 0; i < operands.size(); i++)
            {
                valueEmitter.EmitValue(operands[i], out);
                out.emplace_back(Instr::Simple("local.set", { fctx.argTmpNames[i] }));
            }

            // Assign temps to params
            for (size_t i = 0; i < operands.size(); i++)
            {
                out.emplace_back(Instr::Simple("local.get", { fctx.argTmpNames[i] }));
                out.emplace_back(Instr::Simple("local.set", { fctx.paramNames[i] }));
            }

            // Jump to loop entry
            out.emplace_back(Instr::Simple("br", { fctx.entryLabel }));
            return;
        }
    }

    // Default: compute value, then return via br $ret
    valueEmitter.EmitValue(op, out);
    out.emplace_back(Instr::Simple("br", { fctx.retLabel }));
}

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

template<typename TNumber>
FuncDef LowerOneFunctionToIR(const OperatorInformation& info, const CompilationContext& context,
                             const NameResolver& names, const WasmTextOptions& opt)
{
    using TT = TypeTraits<TNumber>;

    FuncDef f;
    if (info.isMain)
    {
        f.internalName = "$main";
        f.exportName = opt.mainExportName;
    }
    else
    {
        f.internalName = names.GetFuncName(info.definition);
        f.exportName = std::nullopt;
    }

    // Params
    std::vector<std::string> paramNames;
    paramNames.reserve(static_cast<size_t>(info.definition.GetNumOperands()));

    for (int i = 0; i < info.definition.GetNumOperands(); i++)
    {
        std::string name = "$arg" + std::to_string(i);
        f.params.push_back({ name, TT::numType });
        paramNames.push_back(name);
    }

    // Locals
    // - tmp: for store expressions (local.tee)
    // - argtmps: for self-tail-call argument evaluation (only if needed, but declaring is cheap)
    FuncLoweringContext<TNumber> fctx;
    fctx.currentDefinition = info.isMain ? nullptr : &info.definition;
    fctx.paramNames = paramNames;
    fctx.tmpLocal = "$tmp";
    fctx.idxLocal = "$idx";

    f.locals.push_back({ fctx.tmpLocal, TT::numType });
    f.locals.push_back({ fctx.idxLocal, TT::numType });

    if (!info.isMain)
    {
        for (int i = 0; i < info.definition.GetNumOperands(); i++)
        {
            std::string t = "$argtmp" + std::to_string(i);
            f.locals.push_back({ t, TT::numType });
            fctx.argTmpNames.push_back(t);
        }
    }

    f.result = TT::numType;

    // Emit body:
    // (block $ret (result T)
    //   (loop $entry
    //     <tail lowering of root expression>
    //     unreachable
    //   )
    //   unreachable
    // )
    ValueEmitter<TNumber> valueEmitter(context, names, opt);
    valueEmitter.SetCurrentFunctionLocals(paramNames, fctx.tmpLocal, fctx.idxLocal);

    std::vector<Instr> loopBody;
    EmitTailExpression<TNumber>(info.op, valueEmitter, names, opt, fctx, loopBody);

    // Safety: if the tail lowering didn't terminate for some reason, trap.
    if (loopBody.empty() || loopBody.back().kind != Instr::Kind::Simple ||
        loopBody.back().opcode != "unreachable")
    {
        loopBody.emplace_back(Instr::Simple("unreachable"));
    }

    std::vector<Instr> blockBody;
    blockBody.emplace_back(Instr::Loop(fctx.entryLabel, std::move(loopBody)));
    blockBody.emplace_back(Instr::Simple("unreachable"));

    f.body.emplace_back(Instr::Block(fctx.retLabel, TT::numType, std::move(blockBody)));

    return f;
}

NameResolver BuildNameResolver(const std::shared_ptr<const Operator>& mainOp,
                               const CompilationContext& context, const WasmTextOptions& opt)
{
    NameResolver r;

    // Globals for variables
    auto vars = GatherVariableNames(mainOp, context);
    for (auto v : vars)
    {
        std::string g = "$g_" + opt.globalVarPrefix;

        // The default variable (empty name) must never collide with user-defined names.
        if (v.empty())
        {
            g += "default";
        }
        else
        {
            g += "var_";
            g += SanitizeId(v);
        }

        r.globalsByVar.emplace(std::string(v), std::move(g));
    }

    // Functions for user-defined operators
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        const auto& def = it->second.GetDefinition();
        std::string f = "$" + opt.funcPrefix + SanitizeId(def.GetName());
        r.funcsByDefKey.emplace(r.KeyOf(def), std::move(f));
    }

    return r;
}

template<typename TNumber>
ModuleDef LowerModuleToIR(const std::shared_ptr<const Operator>& mainOp,
                          const CompilationContext& context, const WasmTextOptions& opt)
{
    using TT = TypeTraits<TNumber>;

    ModuleDef m;

    // Imports (env.getchar, env.putchar)
    m.imports.push_back({ opt.importModule, opt.importGetChar, "$getchar", {}, ValType::I32 });
    m.imports.push_back(
        { opt.importModule, opt.importPutChar, "$putchar", { ValType::I32 }, std::nullopt });

    m.imports.push_back(
        { opt.importModule, opt.importMemGet, "$mem_get", { TT::numType }, TT::numType });
    m.imports.push_back({ opt.importModule,
                          opt.importMemSet,
                          "$mem_set",
                          { TT::numType, TT::numType },
                          std::nullopt });

    // Memory for global array
    m.memory.internalName = "$mem";

    // Ensure the fast memory region fits:
    //   bytes = fastMemoryBaseOffsetBytes + fastMemoryLimitElements * sizeof(TNumber)
    // Note: This does not constrain the fallback path, which can represent sparse/unbounded
    // indices.
    uint64_t requiredBytes = static_cast<uint64_t>(opt.fastMemoryBaseOffsetBytes) +
        static_cast<uint64_t>(opt.fastMemoryLimitElements) * static_cast<uint64_t>(TT::byteSize);
    uint32_t requiredPages = static_cast<uint32_t>((requiredBytes + 65535ull) / 65536ull);

    m.memory.minPages = std::max(opt.memoryMinPages, requiredPages);
    m.memory.exportMemory = opt.exportMemory;
    m.memory.exportName = opt.memoryExportName;

    // Names
    NameResolver names = BuildNameResolver(mainOp, context, opt);

    // Globals
    for (auto& kv : names.globalsByVar)
    {
        m.globals.push_back({ kv.second, TT::numType, true, 0 });
    }

    // Functions: main + user-defined
    std::vector<OperatorInformation> infos;
    infos.emplace_back(OperatorDefinition("main", 0), mainOp, true);

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        infos.emplace_back(it->second.GetDefinition(), it->second.GetOperator(), false);
    }

    // stable output
    std::sort(infos.begin() + 1, infos.end(),
              [](auto& a, auto& b) { return a.definition.GetName() < b.definition.GetName(); });

    for (auto& info : infos)
    {
        m.functions.push_back(LowerOneFunctionToIR<TNumber>(info, context, names, opt));
    }

    return m;
}
}

/* =============================================================================
 * Public API
 * ============================================================================= */

template<typename TNumber>
void EmitWatCode(const std::shared_ptr<const Operator>& mainOp, const CompilationContext& context,
                 std::ostream& os, const WasmTextOptions& opt)
{
    // Only i32 / i64 are supported in this emitter.
    static_assert(std::is_same_v<TNumber, int32_t> || std::is_same_v<TNumber, int64_t>,
                  "EmitWatCode currently supports only int32_t and int64_t.");

    ModuleDef m = LowerModuleToIR<TNumber>(mainOp, context, opt);
    WatWriter w(os);
    w.WriteModule(m);
}

// Explicitly instantiate the function for the supported integer types.
template void EmitWatCode<int32_t>(const std::shared_ptr<const Operator>& mainOp,
                                   const CompilationContext& context, std::ostream& os,
                                   const WasmTextOptions& opt);
template void EmitWatCode<int64_t>(const std::shared_ptr<const Operator>& mainOp,
                                   const CompilationContext& context, std::ostream& os,
                                   const WasmTextOptions& opt);
}
