/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2024 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef ENABLE_GMP
#include <gmpxx.h>
#endif // ENABLE_GMP

namespace calc4
{
template<typename TNumber>
class DefaultVariableSource;

template<typename TNumber>
class DefaultGlobalArraySource;

struct DefaultInputSource
{
    int operator()() const
    {
        static_assert(std::is_same_v<decltype(std::cin.get()), int>);
        static_assert(std::char_traits<char>::eof() == -1);
        return std::cin.get();
    }
};

struct BufferedInputSource
{
private:
    std::string_view buffer;
    size_t nextIndex;

public:
    BufferedInputSource(std::string_view buffer) : buffer(buffer), nextIndex(0) {}

    int operator()()
    {
        size_t index = nextIndex;

        if (index < buffer.length())
        {
            nextIndex++;
            return static_cast<int>(buffer[index]);
        }

        return -1;
    }
};

struct StreamInputSource
{
private:
    std::istream* stream;

public:
    StreamInputSource(std::istream* stream) : stream(stream) {}

    int operator()() const
    {
        static_assert(std::is_same_v<decltype(stream->get()), int>);
        static_assert(std::char_traits<char>::eof() == -1);
        return stream->get();
    }
};

struct DefaultPrinter
{
    void operator()(char c) const
    {
        std::cout << c;
    }
};

struct BufferedPrinter
{
private:
    std::string* buffer;

public:
    BufferedPrinter(std::string* buffer) : buffer(buffer) {}

    void operator()(char c) const
    {
        buffer->push_back(c);
    }
};

struct StreamPrinter
{
private:
    std::ostream* stream;

public:
    StreamPrinter(std::ostream* stream) : stream(stream) {}

    void operator()(char c) const
    {
        *stream << c;
    }
};

template<typename TNumber, typename TVariableSource = DefaultVariableSource<TNumber>,
         typename TGlobalArraySource = DefaultGlobalArraySource<TNumber>,
         typename TInputSource = DefaultInputSource, typename TPrinter = DefaultPrinter>
class ExecutionState
{
private:
    TVariableSource variableSource;
    TGlobalArraySource arraySource;
    TInputSource inputSource;
    TPrinter printer;

public:
    ExecutionState() {}

    ExecutionState(TInputSource inputSource, TPrinter printer)
        : inputSource(inputSource), printer(printer)
    {
    }

    TVariableSource& GetVariableSource()
    {
        return variableSource;
    }

    TGlobalArraySource& GetArraySource()
    {
        return arraySource;
    }

    const TVariableSource& GetVariableSource() const
    {
        return variableSource;
    }

    const TGlobalArraySource& GetArraySource() const
    {
        return arraySource;
    }

    int GetChar()
    {
        return inputSource();
    }

    void PrintChar(char c) const
    {
        printer(c);
    }
};

template<typename TNumber>
class DefaultVariableSource
{
private:
    std::unordered_map<std::string, TNumber> variables;

public:
    TNumber Get(const std::string& variableName) const
    {
        auto it = variables.find(variableName);
        if (it != variables.end())
        {
            return it->second;
        }
        else
        {
            return static_cast<TNumber>(0);
        }
    }

    void Set(const std::string& variableName, const TNumber& value)
    {
        variables[variableName] = value;
    }

    const TNumber* TryGet(const std::string& variableName) const
    {
        auto it = variables.find(variableName);
        if (it != variables.end())
        {
            return &*it;
        }
        else
        {
            return nullptr;
        }
    }
};

template<typename TNumber>
class DefaultGlobalArraySource
{
public:
    using IndexType = long long;

private:
    static constexpr IndexType DefaultArraySize = 1024;

    // Values whose indices are frequently accessed are stored in this array
    std::vector<TNumber> array;

    // The others are stored in the dictionary
    std::unordered_map<IndexType, TNumber> dictionary;

public:
    DefaultGlobalArraySource() : array(DefaultArraySize), dictionary() {}

    DefaultGlobalArraySource(size_t arraySize) : array(arraySize), dictionary() {}

    TNumber Get(const TNumber& index) const
    {
        IndexType casted = ToIndexType(index);

        if (IsInArray(casted))
        {
            return array[casted];
        }
        else
        {
            auto it = dictionary.find(casted);
            if (it != dictionary.end())
            {
                return it->second;
            }
            else
            {
                return static_cast<TNumber>(0);
            }
        }
    }

    void Set(const TNumber& index, const TNumber& value)
    {
        IndexType casted = ToIndexType(index);

        if (IsInArray(casted))
        {
            array[casted] = value;
        }
        else
        {
            if (value == static_cast<TNumber>(0))
            {
                // When storing zero, we remove the corresponding element from the dictionary
                dictionary.erase(casted);
            }
            else
            {
                // Otherwise, we normally stored
                dictionary[casted] = value;
            }
        }
    }

private:
    bool IsInArray(IndexType index) const
    {
        using UnsignedIndexType = std::make_unsigned_t<IndexType>;
        return static_cast<UnsignedIndexType>(index) < static_cast<UnsignedIndexType>(array.size());
    }

    static IndexType ToIndexType(const TNumber& value)
    {
#ifdef ENABLE_GMP
        if constexpr (std::is_same<TNumber, mpz_class>::value)
        {
            if (!value.fits_slong_p())
            {
                throw std::overflow_error("Index is out of range.");
            }

            return static_cast<IndexType>(value.get_si());
        }
        else
#endif // ENABLE_GMP
        {
            return static_cast<IndexType>(value);
        }
    }
};
}
