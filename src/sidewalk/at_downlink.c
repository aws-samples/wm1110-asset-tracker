
//TO DO - implement CONFIG downlink messages

// #include <sid_api.h>
// #include <sid_error.h>
// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(at_downlink, CONFIG_TRACKER_LOG_LEVEL);

// #include <asset_tracker.h>
// #include <sidewalk/at_downlink.h>



// static void at_rx_task(void *ctx, void *unused, void *unused2)
// {
// 	// at_ctx_t *at_ctx = (at_ctx_t *)ctx;
// 	ARG_UNUSED(ctx);
// 	ARG_UNUSED(unused);
// 	ARG_UNUSED(unused2);

// 	LOG_DBG("Starting %s ...", __FUNCTION__);

// 	while (1) {
// 		struct at_rx_msg rx_msg;
// 		if (!k_msgq_get(&at_rx_task_msgq, &rx_msg, K_FOREVER)) {
// 			//do something with the downlink messge


// 		}
// 	}

// }
