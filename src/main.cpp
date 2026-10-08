#include <Arduino.h>
#include <WiFi.h>

#include <micro_ros_platformio.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/int32.h>

#include <uxr/client/profile/transport/custom/custom_transport.h>

extern "C" {
bool platformio_transport_open(struct uxrCustomTransport * transport);
bool platformio_transport_close(struct uxrCustomTransport * transport);
size_t platformio_transport_write(
    struct uxrCustomTransport * transport,
    const uint8_t * buf,
    size_t len,
    uint8_t * errcode);

size_t platformio_transport_read(
    struct uxrCustomTransport * transport,
    uint8_t * buf,
    size_t len,
    int timeout,
    uint8_t * errcode);
}

//=========================
// WiFi AP
//=========================

const char *ap_ssid = "esteira";
const char *ap_password = "12345678";

IPAddress agent_ip(192,168,4,2);
size_t agent_port = 8888;

// //=========================
// // Motor
// //=========================

// #define IN1 13
// #define IN2 12
// #define IN3 27
// #define IN4 26
// #define ENA 14

// #define PWM_CHANNEL     0
// #define PWM_FREQ        5000
// #define PWM_RESOLUTION  8

int velocidade = 150;

//=========================
// micro-ROS
//=========================

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

rcl_timer_t timer_states;
rcl_timer_t timer_vel;
// rcl_timer_t timer_posicao;

rcl_publisher_t pub_states;
rcl_publisher_t pub_vel;
// rcl_publisher_t pub_posicao;

std_msgs__msg__Int32 msg_states;
std_msgs__msg__Int32 msg_vel;
// std_msgs__msg__Int32 msg_posicao;
// subscriber
rcl_subscription_t subscriber;

rclc_executor_t executor_sub;

//=========================

#define RCCHECK(fn)                         \
{                                           \
    rcl_ret_t rc = fn;                      \
    if(rc != RCL_RET_OK){                   \
        error_loop();                       \
    }                                       \
}

#define RCSOFTCHECK(fn)                     \
{                                           \
    rcl_ret_t rc = fn;                      \
    (void)rc;                               \
}

void error_loop()
{
    while(true)
    {
        delay(100);
    }
}

//=========================
// Motor
//=========================



//=========================

enum EstadoEsteira
{
    aguardandoPeca,
    movendoMeio,
    movendoFim,
    paraEsteira,
    processaUr
};

EstadoEsteira estado = aguardandoPeca;

// unsigned long ultimaTroca = 0;

//=========================
// Timer motor
//=========================

std_msgs__msg__Int32 msg_sub;   // buffer onde o executor escreve a mensagem recebida

void subscription_callback(const void *msgin)
{
    const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;

    switch(estado)
    {
        case aguardandoPeca:
        //nesta etapa a esteira está parada , e aguarda a chegada de 
        //uma peça , quando o sensor entra em  nivel alto , ele transiciona 
        //de estado
            if(msg->data == 0) estado = movendoMeio;
            break;
        case movendoMeio:
        // 
            if(msg->data == 1) estado = movendoFim;
            break;
        case movendoFim:
            if(msg->data == -1) estado = paraEsteira;
            break;
        case paraEsteira:
            if(msg->data == 0) estado = processaUr;
            break;
        case processaUr:
            if(msg->data == 2) estado = aguardandoPeca;
            break;
    }
}
void timer_states_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    RCLC_UNUSED(last_call_time);
    if(timer == NULL) return;

    msg_states.data = (int32_t)estado;
    RCSOFTCHECK(rcl_publish(&pub_states, &msg_states, NULL));
}
//=========================
// Timer velocidade
//=========================

void timer_vel_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    RCLC_UNUSED(last_call_time);

    if(timer==NULL)
        return;

    msg_vel.data = velocidade;

    RCSOFTCHECK(rcl_publish(&pub_vel,&msg_vel,NULL));
}



//=========================
// Setup
//=========================

void setup()
{
    Serial.begin(115200);

    delay(3000);

    WiFi.softAP(ap_ssid,ap_password);

    delay(2000);



    static micro_ros_agent_locator locator;

    locator.address = agent_ip;
    locator.port = agent_port;

    rmw_uros_set_custom_transport(
        false,
        (void*)&locator,
        platformio_transport_open,
        platformio_transport_close,
        platformio_transport_write,
        platformio_transport_read
    );

    allocator = rcl_get_default_allocator();

    while(rclc_support_init(&support,0,NULL,&allocator)!=RCL_RET_OK)
    {
        Serial.println("Tentando conectar ao Agent...");
        delay(1000);
    }

    Serial.println("Agent conectado!");

    RCCHECK(rclc_node_init_default(
        &node,
        "esp32_node",
        "",
        &support));
    RCCHECK(rclc_executor_add_subscription(
    &executor,
    &subscriber,
    &msg_sub,
    &subscription_callback,
    ON_NEW_DATA));

    // create subscriber
    // const int32 topic_name_machine_state = "aguardandoPeca";
    RCCHECK(rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "esp32/state_machine"));

    RCCHECK(rclc_publisher_init_default(
        &pub_states,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs,msg,Int32),
        "esp32/state_machine"));

    RCCHECK(rclc_publisher_init_default(
        &pub_vel,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs,msg,Int32),
        "esp32/motor_vel"));

    RCCHECK(rclc_timer_init_default(
        &timer_states,
        &support,
        RCL_MS_TO_NS(100),
        timer_states_callback));

    RCCHECK(rclc_timer_init_default(
        &timer_vel,
        &support,
        RCL_MS_TO_NS(500),
        timer_vel_callback));

    RCCHECK(rclc_executor_init(
        &executor,
        &support.context,
        2,
        &allocator));

    RCCHECK(rclc_executor_add_timer(
        &executor,
        &timer_states));

    RCCHECK(rclc_executor_add_timer(
        &executor,
        &timer_vel));

    msg_states.data = 0;
    msg_vel.data = velocidade;

    // ultimaTroca = millis();

    Serial.println("micro-ROS iniciado.");
}

//=========================
// Loop
//=========================

void loop()
{
    rclc_executor_spin_some(&executor,RCL_MS_TO_NS(10));
    delay(10);
}