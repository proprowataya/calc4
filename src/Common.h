﻿/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef NDEBUG
#define UNREACHABLE() assert(false)
#else
#ifdef _MSC_VER
#define UNREACHABLE() __assume(false)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif // _MSC_VER
#endif // !NDEBUG

#ifdef ENABLE_INT128
namespace std
{
std::ostream& operator<<(std::ostream& dest, __int128_t value);
}
#endif // ENABLE_INT128

namespace calc4
{
std::string_view TrimWhiteSpaces(std::string_view str);

struct CharPosition
{
    size_t index;
    int lineNo;
    int charNo;
};

class AnyNumber
{
private:
    struct AnyBase
    {
        virtual ~AnyBase() {}
        virtual const std::type_info& GetType() const = 0;
        virtual std::string ToString() const = 0;
    };

    template<typename TNumber>
    struct AnyValue : public AnyBase
    {
        TNumber value;

        AnyValue(const TNumber& value) : value(value) {}

        const std::type_info& GetType() const override
        {
            return typeid(TNumber);
        }

        std::string ToString() const override
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    };

    std::shared_ptr<AnyBase> value;

public:
    template<typename TNumber>
    AnyNumber(const TNumber& value) : value(std::make_shared<AnyValue<TNumber>>(value))
    {
    }

    template<typename TNumber>
    const TNumber& GetValue() const
    {
        assert(value->GetType() == typeid(TNumber));
        return reinterpret_cast<const AnyValue<TNumber>*>(value.get())->value;
    }

    std::string ToString() const
    {
        return value->ToString();
    }
};
}
