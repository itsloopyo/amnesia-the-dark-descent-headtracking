#pragma once

#include <cmath>

namespace AmnesiaHT {

// Where the interaction ray is pointing, in the picture the head is looking at.
//
// The game draws its crosshair at a fixed screen position, so it marks what the
// player will grab, push or open only while the rendered view IS the aim. Once
// the head turns or leans, the view and the ray are different and the crosshair
// is a lie - which is the whole reason this exists.
//
// The projection takes a vector FROM THE EYE THE FRAME IS DRAWN FROM TO THE
// POINT THE RAY LANDS ON, and the basis the frame is actually rendered with,
// and asks where the first lands in the second. With the head centred that
// vector is just the clean camera's forward; with a positional lean it swings
// by the parallax, which is why it has to be a vector to a point and not a
// direction.
//
// It is deliberately basis-to-basis rather than a formula in yaw / pitch /
// roll: the camera hook hands over the vectors it actually wrote into the view
// matrix, so there is no second derivation of the composition to disagree with
// the first. A per-axis tangent formula would agree with it on single-axis
// poses and drift on combined ones, which is precisely the bug that survives
// testing.
//
// `tanX` / `tanY` are the drawn half-field tangents. HPL2's mfFOV is the
// VERTICAL field in radians (cMath::MatrixPerspectiveProjection takes
// tan(fov*0.5) as the top of the near plane and multiplies by the aspect for
// the sides), so tanY = tan(fov/2) and tanX = aspect * tanY.
//
// Returns false when the aim points at or behind the rendered view, where there
// is no screen position to draw at. NDC is x right, y up, both -1..1 across the
// frame.
inline bool ProjectAimToNdc(const float aim[3],
                            const float fwd[3], const float right[3], const float up[3],
                            float tanX, float tanY, float& ndcX, float& ndcY) {
    const float z = aim[0] * fwd[0] + aim[1] * fwd[1] + aim[2] * fwd[2];
    // Not just "behind the camera": as z goes to zero the projection goes to
    // infinity, and a crosshair at 1e30 is a NaN on its way to a vertex buffer.
    if (!(z > 0.01f) || !(tanX > 0.0f) || !(tanY > 0.0f)) return false;

    const float x = aim[0] * right[0] + aim[1] * right[1] + aim[2] * right[2];
    const float y = aim[0] * up[0] + aim[1] * up[1] + aim[2] * up[2];
    ndcX = (x / z) / tanX;
    ndcY = (y / z) / tanY;
    return std::isfinite(ndcX) && std::isfinite(ndcY);
}

}  // namespace AmnesiaHT
