/*
Copyright (C) 2016  Vicent Selfa (viselol@disca.upv.es)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>


class CAT
{
	protected:

	bool initialized;

	public:

	CAT() = default;
	virtual ~CAT() = default;

	virtual void init() = 0;
	virtual void reset() = 0;

	virtual void set_cbm(uint32_t clos, uint64_t cbm) = 0;
	virtual void add_cpu(uint32_t clos, uint32_t cpu) = 0;

	virtual uint32_t get_clos(uint32_t cpu) const = 0;
	virtual uint64_t get_cbm(uint32_t cos) const = 0;
	virtual uint32_t get_max_closids() const = 0;

	bool is_initialized() const { return initialized; }

	virtual void print() = 0;
};
