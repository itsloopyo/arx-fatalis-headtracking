// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/math/vec3.h"

#include <cmath>

// A sphere swept along a straight line against one triangle: where the lean
// clamp's eye has to stop so that nothing comes closer to it than the radius.
namespace ArxHeadTracking {

using cameraunlock::math::Vec3;

// The point of triangle abc nearest to p (Ericson, Real-Time Collision
// Detection, 5.1.5).
inline Vec3 ClosestPointOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 ap = p - a;
    const float d1 = Vec3::Dot(ab, ap);
    const float d2 = Vec3::Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    const Vec3 bp = p - b;
    const float d3 = Vec3::Dot(ab, bp);
    const float d4 = Vec3::Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return a + ab * (d1 / (d1 - d3));

    const Vec3 cp = p - c;
    const float d5 = Vec3::Dot(ab, cp);
    const float d6 = Vec3::Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return a + ac * (d2 / (d2 - d6));

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }

    const float denom = 1.0f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

inline float DistanceToTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    return (p - ClosestPointOnTriangle(p, a, b, c)).Magnitude();
}

// How far along unit `dir` from `origin`, up to `length`, a sphere of `radius`
// travels before it touches triangle abc. False when it never does.
//
// The distance from a point on a line to a triangle is convex in the line
// parameter, since a triangle is a convex set. That is what makes the search
// exact without solving the sphere-versus-face, edge and vertex cases apart: a
// golden-section search finds the closest approach, and the first contact is
// the single crossing of the radius on the way down to it.
//
// A sphere that already touches the triangle at the start is blocked only if
// the sweep brings it closer. Refusing every direction there would freeze the
// lean against a surface the player is trying to lean away from.
inline bool SweepSphereTriangle(const Vec3& origin, const Vec3& dir, float length, float radius,
                                const Vec3& a, const Vec3& b, const Vec3& c, float& hitDistance) {
    // Both ends further from the triangle's plane than the radius, on the same
    // side, and the sphere cannot reach any part of the triangle in between.
    const Vec3 normal = Vec3::Cross(b - a, c - a);
    const float normalLength = normal.Magnitude();
    if (normalLength > 0.0f) {
        const Vec3 n = normal * (1.0f / normalLength);
        const float startSide = Vec3::Dot(origin - a, n);
        const float endSide = Vec3::Dot(origin + dir * length - a, n);
        if ((startSide > radius && endSide > radius) || (startSide < -radius && endSide < -radius)) {
            return false;
        }
    }

    const Vec3 nearest = ClosestPointOnTriangle(origin, a, b, c);
    const Vec3 away = origin - nearest;
    if (away.Magnitude() <= radius) {
        if (Vec3::Dot(dir, away) < 0.0f) {
            hitDistance = 0.0f;
            return true;
        }
        return false;
    }

    const auto distanceAt = [&](float t) { return DistanceToTriangle(origin + dir * t, a, b, c); };

    constexpr float kInvPhi = 0.6180339887f;
    constexpr int kSearchSteps = 40;
    float lo = 0.0f;
    float hi = length;
    float x1 = hi - (hi - lo) * kInvPhi;
    float x2 = lo + (hi - lo) * kInvPhi;
    float f1 = distanceAt(x1);
    float f2 = distanceAt(x2);
    for (int i = 0; i < kSearchSteps; ++i) {
        if (f1 < f2) {
            hi = x2;
            x2 = x1;
            f2 = f1;
            x1 = hi - (hi - lo) * kInvPhi;
            f1 = distanceAt(x1);
        } else {
            lo = x1;
            x1 = x2;
            f1 = f2;
            x2 = lo + (hi - lo) * kInvPhi;
            f2 = distanceAt(x2);
        }
    }
    float closestT = (lo + hi) * 0.5f;
    if (distanceAt(length) < distanceAt(closestT)) closestT = length;
    if (distanceAt(closestT) > radius) return false;

    // distanceAt(0) > radius >= distanceAt(closestT), and the distance only
    // falls on the way there, so the crossing is unique.
    float outside = 0.0f;
    float inside = closestT;
    for (int i = 0; i < kSearchSteps; ++i) {
        const float mid = (outside + inside) * 0.5f;
        if (distanceAt(mid) > radius) {
            outside = mid;
        } else {
            inside = mid;
        }
    }
    hitDistance = outside;
    return true;
}

}  // namespace ArxHeadTracking
