#include "client_manager/manager.hpp"
#include "config.h"
#include "firebase/firebase.hpp"
#include "lib.hpp"
#include "logging.hpp"
#include "matching/engine.hpp"
#include "process_spawning/spawning.hpp"
#include "rabbitmq/rabbitmq.hpp"

#include <iostream>
#include <string>

#include <rabbitmq-c/amqp.h>

namespace rmq = nutc::rabbitmq;

nutc::manager::ClientManager users;
rmq::RabbitMQ conn(users);

void
handle_sigint(int sig)
{
    log_i(rabbitmq, "Caught SIGINT, closing connection");
    sleep(1);
    exit(sig);
}

int
main()
{
    // Set up logging
    nutc::logging::init(quill::LogLevel::TraceL3);

    // Initialize signal handler
    signal(SIGINT, handle_sigint);

    // Connect to RabbitMQ
    if (!conn.connectedToRMQ()) {
        log_e(rabbitmq, "Failed to initialize connection");
        return 1;
    }

    // Get users from firebase
    glz::json_t::object_t firebase_users = nutc::client::get_all_users();
    users.initialize_from_firebase(firebase_users);

    // Spawn clients
    const int num_clients = nutc::client::spawn_all_clients(users);

    if (num_clients == 0) {
        log_c(client_spawning, "Spawned 0 clients");
        return 1;
    };

    conn.addTicker("ETHUSD");

    // Run exchange
    conn.waitForClients(num_clients);
    conn.handleIncomingMessages();

    return 0;
}
