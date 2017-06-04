#pragma once

#include "cat-policy.hpp"
#include "cat-linux.hpp"


namespace cat
{
namespace policy
{


// Sort applications by slowdown and assign the slowest to COS3, the second most slowest
// to COS2, the third to COS1 and the rest to COS0.
class SFL: public Base
{
	protected:

	// This policy will be applied every this number of intervals
	uint64_t every = -1;

	// Masks should be in reversal order of importance i.e. first mask is gonna be used by COS0,
	// wich will contain most of the processes. The greater the COS number the more advantageous
	// is presumed to be. At least that is what the algorithm expects.
	std::vector<uint64_t> masks;

	// Configure CAT with the provided masks
	void set_masks(const std::vector<uint64_t> &masks);
	void check_masks(const std::vector<uint64_t> &masks) const;

	std::shared_ptr<CATLinux> get_cat(){ return std::dynamic_pointer_cast<CATLinux>(cat); }

	public:

	SFL(uint64_t every, const std::vector<uint64_t> &masks) : Base(), every(every), masks(masks)
	{
		check_masks(masks);
	}

	virtual ~SFL() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};


}} // cat::policy
