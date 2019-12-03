#include <nvs_flash.h>
#include "rest.h"

void app_main() {
	static httpd_handle_t server = NULL;
	ESP_ERROR_CHECK(nvs_flash_init());
	init_camera();
	init_wifi(&server);
}
