#include "intel-cmt-cat/lib/pqos.h"

class CAT
{
	const struct pqos_cpuinfo *p_cpu;
	const struct pqos_cap *p_cap;
	unsigned *p_sockets;
	unsigned sock_count;
	bool initialized;
	bool auto_reset;


	public:

	CAT() = default;
	CAT(bool auto_reset) : auto_reset(auto_reset) {};

	void init();
	void cleanup();
	void set_cos_mask(uint32_t cos, uint64_t mask, uint32_t socket=0);
	void set_cos_cpu(uint32_t cos, uint32_t core);
	void reset();
	bool is_initialized() { return initialized; }
	void print();
};
