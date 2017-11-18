#include <boost/accumulators/framework/accumulator_base.hpp>
#include <boost/accumulators/framework/parameters/sample.hpp>

namespace boost {                           // Putting your accumulators in the
namespace accumulators {                    // impl namespace has some


namespace impl {                            // advantages. See below.

template<typename Sample>
struct last_accumulator : accumulator_base
{
	typedef Sample result_type;

	template<typename Args>
	last_accumulator(Args const & args) : last(args[sample | Sample()]) {}

	template<typename Args>
	void operator ()(Args const & args)
	{
		this->last = args[sample];
	}

	result_type result(dont_care) const     // The result function will also be passed
	{                                       // an argument pack, but we don't use it here,
		return this->last;                  // so we use "dont_care" as the argument type.
	}

	private:
	Sample last;
};

} // namespace impl


namespace tag {

struct last : depends_on<> // Features should inherit from depends_on<> to specify dependencies
{
    // Define a nested typedef called 'impl' that specifies which
    // accumulator implements this feature.
    typedef accumulators::impl::last_accumulator<mpl::_1> impl;
};

} // namespace tag


namespace extract {                     // in the 'extract' namespace

extractor<tag::last> const last = {}; // Simply define our extractor with
                                        // our feature tag, like this.
} // namespace extract
using extract::last;                    // Pull the extractor into the
                                        // enclosing namespace.
}} //namespace boost::accumulators
