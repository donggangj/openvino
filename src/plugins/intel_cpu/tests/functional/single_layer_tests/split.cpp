// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "shared_test_classes/base/ov_subgraph.hpp"
#include "ngraph_functions/builders.hpp"
#include "test_utils/cpu_test_utils.hpp"

using namespace ov::test;
using namespace CPUTestUtils;

namespace CPULayerTestsDefinitions {

typedef std::tuple<
        size_t,                    // Num splits
        int64_t,                   // Axis
        ElementType,               // Net precision
        InputShape,                // Input shapes
        std::vector<size_t>,       // Used outputs indices
        CPUSpecificParams
> splitCPUTestParams;

class SplitLayerCPUTest : public testing::WithParamInterface<splitCPUTestParams>,
                          virtual public SubgraphBaseTest, public CPUTestsBase {
public:
    static std::string getTestCaseName(testing::TestParamInfo<splitCPUTestParams> obj) {
        size_t numSplits;
        int64_t axis;
        ElementType netPrecision;
        InputShape inputShapes;
        InferenceEngine::SizeVector outIndices;
        CPUSpecificParams cpuParams;
        std::tie(numSplits, axis, netPrecision, inputShapes, outIndices, cpuParams) = obj.param;

        std::ostringstream result;
        result << "IS=";
        result << CommonTestUtils::partialShape2str({inputShapes.first}) << "_";
        result << "TS=";
        for (const auto& shape : inputShapes.second) {
            result << CommonTestUtils::vec2str(shape) << "_";
        }
        result << "numSplits=" << numSplits << "_";
        result << "axis=" << axis << "_";
        if (!outIndices.empty()) {
            result << "outIndices" << CommonTestUtils::vec2str(outIndices) << "_";
        }
        result << "netPRC=" << netPrecision << "_";
        result << CPUTestsBase::getTestCaseName(cpuParams);
        return result.str();
    }

protected:
    void SetUp() override {
        targetDevice = CommonTestUtils::DEVICE_CPU;

        size_t axis, numSplits;
        ElementType netPrecision;
        InputShape inputShapes;
        InferenceEngine::SizeVector outIndices;
        CPUSpecificParams cpuParams;
        std::tie(numSplits, axis, netPrecision, inputShapes, outIndices, cpuParams) = this->GetParam();
        if (outIndices.empty()) {
            for (int i = 0; i < numSplits; ++i) {
                outIndices.push_back(i);
            }
        }

        std::tie(inFmts, outFmts, priority, selectedType) = cpuParams;
        selectedType += std::string("_") + InferenceEngine::details::convertPrecision(netPrecision).name();

        init_input_shapes({inputShapes});

        auto params = ngraph::builder::makeDynamicParams(netPrecision, inputDynamicShapes);
        auto paramOuts = ngraph::helpers::convert2OutputVector(
                ngraph::helpers::castOps2Nodes<ngraph::op::Parameter>(params));
        auto split = std::dynamic_pointer_cast<ngraph::opset5::Split>(ngraph::builder::makeSplit(paramOuts[0],
                                                                                                 netPrecision, numSplits, axis));
        ngraph::ResultVector results;

        for (int i = 0; i < outIndices.size(); i++) {
            // This WA is necessary because result nodes connected to the same output of the split node (or any node) are deduplicated
            // on the CNNNetwork level. It might not be needed when the CPU plugin moves completely to nGraph.
            // This is still a single layer test since the Relu nodes are added only as a WA.

            auto fakeEltwise = std::make_shared<ngraph::opset5::Relu>(split->output(outIndices[i]));
            results.push_back(std::make_shared<ngraph::opset5::Result>(fakeEltwise));
        }
        split->get_rt_info() = getCPUInfo();
        function = std::make_shared<ngraph::Function>(results, params, "split");
    }
};

TEST_P(SplitLayerCPUTest, CompareWithRefs) {
    run();
//     CheckPluginRelatedResults(executableNetwork, "Split");
}

namespace {
const auto planar_4D_ref = CPUSpecificParams{{nchw}, {nchw}, {"ref"}, "ref"};
const auto planar_5D_ref = CPUSpecificParams{{ncdhw}, {ncdhw}, {"ref"}, "ref"};

const auto planar_4D = CPUSpecificParams{{nchw}, {nchw}, {}, "unknown"};
const auto planar_5D = CPUSpecificParams{{ncdhw}, {ncdhw}, {}, "unknown"};

const auto perChannels_4D = CPUSpecificParams{{nhwc}, {nhwc}, {}, "ref"};
const auto perChannels_5D = CPUSpecificParams{{ndhwc}, {ndhwc}, {}, "ref"};

const auto perChannelsToPlanar_4D = CPUSpecificParams{{nhwc}, {nchw}, {}, "ref"};
const auto perChannelsToPlanar_5D = CPUSpecificParams{{ndhwc}, {ncdhw}, {}, "ref"};

const auto blocked8_4D = CPUSpecificParams{{nChw8c}, {nChw8c}, {}, "unknown"};
const auto blocked8_5D = CPUSpecificParams{{nCdhw8c}, {nCdhw8c}, {}, "unknown"};

const auto blocked8_4D_ref = CPUSpecificParams{{nChw8c}, {nChw8c}, {}, "ref"};
const auto blocked8_5D_ref = CPUSpecificParams{{nCdhw8c}, {nCdhw8c}, {}, "ref"};

const auto blocked16_4D = CPUSpecificParams{{nChw16c}, {nChw16c}, {}, "unknown"};
const auto blocked16_5D = CPUSpecificParams{{nCdhw16c}, {nCdhw16c}, {}, "unknown"};

const auto blocked16_4D_ref = CPUSpecificParams{{nChw16c}, {nChw16c}, {}, "ref"};
const auto blocked16_5D_ref = CPUSpecificParams{{nCdhw16c}, {nCdhw16c}, {}, "ref"};

// List of precisions natively supported by onednn.
const std::vector<ElementType> netPrecisions = {
        ElementType::i8,
        ElementType::i32,
        ElementType::f32,
        ElementType::bf16
};

const std::vector<std::vector<size_t>> outIndices3 = {{0, 1, 2}, {0, 1, 1, 0, 2}, {0, 0, 0, 2}};
const std::vector<std::vector<size_t>> outIndices4 = {{0, 1, 2, 3}, {0, 1, 1, 0, 2, 3}, {0, 0, 0, 2, 3}};

const std::vector<InputShape> inputShapes4D_Nspc2NcspSpecial = {
        { {}, {{3, 8, 11, 9}} },
        {
            // dynamic
            {-1, -1, -1, -1},
            // target
            {
                {1, 4, 5, 7},
                {3, 8, 5, 9},
                {5, 16, 1, 8}
            }
        },
        {
            // dynamic
            {{1, 5}, {1, 64}, {1, 25}, {2, 10}},
            // target
            {
                {2, 8, 5, 7},
                {1, 4, 10, 2},
                {3, 16, 5, 9}
            }
        },
        {
            // dynamic
            {{1, 5}, 8, 5, 7},
            // target
            {
                {2, 8, 5, 7},
                {1, 8, 5, 7},
                {2, 8, 5, 7},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_Nspc2NcspSpecial, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(4),
                                ::testing::Values(1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes4D_Nspc2NcspSpecial),
                                ::testing::ValuesIn(outIndices4),
                                ::testing::Values(perChannelsToPlanar_4D)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes5D_Nspc2NcspSpecial = {
        { {}, {{3, 9, 5, 9, 11}} },
        {
            // dynamic
            {-1, -1, -1, -1, -1},
            // target
            {
                {1, 12, 5, 7, 5},
                {3, 6, 8, 9, 1},
                {5, 9, 1, 8, 2}
            }
        },
        {
            // dynamic
            {{1, 5}, {1, 64}, {1, 25}, {2, 10}, {1, 64}},
            // target
            {
                {2, 6, 5, 7, 7},
                {1, 3, 10, 2, 11},
                {3, 9, 4, 9, 8}
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_Nspc2NcspSpecial, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes5D_Nspc2NcspSpecial),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(perChannelsToPlanar_5D)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes4D_planar = {
        { {}, {{3, 24, 24, 9}} },
        {
            // dynamic
            {-1, -1, -1, -1},
            // target
            {
                {1, 15, 12, 9},
                {3, 1, 9, 12},
                {5, 5, 6, 6}
            }
        },
        {
            // dynamic
            {{1, 5}, {1, 64}, {1, 48}, {2, 48}},
            // target
            {
                {2, 5, 6, 9},
                {1, 7, 12, 6},
                {3, 11, 9, 3}
            }
        },
        {
            // dynamic
            {{1, 5}, 5, 6, 9},
            // target
            {
                {2, 5, 6, 9},
                {1, 5, 6, 9},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_planar, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(2, 3),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes4D_planar),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(planar_4D, planar_4D_ref, perChannels_4D)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes4D_block = {
        { {}, {{3, 16, 12, 12}} },
        {
            // dynamic
            {-1, 16, -1, -1},
            // target
            {
                {1, 16, 12, 12},
                {3, 16, 12, 12},
                {5, 16, 12, 12}
            }
        },
        {
            // dynamic
            {{1, 5}, 16, {1, 48}, {2, 24}},
            // target
            {
                {2, 16, 12, 12},
                {1, 16, 12, 12},
                {3, 16, 12, 12}
            }
        },
        {
            // dynamic
            {{1, 5}, 16, 12, 12},
            // target
            {
                {2, 16, 12, 12},
                {1, 16, 12, 12}
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_Block8, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(2, 3),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes4D_block),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(blocked8_4D_ref)),
                        SplitLayerCPUTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_Block16, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(4),
                                ::testing::Values(2, 3),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes4D_block),
                                ::testing::ValuesIn(outIndices4),
                                ::testing::Values(blocked16_4D_ref)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes5D_planar = {
        { {}, {{3, 5, 3, 6, 12}} },
        {
            // dynamic
            {-1, -1, -1, -1, -1},
            // target
            {
                {1, 15, 12, 3, 9},
                {3, 1, 6, 12, 3},
                {5, 5, 6, 6, 6}
            }
        },
        {
            // dynamic
            {{1, 5}, {1, 64}, {1, 48}, {2, 48}, {1, 40}},
            // target
            {
                {2, 5, 12, 3, 6},
                {1, 7, 12, 6, 9},
                {3, 11, 9, 3, 30}
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_planar, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(2, 3, 4),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes5D_planar),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(planar_5D, planar_5D_ref, perChannels_5D)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes5D_block = {
        { {}, {{3, 16, 24, 12, 36}} },
        {
            // dynamic
            {-1, 16, -1, -1, -1},
            // target
            {
                {1, 16, 12, 24, 24},
                {3, 16, 12, 12, 12},
                {5, 16, 12, 12, 24}
            }
        },
        {
            // dynamic
            {{1, 5}, 16, {1, 48}, {2, 24}, {3, 64}},
            // target
            {
                {2, 16, 12, 12, 24},
                {1, 16, 12, 12, 24},
                {3, 16, 12, 12, 12}
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_Block8, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(2, 3, 4),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes5D_block),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(blocked8_5D_ref)),
                        SplitLayerCPUTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_Block16, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(4),
                                ::testing::Values(2, 3, 4),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes5D_block),
                                ::testing::ValuesIn(outIndices4),
                                ::testing::Values(blocked16_5D_ref)),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes3D = {
        { {}, {{14, 28, 21}} },
        {
            // dynamic
            {-1, -1, -1},
            // target
            {
                {7, 21, 14},
                {21, 7, 14},
                {21, 14, 7},
            }
        },
        {
            // dynamic
            {{1, 60}, {1, 50}, {1, 48}},
            // target
            {
                {14, 21, 7},
                {21, 7, 14},
                {7, 14, 21},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split3D, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(7),
                                ::testing::Values(0, 1, 2),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes3D),
                                ::testing::Values(std::vector<size_t>({})),
                                ::testing::Values(CPUSpecificParams{{}, {}, {}, "unknown"}, CPUSpecificParams{{}, {}, {"ref"}, "ref"})),
                                SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes2D = {
        { {}, {{6, 12}} },
        {
            // dynamic
            {-1, -1},
            // target
            {
                {2, 8},
                {10, 4},
                {2, 6},
            }
        },
        {
            // dynamic
            {{1, 60}, {1, 50}},
            // target
            {
                {2, 4},
                {4, 4},
                {6, 12},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split2D, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(2),
                                ::testing::Values(0, 1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes2D),
                                ::testing::Values(std::vector<size_t>({})),
                                ::testing::Values(CPUSpecificParams{{}, {}, {}, "unknown"}, CPUSpecificParams{{}, {}, {"ref"}, "ref"})),
                        SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes1D = {
        { {}, {{10}} },
        {
            // dynamic
            {-1},
            // target
            {
                {5},
                {15},
                {10},
            }
        },
        {
            // dynamic
            {{1, 60}},
            // target
            {
                {15},
                {5},
                {10},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split1D, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(5),
                                ::testing::Values(0),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes1D),
                                ::testing::Values(std::vector<size_t>({})),
                                ::testing::Values(CPUSpecificParams{{}, {}, {}, "unknown"}, CPUSpecificParams{{}, {}, {"ref"}, "ref"})),
                            SplitLayerCPUTest::getTestCaseName);

const std::vector<InputShape> inputShapes4D_dynBatch = {
        {
            // dynamic
            {{1, 10}, 6, 6, 9},
            // target
            {
                {6, 6, 6, 9},
                {9, 6, 6, 9},
            }
        },
};

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_by_batch, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::ValuesIn(inputShapes4D_dynBatch),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(planar_4D, planar_4D_ref, perChannels_4D)),
                        SplitLayerCPUTest::getTestCaseName);

// ============================================== inPlace cases ============================================
INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_Block8inPlace, SplitLayerCPUTest,
                    ::testing::Combine(
                            ::testing::Values(3),
                            ::testing::Values(0, 1),
                            ::testing::ValuesIn(netPrecisions),
                            ::testing::Values(InputShape{ {}, {{3, 24, 24, 9}} }),
                            ::testing::ValuesIn(outIndices3),
                            ::testing::Values(planar_4D, planar_4D_ref, perChannels_4D, blocked8_4D)),
                    SplitLayerCPUTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Split4D_CPU_Block16inPlace, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(4),
                                ::testing::Values(0, 1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::Values(InputShape{ {}, {{4, 64, 32, 12}} }),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(blocked16_4D)),
                        SplitLayerCPUTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_Block8inPlace, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(3),
                                ::testing::Values(0, 1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::Values(InputShape{ {}, {{3, 24, 24, 9, 15}} }),
                                ::testing::ValuesIn(outIndices3),
                                ::testing::Values(planar_5D, planar_5D_ref, perChannels_5D, blocked8_5D)),
                        SplitLayerCPUTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Split5D_CPU_Block16inPlace, SplitLayerCPUTest,
                        ::testing::Combine(
                                ::testing::Values(4),
                                ::testing::Values(0, 1),
                                ::testing::ValuesIn(netPrecisions),
                                ::testing::Values(InputShape{ {}, {{4, 64, 32, 12, 20}} }),
                                ::testing::ValuesIn(outIndices4),
                                ::testing::Values(blocked16_5D)),
                        SplitLayerCPUTest::getTestCaseName);

} // namespace

} // namespace CPULayerTestsDefinitions
