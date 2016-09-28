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

#include <vector>
#include <boost/dynamic_bitset_fwd.hpp>


#define MAX_CPUS 24
#define MAX_WAYS 20


boost::dynamic_bitset<> cos_get_cpus(std::string cos);
void cos_set_cpus(std::string cos, boost::dynamic_bitset<> cpus);

boost::dynamic_bitset<> cos_get_schemata(std::string cos);
void cos_set_schemata(std::string cos, boost::dynamic_bitset<> schemata);

std::vector<std::string> cos_get_tasks(std::string cos);
void cos_set_tasks(std::string cos, std::vector<std::string> tasks);
void cos_reset_tasks(std::string cos);

void cos_create(std::string cos, boost::dynamic_bitset<> schemata, std::vector<std::string> tasks);
void cos_create(std::string cos, boost::dynamic_bitset<> schemata, boost::dynamic_bitset<> cpus, std::vector<std::string> tasks = {});
void cos_delete(std::string cos);

void cat_reset();
