#include "cat.hpp"
#include "intel-cmt-cat/lib/pqos.h"

class CATIntel : public CAT
{
	const struct pqos_cpuinfo *p_cpu;
	const struct pqos_cap *p_cap;
	unsigned *p_sockets;
	unsigned sock_count;

	public:

	CATIntel() = default;
	~CATIntel() = default;

	void init() override;
	void reset() override;

	void set_cbm(uint32_t clos, uint64_t cbm) override;
	void add_cpu(uint32_t clos, uint32_t cpu) override;

	uint32_t get_clos(uint32_t cpu) const override;
	uint64_t get_cbm(uint32_t cos) const override;
	uint32_t get_max_closids() const override;

	void print() override;
};
