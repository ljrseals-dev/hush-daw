// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/composer.hpp"
#include "hush/composer/lite.hpp"

#if defined(HUSH_HAS_ONNXRUNTIME)
#  include "hush/composer/neural.hpp"
#endif

#include <stdexcept>

namespace hush::composer {

std::unique_ptr<IComposer> makeComposer(Backend which) {
    return makeComposer(which, /*modelPath=*/"");
}

std::unique_ptr<IComposer> makeComposer(Backend which,
                                        const std::string& modelPath) {
    switch (which) {
        case Backend::Lite:
            return std::make_unique<LiteComposer>();

        case Backend::Neural: {
#if defined(HUSH_HAS_ONNXRUNTIME)
            if (modelPath.empty()) {
                throw std::runtime_error(
                    "makeComposer(Backend::Neural): modelPath is required");
            }
            return std::make_unique<NeuralComposer>(modelPath);
#else
            (void)modelPath;
            throw std::runtime_error(
                "makeComposer(Backend::Neural): this binary was built "
                "without ONNX Runtime support");
#endif
        }
    }
    throw std::runtime_error("makeComposer: unknown Backend value");
}

Available availableBackends() {
    Available a;
    a.lite   = true;
#if defined(HUSH_HAS_ONNXRUNTIME)
    a.neural = true;
#else
    a.neural = false;
#endif
    return a;
}

std::unique_ptr<IComposer> makeLiteForTest() {
    return std::make_unique<LiteComposer>();
}

} // namespace hush::composer
