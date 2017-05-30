#include <cassert>
#include <stdexcept>

#include "cat-intel.hpp"
#include "throw-with-trace.hpp"


void CATIntel::init()
{
	int ret;
	struct pqos_config cfg = {};

	cfg.fd_log = STDERR_FILENO;
	cfg.verbose = 0;

	/* PQoS Initialization - Check and initialize CAT and CMT capability */
	ret = pqos_init(&cfg);
	if (ret != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not initialize PQoS library"));

	/* Get CMT capability and CPU info pointer */
	ret = pqos_cap_get(&p_cap, &p_cpu);
	if (ret != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not retrieve PQoS capabilities"));

	/* Get CPU socket information to set COS */
	p_sockets = pqos_cpu_get_sockets(p_cpu, &sock_count);
	if (p_sockets == NULL)
		throw_with_trace(std::runtime_error("Could not retrieve CPU socket information\n"));

	initialized = true;
	reset();
}


void CATIntel::set_cbm(uint32_t cos, uint64_t mask)
{
	if (!initialized)
		throw_with_trace(std::runtime_error("Could not set mask: init method must be called first"));

	struct pqos_l3ca l3ca_cos = {};
	l3ca_cos.class_id = cos;
	l3ca_cos.u.ways_mask = mask;

	uint32_t socket = 0;
	int ret = pqos_l3ca_set(p_sockets[socket], 1, &l3ca_cos);
	if  (ret != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not set COS mask"));
}


void CATIntel::add_cpu(uint32_t clos, uint32_t cpu)
{
	if (!initialized)
		throw_with_trace(std::runtime_error("Could not associate cpu: init method must be called first"));

	int ret = pqos_alloc_assoc_set(cpu, clos);
	if (ret != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not associate core with class of service"));
}


uint32_t CATIntel::get_clos(uint32_t cpu) const
{
	uint32_t clos;
	if (pqos_alloc_assoc_get(cpu, &clos) != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not get COS for CPU " + std::to_string(cpu)));
	return clos;
}


uint64_t CATIntel::get_cbm(uint32_t clos) const
{
	struct pqos_l3ca l3ca[PQOS_MAX_L3CA_COS];
	uint32_t num_cos;
	uint32_t socket = 0;

	if (pqos_l3ca_get(socket, PQOS_MAX_L3CA_COS, &num_cos, l3ca) != PQOS_RETVAL_OK)
		 throw_with_trace(std::runtime_error("Could not get mask for COS" + std::to_string(clos)));

	assert(l3ca[clos].class_id == clos);

	return l3ca[clos].u.ways_mask;
}


uint32_t CATIntel::get_max_closids() const
{
	uint32_t max_num_cos;
	if (pqos_l3ca_get_cos_num(p_cap, &max_num_cos) != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("Could not get the max number of Classes Of Service"));
	return max_num_cos;
}


void CATIntel::reset()
{
	if (!initialized)
		throw_with_trace(std::runtime_error("Could not reset: init method must be called first"));

	int ret = pqos_alloc_reset(PQOS_REQUIRE_CDP_ANY);
	if (ret != PQOS_RETVAL_OK)
		throw_with_trace(std::runtime_error("CAT reset returned error code " + std::to_string(ret)));
}


void CATIntel::print()
{
}
