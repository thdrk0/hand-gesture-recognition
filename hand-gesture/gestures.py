"""Geometric finger counting and time-based gesture stabilization."""
import math


def distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def angle(a, b, c):
    u = tuple(x - y for x, y in zip(a, b))
    v = tuple(x - y for x, y in zip(c, b))
    norm = distance(a, b) * distance(c, b)
    if norm < 1e-9:
        return 0.0
    return math.degrees(math.acos(max(-1, min(1, sum(x*y for x, y in zip(u, v)) / norm))))


def finger_states(points):
    """Return thumb, index, middle, ring and pinky extension states."""
    if len(points) != 21:
        raise ValueError("21 articulations sont necessaires")
    wrist = points[0]
    extended = []
    for base in (5, 9, 13, 17):
        mcp, pip, dip, tip = points[base:base + 4]
        extended.append(
            angle(mcp, pip, dip) > 140
            and angle(pip, dip, tip) > 135
            and distance(tip, wrist) > distance(pip, wrist) * 1.05
        )
    palm_width = distance(points[5], points[17])
    thumb = (
        palm_width > 1e-9
        and angle(points[1], points[2], points[3]) > 130
        and angle(points[2], points[3], points[4]) > 140
        and distance(points[4], points[5]) > palm_width * 0.65
    )
    return [bool(thumb), *extended]


def count_fingers(points):
    """Count extended fingers from 21 world landmarks (either hand)."""
    return sum(finger_states(points))


class Stabilizer:
    """Emit once per held gesture; a stable release allows retriggering."""
    def __init__(self, hold_seconds=0.45):
        self.hold_seconds = hold_seconds
        self.candidate = None
        self.since = None
        self.current = None

    def update(self, value, now):
        if self.since is None or value != self.candidate:
            self.candidate, self.since = value, now
        event = None
        if now - self.since >= self.hold_seconds and value != self.current:
            self.current = value
            if value is not None:
                event = value
        return self.current, event
