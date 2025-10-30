#include <algorithm>
#include <ranges>
#include <utility>
#include <gtest/gtest.h>

#include "../include/CObjectPool.hpp"

using namespace ObjectPool;

namespace Tests::ObjectPool
{
// Define a test object which can be used with the ObjectPool
struct CColor
{
	uint8_t r = 255u;
	uint8_t g = 255u;
	uint8_t b = 255u;
};

TEST(ObjectPool, Constructor_Default)
{
	auto colorPool = CObjectPool<CColor>(5);
	EXPECT_EQ(colorPool.Size(), 5);
	EXPECT_EQ(colorPool.ObjectsInUse(), 0);

	// Verify all objects are initialized with default constructor
	for (size_t idx = 0; idx < 5; ++idx)
	{
		EXPECT_FALSE(colorPool.IsInUse(idx));
		CColor* pColor = colorPool[idx];
		EXPECT_NE(pColor, nullptr);
		EXPECT_EQ(pColor->r, 255u);
		EXPECT_EQ(pColor->g, 255u);
		EXPECT_EQ(pColor->b, 255u);
	}
}

TEST(ObjectPool, Constructor_WithArgs)
{
	constexpr size_t poolSize = 3;
	auto colorPool = CObjectPool<CColor>(poolSize, 255u, 128u, 64u);
	EXPECT_EQ(colorPool.Size(), poolSize);

	// Verify all objects are initialized with provided arguments
	for (size_t idx = 0; idx < 3; ++idx)
	{
		const CColor* pColor = colorPool[idx];
		EXPECT_EQ(pColor->r, 255u);
		EXPECT_EQ(pColor->g, 128u);
		EXPECT_EQ(pColor->b, 64u);
	}
}

TEST(ObjectPool, Overflow)
{
	auto colorPool = CObjectPool<CColor>(1);
	size_t idx;
	auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(idx, 0);
	result = colorPool.UseNext(idx);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), EPoolError::FULL);
}

TEST(ObjectPool, Use_Specific_Position)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Use position 2
	const auto color = colorPool.Use(2);
	ASSERT_TRUE(color.has_value());
	const CColor* pColor = color.value();
	EXPECT_NE(pColor, nullptr);
	EXPECT_TRUE(colorPool.IsInUse(2));
	EXPECT_EQ(colorPool.ObjectsInUse(), 1);

	// Try to use same position again - should return error
	const auto color2 = colorPool.Use(2);
	ASSERT_FALSE(color2.has_value());
	EXPECT_EQ(color2.error(), EPoolError::ALREADY_IN_USE);
	EXPECT_EQ(colorPool.ObjectsInUse(), 1);

	// Use another position
	const auto color3 = colorPool.Use(4);
	ASSERT_TRUE(color3.has_value());
	EXPECT_TRUE(colorPool.IsInUse(4));
	EXPECT_EQ(colorPool.ObjectsInUse(), 2);
}

TEST(ObjectPool, Use_OutOfBounds)
{
	auto colorPool = CObjectPool<CColor>(3);

	// Try to use position beyond pool size - should return error
	const auto color = colorPool.Use(4);
	ASSERT_FALSE(color.has_value());
	EXPECT_EQ(color.error(), EPoolError::OUT_OF_RANGE);
}

TEST(ObjectPool, UseNext_Sequential)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t useNextIdx;

	// Use objects sequentially
	for (size_t poolIdx = 0; poolIdx < 5; ++poolIdx)
	{
		const auto result = colorPool.UseNext(useNextIdx);
		ASSERT_TRUE(result.has_value());
		const CColor* pColor = result.value();
		EXPECT_NE(pColor, nullptr);
		EXPECT_EQ(useNextIdx, poolIdx);
		EXPECT_TRUE(colorPool.IsInUse(poolIdx));
	}

	EXPECT_EQ(colorPool.ObjectsInUse(), 5);

	// Pool is full - should return error
	const auto result = colorPool.UseNext(useNextIdx);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), EPoolError::FULL);
}

TEST(ObjectPool, UseNext_WithGaps)
{
	auto colorPool = CObjectPool<CColor>(3);
	size_t idx;

	// Use positions 0, 1, 2
	auto result = colorPool.UseNext(idx);
	result = colorPool.UseNext(idx);
	result = colorPool.UseNext(idx);
	// filled all positions

	// UnUse position 1 to create a gap
	const auto result1 = colorPool.UnUse(1);
	ASSERT_TRUE(result1.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 2);

	// UseNext should find the gap at position 1
	result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	const CColor* pColor = result.value();
	EXPECT_NE(pColor, nullptr);
	EXPECT_EQ(idx, 1);
	EXPECT_EQ(colorPool.ObjectsInUse(), 3);
}

TEST(ObjectPool, Get_Method)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Get on unused object should return error
	auto resultGet = colorPool.Get(2);
	ASSERT_FALSE(resultGet.has_value());
	EXPECT_EQ(resultGet.error(), EPoolError::NOT_IN_USE);

	// Use object at position 2
	auto resultUse = colorPool.UseNext(idx); // 0
	ASSERT_TRUE(resultUse.has_value());
	resultUse = colorPool.Use(2);
	ASSERT_TRUE(resultUse.has_value());

	// Get should now return valid pointer
	const auto resultGet2 = colorPool.Get(2);
	ASSERT_TRUE(resultGet2.has_value());
	CColor* pColor2 = resultGet2.value();
	EXPECT_NE(pColor2, nullptr);

	// Modify and verify
	pColor2->r = 100;
	const auto result3 = colorPool.Get(2);
	ASSERT_TRUE(result3.has_value());
	const CColor* pColor3 = result3.value();
	EXPECT_EQ(pColor3->r, 100);

	// Get out of bounds should return error
	auto result4 = colorPool.Get(10);
	ASSERT_FALSE(result4.has_value());
	EXPECT_EQ(result4.error(), EPoolError::OUT_OF_RANGE);
}

TEST(ObjectPool, Get_Const)
{
	auto colorPool = CObjectPool<CColor>(3, 50, 100, 150);
	size_t idx;
	const auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());

	const auto& constPool = colorPool;

	// Get on used object
	const auto result1 = constPool.Get(0);
	ASSERT_TRUE(result1.has_value());
	const CColor* pColor = result1.value();
	EXPECT_NE(pColor, nullptr);
	EXPECT_EQ(pColor->r, 50);

	// Get on unused object
	const auto result2 = constPool.Get(1);
	ASSERT_FALSE(result2.has_value());
	EXPECT_EQ(result2.error(), EPoolError::NOT_IN_USE);

	// Get out of bounds
	const auto result3 = constPool.Get(10);
	ASSERT_FALSE(result3.has_value());
	EXPECT_EQ(result3.error(), EPoolError::OUT_OF_RANGE);
}

TEST(ObjectPool, IsInUse)
{
	auto colorPool = CObjectPool<CColor>(5);

	// All should be unused initially
	for (size_t idx = 0; idx < 5; ++idx)
	{
		EXPECT_FALSE(colorPool.IsInUse(idx));
	}

	// Use some objects
	auto result = colorPool.Use(1);
	ASSERT_TRUE(result.has_value());
	result = colorPool.Use(3);
	ASSERT_TRUE(result.has_value());

	EXPECT_FALSE(colorPool.IsInUse(0));
	EXPECT_TRUE(colorPool.IsInUse(1));
	EXPECT_FALSE(colorPool.IsInUse(2));
	EXPECT_TRUE(colorPool.IsInUse(3));
	EXPECT_FALSE(colorPool.IsInUse(4));

	// Out of bounds should return false
	EXPECT_FALSE(colorPool.IsInUse(10));
}

TEST(ObjectPool, UnUse_NoArgs)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	auto result = colorPool.UseNext(idx); // 0
	ASSERT_TRUE(result.has_value());
	// skip 1
	// Use object at position 2
	result = colorPool.Use(2);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 2);
	EXPECT_TRUE(colorPool.IsInUse(2));

	// Modify the object
	auto result2 = colorPool.Get(2);
	ASSERT_TRUE(result2.has_value());
	CColor* pColor = result2.value();
	pColor->r = 200;

	// UnUse should reset it and return success
	auto resultUnuse2 = colorPool.UnUse(2);
	ASSERT_TRUE(resultUnuse2.has_value());
	EXPECT_FALSE(colorPool.IsInUse(2));
	EXPECT_EQ(colorPool.ObjectsInUse(), 1);

	// Object should be reset (default constructed)
	result2 = colorPool.Use(2);
	ASSERT_TRUE(result2.has_value());
	const auto resultGet2 = colorPool.Get(2);
	ASSERT_TRUE(resultGet2.has_value());
	const CColor* pColor2 = resultGet2.value();
	EXPECT_NE(pColor2->r, 200);

	// UnUse on already unused object should return error
	const auto resultUnuse3 = colorPool.UnUse(3);
	ASSERT_FALSE(resultUnuse3.has_value());
	EXPECT_EQ(resultUnuse3.error(), EPoolError::ALREADY_UNUSED);
	const auto resultUnuse3Again = colorPool.UnUse(3);
	ASSERT_FALSE(resultUnuse3Again.has_value());
	EXPECT_EQ(resultUnuse3Again.error(), EPoolError::ALREADY_UNUSED);
}

TEST(ObjectPool, UnUse_WithArgs)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Use and modify object at position 1
	auto result1 = colorPool.Use(1);
	ASSERT_TRUE(result1.has_value());
	result1 = colorPool.Get(1);
	ASSERT_TRUE(result1.has_value());
	CColor* pColor = result1.value();
	pColor->r = 50;
	pColor->g = 60;

	// UnUse with custom arguments
	const auto result = colorPool.UnUse(1, 10, 20, 30);
	ASSERT_TRUE(result.has_value());
	EXPECT_FALSE(colorPool.IsInUse(1));

	// Object should be reconstructed with new values
	result1 = colorPool.Use(1);
	const auto result2 = colorPool.Get(1);
	ASSERT_TRUE(result2.has_value());
	const CColor* pColor2 = result2.value();
	EXPECT_EQ(pColor2->r, 10);
	EXPECT_EQ(pColor2->g, 20);
	EXPECT_EQ(pColor2->b, 30);

	// UnUse on unused object should return error
	const auto result3 = colorPool.UnUse(3, 1, 2, 3);
	ASSERT_FALSE(result3.has_value());
	EXPECT_EQ(result3.error(), EPoolError::ALREADY_UNUSED);
}

TEST(ObjectPool, Replace_NoArgs)
{
	auto colorPool = CObjectPool<CColor>(5, 100, 100, 100);

	// Modify object at position 2
	auto* color = colorPool[2];
	color->r = 255;

	// Replace should reset it to default constructed
	const auto result = colorPool.Replace(2);
	ASSERT_TRUE(result.has_value());
	EXPECT_FALSE(colorPool.IsInUse(2));

	const auto* color2 = colorPool[2];
	EXPECT_NE(color2->r, 100); // Should be default constructed, not with constructor args
	EXPECT_EQ(color2->r, 255); // 255 = default value
}

TEST(ObjectPool, Replace_WithArgs)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Replace with custom values
	const auto result = colorPool.Replace(3, 11, 22, 33);
	ASSERT_TRUE(result.has_value());
	EXPECT_FALSE(colorPool.IsInUse(3));

	const auto* color = colorPool[3];
	EXPECT_EQ(color->r, 11);
	EXPECT_EQ(color->g, 22);
	EXPECT_EQ(color->b, 33);
}

TEST(ObjectPool, Replace_OutOfBounds)
{
	auto colorPool = CObjectPool<CColor>(3);

	// Replace out of bounds should return error
	const auto result1 = colorPool.Replace(5);
	ASSERT_FALSE(result1.has_value());
	EXPECT_EQ(result1.error(), EPoolError::OUT_OF_RANGE);

	const auto result2 = colorPool.Replace(5, 1, 2, 3);
	ASSERT_FALSE(result2.has_value());
	EXPECT_EQ(result2.error(), EPoolError::OUT_OF_RANGE);
}

TEST(ObjectPool, Operator_Brackets)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Test operator[] access
	auto* color0 = colorPool[0];
	auto* color3 = colorPool[3];
	EXPECT_NE(color0, nullptr);
	EXPECT_NE(color3, nullptr);

	// Modify through operator[]
	color0->r = 77;
	color3->g = 88;

	// Verify modifications persist
	EXPECT_EQ(colorPool[0]->r, 77);
	EXPECT_EQ(colorPool[3]->g, 88);
}

TEST(ObjectPool, UseNextReplace_NoArgs)
{
	auto colorPool = CObjectPool<CColor>(3, 50, 50, 50);
	size_t idx;

	// UseNextReplace should replace with default constructor
	auto result = colorPool.UseNextReplace(idx);
	ASSERT_TRUE(result.has_value());
	CColor* pColor = result.value();
	EXPECT_NE(pColor, nullptr);
	EXPECT_EQ(idx, 0);
	EXPECT_TRUE(colorPool.IsInUse(0));

	// Object should be default constructed, not with constructor args
	EXPECT_NE(pColor->r, 50);

	// Continue using
	result = colorPool.UseNextReplace(idx); // 1
	result = colorPool.UseNextReplace(idx); // 2
	EXPECT_EQ(idx, 2);
	EXPECT_EQ(colorPool.ObjectsInUse(), 3);

	// Pool is full
	const auto result2 = colorPool.UseNextReplace(idx);
	ASSERT_FALSE(result2.has_value());
	EXPECT_EQ(result2.error(), EPoolError::FULL);
}

TEST(ObjectPool, UseNextReplace_WithArgs)
{
	auto colorPool = CObjectPool<CColor>(4);
	size_t idx;

	// UseNextReplace with custom arguments
	auto result1 = colorPool.UseNextReplace(idx, 1, 2, 3);
	ASSERT_TRUE(result1.has_value());
	CColor* pColor1 = result1.value();
	EXPECT_NE(pColor1, nullptr);
	EXPECT_EQ(idx, 0);
	EXPECT_EQ(pColor1->r, 1);
	EXPECT_EQ(pColor1->g, 2);

	auto result2 = colorPool.UseNextReplace(idx, 5, 6, 7);
	ASSERT_TRUE(result2.has_value());
	CColor* pColor2 = result2.value();
	EXPECT_NE(pColor2, nullptr);
	EXPECT_EQ(idx, 1);
	EXPECT_EQ(pColor2->r, 5);
	EXPECT_EQ(pColor2->g, 6);

	// Fill the pool
	auto resultFill = colorPool.UseNextReplace(idx, 9, 10, 11);
	ASSERT_TRUE(resultFill.has_value());
	resultFill = colorPool.UseNextReplace(idx, 13, 14, 15);
	ASSERT_TRUE(resultFill.has_value());

	// Pool is full
	const auto resultFull = colorPool.UseNextReplace(idx, 99, 99, 99);
	ASSERT_FALSE(resultFull.has_value());
	EXPECT_EQ(resultFull.error(), EPoolError::FULL);
}

TEST(ObjectPool, Size_And_ObjectsInUse)
{
	auto colorPool = CObjectPool<CColor>(10);

	EXPECT_EQ(colorPool.Size(), 10);
	EXPECT_EQ(colorPool.ObjectsInUse(), 0);

	// Use some objects
	auto result = colorPool.Use(0);
	ASSERT_TRUE(result.has_value());
	result = colorPool.Use(5);
	ASSERT_TRUE(result.has_value());
	result = colorPool.Use(9);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(colorPool.Size(), 10);
	EXPECT_EQ(colorPool.ObjectsInUse(), 3);

	// UnUse one
	const auto result5 = colorPool.UnUse(5);
	ASSERT_TRUE(result5.has_value());

	EXPECT_EQ(colorPool.Size(), 10);
	EXPECT_EQ(colorPool.ObjectsInUse(), 2);
}

TEST(ObjectPool, Complex_Lifecycle)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use first 3 positions
	auto result = colorPool.UseNext(idx); // 0
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx); // 1
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx); // 2
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 3);

	// Modify objects
	colorPool[0]->r = 10;
	colorPool[1]->r = 20;
	colorPool[2]->r = 30;

	// UnUse middle one
	auto resultUnuse = colorPool.UnUse(1);
	ASSERT_TRUE(resultUnuse.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 2);

	// Use remaining slots before it can find 1
	result = colorPool.UseNext(idx); // 3
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx); // 4
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 4);

	// UseNext should fill the gap at 1 now
	result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	const CColor* pColor = result.value();
	EXPECT_EQ(idx, 1);
	EXPECT_NE(pColor->r, 20); // Should be reset

	// UnUse multiple
	resultUnuse = colorPool.UnUse(0);
	ASSERT_TRUE(resultUnuse.has_value());
	resultUnuse = colorPool.UnUse(4);
	ASSERT_TRUE(resultUnuse.has_value());
	EXPECT_EQ(colorPool.ObjectsInUse(), 3);

	// Verify specific states
	EXPECT_FALSE(colorPool.IsInUse(0));
	EXPECT_TRUE(colorPool.IsInUse(1));
	EXPECT_TRUE(colorPool.IsInUse(2));
	EXPECT_TRUE(colorPool.IsInUse(3));
	EXPECT_FALSE(colorPool.IsInUse(4));
}

TEST(ObjectPool, UseNextReplace_FillsGaps)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t pos = 0; pos < 5; ++pos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
	}

	// UnUse position 2
	const auto resultUnuse = colorPool.UnUse(2);
	ASSERT_TRUE(resultUnuse.has_value());

	// UseNextReplace should find and use the gap
	const auto result = colorPool.UseNextReplace(idx, 99, 99, 99);
	ASSERT_TRUE(result.has_value());
	CColor* pColor = result.value();
	EXPECT_NE(pColor, nullptr);
	EXPECT_EQ(idx, 2);
	EXPECT_EQ(pColor->r, 99);
	EXPECT_TRUE(colorPool.IsInUse(2));
}

TEST(ObjectPool, Iterators)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolPos = 0; poolPos < 5; ++poolPos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		CColor* pColor = result.value();
		pColor->r = poolPos;
	}

	// UnUse position 2
	const auto resultUnuse = colorPool.UnUse(2);
	ASSERT_TRUE(resultUnuse.has_value());

	idx = 0;
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it)
	{
		CColor& color = *it;
		EXPECT_EQ(color.r, idx);
		++idx;
		if (idx == 2)
			++idx; // skip 2
	}
}

TEST(ObjectPool, Iterator_DefaultConstructor)
{
	// Test that default constructor works
	const CObjectPool<CColor>::CIterator defaultIt;
	const CObjectPool<CColor>::CIterator anotherDefaultIt;

	// Two default-constructed iterators should be equal
	EXPECT_EQ(defaultIt, anotherDefaultIt);
}

TEST(ObjectPool, Iterator_PostIncrement)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use positions 0, 1, 2
	auto result = colorPool.UseNext(idx); // 0
	ASSERT_TRUE(result.has_value());
	colorPool[0]->r = 10;
	result = colorPool.UseNext(idx); // 1
	ASSERT_TRUE(result.has_value());
	colorPool[1]->r = 20;
	result = colorPool.UseNext(idx); // 2
	ASSERT_TRUE(result.has_value());
	colorPool[2]->r = 30;

	auto it = colorPool.begin();
	EXPECT_EQ(it->r, 10);

	// Test post-increment - returns old value
	const auto oldIt = it++;
	EXPECT_EQ(oldIt->r, 10); // Old iterator still points to first element
	EXPECT_EQ(it->r, 20); // New iterator advanced to second element
}

TEST(ObjectPool, Iterator_ArrowOperator)
{
	auto colorPool = CObjectPool<CColor>(3);
	size_t idx;

	const auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	colorPool[0]->r = 100;
	colorPool[0]->g = 150;
	colorPool[0]->b = 200;

	const auto it = colorPool.begin();

	// Test arrow operator
	EXPECT_EQ(it->r, 100);
	EXPECT_EQ(it->g, 150);
	EXPECT_EQ(it->b, 200);
}

TEST(ObjectPool, Iterator_InequalityOperator)
{
	auto colorPool = CObjectPool<CColor>(3);
	size_t idx;

	auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());

	const auto it1 = colorPool.begin();
	auto it2 = colorPool.begin();
	const auto end = colorPool.end();

	// Same position should be equal (not inequal)
	EXPECT_FALSE(it1 != it2);

	// Different positions should be inequal
	++it2;
	EXPECT_TRUE(it1 != it2);

	// Begin and end should be inequal
	EXPECT_TRUE(it1 != end);
}

TEST(ObjectPool, Iterator_EmptyPool)
{
	auto colorPool = CObjectPool<CColor>(5);

	// No objects in use - Begin should equal End
	const auto begin = colorPool.begin();
	const auto end = colorPool.end();
	EXPECT_EQ(begin, end);
}

TEST(ObjectPool, Iterator_SingleElement)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use only one element
	const auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	colorPool[0]->r = 42;

	auto it = colorPool.begin();
	EXPECT_EQ(it->r, 42);

	// Increment should reach end
	++it;
	EXPECT_EQ(it, colorPool.end());
}

TEST(ObjectPool, Iterator_LastElementOnly)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Use only the last position
	const auto result = colorPool.Use(4);
	ASSERT_TRUE(result.has_value());
	colorPool[4]->r = 99;

	auto it = colorPool.begin();
	EXPECT_EQ(it->r, 99);

	// Only one element, so next should be end
	++it;
	EXPECT_EQ(it, colorPool.end());
}

TEST(ObjectPool, Iterator_SkipMultipleGaps)
{
	auto colorPool = CObjectPool<CColor>(10);

	// Use positions 0, 3, 5, 9 (many gaps)
	auto result = colorPool.Use(0);
	ASSERT_TRUE(result.has_value());
	colorPool[0]->r = 0;
	result = colorPool.Use(3);
	colorPool[3]->r = 3;
	result = colorPool.Use(5);
	colorPool[5]->r = 5;
	result = colorPool.Use(9);
	colorPool[9]->r = 9;

	std::vector<uint8_t> values;
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it)
	{
		values.push_back(it->r);
	}

	EXPECT_EQ(values.size(), 4);
	EXPECT_EQ(values[0], 0);
	EXPECT_EQ(values[1], 3);
	EXPECT_EQ(values[2], 5);
	EXPECT_EQ(values[3], 9);
}

TEST(ObjectPool, Iterator_RangeBasedForLoop)
{
	auto colorPool = CObjectPool<CColor>(5);

	// Use positions 1, 2, 4
	auto result = colorPool.Use(1);
	ASSERT_TRUE(result.has_value());
	colorPool[1]->r = 10;
	result = colorPool.Use(2);
	ASSERT_TRUE(result.has_value());
	colorPool[2]->r = 20;
	result = colorPool.Use(4);
	ASSERT_TRUE(result.has_value());
	colorPool[4]->r = 40;

	std::vector<uint8_t> values;
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it)
	{
		values.push_back(it->r);
	}

	EXPECT_EQ(values.size(), 3);
	EXPECT_EQ(values[0], 10);
	EXPECT_EQ(values[1], 20);
	EXPECT_EQ(values[2], 40);
}

TEST(ObjectPool, Iterator_ModifyThroughIterator)
{
	auto colorPool = CObjectPool<CColor>(3);
	size_t idx;

	auto result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());
	result = colorPool.UseNext(idx);
	ASSERT_TRUE(result.has_value());

	// Modify through iterator
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it)
	{
		it->r = 123;
	}

	// Verify modifications
	EXPECT_EQ(colorPool[0]->r, 123);
	EXPECT_EQ(colorPool[1]->r, 123);
	EXPECT_EQ(colorPool[2]->r, 123);
}

TEST(ObjectPool, Iterator_AllPositionsUsed)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolPos = 0; poolPos < 5; ++poolPos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolPos]->r = poolPos * 10;
	}

	// Iterate through all
	int count = 0;
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it)
	{
		EXPECT_EQ(it->r, count * 10);
		++count;
	}
	EXPECT_EQ(count, 5);
}

TEST(ObjectPool, Range_Based_Loopable)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolPos = 0; poolPos < 5; ++poolPos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		CColor* pColor = result.value();
		pColor->r = 123;
		pColor->g = poolPos;
	}

	// UnUse position 2
	const auto resultUnuse = colorPool.UnUse(2);
	ASSERT_TRUE(resultUnuse.has_value());

	// Use the range-based loop
	for (const auto& color : colorPool)
	{
		EXPECT_EQ(color.r, 123);
		EXPECT_NE(color.g, 2);
	}
}

TEST(ObjectPool, Iterator_STL_FindIf)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolPos = 0; poolPos < 5; ++poolPos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		CColor* pColor = result.value();
		pColor->r = poolPos;
	}

	// UnUse position 2
	const auto result = colorPool.UnUse(2);
	ASSERT_TRUE(result.has_value());

	auto isColor = [](const CColor& color)
	{
		return color.r == 4;
	};
	const auto it = std::find_if(colorPool.begin(), colorPool.end(), isColor);
	EXPECT_EQ(it->r, 4);
}

TEST(ObjectPool, Iterator_Ranges_FindIf)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolPos = 0; poolPos < 5; ++poolPos)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		CColor* pColor = result.value();
		pColor->r = poolPos * 10;
	}

	// UnUse position 2
	const auto result = colorPool.UnUse(2);
	ASSERT_TRUE(result.has_value());

	auto isColor = [](const CColor& color)
	{
		return color.r == 40;
	};
	const auto it = std::ranges::find_if(colorPool.begin(), colorPool.end(), isColor);
	EXPECT_EQ(it->r, 40);
}

TEST(ObjectPool, Iterator_Ranges_CountIf)
{
	auto colorPool = CObjectPool<CColor>(10);
	size_t idx;

	// Use some positions with specific values
	for (size_t poolIdx = 0; poolIdx < 10; ++poolIdx)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolIdx]->r = poolIdx % 2 == 0 ? 100 : 50;
	}

	// UnUse positions 2 and 6
	auto result = colorPool.UnUse(2);
	ASSERT_TRUE(result.has_value());
	result = colorPool.UnUse(6);
	ASSERT_TRUE(result.has_value());

	// Count elements where r == 100
	const auto count = std::ranges::count_if(
		colorPool.begin(), colorPool.end(),
		[](const CColor& color)
		{
			return color.r == 100;
		});

	EXPECT_EQ(count, 3); // positions 0, 4, 8 (2 and 6 are unused)
}

TEST(ObjectPool, Iterator_Ranges_Transform)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolIdx = 0; poolIdx < 5; ++poolIdx)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolIdx]->r = poolIdx;
		colorPool[poolIdx]->g = poolIdx * 2;
	}

	// Use ranges::transform to extract r values
	std::vector<uint8_t> redValues;
	std::ranges::transform(
		colorPool.begin(), colorPool.end(),
		std::back_inserter(redValues),
		[](const CColor& color)
		{
			return color.r;
		});

	EXPECT_EQ(redValues.size(), 5);
	for (size_t poolIdx = 0; poolIdx < 5; ++poolIdx)
	{
		EXPECT_EQ(redValues[poolIdx], poolIdx);
	}
}

TEST(ObjectPool, Iterator_Ranges_AllOf_AnyOf_NoneOf)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions with r >= 10
	for (size_t poolIdx = 0; poolIdx < 5; ++poolIdx)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolIdx]->r = 10 + poolIdx;
	}

	// Test all_of - all elements have r >= 10
	const bool allAbove10 = std::ranges::all_of(
		colorPool.begin(), colorPool.end(),
		[](const CColor& color)
		{
			return color.r >= 10;
		});
	EXPECT_TRUE(allAbove10);

	// Test any_of - at least one element has r == 12
	const bool anyIs12 = std::ranges::any_of(
		colorPool.begin(), colorPool.end(),
		[](const CColor& color)
		{
			return color.r == 12;
		});
	EXPECT_TRUE(anyIs12);

	// Test none_of - no elements have r > 20
	const bool noneAbove20 = std::ranges::none_of(
		colorPool.begin(), colorPool.end(),
		[](const CColor& color)
		{
			return color.r > 20;
		});
	EXPECT_TRUE(noneAbove20);
}

TEST(ObjectPool, Iterator_Ranges_ForEach)
{
	auto colorPool = CObjectPool<CColor>(5);
	size_t idx;

	// Use all positions
	for (size_t poolIdx = 0; poolIdx < 5; ++poolIdx)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolIdx]->r = poolIdx;
	}

	// Use ranges::for_each to modify all elements
	std::ranges::for_each(
		colorPool.begin(), colorPool.end(),
		[](CColor& color)
		{
			color.r += 100;
		});

	// Verify modifications
	int redValue = 0;
	for (auto it = colorPool.begin(); it != colorPool.end(); ++it, ++redValue)
	{
		EXPECT_EQ(it->r, redValue + 100);
	}
}

TEST(ObjectPool, Iterator_Ranges_Views_Filter)
{
	auto colorPool = CObjectPool<CColor>(10);
	size_t idx;

	// Use all positions with different r values
	for (size_t poolIdx = 0; poolIdx < 10; ++poolIdx)
	{
		auto result = colorPool.UseNext(idx);
		ASSERT_TRUE(result.has_value());
		colorPool[poolIdx]->r = poolIdx * 10;
		colorPool[poolIdx]->g = 50;
		colorPool[poolIdx]->b = 100;
	}

	// UnUse position 5
	auto result = colorPool.UnUse(5);
	ASSERT_TRUE(result.has_value());

	// Use a view to filter colors where r >= 40
	auto highRedColors = std::ranges::subrange(
		colorPool.begin(), colorPool.end()
	) | std::views::filter(
		[](const CColor& color)
		{
			return color.r >= 40;
		});

	// Modify only the 'g' value for filtered colors
	for (auto& color : highRedColors)
	{
		color.g = 200; // Only modify g, leave r and b unchanged
	}

	// Verify that only colors with r >= 40 had their g value modified
	EXPECT_EQ(colorPool[0]->g, 50); // r=0, not modified
	EXPECT_EQ(colorPool[1]->g, 50); // r=10, not modified
	EXPECT_EQ(colorPool[2]->g, 50); // r=20, not modified
	EXPECT_EQ(colorPool[3]->g, 50); // r=30, not modified
	EXPECT_EQ(colorPool[4]->g, 200); // r=40, modified
	// position 5 is unused
	EXPECT_EQ(colorPool[6]->g, 200); // r=60, modified
	EXPECT_EQ(colorPool[7]->g, 200); // r=70, modified
	EXPECT_EQ(colorPool[8]->g, 200); // r=80, modified
	EXPECT_EQ(colorPool[9]->g, 200); // r=90, modified

	// Verify r and b values remain unchanged for all
	for (size_t poolIdx = 0; poolIdx < 10; ++poolIdx)
	{
		if (poolIdx == 5)
			continue; // Skip unused position
		EXPECT_EQ(colorPool[poolIdx]->r, poolIdx * 10);
		EXPECT_EQ(colorPool[poolIdx]->b, 100);
	}
}
}
