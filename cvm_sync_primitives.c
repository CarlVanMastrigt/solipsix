/**
Copyright 2024 Carl van Mastrigt

This file is part of cvm_shared.

cvm_shared is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

cvm_shared is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with cvm_shared.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "cvm_shared.h"

#warning do we want a syetem to spin up new workers if/when a gate is stalled? (trylock or atomic checking only?)


void cvm_sync_establish_ordering_typed(union cvm_sync_primitive* a, union cvm_sync_primitive* b)
{
    b->sync_functions->add_dependency(b);
    // because sucessor may signal the dependency immediately, b must have a dependency set first
    a->sync_functions->add_successor(a,b);
}

void cvm_sync_establish_ordering(void* a, void* b)
{
    cvm_sync_establish_ordering_typed(a, b);
}
