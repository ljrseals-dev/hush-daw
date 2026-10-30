// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "onnx_session.hpp"

#include <onnxruntime_cxx_api.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace hush::composer::neural {

namespace {

/// Format an ORT exception into a std::string with the error code
/// included. Every throw in this file goes through this helper so
/// nothing is silently swallowed.
std::string describeOrtFailure(const Ort::Exception& e) {
    std::ostringstream os;
    os << "onnxruntime: " << e.what()
       << " (code " << static_cast<int>(e.GetOrtErrorCode()) << ")";
    return os.str();
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct OnnxSession::Impl {
    Ort::Env     env{ORT_LOGGING_LEVEL_WARNING, "hush-neural"};
    Ort::Session session{nullptr};

    std::string inputName;
    std::string outputName;
    int         vocabSize = 0;

    // Reused across run() calls so the hot path does not allocate.
    // Not thread-safe; see the header.
    std::vector<Ort::Value> inputTensors;
    std::vector<Ort::Value> outputTensors;

    explicit Impl(const std::string& path,
                  bool useCuda,
                  int  intraOpThreads)
        : session(nullptr) {

        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (intraOpThreads > 0)
            opts.SetIntraOpNumThreads(intraOpThreads);

#if defined(HUSH_NEURAL_WITH_CUDA)
        if (useCuda) {
            OrtCUDAProviderOptions cuda{};
            cuda.device_id = 0;
            opts.AppendExecutionProvider_CUDA(cuda);
        } else {
            (void)useCuda;
        }
#else
        if (useCuda) {
            throw std::runtime_error(
                "OnnxSession: useCuda=true but the build was not "
                "compiled with HUSH_NEURAL_WITH_CUDA. Rebuild with "
                "-DHUSH_NEURAL_WITH_CUDA=ON, or pass useCuda=false.");
        }
        (void)intraOpThreads;
#endif

        try {
#if defined(_WIN32)
            std::wstring wide(path.begin(), path.end());
            session = Ort::Session(env, wide.c_str(), opts);
#else
            session = Ort::Session(env, path.c_str(), opts);
#endif
        } catch (const Ort::Exception& e) {
            throw std::runtime_error(
                "failed to load model '" + path + "': "
                + describeOrtFailure(e));
        }

        Ort::AllocatorWithDefaultOptions alloc;

        if (session.GetInputCount() != 1) {
            std::ostringstream os;
            os << "model must have exactly 1 input, has "
               << session.GetInputCount();
            throw std::runtime_error(os.str());
        }
        if (session.GetOutputCount() != 1) {
            std::ostringstream os;
            os << "model must have exactly 1 output, has "
               << session.GetOutputCount();
            throw std::runtime_error(os.str());
        }

        {
            auto n = session.GetInputNameAllocated(0, alloc);
            inputName = n.get();
        }
        {
            auto n = session.GetOutputNameAllocated(0, alloc);
            outputName = n.get();
        }

        const auto outInfo    = session.GetOutputTypeInfo(0);
        const auto tensorInfo = outInfo.GetTensorTypeAndShapeInfo();
        const auto shape      = tensorInfo.GetShape();

        if (shape.size() != 3 || shape[2] <= 0) {
            std::ostringstream os;
            os << "model output shape must be [1, seq, vocab]; got [";
            for (size_t k = 0; k < shape.size(); ++k) {
                if (k) os << ", ";
                os << shape[k];
            }
            os << "]";
            throw std::runtime_error(os.str());
        }
        vocabSize = static_cast<int>(shape[2]);
    }
};

// ---------------------------------------------------------------------------
// OnnxSession
// ---------------------------------------------------------------------------

OnnxSession::OnnxSession(const std::string& modelPath,
                         bool useCuda,
                         int  intraOpThreads)
    : impl_(std::make_unique<Impl>(modelPath, useCuda, intraOpThreads)) {}

OnnxSession::~OnnxSession() = default;

int OnnxSession::vocabSize() const {
    return impl_->vocabSize;
}

std::string OnnxSession::inputName() const {
    return impl_->inputName;
}

std::string OnnxSession::outputName() const {
    return impl_->outputName;
}

std::vector<float> OnnxSession::run(const int* inputIds,
                                    int seqLen,
                                    int& vocabSizeOut) {
    const auto all = runAllPositions(inputIds, seqLen, vocabSizeOut);
    const size_t v = static_cast<size_t>(vocabSizeOut);
    const size_t lastOffset = static_cast<size_t>(seqLen - 1) * v;
    std::vector<float> last(v);
    std::memcpy(last.data(), all.data() + lastOffset,
                v * sizeof(float));
    return last;
}

std::vector<float> OnnxSession::runAllPositions(const int* inputIds,
                                                int seqLen,
                                                int& vocabSizeOut) {
    if (seqLen <= 0)
        throw std::invalid_argument(
            "OnnxSession::runAllPositions: seqLen must be > 0");
    if (inputIds == nullptr)
        throw std::invalid_argument(
            "OnnxSession::runAllPositions: inputIds is null");

    vocabSizeOut = impl_->vocabSize;

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    // The model expects int64 input. Convert from int.
    std::vector<int64_t> inputIds64(static_cast<size_t>(seqLen));
    for (int i = 0; i < seqLen; ++i)
        inputIds64[static_cast<size_t>(i)] = inputIds[i];

    const int64_t inputShape[2] = {1, static_cast<int64_t>(seqLen)};

    impl_->inputTensors.clear();
    impl_->inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memInfo,
        inputIds64.data(), inputIds64.size(),
        inputShape, 2));

    const char* inNames[]  = { impl_->inputName.c_str()  };
    const char* outNames[] = { impl_->outputName.c_str() };

    impl_->outputTensors.clear();
    try {
        impl_->outputTensors = impl_->session.Run(
            Ort::RunOptions{nullptr},
            inNames, impl_->inputTensors.data(), 1,
            outNames, 1);
    } catch (const Ort::Exception& e) {
        throw std::runtime_error("inference failed: "
                                 + describeOrtFailure(e));
    }

    if (impl_->outputTensors.empty())
        throw std::runtime_error(
            "inference returned no output tensor");

    auto info  = impl_->outputTensors[0].GetTensorTypeAndShapeInfo();
    const auto shape = info.GetShape();
    if (shape.size() != 3 ||
        shape[0] != 1 ||
        shape[1] != static_cast<int64_t>(seqLen) ||
        shape[2] != impl_->vocabSize) {
        std::ostringstream os;
        os << "inference output shape mismatch; expected [1, "
           << seqLen << ", " << impl_->vocabSize << "], got [";
        for (size_t k = 0; k < shape.size(); ++k) {
            if (k) os << ", ";
            os << shape[k];
        }
        os << "]";
        throw std::runtime_error(os.str());
    }

    const float* data = impl_->outputTensors[0].GetTensorData<float>();
    const size_t total = static_cast<size_t>(seqLen)
                       * static_cast<size_t>(impl_->vocabSize);
    return std::vector<float>(data, data + total);
}

} // namespace hush::composer::neural
