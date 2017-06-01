#include <iostream>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libcpuid.h>

#include "cat-linux.hpp"


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

class CATLinuxAPI : public testing::Test
{
	protected:
	class CATLinuxTest : public CATLinux
	{
		FRIEND_TEST(CATLinuxAPI, SetGetCBM);
		FRIEND_TEST(CATLinuxAPI, Reset);
		FRIEND_TEST(CATLinuxAPI, Init);
	};

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
	ASSERT_THROW(cat.set_cbm(0, 0x5), std::runtime_error);
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
