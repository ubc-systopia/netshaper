#include <doca_argp.h>
#include <doca_flow.h>
#include <doca_log.h>

DOCA_LOG_REGISTER(DOCA_PEER_1::MAIN);

int main(int argc, char* argv[]) {
    doca_error_t err;
    int exit_status = EXIT_FAILURE;
    struct application_dpdk_config {
        .port_config.nb_ports = 2,
        .port_config.nb_queues = 1,
        // TODO(arun): do we need this hairpin queue
        .port_config.nb_hairpin_queues = 1,
        .port_config.isolated_mode = 1,
        .sft_config = {0},
    };

    // Initialize the DOCA logger backend
    result = doca_log_standard_backend();
    if (result != DOCA_SUCCESS) {
        goto cleanup;
    }

    result = doca_argp_init("DOCA Peer 1", NULL);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERROR("Failed to initialize DOCA argp");
        goto cleanup;
    }
    doca_argp_set_dpdk_program(dpdk_init);
    result = doca_argp_start(argc-1, argv+1);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERROR("Failed to parse DOCA arguments");
        goto argp_cleanup;
    }

    // Update queues and ports
    result = dpdk_queues_and_ports_init(&application_dpdk_config);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERROR("Failed to initialize queues and ports");
        goto dpdk_cleanup;
    }

dpdk_cleanup:
    dpdk_fini();
argp_cleanup:
    doca_argp_cleanup();
cleanup:
    if (exit_status == EXIT_SUCCESS) {
        DOCA_LOG_INFO("DOCA application exited successfully");
    } else {
        DOCA_LOG_ERROR("DOCA application exited with error");
    }
    return exit_status;
}
