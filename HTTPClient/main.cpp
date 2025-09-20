#include <stdio.h>
#include <string>

#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <nvs.h>

#include "driver/gpio.h"

#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Ethernet.h"
#include "WiFi.h"
#include "SDCard.h"
#include "HTTPClient.h"
#include "esp_crt_bundle.h"

#define TAG "APP"

const char *root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFCzCCAvOgAwIBAgIQf/AFoHxM3tEArZ1mpRB7mDANBgkqhkiG9w0BAQsFADBH\n"
    "MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\n"
    "QzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMjMxMjEzMDkwMDAwWhcNMjkwMjIw\n"
    "MTQwMDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNl\n"
    "cnZpY2VzMQwwCgYDVQQDEwNXUjIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n"
    "AoIBAQCp/5x/RR5wqFOfytnlDd5GV1d9vI+aWqxG8YSau5HbyfsvAfuSCQAWXqAc\n"
    "+MGr+XgvSszYhaLYWTwO0xj7sfUkDSbutltkdnwUxy96zqhMt/TZCPzfhyM1IKji\n"
    "aeKMTj+xWfpgoh6zySBTGYLKNlNtYE3pAJH8do1cCA8Kwtzxc2vFE24KT3rC8gIc\n"
    "LrRjg9ox9i11MLL7q8Ju26nADrn5Z9TDJVd06wW06Y613ijNzHoU5HEDy01hLmFX\n"
    "xRmpC5iEGuh5KdmyjS//V2pm4M6rlagplmNwEmceOuHbsCFx13ye/aoXbv4r+zgX\n"
    "FNFmp6+atXDMyGOBOozAKql2N87jAgMBAAGjgf4wgfswDgYDVR0PAQH/BAQDAgGG\n"
    "MB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/\n"
    "AgEAMB0GA1UdDgQWBBTeGx7teRXUPjckwyG77DQ5bUKyMDAfBgNVHSMEGDAWgBTk\n"
    "rysmcRorSCeFL1JmLO/wiRNxPjA0BggrBgEFBQcBAQQoMCYwJAYIKwYBBQUHMAKG\n"
    "GGh0dHA6Ly9pLnBraS5nb29nL3IxLmNydDArBgNVHR8EJDAiMCCgHqAchhpodHRw\n"
    "Oi8vYy5wa2kuZ29vZy9yL3IxLmNybDATBgNVHSAEDDAKMAgGBmeBDAECATANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEARXWL5R87RBOWGqtY8TXJbz3S0DNKhjO6V1FP7sQ02hYS\n"
    "TL8Tnw3UVOlIecAwPJQl8hr0ujKUtjNyC4XuCRElNJThb0Lbgpt7fyqaqf9/qdLe\n"
    "SiDLs/sDA7j4BwXaWZIvGEaYzq9yviQmsR4ATb0IrZNBRAq7x9UBhb+TV+PfdBJT\n"
    "DhEl05vc3ssnbrPCuTNiOcLgNeFbpwkuGcuRKnZc8d/KI4RApW//mkHgte8y0YWu\n"
    "ryUJ8GLFbsLIbjL9uNrizkqRSvOFVU6xddZIMy9vhNkSXJ/UcZhjJY1pXAprffJB\n"
    "vei7j+Qi151lRehMCofa6WBmiA4fx+FOVsV2/7R6V2nyAiIJJkEd2nSi5SnzxJrl\n"
    "Xdaqev3htytmOPvoKWa676ATL/hzfvDaQBEcXd2Ppvy+275W+DKcH0FBbX62xevG\n"
    "iza3F4ydzxl6NJ8hk8R+dDXSqv1MbRT1ybB5W0k8878XSOjvmiYTDIfyc9acxVJr\n"
    "Y/cykHipa+te1pOhv7wYPYtZ9orGBV5SGOJm4NrB3K1aJar0RfzxC3ikr7Dyc6Qw\n"
    "qDTBU39CluVIQeuQRgwG3MuSxl7zRERDRilGoKb8uY45JzmxWuKxrfwT/478JuHU\n"
    "/oTxUFqOl2stKnn7QGTq8z29W+GgBLCXSBxC9epaHM0myFH/FJlniXJfHeytWt0=\n"
    "-----END CERTIFICATE-----\n";

// Function to initialize SNTP with multiple servers
void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set multiple NTP servers
    esp_sntp_setservername(0, "pool.ntp.org");    // Primary server
    esp_sntp_setservername(1, "time.nist.gov");   // Secondary server
    esp_sntp_setservername(2, "time.google.com"); // Tertiary server

    esp_sntp_init();
}

// Function to obtain and print time
void obtain_time(void)
{
    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2025 - 1900) && ++retry < retry_count)
    {
        ESP_LOGD(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count)
    {
        ESP_LOGE(TAG, "Failed to obtain time.");
        return;
    }

    ESP_LOGI(TAG, "Time synchronized.");
}

// Function to set timezone
void set_timezone(const char *timezone)
{
    ESP_LOGI(TAG, "Setting timezone to: %s", timezone);
    setenv("TZ", timezone, 1); // Set the TZ environment variable
    tzset();                   // Apply the timezone change
}

// Print current calendar information
void print_calendar()
{
    time_t now;
    struct tm timeinfo;
    char buffer[64];

    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(buffer, sizeof(buffer), "Date: %Y-%m-%d, Time: %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "%s", buffer);
    ESP_LOGI(TAG, "Weekday: %d (0=Sunday, 6=Saturday)", timeinfo.tm_wday);
    ESP_LOGI(TAG, "Day of the year: %d", timeinfo.tm_yday + 1);
}

static void timeTask(void *pc)
{
    // Synchronize time
    obtain_time();

    while (1)
    {
        print_calendar();
        vTaskDelay(pdMS_TO_TICKS(300000)); // Print calendar every 10 seconds
    }
}

void testmany()
{
    HTTPClient http;
    // http.setReuse(false);
    http.begin("https://postman-echo.com/post");
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST("{\"device\" : \"ESP32\",\"status\" : \"OK}");
    if (httpCode > 0)
    {
        ESP_LOGI(TAG, "HTTP GET code: %d", httpCode);
        if (httpCode == HTTP_CODE_OK)
        {
            ESP_LOGI(TAG, "HTTP GET successful. Response:");
            if (http.available() > 0 || http.connected())
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "%s", payload.c_str());
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET failed, error: %s", HTTPClient::errorToString(httpCode).c_str());
    }

    if (http.setURL("http://httpbin.org/get"))
    {
        int httpCode = http.GET();
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "HTTP GET code: %d", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                ESP_LOGI(TAG, "HTTP GET successful. Response:");
                if (http.available() > 0 || http.connected())
                {
                    String payload = http.readString();
                    ESP_LOGI(TAG, "%s", payload.c_str());
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP GET failed, error: %s", HTTPClient::errorToString(httpCode).c_str());
        }
    }

    // Test 1: Postman Echo POST
    ESP_LOGI(TAG, "=== Testing POST with Postman Echo ===");
    http.setURL("https://postman-echo.com/post"); // Use internal crt_bundle
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST("{\"device\":\"ESP32\",\"status\":\"OK\",\"test\":\"connection_reuse\"}");

    if (httpCode > 0)
    {
        ESP_LOGI(TAG, "POST Response Code: %d", httpCode);
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.readString();
            ESP_LOGI(TAG, "POST Response: %s", payload.c_str());
        }
    }
    else
    {
        ESP_LOGE(TAG, "POST failed: %s", HTTPClient::errorToString(httpCode).c_str());
    }

    // Test 2: Reuse connection for GET (same domain)
    ESP_LOGI(TAG, "\n=== Testing Connection Reuse with GET ===");
    if (http.setURL("https://postman-echo.com/get?param1=value1&param2=value2"))
    {
        httpCode = http.GET();
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "GET Response Code: %d", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "GET Response: %s", payload.c_str());
            }
        }
    }

    // Test 3: Different domain (should create new connection)
    ESP_LOGI(TAG, "\n=== Testing Different Domain ===");
    if (http.setURL("https://jsonplaceholder.typicode.com/posts/1"))
    {
        httpCode = http.GET();
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "JSONPlaceholder Response Code: %d", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "JSONPlaceholder Response: %s", payload.c_str());
            }
        }
    }

    // Test 4: ReqRes API
    ESP_LOGI(TAG, "\n=== Testing ReqRes API ===");
    if (http.setURL("https://reqres.in/api/users/2"))
    {
        httpCode = http.GET();
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "ReqRes Response Code: %d", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "ReqRes Response: %s", payload.c_str());
            }
        }
    }

    // Test 5: Test custom headers and different methods
    ESP_LOGI(TAG, "\n=== Testing PUT with Custom Headers ===");
    if (http.setURL("https://postman-echo.com/put"))
    {
        http.addHeader("X-Custom-Header", "ESP32-Test");
        http.addHeader("Accept", "application/json");
        httpCode = http.PUT("{\"action\":\"update\",\"data\":\"test_data\"}");
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "PUT Response Code: %d", httpCode);
            String payload = http.readString();
            ESP_LOGI(TAG, "PUT Response: %s", payload.c_str());
        }
    }

    // Test 5: Test custom headers and different methods
    ESP_LOGI(TAG, "\n=== Testing PUT with Custom Headers ===");
    if (http.setURL("https://postman-echo.com/put"))
    {
        http.removeHeader("X-Custom-Header");
        http.removeHeader("Accept");
        httpCode = http.PUT("{\"action\":\"update\",\"data\":\"test_data\"}");
        if (httpCode > 0)
        {
            ESP_LOGI(TAG, "PUT Response Code: %d", httpCode);
            String payload = http.readString();
            ESP_LOGI(TAG, "PUT Response: %s", payload.c_str());
        }
    }

    http.end(); // Clean up
}

// Test function to understand esp_http_client_get_chunk_length behavior
void testChunkLengthBehavior()
{
    esp_http_client_config_t config = {
        .url = "https://postman-echo.com/stream/3", // Known chunked endpoint
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        esp_http_client_cleanup(client);
        return;
    }

    // Fetch headers
    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Content length: %d", content_length);

    bool is_chunked = esp_http_client_is_chunked_response(client);
    ESP_LOGI(TAG, "Is chunked: %s", is_chunked ? "YES" : "NO");

    if (is_chunked)
    {
        int chunk_num = 0;

        while (true)
        {
            int chunk_len = 0;
            err = esp_http_client_get_chunk_length(client, &chunk_len);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get chunk length: %s", esp_err_to_name(err));
                break;
            }

            ESP_LOGI(TAG, "\n=== CHUNK %d ===", chunk_num);
            ESP_LOGI(TAG, "Initial chunk length: %d", chunk_len);

            if (chunk_len == 0)
            {
                ESP_LOGI(TAG, "End of chunks reached");
                break;
            }

            // Test: Read some data and check chunk length again
            char buffer[50];
            int bytes_to_read = (chunk_len > 20) ? 20 : chunk_len;

            int bytes_read = esp_http_client_read(client, buffer, bytes_to_read);
            ESP_LOGI(TAG, "Read %d bytes from chunk", bytes_read);

            // Check chunk length again after reading
            int chunk_len_after = 0;
            err = esp_http_client_get_chunk_length(client, &chunk_len_after);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "Chunk length after reading %d bytes: %d", bytes_read, chunk_len_after);
                ESP_LOGI(TAG, "Does chunk length decrease? %s",
                         (chunk_len_after < chunk_len) ? "YES" : "NO");
            }

            // Read remaining data in chunk
            if (chunk_len_after > 0)
            {
                char *remaining_buffer = (char *)malloc(chunk_len_after + 1);
                int remaining_read = esp_http_client_read(client, remaining_buffer, chunk_len_after);
                remaining_buffer[remaining_read] = '\0';
                ESP_LOGI(TAG, "Read remaining %d bytes: %s", remaining_read, remaining_buffer);
                free(remaining_buffer);

                // Check chunk length one more time
                int final_chunk_len = 0;
                err = esp_http_client_get_chunk_length(client, &final_chunk_len);
                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "Final chunk length: %d", final_chunk_len);
                }
            }

            chunk_num++;
            if (chunk_num > 10)
                break; // Safety limit
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

// Test peek() behavior with chunked responses
void testPeekWithChunked()
{
    HTTPClient http;
    http.begin("https://postman-echo.com/stream/3", NULL, true);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {

        vTaskDelay(pdMS_TO_TICKS(100));

        if (http.isResponseChunked())
        {
            ESP_LOGI(TAG, "Testing peek() with chunked response");
            int iter = 0;

            while (http.available() > 0)
            {

                ESP_LOGI(TAG, "\n--- New Chunk ---");
                ESP_LOGI(TAG, "Chunk size: %d", http.getCurrentChunkSize());
                ESP_LOGI(TAG, "Chunk remaining: %d", http.getCurrentChunkRemaining());

                // Test peek multiple times (should return same character)
                int peeked1 = http.peek();
                int peeked2 = http.peek();
                int peeked3 = http.peek();

                ESP_LOGI(TAG, "Peek 1: %c (%d)", (char)peeked1, peeked1);
                ESP_LOGI(TAG, "Peek 2: %c (%d)", (char)peeked2, peeked2);
                ESP_LOGI(TAG, "Peek 3: %c (%d)", (char)peeked3, peeked3);

                if (peeked1 == peeked2 && peeked2 == peeked3)
                {
                    ESP_LOGI(TAG, "✓ Peek consistency test PASSED");
                }
                else
                {
                    ESP_LOGE(TAG, "✗ Peek consistency test FAILED");
                }

                // Now actually read the peeked character
                int actual = http.read();
                ESP_LOGI(TAG, "Actual read: %c (%d)", (char)actual, actual);

                if (actual == peeked1)
                {
                    ESP_LOGI(TAG, "✓ Peek vs read test PASSED");
                }
                else
                {
                    ESP_LOGE(TAG, "✗ Peek vs read test FAILED");
                }

                ESP_LOGI(TAG, "After read - remaining: %d", http.getCurrentChunkRemaining());

                // Read a few more characters from this chunk
                int count = 0;
                while (http.getCurrentChunkRemaining() > 0 && count < 10)
                {
                    int c = http.read();
                    if (c >= 0)
                    {
                        ESP_LOGI(TAG, "Read char %d: %c", count, (char)c);
                    }
                    count++;
                }
                iter++;
                if (iter > 10)
                {
                    break;
                }
            }
        }
        else
        {
            ESP_LOGI(TAG, "Response not chunked, testing normal peek");

            // Test peek with normal response
            int peeked = http.peek();
            int actual = http.read();

            ESP_LOGI(TAG, "Peeked: %c (%d)", (char)peeked, peeked);
            ESP_LOGI(TAG, "Actual: %c (%d)", (char)actual, actual);

            if (peeked == actual)
            {
                ESP_LOGI(TAG, "✓ Normal peek test PASSED");
            }
            else
            {
                ESP_LOGE(TAG, "✗ Normal peek test FAILED");
            }
        }
    }

    http.end();
}

// Test peek at chunk boundaries
void testPeekAtChunkBoundaries()
{
    HTTPClient http;
    http.begin("https://postman-echo.com/stream/2", NULL, true);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {

        vTaskDelay(pdMS_TO_TICKS(100));

        if (http.isResponseChunked())
        {
            ESP_LOGI(TAG, "Testing peek at chunk boundaries");

            // Read until near end of first chunk
            while (http.getCurrentChunkRemaining() > 2)
            {
                http.read();
            }

            ESP_LOGI(TAG, "Near end of chunk, remaining: %d", http.getCurrentChunkRemaining());

            // Peek at last character of current chunk
            int peek1 = http.peek();
            ESP_LOGI(TAG, "Peek near chunk end: %c (%d)", (char)peek1, peek1);

            // Read the last character
            int read1 = http.read();
            ESP_LOGI(TAG, "Read last char of chunk: %c (%d)", (char)read1, read1);
            ESP_LOGI(TAG, "Chunk remaining after read: %d", http.getCurrentChunkRemaining());

            // Now peek should get first character of next chunk
            int peek2 = http.peek();
            ESP_LOGI(TAG, "Peek first char of next chunk: %c (%d)", (char)peek2, peek2);
            ESP_LOGI(TAG, "New chunk size: %d", http.getCurrentChunkSize());
            ESP_LOGI(TAG, "New chunk remaining: %d", http.getCurrentChunkRemaining());

            // Verify the peek worked correctly
            int read2 = http.read();
            ESP_LOGI(TAG, "Read first char of next chunk: %c (%d)", (char)read2, read2);

            if (peek2 == read2)
            {
                ESP_LOGI(TAG, "✓ Chunk boundary peek test PASSED");
            }
            else
            {
                ESP_LOGE(TAG, "✗ Chunk boundary peek test FAILED");
            }
        }
    }

    http.end();
}

extern "C" void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // WiFi.begin("project", "12345678");
    Eth.begin();
    // initialize_sntp();
    // // Set timezone (e.g., Asia/Kolkata for UTC+5:30)
    // set_timezone("IST-5:30");
    // xTaskCreate(timeTask, "time_task", 4096, NULL, 5, NULL);

    // testChunkLengthBehavior();
    // testPeekWithChunked();
    // testPeekAtChunkBoundaries();
    testmany();
    // Test redirect handling
    String google_sheet = "https://script.google.com/macros/s/AKfycbxWTtLglpMpj-2KVLtH4uf5MLdnTN5bFGRENS9yGbBxy6gCAOelatKPgNNmSHkAx6OulQ/exec?sts=write&sid=244542323&name=Sachin_Kushawaha&dsts=ENTRY&addr=Lucknow&phn=06306719645";

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(2);
    http.begin("http://httpbin.org/absolute-redirect/4");
    int httCode = http.GET();
    if (httCode > 0)
    {
        ESP_LOGI(TAG, "HTTP GET code: %d", httCode);
        if (httCode == HTTP_CODE_OK)
        {
            ESP_LOGI(TAG, "HTTP GET successful. Response:");
            if (http.available() > 0 || http.connected())
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "%s", payload.c_str());
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET failed, error: %s", HTTPClient::errorToString(httCode).c_str());
    }

    http.setRedirectLimit(5);
    http.begin(google_sheet, root_ca);
    httCode = http.GET();
    if (httCode > 0)
    {
        ESP_LOGI(TAG, "HTTP GET code: %d", httCode);
        if (httCode == HTTP_CODE_OK)
        {
            ESP_LOGI(TAG, "HTTP GET successful. Response:");
            if (http.available() > 0 || http.connected())
            {
                String payload = http.readString();
                ESP_LOGI(TAG, "%s", payload.c_str());
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET failed, error: %s", HTTPClient::errorToString(httCode).c_str());
    }
}