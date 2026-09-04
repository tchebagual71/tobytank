#include "memory/identity_store.h"

#include <stdbool.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "tobytank_identity";

#define IDENTITY_NAMESPACE "tobytank"
#define IDENTITY_COUNTER_KEY "id_counter"

static tobytank_identity_allocator_t s_allocator;
static bool s_initialized;
static bool s_history_lost;

static esp_err_t read_counter(uint64_t *counter)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(IDENTITY_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* First boot on a fresh device: nothing has been reserved yet. */
        *counter = 0;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_get_u64(handle, IDENTITY_COUNTER_KEY, counter);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *counter = 0;
        ret = ESP_OK;
    }
    nvs_close(handle);
    return ret;
}

static esp_err_t write_counter(uint64_t counter)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(IDENTITY_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_u64(handle, IDENTITY_COUNTER_KEY, counter);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

/* Claims a block and persists it before any identity from it is used. */
static esp_err_t reserve_block(void)
{
    uint64_t counter = 0;
    esp_err_t ret = read_counter(&counter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not read the identity counter: %s", esp_err_to_name(ret));
        return ret;
    }

    tobytank_identity_allocator_t candidate;
    tobytank_identity_reserve_block(&candidate, counter);
    if (tobytank_identity_block_exhausted(&candidate)) {
        ESP_LOGE(TAG, "Identity space exhausted at persisted counter %llu",
                 (unsigned long long)counter);
        return ESP_ERR_NO_MEM;
    }

    ret = write_counter(candidate.reserved_counter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not persist the identity counter: %s", esp_err_to_name(ret));
        return ret;
    }

    s_allocator = candidate;
    ESP_LOGI(TAG, "Reserved identities %llu..%llu (counter now %llu)",
             (unsigned long long)s_allocator.next_identity,
             (unsigned long long)(s_allocator.block_end - 1u),
             (unsigned long long)s_allocator.reserved_counter);
    return ESP_OK;
}

esp_err_t tobytank_identity_store_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_history_lost = false;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* The NVS partition is unusable. Erasing only that partition is the
           standard recovery, but it destroys the identity history, so say so
           plainly instead of quietly restarting from zero. */
        ESP_LOGW(TAG, "NVS unusable (%s); erasing the NVS partition. "
                      "Identity history is lost and uniqueness restarts from zero.",
                 esp_err_to_name(ret));
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Could not erase NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
        s_history_lost = true;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = reserve_block();
    if (ret != ESP_OK) {
        return ret;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t tobytank_identity_store_next(tobytank_identity_t *out_identity)
{
    if (out_identity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (tobytank_identity_block_exhausted(&s_allocator)) {
        const esp_err_t ret = reserve_block();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (!tobytank_identity_next(&s_allocator, out_identity)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

tobytank_identity_t tobytank_identity_store_peek(void)
{
    return s_initialized ? s_allocator.next_identity : TOBYTANK_IDENTITY_INVALID;
}

uint64_t tobytank_identity_store_remaining(void)
{
    return s_initialized ? tobytank_identity_block_remaining(&s_allocator) : 0;
}

uint64_t tobytank_identity_store_counter(void)
{
    return s_initialized ? s_allocator.reserved_counter : 0;
}

int tobytank_identity_store_history_lost(void)
{
    return s_history_lost ? 1 : 0;
}
