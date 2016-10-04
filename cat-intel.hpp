#include "pqos.h"

class CAT
{
    const struct pqos_cpuinfo *p_cpu = NULL;
    const struct pqos_cap *p_cap = NULL;
	unsigned *p_sockets = NULL;
    unsigned sock_count = 0;

	void cleanup();

	public:

	CAT();
	~CAT();

	void set_cos_mask(uint32_t cos, uint64_t mask, uint32_t socket=0);
	void set_cos_cpus(uint32_t cos, uint32_t core);
};
