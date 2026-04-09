from dataclasses import dataclass
import build123d as bd
from build123d import MM
import math


@dataclass(kw_only=True, frozen=True)
class FrictionConeParameters:
    """Parameters for a friction cone."""

    large_diameter: float
    small_diameter: float
    face_width: float
    clearance: float = 0.05
    wall_thickness: float

    @property
    def large_radius(self) -> float:
        return self.large_diameter / 2

    @property
    def small_radius(self) -> float:
        return self.small_diameter / 2

    @property
    def half_angle_radians(self) -> float:
        return math.atan((self.large_radius - self.small_radius) / self.face_width)

    @property
    def half_angle_degrees(self) -> float:
        return math.degrees(self.half_angle_radians)

    @property
    def bore_large_radius(self) -> float:
        return self.large_radius - self.wall_thickness

    @property
    def bore_small_radius(self) -> float:
        return self.small_radius - self.wall_thickness


def make_friction_cone(params: FrictionConeParameters):
    """Create a friction cone based on the provided parameters."""
    cone = bd.Cone(
        bottom_radius=params.large_radius,
        top_radius=params.small_radius,
        height=params.face_width,
    )

    cone_bore = bd.Cone(
        bottom_radius=params.bore_large_radius,
        top_radius=params.bore_small_radius,
        height=params.face_width + params.clearance,
    )
    return cone - cone_bore


def make_cone_pair(params: FrictionConeParameters):
    """Create a pair of friction cones based on the provided parameters."""
    cone1 = make_friction_cone(params)
    cone2 = bd.Pos(
        (0, 0, -(params.clearance + params.wall_thickness))
    ) * make_friction_cone(params)

    return cone1, cone2


if __name__ == "__main__":
    # from yacv_server import show
    from ocp_vscode import show_all

    cone1, cone2 = make_cone_pair(
        FrictionConeParameters(
            large_diameter=20 * MM,
            small_diameter=15 * MM,
            face_width=5 * MM,
            wall_thickness=2 * MM,
            clearance=2 * MM,
        )
    )
    show_all()
