#include "client_manager/client_manager.hpp"
#include "config.h"
#include "lib.hpp"
#include "logging.hpp"
#include "matching/engine/engine.hpp"
#include "networking/firebase/firebase.hpp"
#include "networking/rabbitmq/rabbitmq.hpp"
#include "process_spawning/spawning.hpp"
#include "utils/dev_mode/dev_mode.hpp"

#include <argparse/argparse.hpp>

#include <iostream>
#include <string>

#include <rabbitmq-c/amqp.h>

namespace rmq = nutc::rabbitmq;

nutc::manager::ClientManager users;
nutc::engine_manager::Manager engine_manager;

static std::tuple<bool>
process_arguments(int argc, const char** argv)
{
    argparse::ArgumentParser program(
        "NUTC24", VERSION, argparse::default_arguments::help
    );

    program.add_argument("-D", "--dev")
        .help("Enable development features")
        .action([](const auto& /* unused */) {})
        .default_value(false)
        .implicit_value(true)
        .nargs(0);

    program.add_argument("-V", "--version")
        .help("prints version information and exits")
        .action([&](const auto& /* unused */) {
            fmt::println("NUTC24  v{}", VERSION);
            exit(0); // NOLINT(concurrency-*)
        })
        .default_value(false)
        .implicit_value(true)
        .nargs(0);

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        exit(1); // NOLINT(concurrency-*)
    }

    return std::make_tuple(program.get<bool>("--dev"));
}

void
handle_sigint(int sig)
{
    log_i(rabbitmq, "Caught SIGINT, closing connection");
    sleep(1);
    exit(sig);
}

int
main(int argc, const char** argv)
{
    auto [dev_mode] = process_arguments(argc, argv);

    // Set up logging
    nutc::logging::init(quill::LogLevel::TraceL3);

    if (dev_mode) {
        log_t1(main, "Initializing NUTC24 in development mode...");
        nutc::dev_mode::create_algo_files(DEBUG_NUM_USERS);
    }

    // Initialize signal handler
    signal(SIGINT, handle_sigint);

    auto& rmq_conn = rmq::RabbitMQConnectionManager::getInstance();

    // Connect to RabbitMQ
    if (!rmq_conn.connectedToRMQ()) {
        log_e(rabbitmq, "Failed to initialize connection");
        return 1;
    }

    int num_clients = nutc::client::initialize(users, dev_mode);

    engine_manager.add_engine("A");
    engine_manager.add_engine("B");
    engine_manager.add_engine("C");

    // Run exchange
    rmq::RabbitMQClientManager::waitForClients(users, num_clients);
    rmq::RabbitMQClientManager::sendStartTime(users, CLIENT_WAIT_SECS);
    rmq::RabbitMQOrderHandler::addLiquidityToTicker(
        users, engine_manager, "A", 1000, 100
    );
    rmq::RabbitMQOrderHandler::addLiquidityToTicker(
        users, engine_manager, "B", 2000, 200
    );
    rmq::RabbitMQOrderHandler::addLiquidityToTicker(
        users, engine_manager, "C", 3000, 300
    );
    rmq::RabbitMQConsumer::handleIncomingMessages(users, engine_manager);

    return 0;
}
