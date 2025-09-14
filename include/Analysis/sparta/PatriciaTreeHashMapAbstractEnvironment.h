/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Analysis/sparta/AbstractEnvironment.h>
#include <Analysis/sparta/PatriciaTreeHashMap.h>

namespace sparta {

/*
 * An abstract environment based on `PatriciaTreeHashMap`.
 *
 * See `AbstractEnvironment` for more information.
 */
template <typename Variable, typename Domain>
using PatriciaTreeHashMapAbstractEnvironment = AbstractEnvironment<
    PatriciaTreeHashMap<Variable, Domain, TopValueInterface<Domain>>>;

} // namespace sparta
