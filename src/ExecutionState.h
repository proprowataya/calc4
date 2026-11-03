/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
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
            // NOTE: buffer[index] is a char. When 'char' is signed (common on x86), values in
            // 0x80..0xFF would be negative and could collide with EOF (-1). Align with
            // std::istream::get() semantics: return 0..255 or EOF.
            return static_cast<int>(static_cast<unsigned char>(buffer[index]));
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

// Store indices near zero in a fixed array and all other indices in adaptive pages.
// All indices default to zero.
template<typename TNumber>
class DefaultGlobalArraySource
{
    // ResetCaches requires default construction that cannot throw.
    static_assert(std::is_nothrow_default_constructible_v<TNumber>);

    // ResetCaches and page promotion require move assignment that cannot throw.
    static_assert(std::is_nothrow_move_assignable_v<TNumber>);

public:
    // Central storage covers a range centered on zero.
    static constexpr size_t CentralArrayLength = 2048;
    static_assert(CentralArrayLength > 0);
    static_assert(CentralArrayLength <= static_cast<size_t>(std::numeric_limits<int>::max()));

    // Each page uses the low bits of an index as its offset.
    static constexpr int PageOffsetBits = 10;
    static constexpr size_t PageLength = static_cast<size_t>(1) << PageOffsetBits;
    static_assert(PageOffsetBits >= 0 && PageOffsetBits < std::numeric_limits<size_t>::digits);

    // Must be a power of two for masked page cache lookup.
    static constexpr size_t PageCacheLength = 64;
    static_assert(PageCacheLength > 0 && (PageCacheLength & (PageCacheLength - 1)) == 0);

    // Nonzero element count that triggers promotion to dense storage.
    static constexpr size_t DenseElementThreshold = 64;
    static_assert(DenseElementThreshold > 0 && DenseElementThreshold <= PageLength);

    // Recorded sparse page access count that triggers promotion to dense storage.
    static constexpr size_t DenseAccessThreshold = 1024;
    static_assert(DenseAccessThreshold > 0);

private:
    struct NumberHash
    {
        size_t operator()(const TNumber& value) const noexcept
        {
            // Anchor optional branches so each can start with else if.
            // This prevents clang-format indentation issues around preprocessor directives.
            if constexpr (false)
            {
            }
#ifdef ENABLE_GMP
            else if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                // GMP exposes the magnitude as unsigned chunks called limbs.
                // Its sign is included separately because limb access ignores it.
                size_t hash = value < 0 ? 1 : 0;
                for (size_t i = 0; i < mpz_size(value.get_mpz_t()); ++i)
                {
                    auto limb = mpz_getlimbn(value.get_mpz_t(), static_cast<mp_size_t>(i));
                    hash = Combine(hash, std::hash<mp_limb_t>{}(limb));
                }

                return hash;
            }
#endif // ENABLE_GMP
#ifdef ENABLE_INT128
            else if constexpr (std::is_same_v<TNumber, __int128_t>)
            {
                // Hash both halves so upper index bits contribute to the result.
                auto bits = static_cast<__uint128_t>(value);
                auto low = static_cast<uint64_t>(bits);
                auto high = static_cast<uint64_t>(bits >> 64);
                return Combine(std::hash<uint64_t>{}(low), std::hash<uint64_t>{}(high));
            }
#endif // ENABLE_INT128
            else
            {
                return std::hash<TNumber>{}(value);
            }
        }

    private:
        static size_t Combine(size_t hash, size_t value) noexcept
        {
            // Similar to the historical Boost hash_combine formula.
            // Reference: https://www.boost.org/doc/libs/1_34_1/doc/html/boost/hash_combine.html
            // The 64-bit constant is floor(2^64 / phi), also listed as Q64 in RFC 2040,
            // Section 5.1. Constant reference:
            // https://www.rfc-editor.org/rfc/rfc2040.html#section-5.1

            static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8);

            // Choose a golden ratio constant that matches the size of size_t.
            constexpr size_t GoldenRatio = sizeof(size_t) == 8
                ? static_cast<size_t>(0x9e3779b97f4a7c15ULL)
                : static_cast<size_t>(0x9e3779b9ULL);

            return hash ^ (value + GoldenRatio + (hash << 6) + (hash >> 2));
        }
    };

    // A page starts with sparse storage and permanently promotes to a dense array.
    class AdaptivePage
    {
    private:
        using SparseValues = std::unordered_map<size_t, TNumber>;
        using DenseValues = std::vector<TNumber>;

        // Identify the page within the global array.
        TNumber pageIndex;

        // Store page values in either sparse or dense form.
        std::variant<SparseValues, DenseValues> values;

        // Count sparse reads and nonzero writes to decide when to promote.
        size_t sparseAccessCount = 0;

        // Count dense nonzero elements to check whether the page is empty.
        size_t denseNonZeroCount = 0;

    public:
        explicit AdaptivePage(const TNumber& pageIndex) : pageIndex(pageIndex) {}

        const TNumber& GetPageIndex() const
        {
            return pageIndex;
        }

        bool IsEmpty() const
        {
            if (const auto* sparse = std::get_if<SparseValues>(&values))
            {
                return sparse->empty();
            }

            return denseNonZeroCount == 0;
        }

        TNumber Get(size_t offset)
        {
            assert(offset < PageLength);

            if (const auto* dense = std::get_if<DenseValues>(&values))
            {
                return (*dense)[offset];
            }

            const auto& sparse = std::get<SparseValues>(values);
            auto it = sparse.find(offset);

            // Copy the value before an access can replace sparse storage during promotion.
            TNumber value = it != sparse.end() ? it->second : static_cast<TNumber>(0);

            RecordSparseAccess();
            return value;
        }

        void Set(size_t offset, const TNumber& value)
        {
            assert(offset < PageLength);

            if (auto* dense = std::get_if<DenseValues>(&values))
            {
                bool wasZero = (*dense)[offset] == static_cast<TNumber>(0);
                bool isZero = value == static_cast<TNumber>(0);
                (*dense)[offset] = value;

                if (wasZero != isZero)
                {
                    if (isZero)
                    {
                        assert(denseNonZeroCount > 0);
                        --denseNonZeroCount;
                    }
                    else
                    {
                        assert(denseNonZeroCount < PageLength);
                        ++denseNonZeroCount;
                    }
                }

                return;
            }

            auto& sparse = std::get<SparseValues>(values);

            // Zero is represented by the absence of a sparse dictionary entry.
            if (value == static_cast<TNumber>(0))
            {
                sparse.erase(offset);
                return;
            }

            sparse.insert_or_assign(offset, value);
            RecordSparseAccess();
        }

    private:
        void RecordSparseAccess()
        {
            // Count reads and nonzero writes that actually reach sparse page storage.
            ++sparseAccessCount;

            if (std::get<SparseValues>(values).size() >= DenseElementThreshold ||
                sparseAccessCount >= DenseAccessThreshold)
            {
                PromoteToDense();
            }
        }

        void PromoteToDense()
        {
            auto& sparse = std::get<SparseValues>(values);
            DenseValues dense(PageLength);
            denseNonZeroCount = sparse.size();

            for (auto& [offset, value] : sparse)
            {
                dense[offset] = std::move(value);
            }

            // Replacing sparse storage must not throw after its values have been moved.
            static_assert(noexcept(values = std::move(dense)));
            values = std::move(dense);
        }
    };

    struct PageResolution
    {
        AdaptivePage* page;
        TNumber pageIndex;
        size_t pageOffset;
        size_t cacheIndex;
    };

    // The central range avoids all page lookup overhead.
    std::vector<TNumber> array;

    // These pages contain values outside the central range.
    // Reads may update caches and promote pages without changing observable values.
    mutable std::unordered_map<TNumber, AdaptivePage, NumberHash> pages;

    // Direct mapped cache of recently resolved pages.
    mutable std::array<AdaptivePage*, PageCacheLength> pageCache{};

    // Cache the most recent value read or written outside central storage.
    // Index zero marks this cache as invalid because central storage handles that index.
    mutable TNumber cachedSparseIndex = 0;
    mutable TNumber cachedSparseValue = 0;

public:
    DefaultGlobalArraySource() : array(CentralArrayLength) {}

    // Copied pages have new addresses, so caches are rebuilt on demand.
    DefaultGlobalArraySource(const DefaultGlobalArraySource& other)
        : array(other.array), pages(other.pages)
    {
    }

    DefaultGlobalArraySource(DefaultGlobalArraySource&& other)
        : array(CentralArrayLength), pages(std::move(other.pages))
    {
        // Allocate replacement storage first, then leave it with the source.
        array.swap(other.array);

        // Rebuild destination caches on demand and clear the source caches.
        other.ResetCaches();
    }

    DefaultGlobalArraySource& operator=(DefaultGlobalArraySource other)
    {
        array.swap(other.array);
        pages.swap(other.pages);

        // Only this instance needs a reset because the temporary is about to be destroyed.
        ResetCaches();

        return *this;
    }

    TNumber Get(const TNumber& index) const
    {
        size_t arrayIndex;
        if (TryGetArrayIndex(index, arrayIndex))
        {
            return array[arrayIndex];
        }

        // Repeated reads outside central storage bypass page lookup.
        if (cachedSparseIndex == index)
        {
            return cachedSparseValue;
        }

        return GetSparseValue(index);
    }

    void Set(const TNumber& index, const TNumber& value)
    {
        size_t arrayIndex;
        if (TryGetArrayIndex(index, arrayIndex))
        {
            array[arrayIndex] = value;
            return;
        }

        SetSparseValue(index, value);
    }

private:
    // A successful result guarantees a valid central array position.
    bool TryGetArrayIndex(const TNumber& index, size_t& arrayIndex) const
    {
        assert(array.size() == CentralArrayLength);

        constexpr int CentralStart = -static_cast<int>(CentralArrayLength / 2);
        constexpr int CentralEnd = CentralStart + static_cast<int>(CentralArrayLength);
        // Check bounds before subtraction to avoid signed integer overflow.
        if (index < CentralStart || index >= CentralEnd)
        {
            return false;
        }

        arrayIndex = ToSize(index - CentralStart);
        return true;
    }

    TNumber GetSparseValue(const TNumber& index) const
    {
        auto resolved = ResolvePage(index);
        TNumber value = resolved.page != nullptr ? resolved.page->Get(resolved.pageOffset)
                                                 : static_cast<TNumber>(0);
        CacheSparseValue(index, value);
        return value;
    }

    void SetSparseValue(const TNumber& index, const TNumber& value)
    {
        auto [page, pageIndex, pageOffset, cacheIndex] = ResolvePage(index);

        if (page == nullptr)
        {
            // Writing zero to an absent page neither allocates storage nor evicts a cached page.
            if (value == static_cast<TNumber>(0))
            {
                CacheSparseValue(index, value);
                return;
            }

            page = &pages.try_emplace(pageIndex, pageIndex).first->second;
        }

        // An exception during a page update could leave the cache inconsistent with page storage.
        // Invalidate the value cache before updating the page.
        cachedSparseIndex = 0;

        // Write the value and allow the page to promote to dense storage if needed.
        page->Set(pageOffset, value);
        // Remove pages once all their values are zero.
        if (page->IsEmpty())
        {
            pages.erase(pageIndex);
            page = nullptr;
        }

        // Update both caches after writing the value and removing any empty page.
        pageCache[cacheIndex] = page;
        CacheSparseValue(index, value);
    }

    PageResolution ResolvePage(const TNumber& index) const
    {
#ifdef ENABLE_GMP
        // mpz_class uses arithmetic right shift through mpz_fdiv_q_2exp.
        // Its shift operation is not constexpr and cannot be checked with static_assert.
        if constexpr (!std::is_same_v<TNumber, mpz_class>)
#endif // ENABLE_GMP
        {
            static_assert((static_cast<TNumber>(-1) >> PageOffsetBits) == static_cast<TNumber>(-1),
                          "Arithmetic right shift is required.");
        }

        // Bits above the page offset form the signed page number.
        TNumber pageIndex = index >> PageOffsetBits;
        TNumber pageOffset = index & static_cast<TNumber>(PageLength - 1);

        // The low bits of the page number select a slot in the power of two cache.
        size_t cacheIndex = ToSize(pageIndex & static_cast<TNumber>(PageCacheLength - 1));
        AdaptivePage* page = pageCache[cacheIndex];

        // If the requested page is not cached, look it up in the page dictionary.
        if (page == nullptr || page->GetPageIndex() != pageIndex)
        {
            auto it = pages.find(pageIndex);
            page = it != pages.end() ? &it->second : nullptr;

            // Missing pages leave the existing cache slot unchanged.
            if (page != nullptr)
            {
                pageCache[cacheIndex] = page;
            }
        }

        return { page, std::move(pageIndex), ToSize(pageOffset), cacheIndex };
    }

    void ResetCaches() noexcept
    {
        pageCache.fill(nullptr);

        // Assigning zero retains GMP allocations, so use TNumber{} to release them
        // through move assignment.
        cachedSparseIndex = TNumber{};
        cachedSparseValue = TNumber{};
    }

    // Keep the cached index and value synchronized.
    void CacheSparseValue(const TNumber& index, const TNumber& value) const
    {
        // Invalidate the value cache so a failed value assignment leaves it disabled.
        cachedSparseIndex = 0;
        cachedSparseValue = value;

        // Set the index last to enable lookups for the updated value.
        cachedSparseIndex = index;
    }

    // Only bounded storage offsets are converted to size_t.
    static size_t ToSize(const TNumber& value)
    {
#ifdef ENABLE_GMP
        if constexpr (std::is_same_v<TNumber, mpz_class>)
        {
            return static_cast<size_t>(value.get_ui());
        }
        else
#endif // ENABLE_GMP
        {
            return static_cast<size_t>(value);
        }
    }
};
}
