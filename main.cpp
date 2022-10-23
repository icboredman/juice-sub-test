// based on https://github.com/eclipse/paho.mqtt.cpp/blob/master/src/samples/async_subscribe.cpp
// subscribes to 'juice' topic and prints contents of battery data
//
//
// This is a Paho MQTT C++ client, sample application.
//
// This application is an MQTT subscriber using the C++ asynchronous client
// interface, employing callbacks to receive messages and status updates.
//
// The sample demonstrates:
//  - Connecting to an MQTT server/broker.
//  - Subscribing to a topic
//  - Receiving messages through the callback API
//  - Receiving network disconnect updates and attempting manual reconnects.
//  - Using a "clean session" and manually re-subscribing to topics on
//    reconnect.
//

/*******************************************************************************
 * Copyright (c) 2013-2020 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

// <code>


#include <iostream> // cin, cout
#include <iomanip>
#include <thread>
#include <chrono>
#include <mqtt/async_client.h>

#include "powerdata.pb.h"

using namespace std;

const string SERVER_ADDRESS { "tcp://localhost:1883" };
const string TOPIC { "aria/fish00000/fish/juice" };
//const string TOPIC { "internal/juice" };
const int QOS = 0;
const string CLIENT_ID("juice_subcribe_test");

const int	N_RETRY_ATTEMPTS = 5;

typedef struct alignas(4)
{
    struct alignas(4) sGauge {
        float VBat;
        float SoC;
    } gauge;
    struct alignas(4) sCharger {
        float VBus;
        float VSys;
        float VBat;
        float IIn;
        float IChg;
        float IDchg;
    } charger;
    struct alignas(1) sStat {
        bool source;
        bool charging;
        bool fastCharge;
        bool preCharge;
        uint16_t faults;
    } status;
} tPowerData;

tPowerData juice;

/////////////////////////////////////////////////////////////////////////////

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string& name) : name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

    //GOOGLE_PROTOBUF_VERIFY_VERSION;
    protopower::Gauge juice_gauge;

    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override {}

    // (Re)connection success
    void connected(const std::string& cause) override {
        std::cout << "\nConnection success" << std::endl;
        std::cout << "\nSubscribing to topic '" << TOPIC << "'\n"
                  << "\tfor client " << CLIENT_ID
                  << " using QoS" << QOS << "\n"
                  << "\nPress Q<Enter> to quit\n" << std::endl;

        cli_.subscribe(TOPIC, QOS, nullptr, subListener_);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "Message arrived, topic: '" << msg->get_topic() << "'" << std::endl;
        string topic = msg->get_topic();
        topic = topic.substr(0,9);
        if (topic == "internal/")
            memcpy(&juice, msg->get_payload().data(), sizeof(juice));
            //memcpy(juice_, msg->get_payload().data(), sizeof(tPowerData));
        else if (topic == "aria/fish")
        {
            juice_gauge.ParseFromString( msg->get_payload_str() );
            juice.gauge.VBat = juice_gauge.vbat();
            juice.gauge.SoC  = juice_gauge.soc();
            juice.status.charging = juice_gauge.charging();
        }

        std::cout << "\tSrc Chg F P faults" << std::endl;
        std::cout << "\t------------------" << std::endl;
        std::cout << "\t" << (int)juice.status.source << "   " <<
                             (int)juice.status.charging << "   " <<
                             (int)juice.status.fastCharge << " " <<
                             (int)juice.status.preCharge << " " <<
                             std::hex << (int)juice.status.faults << std::dec << std::endl;
        std::cout << "\tVBus: " << juice.charger.VBus << "\tIIn:   " << juice.charger.IIn << std::endl;
        std::cout << "\tVSys: " << juice.charger.VSys << "\tIChg:  " << juice.charger.IChg << std::endl;
        std::cout << "\tVBat: " << juice.charger.VBat << "\tIDchg: " << juice.charger.IDchg << std::endl;
        std::cout << "\tGBat: " << juice.gauge.VBat   << "\tSoC:   " << juice.gauge.SoC << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
            : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
};

/////////////////////////////////////////////////////////////////////////////


int main(int argc, char **argv)
{
    // A subscriber often wants the server to remember its messages when its
    // disconnected. In that case, it needs a unique ClientID and a
    // non-clean session.

    mqtt::async_client cli(SERVER_ADDRESS, CLIENT_ID);

    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);

    // Install the callback(s) before connecting.
    callback cb(cli, connOpts);
    cli.set_callback(cb);

    // Start the connection.
    // When completed, the callback will subscribe to topic.

    try {
        std::cout << "Connecting to the MQTT server..." << std::flush;
        cli.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '"
                  << SERVER_ADDRESS << "'" << exc << std::endl;
        return 1;
    }

    // Just block till user tells us to quit.
    while (std::tolower(std::cin.get()) != 'q')
        ;

    // Disconnect
    try {
        std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
        cli.disconnect()->wait();
        std::cout << "OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc << std::endl;
        return 1;
    }

    return 0;
}
// </code>
