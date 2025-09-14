/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Analysis/sparta/AbstractEnvironment.h>
#include <Analysis/sparta/PatriciaTreeMap.h>

namespace sparta {

/*
 * An abstract environment based on Patricia trees that is cheap to copy.
 *
 * In order to minimize the size of the underlying tree, we do not explicitly
 * represent bindings of a variable to the Top element.
 *
 * See AbstractEnvironment.h for more details about abstract
 * environments.
 */
template <typename Variable, typename Domain>
using PatriciaTreeMapAbstractEnvironment = AbstractEnvironment<
    PatriciaTreeMap<Variable, Domain, TopValueInterface<Domain>>>;

} // namespace sparta
