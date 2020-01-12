#include "rest.h"

void app_main() {
	static httpd_handle_t server = NULL;
	init_flash();
	init_camera();
	init_wifi(&server);
}
