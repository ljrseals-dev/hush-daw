// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace hush::composer::neural {

/// Thin wrapper over Ort::Session. Isolates every ONNX Runtime call
/// so the rest of the engine does not include the ORT headers and
/// does not need to know the input/output tensor names or shapes.
///
/// Thread-safety: not safe for concurrent run() calls. Each thread
/// needs its own OnnxSession, or an external mutex.
class OnnxSession {
public:
    /// @param modelPath       .onnx file. Throws std::runtime_error
    ///                        if it cannot be loaded.
    /// @param useCuda         Request the CUDA execution provider.
    ///                        Throws std::runtime_error if built
    ///                        without HUSH_NEURAL_WITH_CUDA.
    /// @param intraOpThreads  0 = ORT default.
    explicit OnnxSession(const std::string& modelPath,
                         bool useCuda = false,
                         int  intraOpThreads = 0);
    ~OnnxSession();

    OnnxSession(const OnnxSession&)            = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;

    /// Run a forward pass over one token sequence.
    /// Returns the logits for the LAST position only, as a flat
    /// row-major vector of length vocabSize.
    std::vector<float> run(const int* inputIds,
                           int seqLen,
                           int& vocabSizeOut);

    /// Same, but returns logits for every position.
    /// Layout: [seqLen][vocabSize], row-major.
    std::vector<float> runAllPositions(const int* inputIds,
                                       int seqLen,
                                       int& vocabSizeOut);

    /// Vocab size implied by the model's output dimension.
    int vocabSize() const;

    /// Input and output tensor names, for diagnostics.
    std::string inputName()  const;
    std::string outputName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hush::composer::neural
