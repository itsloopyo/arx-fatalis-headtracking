// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "arx_game.h"

namespace ArxHeadTracking {

struct ViewRoll {
    float cosine = 1.0f;
    float sine = 0.0f;

    explicit ViewRoll(float degrees = 0.0f)
        : cosine(std::cos(degrees * kDegToRad)),
          sine(std::sin(degrees * kDegToRad)) {}

    void Apply(float& x, float& y) const {
        const float rotatedX = cosine * x + sine * y;
        y = cosine * y - sine * x;
        x = rotatedX;
    }

    void ApplyScreen(float& x, float& y, float centreX, float centreY) const {
        x -= centreX;
        y -= centreY;
        Apply(x, y);
        x += centreX;
        y += centreY;
    }

    void ApplyViewMatrix(float* matrix) const {
        // The D3D culling matrix uses upward-positive y; CPU screen y grows down.
        for (int row = 0; row < 4; ++row) {
            float down = -matrix[row * 4 + 1];
            Apply(matrix[row * 4], down);
            matrix[row * 4 + 1] = -down;
        }
    }
};

}  // namespace ArxHeadTracking
