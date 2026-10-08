import time
import rclpy
from std_msgs.msg import Int32


def main():
    rclpy.init()
    node = rclpy.create_node('publisher_test')
    pub = node.create_publisher(Int32, '/esp32/state_cmd', 10)

    # espera o subscriber aparecer antes de publicar
    while pub.get_subscription_count() == 0:
        rclpy.spin_once(node, timeout_sec=0.1)

    msg = Int32(data=2)
    pub.publish(msg)
    node.get_logger().info(f'Publicado: {msg.data}')

    time.sleep(0.2)   # dá tempo de a mensagem sair antes de encerrar
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()