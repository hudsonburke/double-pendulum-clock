from dataclasses import dataclass
import math
from typing import cast

import build123d as bd


@dataclass(kw_only=True, frozen=True)
class BeltProfile:
    """Tooth geometry constants for a belt profile."""

    pitch: float  # Distance between tips
    depth: float


BELT_PROFILE_SPECS: dict[str, BeltProfile] = {
    "GT2": BeltProfile(pitch=2.0, depth=0.75),
    "GT3": BeltProfile(pitch=3.0, depth=1.14),
    "GT5": BeltProfile(pitch=5.0, depth=1.9),
}


@dataclass(kw_only=True, frozen=True)
class Thread:
    """Metric thread specification."""

    major_diameter: float
    pitch: float
    length: float

    @property
    def major_radius(self) -> float:
        return self.major_diameter / 2


# Common metric threads
M3 = Thread(major_diameter=3.0, pitch=0.5, length=3.0)
M4 = Thread(major_diameter=4.0, pitch=0.7, length=4.0)
M5 = Thread(major_diameter=5.0, pitch=0.8, length=5.0)


@dataclass(kw_only=True, frozen=True)
class BeltPulleyParameters:
    profile: BeltProfile = BELT_PROFILE_SPECS["GT2"]
    bore_diameter: float = 6.0
    tooth_count: int = 20
    face_width: float = 7.0
    flange_diameter: float = 0
    flange_width: float = 0
    hub_diameter: float = 0
    hub_width: float = 0
    set_screw: Thread | None = None

    def __post_init__(self) -> None:
        if self.bore_diameter <= 0:
            raise ValueError(f"bore_diameter must be > 0, got {self.bore_diameter}")
        if self.face_width <= 0:
            raise ValueError(f"face_width must be > 0, got {self.face_width}")
        if self.flange_diameter and self.flange_diameter <= self.pitch_diameter:
            raise ValueError(
                f"flange_diameter ({self.flange_diameter}) must exceed "
                f"pitch_diameter ({self.pitch_diameter:.3f})"
            )
        if self.hub_diameter and self.hub_diameter <= self.bore_diameter:
            raise ValueError(
                f"hub_diameter ({self.hub_diameter}) must exceed "
                f"bore_diameter ({self.bore_diameter})"
            )

    @property
    def pitch(self) -> float:
        return self.profile.pitch

    @property
    def tooth_depth(self) -> float:
        return self.profile.depth

    @property
    def flange_radius(self) -> float:
        return self.flange_diameter / 2

    @property
    def flange_offset(self) -> float:
        return self.face_width / 2 + self.flange_width / 2

    @property
    def bore_radius(self) -> float:
        return self.bore_diameter / 2

    @property
    def width(self) -> float:
        return 2 * self.flange_width + self.hub_width + self.face_width

    @property
    def pitch_diameter(self) -> float:
        return self.pitch * self.tooth_count / math.pi

    @property
    def pitch_radius(self) -> float:
        return self.pitch_diameter / 2

    @property
    def outer_diameter(self) -> float:
        return self.pitch_diameter - 2 * self.tooth_depth

    @property
    def outer_radius(self) -> float:
        return self.outer_diameter / 2

    @property
    def tooth_width(self) -> float:
        return math.pi * self.outer_diameter / self.tooth_count

    @property
    def tooth_center_radius(self) -> float:
        return self.outer_radius + self.tooth_depth / 2.0

    @property
    def hub_radius(self) -> float:
        return self.hub_diameter / 2

    @property
    def hub_offset(self) -> float:
        return self.face_width / 2 + self.flange_width + self.hub_width / 2


def _has_geometry(diameter: float, width: float) -> bool:
    """True if both diameter and width are positive (non-degenerate)."""
    return diameter > 0 and width > 0


def make_belt_pulley(params: BeltPulleyParameters, units=bd.MM) -> bd.Part:
    parts: list[bd.Shape] = []

    base = bd.Cylinder(radius=params.pitch_radius, height=params.face_width)
    parts.append(base)

    if _has_geometry(params.flange_diameter, params.flange_width):
        flange_top = bd.Pos(0, 0, params.flange_offset) * bd.Cylinder(
            radius=params.flange_radius, height=params.flange_width
        )
        flange_bot = bd.Pos(0, 0, -params.flange_offset) * bd.Cylinder(
            radius=params.flange_radius, height=params.flange_width
        )
        parts += [flange_top, flange_bot]

    if _has_geometry(params.hub_diameter, params.hub_width):
        hub = bd.Pos(0, 0, -params.hub_offset) * bd.Cylinder(
            radius=params.hub_radius, height=params.hub_width
        )
        parts.append(hub)

    grooves = [
        bd.Rot(0, 0, i * 360 / params.tooth_count)
        * bd.Pos(params.tooth_center_radius, 0, 0)
        * bd.Box(
            length=params.tooth_depth,
            width=params.tooth_width,
            height=params.face_width,
        )
        for i in range(params.tooth_count)
    ]

    cuts: list[bd.Shape] = []
    cuts += grooves

    bore = bd.Cylinder(radius=params.bore_radius, height=params.width)
    cuts.append(bore)

    if params.set_screw is not None:
        screw_hole = (
            bd.Pos(0, 0, -params.hub_offset)
            * bd.Rot(0, 90, 0)
            * bd.Cylinder(
                radius=params.set_screw.major_radius, height=params.hub_diameter
            )
        )
        cuts.append(screw_hole)

    pulley = cast(bd.Part, bd.Part() + parts)
    for cut in cuts:
        pulley -= cut
    return cast(bd.Part, pulley)


if __name__ == "__main__":
    from ocp_vscode import show_all

    pulley = make_belt_pulley(
        BeltPulleyParameters(
            flange_diameter=20,
            flange_width=1,
            hub_diameter=10,
            hub_width=5,
            set_screw=M4,
        )
    )
    show_all()
