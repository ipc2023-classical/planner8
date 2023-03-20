/***
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#include <dynet_c/api.h>
#include "internal.h"

void dynetTest(void)
{
    dynetDynetParams_t *params;
    int ret = dynetCreateDynetParams(&params);
    ASSERT_RUNTIME(ret == DYNET_C_OK);
    ret = dynetSetDynetParamsRandomSeed(params, 123);
    ASSERT_RUNTIME(ret == DYNET_C_OK);
    ret = dynetSetDynetParamsAutobatch(params, DYNET_C_FALSE);
    ASSERT_RUNTIME(ret == DYNET_C_OK);
    ret = dynetSetDynetParamsSharedParameters(params, DYNET_C_TRUE);
    ASSERT_RUNTIME(ret == DYNET_C_OK);

    ret = dynetInitialize(params);
    ASSERT_RUNTIME(ret == DYNET_C_OK);

    // dynetComputationGraph_t
    // dynetParameterCollection_t
    // dynetApply ...

    dynetDeleteDynetParams(params);
}
