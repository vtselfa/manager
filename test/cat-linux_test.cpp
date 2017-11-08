#include <iostream>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libcpuid.h>

#include "cat-linux.hpp"
#include "cat-intel.hpp"


using testing::Ge;
using testing::Le;
using testing::Eq;
using testing::AllOf;
using testing::AnyOf;


TEST(CATInfo, Read)
{
	auto cat_info = cat_read_info();
	ASSERT_EQ(cat_info.size(), 1U);
	ASSERT_EQ(cat_info["L3"].cache, "L3");
	ASSERT_EQ(cat_info["L3"].cbm_mask, 0xfffffULL);
	ASSERT_THAT(cat_info["L3"].min_cbm_bits, AllOf(Ge(1U),Le(2U)));
	ASSERT_THAT(cat_info["L3"].num_closids, AnyOf(Eq(4U),Eq(16U)));
}

TEST(CPUID, Present)
{
	ASSERT_TRUE(cpuid_present());
}

class CATLinuxTest : public CATLinux
{
	FRIEND_TEST(CATLinuxAPI, SetGetCBM);
	FRIEND_TEST(CATLinuxAPI, Reset);
	FRIEND_TEST(CATLinuxAPI, Init);
	FRIEND_TEST(CATLinuxConsistency, SetCBM);
	FRIEND_TEST(CATLinuxConsistency, AddCPU);
	FRIEND_TEST(CATLinuxConsistency, Reset);
	FRIEND_TEST(CATLinuxConsistency, Init);
	FRIEND_TEST(CATLinuxConsistency, NonContiguousMask);
};

class CATLinuxAPI : public testing::Test
{
	protected:

	CATLinuxTest cat;
	struct cpu_id_t data;

	virtual void SetUp() override
	{
		struct cpu_raw_data_t raw;

		cat = CATLinuxTest();
		cat.init();

		ASSERT_GE(cpuid_get_raw_data(&raw), 0);
		ASSERT_GE(cpu_identify(&raw, &data), 0);
	}

	virtual void TearDown() override
	{
		cat.reset();
	}
};

TEST_F(CATLinuxAPI, SetGetCBM)
{
	auto info = cat.get_info();
	uint32_t clos;
	uint64_t cbm;
	for (clos = 0; clos < cat.get_max_closids(); clos++)
	{
		for (cbm = info.cbm_mask; cbm >= ((1ULL << info.min_cbm_bits) - 1); cbm >>= 1)
		{
			cat.set_cbm(clos, cbm);
			ASSERT_EQ(cat.get_cbm(clos), cbm);
		}
	}

	// This CLOS should not exist...
	ASSERT_THROW(cat.set_cbm(clos, info.cbm_mask), std::runtime_error);
	ASSERT_THROW(cat.get_cbm(clos), std::runtime_error);

	// Invalid CBMs
	ASSERT_THROW(cat.set_cbm(0, 0x0), std::runtime_error);
	ASSERT_THROW(cat.set_cbm(0, cbm), std::runtime_error);
}

TEST_F(CATLinuxAPI, AddCPU)
{
	uint32_t clos;
	int32_t cpu;
	for (clos = 0; clos < cat.get_max_closids(); clos++)
	{
		for (cpu = 0; cpu < data.total_logical_cpus; cpu++)
		{
			cat.add_cpu(clos, cpu);
			ASSERT_EQ(cat.get_clos(cpu), clos);
		}
	}
}

TEST_F(CATLinuxAPI, Init)
{
	auto clos_dirs = cat.get_clos_dirs();
	ASSERT_EQ(clos_dirs.size() + 1, cat.get_max_closids());
}

TEST_F(CATLinuxAPI, Reset)
{
	cat.add_cpu(1, 0);
	cat.add_cpu(2, 1);
	cat.add_cpu(3, 2);
	cat.set_cbm(0, cat.get_info().cbm_mask >> 1);

	cat.reset();
	ASSERT_EQ(cat.get_cbm(0), cat.get_info().cbm_mask);
	for (int32_t cpu = 0; cpu < data.total_logical_cpus; cpu++)
	{
		ASSERT_EQ(cat.get_clos(cpu), 0U);
	}
	ASSERT_EQ(cat.get_cbm(0), cat.get_info().cbm_mask);

	auto clos_dirs = cat.get_clos_dirs();
	ASSERT_EQ(clos_dirs.size(), 0U);
	ASSERT_THROW(cat.set_cbm(1, cat.get_info().cbm_mask), std::runtime_error);
}


class CATLinuxConsistency : public testing::Test
{
	protected:

	static CATIntel icat;

	CATLinuxTest lcat;
	struct cpu_id_t data;

	static void SetUpTestCase()
	{
		icat = CATIntel();
		icat.init();
	}

	virtual void SetUp() override
	{
		struct cpu_raw_data_t raw;

		lcat = CATLinuxTest();
		lcat.init();

		ASSERT_GE(cpuid_get_raw_data(&raw), 0);
		ASSERT_GE(cpu_identify(&raw, &data), 0);
	}

	virtual void TearDown() override
	{
		lcat.reset();
		icat.reset();
	}
};
CATIntel CATLinuxConsistency::icat;

TEST_F(CATLinuxConsistency, SetCBM)
{
	auto info = lcat.get_info();
	for (uint32_t clos = 0; clos < lcat.get_max_closids(); clos++)
	{
		for (uint64_t cbm = info.cbm_mask; cbm >= ((1ULL << info.min_cbm_bits) - 1); cbm >>= 1)
		{
			lcat.set_cbm(clos, cbm);
			ASSERT_EQ(icat.get_cbm(clos), cbm);
		}
	}
}

TEST_F(CATLinuxConsistency, AddCPU)
{
	uint32_t clos;
	int32_t cpu;
	for (clos = 0; clos < lcat.get_max_closids(); clos++)
	{
		for (cpu = 0; cpu < data.total_logical_cpus; cpu++)
		{
			lcat.add_cpu(clos, cpu);
			ASSERT_EQ(icat.get_clos(cpu), clos);
		}
	}
}

TEST_F(CATLinuxConsistency, Reset)
{
	lcat.add_cpu(1, 0);
	lcat.add_cpu(2, 1);
	lcat.add_cpu(3, 2);
	for (uint32_t clos = 0; clos < lcat.get_max_closids(); clos++)
		lcat.set_cbm(clos, lcat.get_info().cbm_mask >> 1);

	lcat.reset();

	for (uint32_t clos = 0; clos < lcat.get_max_closids(); clos++)
		ASSERT_EQ(icat.get_cbm(clos), lcat.get_info().cbm_mask);

	for (int32_t cpu = 0; cpu < data.total_logical_cpus; cpu++)
	{
		ASSERT_EQ(icat.get_clos(cpu), 0U);
	}
	for (uint32_t clos = 0; clos < lcat.get_max_closids(); clos++)
	{
		ASSERT_EQ(icat.get_cbm(clos), lcat.get_info().cbm_mask);
	}
}

TEST_F(CATLinuxConsistency, Init)
{
	for (uint32_t clos = 0; clos < lcat.get_max_closids(); clos++)
	{
		ASSERT_EQ(lcat.get_cbm(clos), lcat.get_info().cbm_mask);
		ASSERT_EQ(icat.get_cbm(clos), lcat.get_info().cbm_mask);
	}

	for (int32_t cpu = 0; cpu < data.total_logical_cpus; cpu++)
	{
		ASSERT_EQ(lcat.get_clos(cpu), 0U);
		ASSERT_EQ(icat.get_clos(cpu), 0U);
	}
}

// Test for support of non-contiguous masks.
// Needs a patched kernel and a patched Intel library
TEST_F(CATLinuxConsistency, NonContiguousMask)
{
	size_t cbm = 0xF0F;
	int clos;

	clos = 0;
	lcat.set_cbm(clos, cbm);
	ASSERT_EQ(lcat.get_cbm(clos), cbm);

	clos = 1;
	icat.set_cbm(clos, cbm);
	ASSERT_EQ(icat.get_cbm(clos), cbm);
}
