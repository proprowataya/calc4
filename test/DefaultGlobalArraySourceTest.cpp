/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "ExecutionState.h"
#include "TestCommon.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
template<typename TNumber>
class DefaultGlobalArraySourceTest : public ::testing::Test
{
public:
    using Source = calc4::DefaultGlobalArraySource<TNumber>;

    static constexpr int PageLength = static_cast<int>(Source::PageLength);

    // Choose a positive page boundary beyond the central array length.
    static constexpr int PositivePageStart =
        (static_cast<int>(Source::CentralArrayLength) / PageLength + 1) * PageLength;

    // Treat the negated boundary as the exclusive end of the negative page.
    static constexpr int NegativePageStart = -PositivePageStart - PageLength;
};

// clang-format off
using NumberTypes = ::testing::Types<int32_t, int64_t
#ifdef ENABLE_INT128
                                    , __int128_t
#endif // ENABLE_INT128
#ifdef ENABLE_GMP
                                    , mpz_class
#endif // ENABLE_GMP
                                    >;
// clang-format on

TYPED_TEST_SUITE(DefaultGlobalArraySourceTest, NumberTypes);

TYPED_TEST(DefaultGlobalArraySourceTest, IndicesRemainDistinctAcrossCentralBoundaries)
{
    using Source = typename TestFixture::Source;

    // Find the first and last indices stored in the central array.
    constexpr int CentralStart = -static_cast<int>(Source::CentralArrayLength) / 2;
    constexpr int CentralEnd = CentralStart + static_cast<int>(Source::CentralArrayLength) - 1;

    Source source;

    // Assign distinct values just inside and outside both boundaries.
    source.Set(CentralStart - 1, 1);
    source.Set(CentralStart, 2);
    source.Set(CentralEnd, 3);
    source.Set(CentralEnd + 1, 4);

    // Verify that crossing either boundary does not mix values.
    ASSERT_EQ_HELPER(TypeParam, 1, source.Get(CentralStart - 1));
    ASSERT_EQ_HELPER(TypeParam, 2, source.Get(CentralStart));
    ASSERT_EQ_HELPER(TypeParam, 3, source.Get(CentralEnd));
    ASSERT_EQ_HELPER(TypeParam, 4, source.Get(CentralEnd + 1));
}

TYPED_TEST(DefaultGlobalArraySourceTest, ExtremeIndicesRemainDistinct)
{
    using Source = typename TestFixture::Source;

    TypeParam firstIndex;
    TypeParam secondIndex;
#ifdef ENABLE_GMP
    if constexpr (std::is_same_v<TypeParam, mpz_class>)
    {
        // Exercise GMP with indices beyond the 64 bit integer range.
        secondIndex = static_cast<TypeParam>(1) << 100;
        firstIndex = -secondIndex;
    }
    else
#endif // ENABLE_GMP
    {
        // Cover the minimum and maximum indices of each fixed size integer type.
        firstIndex = std::numeric_limits<TypeParam>::min();
        secondIndex = std::numeric_limits<TypeParam>::max();
    }

    TypeParam firstValue = 1;
    TypeParam secondValue = 2;
    TypeParam centerValue = 3;
    Source source;

    // Store distinct values at both extremes and at zero.
    source.Set(firstIndex, firstValue);
    source.Set(secondIndex, secondValue);
    source.Set(0, centerValue);

    // Verify that the distant indices remain independent.
    ASSERT_EQ_HELPER(TypeParam, firstValue, source.Get(firstIndex));
    ASSERT_EQ_HELPER(TypeParam, secondValue, source.Get(secondIndex));
    ASSERT_EQ_HELPER(TypeParam, centerValue, source.Get(0));
}

TYPED_TEST(DefaultGlobalArraySourceTest, PagesPreserveValuesAcrossElementCountPromotion)
{
    using Source = typename TestFixture::Source;

    // Ensure that the element threshold can be reached within one page.
    constexpr int ValueCount = static_cast<int>(Source::DenseElementThreshold);
    static_assert(ValueCount > 0 && ValueCount <= TestFixture::PageLength);

    // Exercise complete pages outside the central array on both sides of zero.
    for (int firstIndex : { TestFixture::PositivePageStart, TestFixture::NegativePageStart })
    {
        Source source;

        // Fill one page until its element count triggers promotion.
        for (int i = 0; i < ValueCount; i++)
        {
            int index = firstIndex + i;
            source.Set(index, -index);
        }

        // Verify that promotion preserves all stored values.
        for (int i = 0; i < ValueCount; i++)
        {
            int index = firstIndex + i;
            ASSERT_EQ_HELPER(TypeParam, -index, source.Get(index));
        }

        // Clear alternating values after promotion to exercise dense page updates.
        for (int i = 0; i < ValueCount; i += 2)
        {
            source.Set(firstIndex + i, 0);
        }

        // Verify that cleared values read as zero and other values remain unchanged.
        for (int i = 0; i < ValueCount; i++)
        {
            int index = firstIndex + i;
            int expected = i % 2 == 0 ? 0 : -index;
            ASSERT_EQ_HELPER(TypeParam, expected, source.Get(index));
        }
    }
}

TYPED_TEST(DefaultGlobalArraySourceTest, RepeatedWritesPreserveValues)
{
    using Source = typename TestFixture::Source;

    constexpr int FirstIndex = TestFixture::PositivePageStart;
    constexpr int SecondIndex = FirstIndex + 1;
    constexpr int WriteCount = static_cast<int>(Source::DenseAccessThreshold);

    Source source;

    // Store two distinct nonzero values in the same page outside central storage.
    source.Set(FirstIndex, 1);
    source.Set(SecondIndex, 43);

    // Repeated writes trigger promotion without reaching the element count threshold.
    for (int i = 1; i <= WriteCount; i++)
    {
        source.Set(FirstIndex, i);
    }

    // Read SecondIndex before FirstIndex because the last write cached FirstIndex.
    // This makes both reads access page storage instead of the exact value cache.
    ASSERT_EQ_HELPER(TypeParam, 43, source.Get(SecondIndex));
    ASSERT_EQ_HELPER(TypeParam, WriteCount, source.Get(FirstIndex));

    // Clear FirstIndex. The nonzero count falls from two to one, so the page must stay alive.
    source.Set(FirstIndex, 0);

    // Write a new nonzero value to FirstIndex. The count must increase from one to two.
    source.Set(FirstIndex, 42);

    // Clear SecondIndex. FirstIndex still holds 42.
    // The nonzero count falls to one, so the page must stay alive.
    source.Set(SecondIndex, 0);

    // The last write cached SecondIndex, so this read checks page storage.
    // A missing count increment would have removed the page and lost the new value.
    ASSERT_EQ_HELPER(TypeParam, 42, source.Get(FirstIndex));

    // Clear FirstIndex again. The count reaches zero, so the page should now be removed.
    source.Set(FirstIndex, 0);

    // Read SecondIndex after page removal and replace the cached zero for FirstIndex.
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(SecondIndex));

    // Read FirstIndex from storage to verify that the value 42 is no longer visible.
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(FirstIndex));
}

TYPED_TEST(DefaultGlobalArraySourceTest, RepeatedReadsPreserveValues)
{
    using Source = typename TestFixture::Source;

    constexpr int FirstIndex = TestFixture::PositivePageStart;
    constexpr int SecondIndex = FirstIndex + 1;
    constexpr int ReadIterationCount = static_cast<int>(Source::DenseAccessThreshold);

    Source source;

    // Store two distinct nonzero values in the same page outside central storage.
    source.Set(FirstIndex, 42);
    source.Set(SecondIndex, 43);

    // Alternate indices to bypass the exact value cache and trigger promotion through reads.
    // Each iteration records two page accesses, so this exceeds the promotion threshold.
    for (int i = 0; i < ReadIterationCount; i++)
    {
        ASSERT_EQ_HELPER(TypeParam, 42, source.Get(FirstIndex));
        ASSERT_EQ_HELPER(TypeParam, 43, source.Get(SecondIndex));
    }
}

TYPED_TEST(DefaultGlobalArraySourceTest, ValuesCanBeClearedAndWrittenAgain)
{
    using Source = typename TestFixture::Source;

    // Choose two indices in the same page outside the central array.
    constexpr int Index = TestFixture::PositivePageStart;
    constexpr int OtherIndex = Index + 1;

    Source source;

    // Store a value and read it from the value cache.
    source.Set(Index, 42);
    ASSERT_EQ_HELPER(TypeParam, 42, source.Get(Index));

    // Read another offset so the next read of Index accesses page storage.
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(OtherIndex));
    ASSERT_EQ_HELPER(TypeParam, 42, source.Get(Index));

    // Clear the only value to remove the page, then check the value cache and page storage.
    source.Set(Index, 0);
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(Index));
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(OtherIndex));
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(Index));

    // Recreate the page with a different value, then check the value cache and page storage.
    source.Set(Index, 43);
    ASSERT_EQ_HELPER(TypeParam, 43, source.Get(Index));
    ASSERT_EQ_HELPER(TypeParam, 0, source.Get(OtherIndex));
    ASSERT_EQ_HELPER(TypeParam, 43, source.Get(Index));
}

TYPED_TEST(DefaultGlobalArraySourceTest, PageCacheCollisionsPreserveValues)
{
    using Source = typename TestFixture::Source;

    // Use more pages than cache slots so at least two pages must share a slot.
    constexpr int PageCount = static_cast<int>(Source::PageCacheLength) + 1;
    constexpr int FirstIndex = TestFixture::PositivePageStart;

    Source source;

    // Write distinct nonzero values at page starts outside central storage.
    for (int i = 0; i < PageCount; i++)
    {
        int index = FirstIndex + i * TestFixture::PageLength;
        source.Set(index, -index);
    }

    // Verify that every value survives cache collisions.
    for (int i = 0; i < PageCount; i++)
    {
        int index = FirstIndex + i * TestFixture::PageLength;
        ASSERT_EQ_HELPER(TypeParam, -index, source.Get(index));
    }

    // Update the first page after reading all pages.
    source.Set(FirstIndex, 1);

    // Verify the updated value and all unchanged values.
    for (int i = 0; i < PageCount; i++)
    {
        int index = FirstIndex + i * TestFixture::PageLength;
        int expected = i == 0 ? 1 : -index;
        ASSERT_EQ_HELPER(TypeParam, expected, source.Get(index));
    }
}

TYPED_TEST(DefaultGlobalArraySourceTest, CopiesPreserveIndependentValues)
{
    using Source = typename TestFixture::Source;

    constexpr int SparseIndex = TestFixture::PositivePageStart;
    constexpr int DenseIndex = TestFixture::NegativePageStart;
    constexpr int ValueCount = static_cast<int>(Source::DenseElementThreshold);

    // Prepare central storage, one sparse page, and one dense page.
    Source original;
    original.Set(0, 7);
    original.Set(SparseIndex, 42);
    for (int i = 0; i < ValueCount; i++)
    {
        original.Set(DenseIndex + i, i + 1);
    }

    // Populate the page cache and value cache before copying.
    ASSERT_EQ_HELPER(TypeParam, 42, original.Get(SparseIndex));

    // Copy the prepared state using the copy constructor.
    Source copied(original);

    // Give the assignment destination different values and cached pages.
    // Cache SparseIndex last so the first sparse read detects a stale value cache.
    Source assigned;
    assigned.Set(0, -1);
    assigned.Set(DenseIndex, -1);
    assigned.Set(SparseIndex, -1);
    assigned = original;

    // Change the source in all three storage regions to check that copies are independent.
    original.Set(0, -1);
    original.Set(SparseIndex, -1);
    original.Set(DenseIndex, -1);

    // Both copies must still contain every value from the original state.
    for (const auto* source : { &copied, &assigned })
    {
        ASSERT_EQ_HELPER(TypeParam, 7, source->Get(0));
        ASSERT_EQ_HELPER(TypeParam, 42, source->Get(SparseIndex));
        for (int i = 0; i < ValueCount; i++)
        {
            ASSERT_EQ_HELPER(TypeParam, i + 1, source->Get(DenseIndex + i));
        }

        // Read SparseIndex again after dense values to bypass the value cache.
        ASSERT_EQ_HELPER(TypeParam, 42, source->Get(SparseIndex));
    }
}

TYPED_TEST(DefaultGlobalArraySourceTest, MovesPreserveValues)
{
    using Source = typename TestFixture::Source;

    constexpr int SparseIndex = TestFixture::PositivePageStart;
    constexpr int DenseIndex = TestFixture::NegativePageStart;
    constexpr int ValueCount = static_cast<int>(Source::DenseElementThreshold);

    // Prepare central storage, one sparse page, and one dense page.
    Source original;
    original.Set(0, 7);
    original.Set(SparseIndex, 42);
    for (int i = 0; i < ValueCount; i++)
    {
        original.Set(DenseIndex + i, i + 1);
    }

    // Populate the page cache and value cache before moving.
    ASSERT_EQ_HELPER(TypeParam, 42, original.Get(SparseIndex));

    // Move construction must transfer all three storage regions.
    Source moved(std::move(original));
    ASSERT_EQ_HELPER(TypeParam, 7, moved.Get(0));
    ASSERT_EQ_HELPER(TypeParam, 42, moved.Get(SparseIndex));
    for (int i = 0; i < ValueCount; i++)
    {
        ASSERT_EQ_HELPER(TypeParam, i + 1, moved.Get(DenseIndex + i));
    }

    // Read SparseIndex again after dense values to bypass the value cache.
    ASSERT_EQ_HELPER(TypeParam, 42, moved.Get(SparseIndex));

    // Give the assignment destination different values and cached pages.
    // Cache SparseIndex last so the first sparse read detects a stale value cache.
    Source assigned;
    assigned.Set(0, -1);
    assigned.Set(DenseIndex, -1);
    assigned.Set(SparseIndex, -1);
    assigned = std::move(moved);

    // Move assignment must preserve the same values as move construction.
    ASSERT_EQ_HELPER(TypeParam, 7, assigned.Get(0));
    ASSERT_EQ_HELPER(TypeParam, 42, assigned.Get(SparseIndex));
    for (int i = 0; i < ValueCount; i++)
    {
        ASSERT_EQ_HELPER(TypeParam, i + 1, assigned.Get(DenseIndex + i));
    }

    // Read SparseIndex again after dense values to bypass the value cache.
    ASSERT_EQ_HELPER(TypeParam, 42, assigned.Get(SparseIndex));
}
}
