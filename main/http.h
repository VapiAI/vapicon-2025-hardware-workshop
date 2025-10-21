#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define HTTP_BUFFER_SIZE 16384
#define LOG_TAG_HTTP "http"

esp_err_t oai_http_event_handler(esp_http_client_event_t *evt) {
  static int output_len;
  switch (evt->event_id) {
    case HTTP_EVENT_REDIRECT:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_REDIRECT");
      esp_http_client_set_redirection(evt->client);
      break;
    case HTTP_EVENT_ERROR:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
               evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA: {
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (output_len == 0 && evt->user_data) {
        memset(evt->user_data, 0, HTTP_BUFFER_SIZE);
      }

      // If user_data buffer is configured, copy the response into the buffer
      int copy_len = 0;
      if (evt->user_data) {
        // The last byte in evt->user_data is kept for the NULL character in
        // case of out-of-bound access.
        copy_len = MIN(evt->data_len, (HTTP_BUFFER_SIZE - output_len));
        if (copy_len) {
          memcpy(((char *)evt->user_data) + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;

      break;
    }
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGD(LOG_TAG_HTTP, "HTTP_EVENT_ON_FINISH");
      output_len = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(LOG_TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
      output_len = 0;
      break;
  }
  return ESP_OK;
}

static char *build_body_json(const char *sdp) {
  cJSON *root = cJSON_CreateObject();
  assert(root != nullptr);

  cJSON_AddStringToObject(root, "sdp", sdp);

  // assistant object
  cJSON *assistant = cJSON_AddObjectToObject(root, "assistant");
  assert(assistant != nullptr);
  cJSON_AddStringToObject(assistant, "assistantId",
                          CONFIG_ASSISTANT_ID);

  // assistantOverrides
  cJSON *overrides = cJSON_AddObjectToObject(assistant, "assistantOverrides");
  assert(overrides != nullptr);

  // clientMessages array
  cJSON *client_msgs = cJSON_AddArrayToObject(overrides, "clientMessages");
  assert(client_msgs != nullptr);

  cJSON_AddItemToArray(client_msgs, cJSON_CreateString("transfer-update"));
  cJSON_AddItemToArray(client_msgs, cJSON_CreateString("transcript"));

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json;
}

void extract_sdp_from_json(char *body) {
  assert(body != nullptr);

  cJSON *root = cJSON_Parse(body);
  assert(root != nullptr);

  const cJSON *sdp = cJSON_GetObjectItemCaseSensitive(root, "sdp");

  if (cJSON_IsString(sdp) && sdp->valuestring) {
    memcpy(body, sdp->valuestring, strlen(sdp->valuestring));
    body[strlen(sdp->valuestring)] = 0;
  }

  cJSON_Delete(root);
}

void do_http_request(const char *offer, char *answer) {
  esp_http_client_config_t config;
  memset(&config, 0, sizeof(esp_http_client_config_t));

  config.url = "https://staging-webrtc.vapi.ai/";
  config.event_handler = oai_http_event_handler;
  config.user_data = answer;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  snprintf(answer, HTTP_BUFFER_SIZE, "Bearer %s", CONFIG_VAPI_PUBLIC_KEY);

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/sdp");
  esp_http_client_set_header(client, "Authorization", answer);

  auto body = build_body_json(offer);
  esp_http_client_set_post_field(client, body, strlen(body));

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK || esp_http_client_get_status_code(client) != 200) {
    ESP_LOGE(LOG_TAG_HTTP, "Error perform http request %s",
             esp_err_to_name(err));
    esp_restart();
  }

  free(body);
  esp_http_client_cleanup(client);
  extract_sdp_from_json(answer);
}
