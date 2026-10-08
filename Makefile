pub_1:

	ros2 topic pub  esp32/state_machine std_msgs/msg/Int32 "{data: 1}"

pub_2:

	ros2 topic pub --once esp32/state_machine std_msgs/msg/Int32 "{data: 2}"

pub_3:

	os2 topic pub --once esp32/state_machine std_msgs/msg/Int32 "{data: -1}"