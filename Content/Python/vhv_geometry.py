"""Plain 2D curve helpers shared by the village scripts (no Unreal dependency).

Curves are lists of (x, y) points in centimetres.
"""

import math


def smooth(points, samples=8):
    """Catmull-Rom curve through the control points."""
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for s in range(samples):
            t = s / float(samples)
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * (2 * p1[k] + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                       + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3)
                for k in (0, 1)))
    out.append(tuple(points[-1]))
    return out


def tangent(curve, i):
    a = curve[max(0, i - 1)]
    b = curve[min(len(curve) - 1, i + 1)]
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1.0
    return dx / length, dy / length


def distance_to_curve(x, y, curve):
    best = float("inf")
    for (ax, ay), (bx, by) in zip(curve, curve[1:]):
        dx, dy = bx - ax, by - ay
        denom = dx * dx + dy * dy
        t = 0.0 if denom == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / denom))
        best = min(best, math.hypot(x - (ax + t * dx), y - (ay + t * dy)))
    return best


def curve_intersections(curve_a, curve_b):
    """Points where two curves cross, with the unit direction of curve_b there: [((x, y), (dx, dy))]."""
    found = []
    for (ax, ay), (bx, by) in zip(curve_a, curve_a[1:]):
        for (cx, cy), (ex, ey) in zip(curve_b, curve_b[1:]):
            rx, ry = bx - ax, by - ay
            sx, sy = ex - cx, ey - cy
            denom = rx * sy - ry * sx
            if abs(denom) < 1e-9:
                continue
            t = ((cx - ax) * sy - (cy - ay) * sx) / denom
            u = ((cx - ax) * ry - (cy - ay) * rx) / denom
            if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
                length = math.hypot(sx, sy) or 1.0
                found.append(((ax + t * rx, ay + t * ry), (sx / length, sy / length)))
    return found
