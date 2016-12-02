#include <cassert>
#include <stdexcept>

#include "cat-intel.hpp"


void CAT::init()
{
	int ret;
	struct pqos_config cfg = {};

	cfg.fd_log = STDERR_FILENO;
	cfg.verbose = 0;

	/* PQoS Initialization - Check and initialize CAT and CMT capability */
	ret = pqos_init(&cfg);
	if (ret != PQOS_RETVAL_OK)
	{
		cleanup();
		throw std::runtime_error("Could not initialize PQoS library");
	}

	/* Get CMT capability and CPU info pointer */
	ret = pqos_cap_get(&p_cap, &p_cpu);
	if (ret != PQOS_RETVAL_OK) {
		cleanup();
		throw std::runtime_error("Could not retrieve PQoS capabilities");
	}

	/* Get CPU socket information to set COS */
	p_sockets = pqos_cpu_get_sockets(p_cpu, &sock_count);
	if (p_sockets == NULL)
	{
		cleanup();
		throw std::runtime_error("Could not retrieve CPU socket information\n");
	}

	initialized = true;

	if (auto_reset)
		reset();
}


void CAT::cleanup()
{
	if (!initialized) return; // The library was not initialized...
	if (auto_reset)
		reset();
	if (p_sockets != NULL)
		free(p_sockets);
	int ret = pqos_fini();
	if (ret != PQOS_RETVAL_OK)
		throw std::runtime_error("Could not shut down PQoS library");
	initialized = false;
}


void CAT::set_cos_mask(uint32_t cos, uint64_t mask, uint32_t socket)
{
	if (!initialized)
		throw std::runtime_error("Could not set mask: init method must be called first");

	struct pqos_l3ca l3ca_cos = {};
	l3ca_cos.class_id = cos;
	l3ca_cos.u.ways_mask = mask;

	int ret = pqos_l3ca_set(p_sockets[socket], 1, &l3ca_cos);
	if  (ret != PQOS_RETVAL_OK)
		throw std::runtime_error("Could not set COS mask");
}


void CAT::set_cos_cpu(uint32_t cos, uint32_t core)
{
	if (!initialized)
		throw std::runtime_error("Could not associate cpu: init method must be called first");

	int ret = pqos_alloc_assoc_set(core, cos);
	if (ret != PQOS_RETVAL_OK)
		throw std::runtime_error("Could not associate core with class of service");
}


uint32_t CAT::get_cos_of_cpu(uint32_t cpu) const
{
	uint32_t cos;
	if (pqos_alloc_assoc_get(cpu, &cos) != PQOS_RETVAL_OK)
		throw std::runtime_error("Could not get COS for CPU " + std::to_string(cpu));
	return cos;
}


uint64_t CAT::get_cos_mask(uint32_t cos, uint32_t socket) const
{
	struct pqos_l3ca l3ca[PQOS_MAX_L3CA_COS];
	uint32_t num_cos;

	if (pqos_l3ca_get(socket, PQOS_MAX_L3CA_COS, &num_cos, l3ca) != PQOS_RETVAL_OK)
		 throw std::runtime_error("Could not get mask for COS" + std::to_string(cos));

	assert(l3ca[cos].class_id == cos);

	return l3ca[cos].u.ways_mask;
}


uint32_t CAT::get_max_num_cos() const
{
	uint32_t max_num_cos;
	if (pqos_l3ca_get_cos_num(p_cap, &max_num_cos) != PQOS_RETVAL_OK)
		throw std::runtime_error("Could not get the max number of Classes Of Service");
	return max_num_cos;
}


void CAT::reset()
{
	if (!initialized)
		throw std::runtime_error("Could not reset: init method must be called first");

	int ret = pqos_alloc_reset(PQOS_REQUIRE_CDP_ANY);
	if (ret != PQOS_RETVAL_OK)
		throw std::runtime_error("CAT reset returned error code " + std::to_string(ret));
}


void CAT::print()
{
}
