import argparse
from pathlib import Path


MODEL_PATH = Path("double_pendulum.xml")


# Edit these top-level values to change the default hand lengths.
DEFAULT_HOUR_HAND_LENGTH = 0.35
DEFAULT_MINUTE_HAND_LENGTH = 0.30


CLOCK_CENTER_Z = 0.72
WALL_X = -0.01
FACE_X = 0.004
MOUNT_X = 0.01
LINK_X = 0.03

HOUR_SHAFT_WIDTH = 0.012
HOUR_SHAFT_DEPTH = 0.008
HOUR_TIP_WIDTH = 0.006
HOUR_TIP_DEPTH = 0.004
HOUR_COUNTERWEIGHT_RADIUS = 0.022
HOUR_COUNTERWEIGHT_HALF_LENGTH = 0.005
HOUR_COUNTERWEIGHT_OFFSET = 0.035
HOUR_DENSITY = 700

MINUTE_SHAFT_WIDTH = 0.009
MINUTE_SHAFT_DEPTH = 0.005
MINUTE_TIP_WIDTH = 0.004
MINUTE_TIP_DEPTH = 0.0025
MINUTE_COUNTERWEIGHT_RADIUS = 0.016
MINUTE_COUNTERWEIGHT_HALF_LENGTH = 0.0035
MINUTE_COUNTERWEIGHT_OFFSET = 0.028
MINUTE_DENSITY = 600

HOUR_TIP_HALF_LENGTH = 0.045
MINUTE_TIP_HALF_LENGTH = 0.03


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate the MuJoCo double pendulum clock model.")
    parser.add_argument(
        "--hour",
        type=float,
        default=DEFAULT_HOUR_HAND_LENGTH,
        help=f"Hour hand length in meters (default: {DEFAULT_HOUR_HAND_LENGTH}).",
    )
    parser.add_argument(
        "--minute",
        type=float,
        default=DEFAULT_MINUTE_HAND_LENGTH,
        help=f"Minute hand length in meters (default: {DEFAULT_MINUTE_HAND_LENGTH}).",
    )
    return parser.parse_args()


def clamp(value: float, lower: float, upper: float) -> float:
    return max(lower, min(value, upper))


def hand_segments(total_length: float, tip_half_length: float, shaft_fraction: float) -> tuple[float, float, float, float]:
    shaft_half_length = clamp(total_length * shaft_fraction, 0.04, total_length / 2 - tip_half_length - 0.01)
    shaft_center = -shaft_half_length
    tip_center = -(total_length - tip_half_length)
    return shaft_half_length, shaft_center, tip_half_length, tip_center


def tick_geom(name: str, y: float, z: float, size: float, color: str) -> str:
    return (
        f'    <geom name="{name}" type="sphere" pos="0.007 {y:.4f} {z:.4f}" '
        f'size="{size:.3f}" rgba="{color}" contype="0" conaffinity="0"/>'
    )


def validate_length(name: str, value: float, tip_half_length: float) -> None:
    min_length = 2 * tip_half_length + 0.03
    if value <= min_length:
        raise ValueError(f"{name} hand length must be greater than {min_length:.3f} m.")


args = parse_args()
hour_hand_length = args.hour
minute_hand_length = args.minute

validate_length("Hour", hour_hand_length, HOUR_TIP_HALF_LENGTH)
validate_length("Minute", minute_hand_length, MINUTE_TIP_HALF_LENGTH)

hour_shaft_half, hour_shaft_center, hour_tip_half, hour_tip_center = hand_segments(
    hour_hand_length, HOUR_TIP_HALF_LENGTH, 0.385714
)
minute_shaft_half, minute_shaft_center, minute_tip_half, minute_tip_center = hand_segments(
    minute_hand_length, MINUTE_TIP_HALF_LENGTH, 0.4
)


tick_lines = [
    tick_geom("tick_12", 0.0, CLOCK_CENTER_Z + 0.30, 0.014, "0.16 0.16 0.18 1"),
    tick_geom("tick_1", 0.15, CLOCK_CENTER_Z + 0.2598, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_2", 0.2598, CLOCK_CENTER_Z + 0.15, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_3", 0.3, CLOCK_CENTER_Z, 0.014, "0.16 0.16 0.18 1"),
    tick_geom("tick_4", 0.2598, CLOCK_CENTER_Z - 0.15, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_5", 0.15, CLOCK_CENTER_Z - 0.2598, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_6", 0.0, CLOCK_CENTER_Z - 0.30, 0.014, "0.16 0.16 0.18 1"),
    tick_geom("tick_7", -0.15, CLOCK_CENTER_Z - 0.2598, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_8", -0.2598, CLOCK_CENTER_Z - 0.15, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_9", -0.3, CLOCK_CENTER_Z, 0.014, "0.16 0.16 0.18 1"),
    tick_geom("tick_10", -0.2598, CLOCK_CENTER_Z + 0.15, 0.009, "0.28 0.28 0.3 1"),
    tick_geom("tick_11", -0.15, CLOCK_CENTER_Z + 0.2598, 0.009, "0.28 0.28 0.3 1"),
]


xml = f"""<mujoco model=\"wall_mounted_double_pendulum\">
  <compiler angle=\"degree\" inertiafromgeom=\"true\" autolimits=\"true\"/>

  <option gravity=\"0 0 -9.81\" timestep=\"0.002\" integrator=\"RK4\"/>

  <default>
    <joint type=\"hinge\" axis=\"1 0 0\" damping=\"0.05\" armature=\"0.001\" limited=\"true\" range=\"-179 179\"/>
    <geom type=\"capsule\" size=\"0.015\" density=\"850\" friction=\"0.6 0.02 0.02\" rgba=\"0.2 0.2 0.25 1\"/>
    <motor ctrllimited=\"true\"/>
    <site size=\"0.01\" rgba=\"0.9 0.2 0.2 1\"/>
  </default>

  <worldbody>
    <light name=\"main_light\" pos=\"1.0 -1.0 2.0\" dir=\"-0.3 0.2 -1\" diffuse=\"0.9 0.9 0.9\" specular=\"0.2 0.2 0.2\"/>

    <geom name=\"wall\" type=\"box\" pos=\"{WALL_X:.2f} 0 0.45\" size=\"0.01 0.45 0.55\" rgba=\"0.82 0.84 0.88 1\"/>
    <geom name=\"clock_face\" type=\"cylinder\" pos=\"{FACE_X:.3f} 0 {CLOCK_CENTER_Z:.2f}\" quat=\"0.707107 0 0.707107 0\" size=\"0.32 0.003\" rgba=\"0.97 0.96 0.9 1\" contype=\"0\" conaffinity=\"0\"/>
    <geom name=\"clock_rim\" type=\"cylinder\" pos=\"0.005 0 {CLOCK_CENTER_Z:.2f}\" quat=\"0.707107 0 0.707107 0\" size=\"0.325 0.002\" rgba=\"0.72 0.58 0.24 1\" contype=\"0\" conaffinity=\"0\"/>
    <geom name=\"clock_hub\" type=\"cylinder\" pos=\"0.006 0 {CLOCK_CENTER_Z:.2f}\" quat=\"0.707107 0 0.707107 0\" size=\"0.03 0.004\" rgba=\"0.72 0.58 0.24 1\" contype=\"0\" conaffinity=\"0\"/>
{chr(10).join(tick_lines)}
    <geom name=\"mount\" type=\"box\" pos=\"{MOUNT_X:.2f} 0 {CLOCK_CENTER_Z:.2f}\" size=\"0.02 0.03 0.03\" rgba=\"0.45 0.33 0.2 1\"/>

    <body name=\"link1\" pos=\"{LINK_X:.2f} 0 {CLOCK_CENTER_Z:.2f}\">
      <joint name=\"shoulder\"/>
      <geom name=\"hour_hand_shaft\" type=\"box\" pos=\"0 0 {hour_shaft_center:.4f}\" size=\"{HOUR_SHAFT_WIDTH:.3f} {HOUR_SHAFT_DEPTH:.3f} {hour_shaft_half:.4f}\" density=\"{HOUR_DENSITY}\" rgba=\"0.12 0.12 0.14 1\"/>
      <geom name=\"hour_hand_tip\" type=\"box\" pos=\"0 0 {hour_tip_center:.4f}\" size=\"{HOUR_TIP_WIDTH:.3f} {HOUR_TIP_DEPTH:.3f} {hour_tip_half:.3f}\" density=\"{HOUR_DENSITY}\" rgba=\"0.12 0.12 0.14 1\"/>
      <geom name=\"hour_hand_counterweight\" type=\"cylinder\" pos=\"0 0 {HOUR_COUNTERWEIGHT_OFFSET:.3f}\" quat=\"0.707107 0 0.707107 0\" size=\"{HOUR_COUNTERWEIGHT_RADIUS:.3f} {HOUR_COUNTERWEIGHT_HALF_LENGTH:.3f}\" density=\"{HOUR_DENSITY}\" rgba=\"0.72 0.58 0.24 1\"/>
      <site name=\"elbow_anchor\" pos=\"0 0 {-hour_hand_length:.4f}\"/>

      <body name=\"link2\" pos=\"0 0 {-hour_hand_length:.4f}\">
        <joint name=\"elbow\"/>
        <geom name=\"minute_hand_shaft\" type=\"box\" pos=\"0 0 {minute_shaft_center:.4f}\" size=\"{MINUTE_SHAFT_WIDTH:.3f} {MINUTE_SHAFT_DEPTH:.3f} {minute_shaft_half:.4f}\" density=\"{MINUTE_DENSITY}\" rgba=\"0.8 0.8 0.82 1\"/>
        <geom name=\"minute_hand_tip\" type=\"box\" pos=\"0 0 {minute_tip_center:.4f}\" size=\"{MINUTE_TIP_WIDTH:.3f} {MINUTE_TIP_DEPTH:.4f} {minute_tip_half:.3f}\" density=\"{MINUTE_DENSITY}\" rgba=\"0.8 0.8 0.82 1\"/>
        <geom name=\"minute_hand_counterweight\" type=\"cylinder\" pos=\"0 0 {MINUTE_COUNTERWEIGHT_OFFSET:.3f}\" quat=\"0.707107 0 0.707107 0\" size=\"{MINUTE_COUNTERWEIGHT_RADIUS:.3f} {MINUTE_COUNTERWEIGHT_HALF_LENGTH:.4f}\" density=\"{MINUTE_DENSITY}\" rgba=\"0.72 0.58 0.24 1\"/>
        <site name=\"tip\" pos=\"0 0 {-minute_hand_length:.4f}\"/>
      </body>
    </body>
  </worldbody>

  <actuator>
    <motor name=\"shoulder_torque\" joint=\"shoulder\" gear=\"1\" ctrlrange=\"-8 8\"/>
    <motor name=\"elbow_torque\" joint=\"elbow\" gear=\"1\" ctrlrange=\"-4 4\"/>
  </actuator>

  <sensor>
    <jointpos name=\"shoulder_pos\" joint=\"shoulder\"/>
    <jointvel name=\"shoulder_vel\" joint=\"shoulder\"/>
    <jointpos name=\"elbow_pos\" joint=\"elbow\"/>
    <jointvel name=\"elbow_vel\" joint=\"elbow\"/>
    <actuatorfrc name=\"shoulder_torque_sensor\" actuator=\"shoulder_torque\"/>
    <actuatorfrc name=\"elbow_torque_sensor\" actuator=\"elbow_torque\"/>
  </sensor>

  <keyframe>
    <key name=\"hanging\" qpos=\"0 0\" ctrl=\"0 0\"/>
  </keyframe>
</mujoco>
"""


MODEL_PATH.write_text(xml)
