/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_temperature_sensor Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include "esp_zb_temperature_sensor.h"
#include "temp_sensor_driver.h"
#include "switch_driver.h"
#include "light_driver.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"





#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile sensor (End Device) source code.
#endif

static const char *TAG = "ESP_ZB_TEMP_SENSOR";
adc_oneshot_unit_handle_t adc_handle;

static int16_t zb_temperature_to_s16(float temp)
{
    return (int16_t)(temp * 100);
}

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

static void esp_app_buttons_handler(switch_func_pair_t *button_func_pair)
{
    if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        ESP_EARLY_LOGI(TAG, "Button pressed! Performing factory reset...");
        esp_zb_factory_reset();
    }
}

static int s_last_reported_water_level_cm = -999;

static void report_water_level(void)
{
    int raw_value = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value);
    float voltage = (raw_value / 4095.0) * 3.3;
    float percentage = ((raw_value / 4095.0) - 0.2) / 0.8;
    int water_level_cm = (percentage * 100 * WATER_LEVEL_SCALER) + WATER_LEVEL_ADDITION;
    int16_t measured_value;

    /* Clean reading: if raw_value is extremely low (disconnected), report -0.1.
       Otherwise, clamp negative water level calculations to 0. */
    if (raw_value < 100) {
        measured_value = zb_temperature_to_s16(-0.1f);
        water_level_cm = -1;
    } else {
        if (water_level_cm < 0) {
            water_level_cm = 0;
        }
        measured_value = zb_temperature_to_s16((float)water_level_cm);
    }

    ESP_LOGI(TAG, "Reporting water level: raw=%d, cm=%d, measured_s16=%d", raw_value, water_level_cm, measured_value);

    /* Update temperature sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &measured_value, false);
    esp_zb_lock_release();

    /* Send report attributes command */
    esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {
        .address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .zcl_basic_cmd.src_endpoint = HA_ESP_SENSOR_ENDPOINT,
    };
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
    esp_zb_lock_release();

    s_last_reported_water_level_cm = water_level_cm;
}

static void esp_app_temp_sensor_handler(float temperature)
{
    int raw_value = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value);
    float voltage = (raw_value / 4095.0) * 3.3;
    float percentage = ((raw_value / 4095.0) - 0.2) / 0.8;
    int water_level_cm = (percentage * 100 * WATER_LEVEL_SCALER) + WATER_LEVEL_ADDITION;
    
    int tracking_cm = water_level_cm;
    if (raw_value < 100) {
        tracking_cm = -1;
    } else if (tracking_cm < 0) {
        tracking_cm = 0;
    }

    printf("Raw: %d, Voltage: %.3fV, Water height cm %d (last reported %d)\n", 
           raw_value, voltage, water_level_cm, s_last_reported_water_level_cm);

    if (s_last_reported_water_level_cm == -999 || abs(tracking_cm - s_last_reported_water_level_cm) >= 5) {
        report_water_level();
    }
}

static bool s_pump_state = false;
static bool s_sleep_mode_state = false;

static void update_sleep_state(void)
{
    bool should_sleep = (s_sleep_mode_state && !s_pump_state);
    ESP_LOGI(TAG, "Evaluating sleep: sleep_switch = %s, pump = %s -> should_sleep = %s",
             s_sleep_mode_state ? "ON" : "OFF", s_pump_state ? "ON" : "OFF", should_sleep ? "YES" : "NO");
             
    if (s_pump_state) {
        /* Pump is ON -> LED Green, never sleep */
        light_driver_set_color_RGB(100, 0, 0);
    } else if (should_sleep) {
        /* Pump is OFF, Sleep is ON -> LED Yellow (Green+Red), enter sleep */
        light_driver_set_color_RGB(100, 100, 0);
    } else {
        /* Pump is OFF, Sleep is OFF -> LED Red, stay awake */
        light_driver_set_color_RGB(0, 100, 0);
    }

    if (should_sleep) {
        esp_zb_set_rx_on_when_idle(false);
        esp_zb_set_default_long_poll_interval(60000);
    } else {
        esp_zb_set_rx_on_when_idle(true);
        esp_zb_set_default_long_poll_interval(500);
    }
}

static void update_sleep_state_callback(uint8_t param)
{
    update_sleep_state();
}

static void defer_or_update_sleep(void)
{
    bool should_sleep = (s_sleep_mode_state && !s_pump_state);
    if (should_sleep) {
        /* Going to sleep: Delay 1 second to allow ZCL responses to transmit */
        esp_zb_scheduler_alarm((esp_zb_callback_t)update_sleep_state_callback, 0, 1000);
    } else {
        /* Waking up: Apply instantly so the stack stays awake and does not sleep */
        update_sleep_state();
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool state = false;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received error status: %d", message->info.status);

    ESP_LOGI(TAG, "Received message: endpoint %d, cluster 0x%x, attr 0x%x", message->info.dst_endpoint, message->info.cluster, message->attribute.id);

    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        state = message->attribute.data.value ? *(bool *)message->attribute.data.value : false;
        
        if (message->info.dst_endpoint == HA_ESP_SWITCH_ENDPOINT) {
            s_pump_state = state;
            ESP_LOGI(TAG, "Water pump set to %s", s_pump_state ? "ON" : "OFF");
            gpio_set_level(PUMP_GPIO_PIN, s_pump_state ? 1 : 0);
            
            /* Report water level instantly on pump switch */
            report_water_level();
            
            /* Apply sleep logic dynamically */
            defer_or_update_sleep();
        }
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGI(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee bdb commissioning");
}

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;
    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(ESP_TEMP_SENSOR_MIN_VALUE, ESP_TEMP_SENSOR_MAX_VALUE);
    if (!is_inited) {
        ESP_RETURN_ON_ERROR(
            temp_sensor_driver_init(&temp_sensor_config, ESP_TEMP_SENSOR_UPDATE_INTERVAL, esp_app_temp_sensor_handler),
            TAG, "Failed to initialize temperature sensor");
        ESP_RETURN_ON_FALSE(switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_app_buttons_handler),
                            ESP_FAIL, TAG, "Failed to initialize switch driver");
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p     = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            
            /* Report initial water level instantly upon successful join */
            report_water_level();
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static esp_zb_cluster_list_t *custom_temperature_sensor_clusters_create(esp_zb_temperature_sensor_cfg_t *temperature_sensor)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(temperature_sensor->basic_cfg));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&(temperature_sensor->identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, esp_zb_temperature_meas_cluster_create(&(temperature_sensor->temp_meas_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    return cluster_list;
}

static esp_zb_cluster_list_t *custom_switch_clusters_create(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    /* Add basic cluster to help ZHA differentiate multi-endpoint switch configs */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* Add identify cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = 0,
    };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* Add On/Off cluster */
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {
        .on_off = false,
    };
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    return cluster_list;
}

static esp_zb_ep_list_t *custom_device_ep_create(uint8_t sensor_endpoint_id, uint8_t switch_endpoint_id, esp_zb_temperature_sensor_cfg_t *temperature_sensor)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    /* Endpoint 10: Temperature Sensor */
    esp_zb_endpoint_config_t sensor_endpoint_config = {
        .endpoint = sensor_endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_temperature_sensor_clusters_create(temperature_sensor), sensor_endpoint_config);

    /* Endpoint 11: Switch / Pump Output */
    esp_zb_endpoint_config_t switch_endpoint_config = {
        .endpoint = switch_endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_switch_clusters_create(), switch_endpoint_config);

    return ep_list;
}

static void esp_zb_task(void *pvParameters)
{
    /* Initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    zb_nwk_cfg.nwk_cfg.zed_cfg.keep_alive = 3000; // Poll every 3 seconds during boot to ensure fast pairing
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized temperature sensor endpoint */
    esp_zb_temperature_sensor_cfg_t sensor_cfg = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
    /* Set (Min|Max)MeasuredValure */
    sensor_cfg.temp_meas_cfg.min_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MIN_VALUE);
    sensor_cfg.temp_meas_cfg.max_value = zb_temperature_to_s16(100);
    esp_zb_ep_list_t *esp_zb_sensor_ep = custom_device_ep_create(HA_ESP_SENSOR_ENDPOINT, HA_ESP_SWITCH_ENDPOINT, &sensor_cfg);

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config);

    /* Initialize the GPIO for the water pump */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pin_bit_mask = (1ULL << PUMP_GPIO_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PUMP_GPIO_PIN, 0); // start with pump OFF

    /* Initialize the onboard RGB LED */
    light_driver_init(true);

    /* Register the device */
    esp_zb_device_register(esp_zb_sensor_ep);

    /* Register the action handler callback for ZCL write attribute commands (switching the pump) */
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .u.send_info.delta.u16 = 100,
        .attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };

    esp_zb_zcl_update_reporting_info(&reporting_info);

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));

    /* Initialize sleep state, polling intervals, and LED color based on initial state */
    update_sleep_state();

    /* Report initial water level instantly at boot */
    report_water_level();

    esp_zb_stack_main_loop();
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    /* Start Zigbee stack task */
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
