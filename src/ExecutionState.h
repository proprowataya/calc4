#pragma once

#include "Common.h"
#include <gmpxx.h>
#include <unordered_map>
#include <vector>

template<typename TNumber>
class DefaultVariableSource;

template<typename TNumber>
class Calc4GlobalArraySource;

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource>
class ExecutionState
{
private:
    TVariableSource variableSource;
    TGlobalArraySource arraySource;

public:
    ExecutionState() {}

    ExecutionState(TVariableSource&& variableSource, TGlobalArraySource&& arraySource)
        : variableSource(variableSource), arraySource(arraySource)
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
class Calc4GlobalArraySource
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
    Calc4GlobalArraySource() : array(DefaultArraySize), dictionary() {}

    Calc4GlobalArraySource(size_t arraySize) : array(arraySize), dictionary() {}

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
        if constexpr (std::is_same<TNumber, mpz_class>::value)
        {
            if (!value.fits_slong_p())
            {
                throw std::overflow_error("Index is out of range.");
            }

            return static_cast<IndexType>(value.get_si());
        }
        else
        {
            return static_cast<IndexType>(value);
        }
    }
};
