from launch import LaunchDescription
from launch.actions import EmitEvent, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.events import Shutdown
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def shutdown_when_node_exits(node, reason):
    return RegisterEventHandler(
        OnProcessExit(
            target_action=node,
            on_exit=[EmitEvent(event=Shutdown(reason=reason))],
        )
    )


def generate_launch_description():
    parameters = PathJoinSubstitution([
        FindPackageShare("panel_bringup"),
        "config",
        "panel.yaml",
    ])

    state_manager = Node(
        package="panel_nodes",
        executable="state_manager_node",
        name="state_manager_node",
        output="screen",
        parameters=[parameters],
    )
    touchscreen_hmi = Node(
        package="panel_nodes",
        executable="touchscreen_hmi_node",
        name="touchscreen_hmi_node",
        output="screen",
        parameters=[parameters],
    )
    ethernet_state_server = Node(
        package="panel_nodes",
        executable="ethernet_state_server_node",
        name="ethernet_state_server_node",
        output="screen",
        parameters=[parameters],
    )

    return LaunchDescription([
        state_manager,
        touchscreen_hmi,
        ethernet_state_server,
        shutdown_when_node_exits(state_manager, "state_manager_node exited"),
        shutdown_when_node_exits(touchscreen_hmi, "touchscreen_hmi_node exited"),
        shutdown_when_node_exits(ethernet_state_server, "ethernet_state_server_node exited"),
    ])
