// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ngraph/pass/graph_rewrite.hpp>
#include <transformations_visibility.hpp>

namespace ov {
namespace pass {

class TRANSFORMATIONS_API BroadcastConstRangeReplacement;

}  // namespace pass
}  // namespace ov

/**
 * @ingroup ie_transformation_common_api
 * @brief BroadcastConstRangeReplacement replaces Constant filled with range values starting from 0 and replaces it with
 * Range op
 */

class ov::pass::BroadcastConstRangeReplacement : public ngraph::pass::MatcherPass {
public:
    OPENVINO_RTTI("BroadcastConstRangeReplacement", "0");
    BroadcastConstRangeReplacement();
};

namespace ngraph {
namespace pass {
using ov::pass::BroadcastConstRangeReplacement;
}  // namespace pass
}  // namespace ngraph
