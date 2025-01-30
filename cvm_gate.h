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

#ifndef CVM_SHARED_H
#include "cvm_shared.h"
#endif

#ifndef CVM_GATE_H
#define CVM_GATE_H

struct cvm_gate_pool
{
    cvm_lockfree_pool available_gates;
};

struct cvm_gate
{
    const struct cvm_sync_primitive_functions* sync_functions;

    struct cvm_gate_pool* pool;

    atomic_uint_fast32_t status;

    mtx_t* mutex;
    cnd_t* condition;
};

void cvm_gate_pool_initialise(struct cvm_gate_pool* pool, size_t capacity_exponent);
void cvm_gate_pool_terminate(struct cvm_gate_pool* pool);


struct cvm_gate* cvm_gate_prepare(struct cvm_gate_pool* pool);

/// used to set up dependencies (calls to cvm_gate_signal) to wait on (dont return from cvm_gate_wait until all signals happen)
/// must know that an gate has at least one outstanding dependency and/or that the gate hasn't been waited on for calling this to be legal
void cvm_gate_add_dependencies(struct cvm_gate* gate, uint_fast32_t dependency_count);

/// must be called once for every dependency added by a call to cvm_gate_add_dependencies
void cvm_gate_signal_dependencies(struct cvm_gate* gate, uint_fast32_t dependency_count);

/// MUST be called at some point as this will clean up the acquired gate
void cvm_gate_wait_and_relinquish(struct cvm_gate* gate);




#endif



