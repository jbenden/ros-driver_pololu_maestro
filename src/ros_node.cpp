#include "ros_node.h"

#include <driver_pololu_maestro/servo_position.h>

#include <sstream>

ros_node::ros_node(int argc, char **argv)
{
    // Initialize the ROS node.
    ros::init(argc, argv, "maestro");

    // Get the node's handle.
    ros_node::m_node = new ros::NodeHandle();

    // Read parameters.
    ros::NodeHandle private_node("~");
    std::string param_serial_port;
    private_node.param<std::string>("serial_port", param_serial_port, "/dev/ttyTHS2");
    int param_baud_rate;
    private_node.param<int>("baud_rate", param_baud_rate, 57600);
    bool param_crc_enabled;
    private_node.param<bool>("crc_enabled", param_crc_enabled, false);
    double param_update_rate;
    private_node.param<double>("update_rate", param_update_rate, 30.0);
    int param_device_number;
    private_node.param<int>("device_number", param_device_number, 12);
    int param_pwm_period;
    private_node.param<int>("pwm_period", param_pwm_period, 20);
    std::vector<int> param_channels;
    private_node.param<std::vector<int>>("channels", param_channels, {0, 1, 2, 3, 4, 5});
    bool param_publish_positions;
    private_node.param<bool>("publish_positions", param_publish_positions, false);

    // Store pwm period.
    ros_node::m_pwm_period = static_cast<unsigned char>(param_pwm_period);

    // Iterate through given channels.
    for(unsigned int i = 0; i < param_channels.size(); i++)
    {
        // Add channel to internal storage.
        ros_node::m_channels.push_back(static_cast<unsigned char>(param_channels.at(i)));

        // Set up subscriber for this channel.
        std::stringstream subscriber_topic;
        subscriber_topic << ros::this_node::getName() << "/set_target/channel_" << param_channels.at(i);
        ros_node::m_subscribers_target.push_back(ros_node::m_node->subscribe<driver_pololu_maestro::servo_target>(subscriber_topic.str(), 1, std::bind(&ros_node::target_callback, this, std::placeholders::_1, param_channels.at(i))));

        // Add position publisher.
        if(param_publish_positions == true)
        {
            std::stringstream publisher_topic;
            publisher_topic << ros::this_node::getName() << "/position/channel_" << param_channels.at(i);
            ros_node::m_publishers_position.push_back(ros_node::m_node->advertise<driver_pololu_maestro::servo_position>(publisher_topic.str(), 1));
        }
    }

    // Initialize ros node members.
    ros_node::m_rate = new ros::Rate(param_update_rate);

    // Create a new driver.
    ros_node::m_driver = new driver(param_serial_port, static_cast<unsigned int>(param_baud_rate), static_cast<unsigned char>(param_device_number), param_crc_enabled);
}
ros_node::~ros_node()
{
    // Clean up resources.
    delete ros_node::m_rate;
    delete ros_node::m_node;
    delete ros_node::m_driver;
}

void ros_node::spin()
{
    while(ros::ok())
    {
        // Publish positions if configured to do so.
        if(ros_node::m_publishers_position.size() > 0)
        {
            for(unsigned int i = 0; i < ros_node::m_channels.size(); i++)
            {
                unsigned char& channel = ros_node::m_channels.at(i);

                try
                {
                    // Read the position from the Maestro.
                    unsigned short position_qus = ros_node::m_driver->get_position(channel);
                    // Convert the quarter microsecond position to microsecond position.
                    float position = static_cast<float>(position_qus) / 4.0f;

                    // Create the position message.
                    driver_pololu_maestro::servo_position message;
                    message.position = position;

                    // Publish the message.
                    ros_node::m_publishers_position.at(i).publish(message);
                }
                catch(...)
                {
                    ROS_WARN_STREAM("Could not read position for channel " << channel << ".");
                }
            }
        }

        // Spin the ros node once.
        ros::spinOnce();

        // Loop.
        ros_node::m_rate->sleep();
    }
}

void ros_node::target_callback(const driver_pololu_maestro::servo_targetConstPtr &message, unsigned char channel)
{
    // Set the speed and acceleration first.
    if(std::isnan(message->acceleration) == false)
    {
        // Convert from %/sec^2 to appropriate units.
        // Appropriate units are based on the PWM period.
        float conversion = 0.000004f * std::pow(static_cast<float>(ros_node::m_pwm_period), 2.0f);
        if(ros_node::m_pwm_period < 20)
        {
            conversion *= 8.0f;
        }
        else
        {
            conversion *= 2.0f;
        }
        // Conversion is now at %/sec^2 to the value the maestro expects.
        // Round to nearest value, and coerce to 0 to 255.
        unsigned short accel = static_cast<unsigned short>(std::max(std::min(static_cast<int>(std::round(message->acceleration * conversion)), 255), 0));

        ros_node::m_driver->set_acceleration(channel, accel);
    }
    if(std::isnan(message->speed) == false)
    {
        // Convert from %/sec to appropriate units.
        // Appropriate units are based on the PWM perid.
        float conversion = 0.004f * static_cast<float>(ros_node::m_pwm_period);
        if(ros_node::m_pwm_period >= 20)
        {
            conversion *= 0.5f;
        }
        // Conversion is now at %/sec^2 to the value the maestro expects.
        // Round to the nearest value, and coerce to 0 to 16383.
        unsigned short speed = static_cast<unsigned short>(std::max(std::min(static_cast<int>(std::round(message->speed * conversion)), 16383), 0));

        ros_node::m_driver->set_speed(channel, speed);
    }

    // Set the target.
    // Clip the target to -500.0 to 2500.0.
    float target_f = std::max(std::min(message->position, 2500.0f), -500.0f);
    // Convert microseconds into microsecond quarters.
    unsigned short target = static_cast<unsigned short>(std::round(target_f * 4.0f));
    ros_node::m_driver->set_target(channel, target);
}
