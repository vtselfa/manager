#pragma once

#include <vector>

#include "cat-intel.hpp"
#include "task.hpp"


class CAT_Policy
{
	protected:

	CAT cat;

	public:

	CAT_Policy() = default;

	void set_cat(CAT cat)      { this->cat = cat; }
	CAT& get_cat()             { return cat; }
	const CAT& get_cat() const { return cat; }

	virtual ~CAT_Policy() = default;

	// Derived classes should perform their operations here.
	// The base class does nothing by default.
	virtual void adjust(uint64_t, const std::vector<Task> &) {}
};


class CAT_Policy_Slowfirst: public CAT_Policy
{
	protected:

	// This policy will be applied every this number of intervals
	uint64_t every = -1;

	// Masks should be in reversal order of importance i.e. first mask is gonna be used by COS0,
	// wich will contain most of the processes. The greater the COS number the more advantageous
	// is presumed to be. At least that is what the algorithm expects.
	std::vector<uint64_t> masks;

	// Configure CAT with the provided masks
	void set_masks(std::vector<uint64_t> &masks);

	public:

	CAT_Policy_Slowfirst(uint64_t every, std::vector<uint64_t> masks) : CAT_Policy(), every(every), masks(masks) {}

	~CAT_Policy_Slowfirst() = default;

	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.
	virtual void adjust(uint64_t current_interval, const std::vector<Task> &tasklist);
};
