from __future__ import annotations
from dataclasses import dataclass
import build123d as bd
import math
from enum import StrEnum

class ToothShape(StrEnum):
    INVOLUTE = "involute"
    TRAPEZOIDAL = "trapezoidal"
    CURVILINEAR = "curvilinear"
    MODIFIED_CURVILINEAR = "modified_curvilinear"
    CYCLOIDAL = "cycloidal"
    TROCHOIDAL = "trochoidal"
    # These might be separate?
    HELICAL = "helical"
    BEVEL = "bevel"
    STRAIGHT = "straight"

@dataclass(kw_only=True, frozen=True)
class RackParameters:
    tooth_count: int
    module: float = 1.0
    pressure_angle_degrees: float = 20.0
    shape: ToothShape = ToothShape.INVOLUTE

"""
Descriptions and calculations based on:
https://khkgears.net/new/gear_knowledge/abcs_of_gears-b/basic_gear_terminology_calculation.html
"""
@dataclass(kw_only=True, frozen=True)
class ToothParameters:
    count: int
    module: float = 1.0
    pressure_angle_degrees: float = 20.0
    shape: str = "involute"

    @property
    def pitch(self) -> float:
        return self.module * math.pi

    @property
    def thickness(self) -> float:
        return self.pitch / 2

    @property
    def addendum(self) -> float:
        """Distance between reference line and tooth tip"""
        return self.module

    @property
    def dedendum(self) -> float:
        """Distance between reference line and tooth root"""
        return 1.25 * self.module

    @property
    def depth(self) -> float:
        """Distance between tooth tip and tooth root"""
        return self.addendum + self.dedendum

@dataclass(kw_only=True, frozen=True)
class SpurGearParameters:
    teeth: ToothParameters
    thickness: float
    
    @property
    def reference_diameter(self) -> float:
        return self.teeth.count * self.teeth.module

    @property
    def root_diameter(self) -> float:
        return self.reference_diameter - 2.5 * self.teeth.module

    @property
    def tip_diameter(self) -> float:
        return self.reference_diameter + 2 * self.teeth.module

def center_distance(gear1: SpurGearParameters, gear2: SpurGearParameters) -> float:
    return (gear1.reference_diameter + gear2.reference_diameter) / 2

def tip_clearance(gear1: SpurGearParameters, gear2: SpurGearParameters) -> float:
    if gear1.teeth.module != gear2.teeth.module:
        raise ValueError("Gears must have the same module to calculate tip clearance")
    return 0.25 * gear1.teeth.module

@dataclass(kw_only=True, frozen=True)
class HelicalGearParameters(SpurGearParameters): ...

class ToothDirection(StrEnum):
    RADIAL_OUTWARD = "radial_outward"
    RADIAL_INWARD = "radial_inward"
    AXIAL = "axial"


@dataclass(kw_only=True, frozen=True)
class DogRingParameters:
    tooth_count: int
    tooth_depth: float
    tooth_direction: ToothDirection
    tooth_chamfer: float 
    inner_diameter: float
    wall_thickness: float
    length: float

    @property
    def inner_radius(self) -> float:
        return self.inner_diameter / 2

    @property
    def outer_diameter(self) -> float:
        return self.inner_diameter + 2 * self.wall_thickness

    @property
    def outer_radius(self) -> float:
        return self.outer_diameter / 2

    @property
    def mid_radius(self) -> float:
        return (self.inner_radius + self.outer_radius) / 2

    @property
    def tooth_width(self) -> float:
        match self.tooth_direction:
            case ToothDirection.RADIAL_OUTWARD:
            case ToothDirection.RADIAL_INWARD:
            case ToothDirection.AXIAL:
            case _:
                raise ValueError(f"Invalid tooth direction: {self.tooth_direction}")
        return _tooth_width(self.mid_radius, self.tooth_count)

    @property
    def tooth_overlap(self) -> float:
        return _tooth_overlap(self.tooth_depth, self.wall_thickness)


def _tooth_width(radius: float, tooth_count: int) -> float:
    pitch_degrees = 360 / tooth_count
    fill_factor = 0.5
    half_angle_radians = math.radians(pitch_degrees * fill_factor) / 2
    return 2 * radius * math.sin(half_angle_radians)


def _tooth_overlap(*dimensions: float) -> float:
    positive_dimensions = [dimension for dimension in dimensions if dimension > 0]
    if not positive_dimensions:
        return 0.01

    return min(0.01, *(dimension / 20 for dimension in positive_dimensions))


def _safe_chamfer(limit: float, *dimensions: float) -> float:
    if limit <= 0:
        return 0

    return min(limit, *(dimension / 2 for dimension in dimensions if dimension > 0))


def _maybe_chamfer_tooth(
    tooth: bd.Part,
    *,
    axis: bd.Axis,
    position: float,
    chamfer: float,
    dimensions: tuple[float, ...],
) -> bd.Part:
    chamfer_amount = _safe_chamfer(chamfer, *dimensions)
    if chamfer_amount <= 0:
        return tooth

    chamfer_edges = tooth.edges().filter_by_position(
        axis,
        position - 1e-6,
        position + 1e-6,
    )
    if len(chamfer_edges) == 0:
        return tooth

    return bd.chamfer(chamfer_edges, length=chamfer_amount)


def make_radial_outward_tooth(params: DogRingParameters) -> bd.Part:
    tooth_width = _tooth_width(params.outer_radius, params.tooth_count)
    overlap = _tooth_overlap(params.tooth_depth, params.wall_thickness)
    tooth = bd.Box(
        length=params.tooth_depth + overlap,
        width=tooth_width,
        height=params.length,
    )
    tooth = _maybe_chamfer_tooth(
        tooth,
        axis=bd.Axis.X,
        position=(params.tooth_depth + overlap) / 2,
        chamfer=params.tooth_chamfer,
        dimensions=(params.tooth_depth + overlap, tooth_width, params.length),
    )
    return (
        bd.Pos(params.outer_radius + params.tooth_depth / 2 - overlap / 2, 0, 0) * tooth
    )


def make_radial_inward_tooth(params: DogRingParameters) -> bd.Part:
    tooth_width = _tooth_width(params.inner_radius, params.tooth_count)
    overlap = _tooth_overlap(params.tooth_depth, params.wall_thickness)
    tooth = bd.Box(
        length=params.tooth_depth + overlap,
        width=tooth_width,
        height=params.length,
    )
    tooth = _maybe_chamfer_tooth(
        tooth,
        axis=bd.Axis.X,
        position=-(params.tooth_depth + overlap) / 2,
        chamfer=params.tooth_chamfer,
        dimensions=(params.tooth_depth + overlap, tooth_width, params.length),
    )
    return (
        bd.Pos(params.inner_radius - params.tooth_depth / 2 + overlap / 2, 0, 0) * tooth
    )


def make_axial_tooth(params: DogRingParameters) -> bd.Part:
    tooth_width = _tooth_width(params.mid_radius, params.tooth_count)
    overlap = _tooth_overlap(params.tooth_depth, params.length)
    tooth = bd.Box(
        length=params.wall_thickness,
        width=tooth_width,
        height=params.tooth_depth + overlap,
    )
    tooth = _maybe_chamfer_tooth(
        tooth,
        axis=bd.Axis.Z,
        position=(params.tooth_depth + overlap) / 2,
        chamfer=params.tooth_chamfer,
        dimensions=(params.wall_thickness, tooth_width, params.tooth_depth + overlap),
    )
    return (
        bd.Pos(
            params.mid_radius,
            0,
            params.length / 2 + params.tooth_depth / 2 - overlap / 2,
        )
        * tooth
    )


def _make_teeth(params: DogRingParameters) -> list[bd.Part]:
    match params.tooth_direction:
        case ToothDirection.RADIAL_OUTWARD:
            tooth = _radial_outward_tooth(params)
        case ToothDirection.RADIAL_INWARD:
            tooth = _radial_inward_tooth(params)
        case ToothDirection.AXIAL:
            tooth = _axial_tooth(params)

    return [
        bd.Rot(0, 0, i * 360 / params.tooth_count) * tooth
        for i in range(params.tooth_count)
    ]


def make_dog_ring(params: DogRingParameters) -> bd.Part:
    dog_ring = bd.Cylinder(radius=params.outer_radius, height=params.length)
    dog_bore = bd.Cylinder(radius=params.inner_radius, height=params.length)

    dog_ring -= dog_bore

    teeth = _make_teeth(params)
    dog_ring += bd.Compound(children=teeth)
    return dog_ring
