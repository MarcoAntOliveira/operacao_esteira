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

void moverFrente()
{
    digitalWrite(IN1,HIGH);
    digitalWrite(IN2,LOW);

    digitalWrite(IN3,HIGH);
    digitalWrite(IN4,LOW);

    ledcWrite(PWM_CHANNEL, velocidade);
}

void moverTras()
{
    digitalWrite(IN1,LOW);
    digitalWrite(IN2,HIGH);

    digitalWrite(IN3,LOW);
    digitalWrite(IN4,HIGH);

    ledcWrite(PWM_CHANNEL, velocidade);
}

void pararMotores()
{
    digitalWrite(IN1,LOW);
    digitalWrite(IN2,LOW);

    digitalWrite(IN3,LOW);
    digitalWrite(IN4,LOW);

    ledcWrite(PWM_CHANNEL,0);
}

//=========================

enum EstadoEsteira
{
    FRENTE,
    PARADO1,
    TRAS,
    PARADO2
};

EstadoEsteira estado = FRENTE;

// unsigned long ultimaTroca = 0;

//=========================
// Timer motor
//=========================

void timer_states_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    RCLC_UNUSED(last_call_time);

    if(timer == NULL)
        return;

    // unsigned long agora = millis();

    switch(estado)
    {
        case FRENTE:

            moverFrente();
            msg_states.data = 1;

            // if(agora-ultimaTroca>2000)
            // {
            //     estado=PARADO1;
            //     ultimaTroca=agora;
            // }

        break;

        case PARADO1:

            pararMotores();
            msg_states.data = 0;

            // if(agora-ultimaTroca>1000)
            // {
            //     estado=TRAS;
            //     ultimaTroca=agora;
            // }

        break;

        case TRAS:

            moverTras();
            msg_states.data = -1;

            // if(agora-ultimaTroca>2000)
            // {
            //     estado=PARADO2;
            //     ultimaTroca=agora;
            // }

        break;

        case PARADO2:

            pararMotores();
            msg_states.data = 0;

            // if(agora-ultimaTroca>1000)
            // {
            //     estado=FRENTE;
            //     ultimaTroca=agora;
            // }

        break;
    }

    RCSOFTCHECK(rcl_publish(&pub_states,&msg_states,NULL));
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


// void timer_pos_callback(rcl_timer_t *timer, int64_t last_call_time)
// {

//     RCLC_UNUSED(last_call_time);

//     if(timer==NULL)
//         return;

//     msg_vel.data = velocidade;

//     RCSOFTCHECK(rcl_publish(&pub_vel,&msg_vel,NULL));

// }
//=========================
// Setup
//=========================

void setup()
{
    Serial.begin(115200);

    delay(3000);

    WiFi.softAP(ap_ssid,ap_password);

    delay(2000);

    pinMode(IN1,OUTPUT);
    pinMode(IN2,OUTPUT);
    pinMode(IN3,OUTPUT);
    pinMode(IN4,OUTPUT);

    ledcSetup(PWM_CHANNEL,PWM_FREQ,PWM_RESOLUTION);
    ledcAttachPin(ENA,PWM_CHANNEL);

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

    RCCHECK(rclc_publisher_init_default(
        &pub_states,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs,msg,Int32),
        "esp32/motor_state"));

    RCCHECK(rclc_publisher_init_default(
        &pub_vel,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs,msg,Int32),
        "esp32/robot_vel"));

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

    msg_.data = 0;
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