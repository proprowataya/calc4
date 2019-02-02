#pragma once

template<typename TNumber>
std::shared_ptr<Operator> Optimize(CompilationContext &context, const std::shared_ptr<Operator> &op);
