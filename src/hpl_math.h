#pragma once
#include <cmath>

// Independent implementation of the textbook 4x4 rotation, translation and
// multiply used to compose head tracking onto the game's view matrix. Written
// to MATCH the engine's convention - row-major float, column-vector (v' = M*v),
// Mul(A,B) = A*B, view built as Rz(-roll) * Rx(-pitch) * Ry(-yaw) * T(-pos) -
// so a head rotation composed the same way lands in the same space.
//
// Matching a convention is not deriving from an implementation: no code is
// taken or adapted from HPL2's cMath / cMatrixf. See THIRD-PARTY-NOTICES.md.

namespace AmnesiaHT::math {

struct Mat4 {
    float m[16];  // m[row*4 + col]
};

// The camera basis and eye position a view matrix encodes, in world space.
// Read back out of the matrix that was actually written into the camera, so
// the crosshair projection cannot disagree with the composition the renderer
// used - there is one derivation, used twice.
struct ViewBasis {
    float eye[3];
    float right[3];
    float up[3];
    float fwd[3];
};

inline Mat4 Identity() {
    return Mat4{{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
}

inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[row * 4 + k] * b.m[k * 4 + col];
            r.m[row * 4 + col] = s;
        }
    return r;
}

inline Mat4 RotateX(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    return Mat4{{1,0,0,0, 0,c,-s,0, 0,s,c,0, 0,0,0,1}};
}

inline Mat4 RotateY(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    return Mat4{{c,0,s,0, 0,1,0,0, -s,0,c,0, 0,0,0,1}};
}

inline Mat4 RotateZ(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    return Mat4{{c,-s,0,0, s,c,0,0, 0,0,1,0, 0,0,0,1}};
}

inline Mat4 Translate(float x, float y, float z) {
    return Mat4{{1,0,0,x, 0,1,0,y, 0,0,1,z, 0,0,0,1}};
}

// Compose the head transform applied in eye space and pre-multiply it onto the
// clean view matrix in place: view' = H * T(-offset) * view.
//
// Rotating the camera by R in its own axes turns the view matrix into R^-1 * V,
// so with the engine's camera orientation being Ry(yaw)*Rx(pitch)*Rz(roll) the
// head transform is H = Rz(-roll) * Rx(-pitch) * Ry(-yaw) for a head pose read
// in the engine's angle convention. Yaw then gets the boundary negation the
// tracker needs - the protocol states no positive directions, and yaw arrives
// mirrored - which lands on H = Rz(-roll) * Rx(-pitch) * Ry(yaw). Pitch and
// roll are left alone: roll was negated here too, matching the rest of the
// fleet, and in game that leaned the view away from the head.
//
// The offset is applied in ORIGINAL view space, before the head rotation, so a
// lean follows the body's orientation rather than the head-turned view. In eye
// space (x right, y up, z back) translating the scene by t moves the eye by -t,
// so the signs below put a positive tracker y at the eye's up and a negative
// tracker z (the core's forward lean) at the eye's forward.
//
// Angles in degrees, offset in metres.
inline void ApplyHeadTracking(float* view,
                              float yawDeg, float pitchDeg, float rollDeg,
                              float offX, float offY, float offZ) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    Mat4 H = Mul(Mul(RotateZ(-rollDeg * kDegToRad),
                     RotateX(-pitchDeg * kDegToRad)),
                 RotateY(yawDeg * kDegToRad));

    Mat4 HT = Mul(H, Translate(offX, -offY, -offZ));

    Mat4 V;
    for (int i = 0; i < 16; ++i) V.m[i] = view[i];

    Mat4 out = Mul(HT, V);
    for (int i = 0; i < 16; ++i) view[i] = out.m[i];
}

// Pull the world-space camera basis and eye position back out of a view matrix.
// The rotation rows are the basis vectors (row 2 points backwards, which is why
// the engine's own GetForward negates it), and the eye is -R^T * translation.
inline void DecomposeView(const float* view, ViewBasis& out) {
    out.right[0] = view[0];  out.right[1] = view[1];  out.right[2] = view[2];
    out.up[0]    = view[4];  out.up[1]    = view[5];  out.up[2]    = view[6];
    out.fwd[0]   = -view[8]; out.fwd[1]   = -view[9]; out.fwd[2]   = -view[10];

    const float t[3] = { view[3], view[7], view[11] };
    out.eye[0] = -(view[0] * t[0] + view[4] * t[1] + view[8]  * t[2]);
    out.eye[1] = -(view[1] * t[0] + view[5] * t[1] + view[9]  * t[2]);
    out.eye[2] = -(view[2] * t[0] + view[6] * t[1] + view[10] * t[2]);
}

}  // namespace AmnesiaHT::math
