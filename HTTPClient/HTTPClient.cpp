#include "HTTPClient.h"
#include <StreamString.h>
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_crt_bundle.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "HTTP"

#define log_e(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
#define log_v(...) ESP_LOGV(LOG_TAG, __VA_ARGS__)
#define log_i(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
#define log_d(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
#define log_w(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)

esp_err_t HTTPClient::_http_event_handler(esp_http_client_event_t *evt)
{
    HTTPClient *self = static_cast<HTTPClient *>(evt->user_data);
    if (!self)
        return ESP_FAIL;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        log_d("HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        log_d("HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_HEADER_SENT:
        log_d("HTTP_EVENT_HEADER_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
    {
        log_d("HTTP_EVENT_ON_HEADER");

        String key(evt->header_key ? evt->header_key : "");
        String val(evt->header_value ? evt->header_value : "");

        // Store response headers (if no filter OR matches filter)
        if (self->collectKeys.empty())
        {
            self->responseHeaders.push_back({key, val});
        }
        else
        {
            for (auto &wanted : self->collectKeys)
            {
                if (key.equalsIgnoreCase(wanted))
                {
                    self->responseHeaders.push_back({key, val});
                    break;
                }
            }
        }
        if (key.equalsIgnoreCase("Location"))
        {
            self->_location = val;
            log_d("Redirect location: %s", self->_location.c_str());
        }
        break;
    }

    case HTTP_EVENT_ON_DATA:
        log_d("HTTP_EVENT_ON_DATA len=%d", evt->data_len);

        // Check if this is a chunked response
        if (self->_size == 0 || self->_size == -1)
        {
            self->_isChunked = esp_http_client_is_chunked_response(self->_client);
            if (self->_isChunked)
            {
                log_d("Detected chunked response");
            }
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        log_d("HTTP_EVENT_ON_FINISH");
        break;

    case HTTP_EVENT_DISCONNECTED:
        log_d("HTTP_EVENT_DISCONNECTED");
        break;

    case HTTP_EVENT_REDIRECT:
        log_d("HTTP_EVENT_REDIRECT -> %s", self->_location.c_str());
        break;
    }

    return ESP_OK;
}

HTTPClient::HTTPClient() {}

HTTPClient::~HTTPClient()
{
    if (_client)
    {
        _client = nullptr;
    }
    end();
}

void HTTPClient::clear()
{
    responseHeaders.clear();
    _returnCode = 0;
    _size = -1;
    _bytesread = 0;
    _peekedChar = -1;
    _location = String();
    _write_len = 0;

    // Reset chunked response state
    _isChunked = false;
    _originalChunkSize = 0;
    _chunkBytesConsumed = 0;
    _chunksFinished = false;
    _needNewChunk = true;
}

bool HTTPClient::begin(String url, const char *CAcert, bool use_crt_bundle)
{
    if (_client)
        end();
    if (!beginInternal(url, "https"))
    {
        return false;
    }

    if (!use_crt_bundle)
    {
        if (!CAcert || strlen(CAcert) == 0)
            return false;

        _CAcert = CAcert;
    }
    _url = url;
    _config.url = _url.c_str();
    _secure = true;
    return true;
}

bool HTTPClient::begin(String url)
{
    if (_client)
        end();

    if (!beginInternal(url, "http"))
    {
        return begin(url, NULL, true);
    }

    _url = url;
    _config.url = _url.c_str();
    return true;
}

bool HTTPClient::beginInternal(String url, const char *expectedProtocol)
{
    log_v("url: %s", url.c_str());

    // check for : (http: or https:
    int index = url.indexOf(':');
    if (index < 0)
    {
        log_e("failed to parse protocol");
        return false;
    }

    String _proto = url.substring(0, index);
    if (_proto != expectedProtocol)
    {
        log_d("unexpected protocol: %s, expected %s", _proto.c_str(), expectedProtocol);
        return false;
    }
    _lastprotocol = _proto;
    return true;
}

bool HTTPClient::begin(const char *host, uint16_t port, const char *uri)
{
    if (_client)
        end();

    _config.host = host;
    _config.port = port;
    _config.path = uri;

    return true;
}

bool HTTPClient::begin(const char *host, uint16_t port, const char *uri, const char *CAcert, bool use_crt_bundle)
{
    if (_client)
        end();

    if (!use_crt_bundle)
    {
        if (!CAcert || strlen(CAcert) == 0)
            return false;

        _CAcert = CAcert;
    }

    _config.host = host;
    _config.port = port;
    _config.path = uri;
    _secure = true;

    return true;
}

bool HTTPClient::begin(const char *host, uint16_t port, const char *uri, const char *CAcert, const char *cli_cert, const char *cli_key)
{
    if (_client)
        end();

    _config.host = host;
    _config.port = port;
    _config.path = uri;

    if (strlen(CAcert) == 0)
        return false;

    _CAcert = CAcert;
    _cli_cert = cli_cert;
    _cli_key = cli_key;
    _secure = true;

    return true;
}

void HTTPClient::end(void)
{
    if (_client)
    {
        if (_connected)
        {
            esp_http_client_close(_client);
            _connected = false;
        }
        esp_http_client_cleanup(_client);
        _client = nullptr;
    }
    _mustReinit = false;
    clear();
}

void HTTPClient::disconnect(bool preserveClient)
{
    if (_client && _connected)
    {
        esp_http_client_close(_client);
        _connected = false;
    }

    if (!preserveClient && _client)
    {
        esp_http_client_cleanup(_client);
        _client = nullptr;
    }

    clear();
}

bool HTTPClient::connected()
{
    if (_client)
    {
        return ((available() > 0) || _connected);
    }
    else
    {
        _connected = false;
        return false;
    }
}

void HTTPClient::setUserAgent(const String &userAgent)
{
    _userAgent = userAgent;
}

void HTTPClient::setAuthorization(const char *user, const char *password)
{
    if (user && password)
    {
        _username = user;
        _password = password;
        if (_client)
        {
            esp_http_client_set_username(_client, user);
            esp_http_client_set_password(_client, password);
        }
    }
}

void HTTPClient::setAuthorizationType(esp_http_client_auth_type_t authType)
{
    if (_client)
        esp_http_client_set_authtype(_client, authType);
    _authorizationType = authType;
}

void HTTPClient::setConnectTimeout(int32_t connectTimeout)
{
    if (_client)
        esp_http_client_set_timeout_ms(_client, connectTimeout);
    _connectTimeout = connectTimeout;
}

void HTTPClient::setKeepAlive(bool enable, int idle, int interval, int count)
{
    _keep_alive_enable = enable;
    _keep_alive_idle = idle;
    _keep_alive_interval = interval;
    _keep_alive_count = count;
}

int HTTPClient::GET()
{
    if (!_reuse && _mustReinit)
    {
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    _write_len = 0;
    int result = sendRequest(HTTP_METHOD_GET);
    if (!_reuse)
    {
        _mustReinit = true;
    }
    return result;
}

int HTTPClient::POST(uint8_t *payload, size_t size)
{
    if (!_reuse && _mustReinit)
    {
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    _write_len = size;
    int result = sendRequest(HTTP_METHOD_POST, payload, size);
    if (!_reuse)
    {
        _mustReinit = true;
    }
    return result;
}

int HTTPClient::POST(String payload)
{
    return POST((uint8_t *)payload.c_str(), payload.length());
}

int HTTPClient::PATCH(uint8_t *payload, size_t size)
{
    if (!_reuse && _mustReinit)
    {
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    _write_len = size;
    int result = sendRequest(HTTP_METHOD_PATCH, payload, size);
    if (!_reuse)
    {
        _mustReinit = true;
    }
    return result;
}

int HTTPClient::PATCH(String payload)
{
    return PATCH((uint8_t *)payload.c_str(), payload.length());
}

int HTTPClient::PUT(uint8_t *payload, size_t size)
{
    if (!_reuse && _mustReinit)
    {
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    _write_len = size;
    int result = sendRequest(HTTP_METHOD_PUT, payload, size);
    if (!_reuse)
    {
        _mustReinit = true;
    }
    return result;
}

int HTTPClient::PUT(String payload)
{
    return PUT((uint8_t *)payload.c_str(), payload.length());
}

esp_err_t HTTPClient::_configClient(esp_http_client_method_t type)
{
    // Initialize client
    if (!_client || !_reuse)
    {
        if (_client && !_reuse)
            end();

        _config.timeout_ms = _connectTimeout;
        _config.keep_alive_enable = _keep_alive_enable;
        _config.keep_alive_idle = _keep_alive_idle;
        _config.keep_alive_interval = _keep_alive_interval;
        _config.keep_alive_count = _keep_alive_count;
        _config.method = type;
        _config.event_handler = &_http_event_handler;
        _config.user_data = this;
        _config.buffer_size = HTTP_TCP_RX_BUFFER_SIZE;
        _config.buffer_size_tx = HTTP_TCP_TX_BUFFER_SIZE;
        _config.user_agent = _userAgent.c_str();
        _config.disable_auto_redirect = true;

        if (_username && _password)
        {
            _config.auth_type = HTTP_AUTH_TYPE_BASIC;
            _config.username = _username;
            _config.password = _password;
        }
        else
        {
            _config.auth_type = HTTP_AUTH_TYPE_NONE;
        }

        if (_secure)
        {
            if (_CAcert)
            {
                _config.cert_pem = _CAcert;
                _config.cert_len = strlen(_CAcert) + 1;
                if (_cli_cert && _cli_key)
                {
                    _config.client_cert_pem = _cli_cert;
                    _config.client_key_pem = _cli_key;
                }
            }
            else
            {
                _config.crt_bundle_attach = esp_crt_bundle_attach;
            }
            _config.transport_type = HTTP_TRANSPORT_OVER_SSL;
        }
        else
        {
            _config.transport_type = HTTP_TRANSPORT_OVER_TCP;
        }

        _client = esp_http_client_init(&_config);
        if (!_client)
        {
            log_e("Unable to create HTTP client");
            return ESP_FAIL;
        }
    }
    else if (_client && _reuse)
    {
        return esp_http_client_set_method(_client, type);
    }
    return ESP_OK;
}

int HTTPClient::sendRequest(esp_http_client_method_t type, String payload)
{
    if (!_reuse && _mustReinit)
    {
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    return sendRequest(type, (uint8_t *)payload.c_str(), payload.length());
}

int HTTPClient::sendRequest(esp_http_client_method_t type, uint8_t *payload, size_t size)
{
    if (!_reuse && _mustReinit)
    {
        return returnError(HTTPC_ERROR_NOT_CONNECTED);
    }
    int code;
    bool redirect = false;
    uint16_t redirectCount = 0;

    if (_configClient(type) != ESP_OK)
        return returnError(HTTPC_ERROR_CLIENT_CONFIG);

    do
    {
        responseHeaders.clear();
        log_d("redirCount: %d\n", redirectCount);

        // send Header
        if (!sendHeader())
            return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);

        // connect to server (or reuse existing connection)
        if (!connect())
            return returnError(HTTPC_ERROR_CONNECTION_REFUSED);

        // send Payload if needed
        if (payload && size > 0)
        {
            write(payload, size);
        }

        code = handleHeaderResponse();
        log_d("sendRequest code=%d\n", code);

        redirect = false;
        if (_followRedirects != HTTPC_DISABLE_FOLLOW_REDIRECTS && redirectCount < _redirectLimit && _location.length() > 0)
        {
            switch (code)
            {
            // redirecting using the same method
            case HTTP_CODE_MOVED_PERMANENTLY:
            case HTTP_CODE_TEMPORARY_REDIRECT:
            {
                if ((_followRedirects == HTTPC_FORCE_FOLLOW_REDIRECTS) || (type == HTTP_METHOD_GET) || (type == HTTP_METHOD_HEAD))
                {
                    redirectCount += 1;
                    log_d("following redirect (the same method): '%s' redirCount: %d\n", _location.c_str(), redirectCount);
                    if (!setURL(_location))
                    {
                        log_d("failed setting URL for redirection\n");
                        break; // no redirection
                    }

                    redirect = true; // redirect using the same request method and payload, different URL
                }
                break;
            }

            case HTTP_CODE_FOUND:
            case HTTP_CODE_SEE_OTHER:
            {
                redirectCount += 1;
                log_d("following redirect (dropped to GET/HEAD): '%s' redirCount: %d\n", _location.c_str(), redirectCount);
                if (!setURL(_location))
                {
                    log_d("failed setting URL for redirection\n");
                    break; // no redirection
                }
                // redirect after changing method to GET/HEAD and dropping payload
                esp_http_client_set_method(_client, (type == HTTP_METHOD_HEAD) ? HTTP_METHOD_HEAD : HTTP_METHOD_GET);
                payload = nullptr;
                size = 0;
                redirect = true;
                break;
            }

            default:
                break;
            }
        }

    } while (redirect);

    if (redirectCount >= _redirectLimit)
    {
        return returnError(HTTPC_ERROR_REDIRECT_LIMIT_REACHED);
    }
    return returnError(code);
}

size_t HTTPClient::write(uint8_t byte)
{
    return write(&byte, 1);
}

size_t HTTPClient::write(const uint8_t *buffer, size_t size)
{
    if (!_connected)
        return returnError(HTTPC_ERROR_NOT_CONNECTED);

    if (size == 0 || !buffer)
        return 0;

    size_t sent_bytes = 0;
    int retry_count = 0;
    const int max_retries = 3;

    while (sent_bytes < size && retry_count < max_retries)
    {
        int sent = esp_http_client_write(_client, (char *)&buffer[sent_bytes], size - sent_bytes);
        if (sent < 0)
        {
            log_e("Write error: %d", sent);
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
        else if (sent == 0)
        {
            retry_count++;
            log_w("Failed to send chunk! Retry %d/%d", retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            sent_bytes += sent;
            retry_count = 0; // Reset retry count on successful write
        }
    }

    if (sent_bytes != size)
    {
        log_e("Failed to send complete payload: %d/%d bytes", sent_bytes, size);
        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
    }
    return sent_bytes;
}

/**
 * sendRequest
 * @param type const char *     "GET", "POST", ....
 * @param stream Stream *       data stream for the message body
 * @param size size_t           size for the message body if 0 not Content-Length is send
 * @return -1 if no info or > 0 when Content-Length is set by server
 */
int HTTPClient::sendRequest(esp_http_client_method_t type, Stream *stream, size_t size)
{
    if (!_reuse && _mustReinit)
    {
        return returnError(HTTPC_ERROR_NOT_CONNECTED);
    }
    if (!stream)
    {
        return returnError(HTTPC_ERROR_NO_STREAM);
    }

    if (_configClient(type) != ESP_OK)
        return returnError(HTTPC_ERROR_CLIENT_CONFIG);

    // send Header
    if (!sendHeader())
    {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    // connect to server
    if (!connect())
    {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    int buff_size = HTTP_TCP_TX_BUFFER_SIZE;

    int len = size;
    int bytesWritten = 0;

    if (len == 0)
    {
        len = -1;
    }

    // if possible create smaller buffer then HTTP_TCP_TX_BUFFER_SIZE
    if ((len > 0) && (len < buff_size))
    {
        buff_size = len;
    }

    // create buffer for read
    uint8_t *buff = (uint8_t *)malloc(buff_size);

    if (buff)
    {
        // read all data from stream and send it to server
        while (connected() && (stream->available() > -1) && (len > 0 || len == -1))
        {
            // get available data size
            int sizeAvailable = stream->available();

            if (sizeAvailable)
            {

                int readBytes = sizeAvailable;

                // read only the asked bytes
                if (len > 0 && readBytes > len)
                {
                    readBytes = len;
                }

                // not read more the buffer can handle
                if (readBytes > buff_size)
                {
                    readBytes = buff_size;
                }

                // read data
                int bytesRead = stream->readBytes(buff, readBytes);

                // write it to Stream
                int bytesWrite = write((const uint8_t *)buff, bytesRead);
                bytesWritten += bytesWrite;

                // are all Bytes a written to stream ?
                if (bytesWrite != bytesRead)
                {
                    log_d("short write, asked for %d but got %d retry...", bytesRead, bytesWrite);

                    // check for write error
                    if (this->getWriteError())
                    {
                        log_d("stream write error %d", this->getWriteError());

                        // reset write error for retry
                        this->clearWriteError();
                    }

                    // some time for the stream
                    vTaskDelay(pdMS_TO_TICKS(1));

                    int leftBytes = (readBytes - bytesWrite);

                    // retry to send the missed bytes
                    bytesWrite = this->write((const uint8_t *)(buff + bytesWrite), leftBytes);
                    bytesWritten += bytesWrite;

                    if (bytesWrite != leftBytes)
                    {
                        // failed again
                        log_d("short write, asked for %d but got %d failed.", leftBytes, bytesWrite);
                        free(buff);
                        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                    }
                }

                // check for write error
                if (this->getWriteError())
                {
                    log_d("stream write error %d", this->getWriteError());
                    free(buff);
                    return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                }

                // count bytes to read left
                if (len > 0)
                {
                    len -= readBytes;
                }

                vTaskDelay(pdMS_TO_TICKS(0));
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        free(buff);

        if (size && (int)size != bytesWritten)
        {
            log_d("Stream payload bytesWritten %d and size %d mismatch!.", bytesWritten, size);
            log_d("ERROR SEND PAYLOAD FAILED!");
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
        else
        {
            log_d("Stream payload written: %d", bytesWritten);
        }
    }
    else
    {
        log_d("too less ram! need %d", buff_size);
        return returnError(HTTPC_ERROR_TOO_LESS_RAM);
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
}

int HTTPClient::getSize(void)
{
    return _size;
}

String HTTPClient::errorToString(int error)
{
    switch (error)
    {
    case HTTPC_ERROR_CONNECTION_REFUSED:
        return F("connection refused");
    case HTTPC_ERROR_SEND_HEADER_FAILED:
        return F("send header failed");
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
        return F("send payload failed");
    case HTTPC_ERROR_NOT_CONNECTED:
        return F("not connected");
    case HTTPC_ERROR_CONNECTION_LOST:
        return F("connection lost");
    case HTTPC_ERROR_NO_STREAM:
        return F("no stream");
    case HTTPC_ERROR_NO_HTTP_SERVER:
        return F("no HTTP server");
    case HTTPC_ERROR_TOO_LESS_RAM:
        return F("too less ram");
    case HTTPC_ERROR_ENCODING:
        return F("Transfer-Encoding not supported");
    case HTTPC_ERROR_STREAM_WRITE:
        return F("Stream write error");
    case HTTPC_ERROR_READ_TIMEOUT:
        return F("read Timeout");
    case HTTPC_ERROR_FETCH_HEADERS:
        return F("fetch headers error");
    case HTTPC_ERROR_REDIRECT_LIMIT_REACHED:
        return F("redirect limit reached");
    case HTTPC_ERROR_CLIENT_CONFIG:
        return F("client configuration error");
    default:
        return String();
    }
}

void HTTPClient::addHeader(const String &name, const String &value, bool first, bool replace)
{
    // not allow set of Header handled by code
    if (!name.equalsIgnoreCase(F("Connection")) && !name.equalsIgnoreCase(F("User-Agent")) && !name.equalsIgnoreCase(F("Accept-Encoding")) && !name.equalsIgnoreCase(F("Host")) && !(name.equalsIgnoreCase(F("Authorization"))))
    {
        if (replace)
        {
            for (auto &h : _requestHeaders)
            {
                if (h.first.equalsIgnoreCase(name))
                {
                    h.second = value;
                    return;
                }
            }
        }
        if (first)
            _requestHeaders.insert(_requestHeaders.begin(), {name, value});
        else
            _requestHeaders.push_back({name, value});
    }
}

void HTTPClient::collectHeaders(const char *headerKeys[], const size_t headerKeysCount)
{
    collectKeys.clear();
    for (size_t i = 0; i < headerKeysCount; i++)
    {
        collectKeys.push_back(String(headerKeys[i]));
    }
    responseHeaders.clear();
}

String HTTPClient::header(const char *name)
{
    for (auto &h : responseHeaders)
    {
        if (h.first.equalsIgnoreCase(name))
            return h.second;
    }
    return String();
}

String HTTPClient::header(size_t i)
{
    if (i < responseHeaders.size())
        return responseHeaders[i].second;
    return String();
}

String HTTPClient::headerName(size_t i)
{
    if (i < responseHeaders.size())
        return responseHeaders[i].first;
    return String();
}

int HTTPClient::headers()
{
    return responseHeaders.size();
}

bool HTTPClient::hasHeader(const char *name)
{
    for (auto &h : responseHeaders)
    {
        if (h.first.equalsIgnoreCase(name) && h.second.length() > 0)
            return true;
    }
    return false;
}

// Implementation of chunked support methods
bool HTTPClient::isChunkedResponse()
{
    return _isChunked;
}

bool HTTPClient::updateChunkInfo()
{
    if (!_client || !_isChunked)
    {
        return false;
    }

    // Only get new chunk info when we need it
    if (_needNewChunk || (_chunkBytesConsumed >= _originalChunkSize))
    {
        esp_err_t err = esp_http_client_get_chunk_length(_client, &_originalChunkSize);
        if (err != ESP_OK)
        {
            log_e("Failed to get chunk length: %s", esp_err_to_name(err));
            return false;
        }

        log_d("New chunk - Original size: %d", _originalChunkSize);

        // Chunk size 0 means end of chunks
        if (_originalChunkSize == 0)
        {
            _chunksFinished = true;
            log_d("All chunks received");
            return false;
        }

        _chunkBytesConsumed = 0;
        _needNewChunk = false;
    }

    return true;
}

int HTTPClient::peek()
{
    if (_peekedChar >= 0)
        return _peekedChar;

    // Check if we have data available
    if (_isChunked && !_chunksFinished)
    {
        if (_chunkBytesConsumed >= _originalChunkSize)
        {
            _needNewChunk = true;
            if (!updateChunkInfo())
            {
                return -1; // No more data
            }
        }
        if (_chunkBytesConsumed >= _originalChunkSize)
        {
            return -1; // No data in current chunk
        }
    }

    uint8_t c;
    int len = esp_http_client_read(_client, (char *)&c, 1);
    if (len == 1)
    {
        _peekedChar = c;
        return _peekedChar;
    }
    return -1;
}

int HTTPClient::read()
{
    if (_peekedChar >= 0)
    {
        int c = _peekedChar;
        _peekedChar = -1;
        return c;
    }
    uint8_t c;
    if (read(&c, 1) == 1)
        return c;

    return -1;
}

int HTTPClient::read(uint8_t *buf, size_t size)
{
    size_t count = 0;

    // consume peeked char if present
    if (_peekedChar >= 0 && size > 0)
    {
        buf[0] = (uint8_t)_peekedChar;
        _peekedChar = -1;
        _bytesread++;
        if (_isChunked)
        {
            _chunkBytesConsumed++;
        }
        count = 1;
    }

    if (count < size)
    {
        size_t requestSize = size - count;

        if (_isChunked && !_chunksFinished)
        {
            // Update chunk info if we've consumed current chunk
            if (_chunkBytesConsumed >= _originalChunkSize)
            {
                _needNewChunk = true;
                if (!updateChunkInfo())
                {
                    return count; // No more data
                }
            }

            // Limit read to remaining bytes in current chunk
            int remainingInChunk = _originalChunkSize - _chunkBytesConsumed;
            if ((int)requestSize > remainingInChunk)
            {
                requestSize = remainingInChunk;
            }
        }

        if (requestSize > 0)
        {
            int len = esp_http_client_read(_client, (char *)(buf + count), requestSize);
            if (len > 0)
            {
                _bytesread += len;
                if (_isChunked)
                {
                    _chunkBytesConsumed += len;

                    // Check if we've finished current chunk
                    if (_chunkBytesConsumed >= _originalChunkSize)
                    {
                        _needNewChunk = true;
                        log_d("Finished chunk of size %d", _originalChunkSize);
                    }
                }
                count += len;
            }
        }
    }

    return count;
}

int HTTPClient::available()
{
    if (_peekedChar >= 0)
    {
        return 1;
    }

    if (_isChunked && !_chunksFinished)
    {
        // Update chunk info if needed
        if (!updateChunkInfo())
        {
            return 0;
        }

        // Return remaining bytes in current chunk (original size - consumed)
        int remaining = _originalChunkSize - _chunkBytesConsumed;
        return (remaining > 0) ? remaining : 0;
    }

    if (_size >= 0)
    {
        return _size - _bytesread;
    }

    return 0;
}

Stream *HTTPClient::getStreamPtr()
{
    if (_connected && _client)
    {
        return static_cast<Stream *>(this);
    }
    return nullptr;
}

Stream &HTTPClient::getStream()
{
    return static_cast<Stream &>(*this);
}

void HTTPClient::flush()
{
    if (_client && connected())
    {
        esp_http_client_flush_response(_client, NULL);
        while (available() > 0)
        {
            read();
        }
    }
}

/**
 * return all payload as String (may need lot of ram or trigger out of memory!)
 * @return String
 */
String HTTPClient::getString(void)
{
    // _size can be -1 when Server sends no Content-Length header
    if (_size > 0 || _size == -1)
    {
        StreamString sstring;
        // try to reserve needed memory (noop if _size == -1)
        if (sstring.reserve((_size + 1)))
        {
            writeToStream(&sstring);
            return sstring;
        }
        else
        {
            log_d("not enough memory to reserve a string! need: %d", (_size + 1));
        }
    }

    return "";
}

/**
 * write all  message body / payload to Stream
 * @param stream Stream *
 * @return bytes written ( negative values are error codes )
 */
int HTTPClient::writeToStream(Stream *stream)
{

    if (!stream)
    {
        return returnError(HTTPC_ERROR_NO_STREAM);
    }

    if (!connected())
    {
        return returnError(HTTPC_ERROR_NOT_CONNECTED);
    }

    // get length of document (is -1 when Server sends no Content-Length header)
    int len = _size;
    int ret = 0;

    if (!_isChunked)
    {
        ret = writeToStreamDataBlock(stream, len);

        // have we an error?
        if (ret < 0)
        {
            return returnError(ret);
        }
    }
    else if (_isChunked)
    {
        int size = 0;
        while (1)
        {
            if (!connected())
            {
                return returnError(HTTPC_ERROR_CONNECTION_LOST);
            }

            // read size of chunk
            len = _originalChunkSize;
            size += len;
            log_v(" read chunk len: %d", len);
            // data left?
            if (len > 0)
            {
                int r = writeToStreamDataBlock(stream, len);
                if (r < 0)
                {
                    // error in writeToStreamDataBlock
                    return returnError(r);
                }
                ret += r;
            }
            else
            {
                // if no length Header use global chunk size
                if (_size <= 0)
                {
                    _size = size;
                }
                // check if we have write all data out
                if (ret != _size)
                {
                    return returnError(HTTPC_ERROR_STREAM_WRITE);
                }
                break;
            }
            // read trailing \r\n at the end of the chunk
            char buf[2];
            auto trailing_seq_len = readBytes((uint8_t *)buf, 2);
            if (trailing_seq_len != 2 || buf[0] != '\r' || buf[1] != '\n')
            {
                return returnError(HTTPC_ERROR_READ_TIMEOUT);
            }
            vTaskDelay(pdMS_TO_TICKS(0));
        }
    }
    else
    {
        return returnError(HTTPC_ERROR_ENCODING);
    }
    //    end();
    disconnect(true);
    return ret;
}

/**
 * write one Data Block to Stream
 * @param stream Stream *
 * @param size int
 * @return < 0 = error >= 0 = size written
 */
int HTTPClient::writeToStreamDataBlock(Stream *stream, int size)
{
    int buff_size = HTTP_TCP_RX_BUFFER_SIZE;
    int len = size;
    int bytesWritten = 0;

    // if possible create smaller buffer then HTTP_TCP_RX_BUFFER_SIZE
    if ((len > 0) && (len < buff_size))
    {
        buff_size = len;
    }

    // create buffer for read
    uint8_t *buff = (uint8_t *)malloc(buff_size);

    if (buff)
    {
        // read all data from server
        while (connected() && (len > 0 || len == -1))
        {

            // get available data size
            size_t sizeAvailable = buff_size;
            if (len < 0)
            {
                sizeAvailable = available();
            }

            if (sizeAvailable)
            {

                int BytesRead = sizeAvailable;

                // read only the asked bytes
                if (len > 0 && BytesRead > len)
                {
                    BytesRead = len;
                }

                // not read more the buffer can handle
                if (BytesRead > buff_size)
                {
                    BytesRead = buff_size;
                }

                // stop if no more reading
                if (BytesRead == 0)
                {
                    break;
                }

                // read data
                int bytesRead = readBytes(buff, BytesRead);

                // write it to Stream
                int bytesWrite = stream->write(buff, bytesRead);
                bytesWritten += bytesWrite;

                // are all Bytes a written to stream ?
                if (bytesWrite != bytesRead)
                {
                    log_d("short write asked for %d but got %d retry...", bytesRead, bytesWrite);

                    // check for write error
                    if (stream->getWriteError())
                    {
                        log_d("stream write error %d", stream->getWriteError());

                        // reset write error for retry
                        stream->clearWriteError();
                    }

                    // some time for the stream
                    vTaskDelay(pdMS_TO_TICKS(1));

                    int leftBytes = (bytesRead - bytesWrite);

                    // retry to send the missed bytes
                    bytesWrite = stream->write((buff + bytesWrite), leftBytes);
                    bytesWritten += bytesWrite;

                    if (bytesWrite != leftBytes)
                    {
                        // failed again
                        log_w("short write asked for %d but got %d failed.", leftBytes, bytesWrite);
                        free(buff);
                        return HTTPC_ERROR_STREAM_WRITE;
                    }
                }

                // check for write error
                if (stream->getWriteError())
                {
                    log_w("stream write error %d", stream->getWriteError());
                    free(buff);
                    return HTTPC_ERROR_STREAM_WRITE;
                }

                // count bytes to read left
                if (len > 0)
                {
                    len -= bytesRead;
                }

                vTaskDelay(pdMS_TO_TICKS(0));
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        free(buff);

        log_v("connection closed or file end (written: %d).", bytesWritten);

        if ((size > 0) && (size != bytesWritten))
        {
            log_d("bytesWritten %d and size %d mismatch!.", bytesWritten, size);
            return HTTPC_ERROR_STREAM_WRITE;
        }
    }
    else
    {
        log_w("too less ram! need %d", buff_size);
        return HTTPC_ERROR_TOO_LESS_RAM;
    }

    return bytesWritten;
}

void HTTPClient::setReuse(bool reuse)
{
    _reuse = reuse;
    if (!reuse)
    {
        _mustReinit = false; // Will be set true after first request
    }
    _mustReinit = false;
}

bool HTTPClient::connect(void)
{
    if (!_client)
        return false;
    if (_connected && _reuse)
        return true; // Connection might still be open, try to reuse it

    // Close any existing connection first
    if (_connected)
    {
        esp_http_client_close(_client);
        _connected = false;
    }

    esp_err_t err = esp_http_client_open(_client, _write_len);
    if (err != ESP_OK)
    {
        log_e("Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }

    _connected = true;
    return true;
}

bool HTTPClient::sendHeader()
{
    if (!_client)
        return false;

    for (auto &h : _requestHeaders)
    {
        if (esp_http_client_set_header(_client, h.first.c_str(), h.second.c_str()) == ESP_FAIL)
            return false;
    }
    return true;
}

void HTTPClient::removeHeader(const String &name)
{
    auto it = _requestHeaders.begin();
    while (it != _requestHeaders.end())
    {
        if (it->first.equalsIgnoreCase(name))
        {
            if (_client)
            {
                esp_err_t err = esp_http_client_delete_header(_client, name.c_str());
                if (err != ESP_OK)
                {
                    log_e("Failed to delete header '%s': %s", name.c_str(), esp_err_to_name(err));
                }
                else
                {
                    log_d("Deleted header '%s' from client", name.c_str());
                }
            }
            it = _requestHeaders.erase(it);
        }
        else
        {
            ++it;
        }
    }
    // print all header after remove
    uint8_t count = 0;
    for (auto &h : _requestHeaders)
    {
        log_d("header[%d]: %s: %s", count++, h.first.c_str(), h.second.c_str());
    }
}

int HTTPClient::handleHeaderResponse()
{
    if (!connected())
        return HTTPC_ERROR_NOT_CONNECTED;

    _returnCode = 0;
    _size = -1;

    // Fetch headers (blocking until all headers arrive)
    _size = esp_http_client_fetch_headers(_client);

    log_d("Content-Length by esp_http_client_fetch_headers: %d", _size);

    if (_size == -ESP_ERR_HTTP_EAGAIN)
    {
        returnError(HTTPC_ERROR_READ_TIMEOUT);
    }
    else if (_size < 0)
    {
        returnError(HTTPC_ERROR_FETCH_HEADERS);
    }

    // HTTP status code
    _returnCode = esp_http_client_get_status_code(_client);
    if (_returnCode <= 0)
        return HTTPC_ERROR_NO_HTTP_SERVER;

    return _returnCode;
}

int HTTPClient::returnError(int error)
{
    if (error < 0)
    {
        log_e("error(%d): %s", error, errorToString(error).c_str());
        _returnCode = error;
        end();
    }
    return error;
}

void HTTPClient::setFollowRedirects(followRedirects_t follow)
{
    _followRedirects = follow;
}

void HTTPClient::setRedirectLimit(uint16_t limit)
{
    _redirectLimit = limit;
}

bool HTTPClient::setURL(const String &url)
{
    if (!_reuse || _mustReinit)
    {
        // Not allowed to set URL if reuse is false and must reinit
        return false;
    }
    if (!_client)
    {
        return false;
    }

    if (!beginInternal(url, _lastprotocol.c_str()))
    {
        log_d("Full Client Reset because last url protocol was different");
        disconnect();
        return begin(url);
    }
    esp_err_t err = esp_http_client_set_url(_client, url.c_str());
    if (err != ESP_OK)
    {
        return false;
    }

    // Reset per-response state but keep request headers intact
    disconnect(true);
    clearRequestSpecificHeaders();
    clearClientHeaders();
    return true;
}

void HTTPClient::clearRequestHeaders()
{
    _requestHeaders.clear();
}

void HTTPClient::clearRequestSpecificHeaders()
{
    // Remove only request-specific headers, keep persistent ones
    auto it = _requestHeaders.begin();
    while (it != _requestHeaders.end())
    {
        if (it->first.equalsIgnoreCase("Content-Length") ||
            it->first.equalsIgnoreCase("Content-Type") ||
            it->first.equalsIgnoreCase("Accept"))
        {
            it = _requestHeaders.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void HTTPClient::clearClientHeaders()
{
    if (!_client)
        return;

    // Clear common headers that might accumulate
    esp_http_client_delete_header(_client, "Content-Type");
    esp_http_client_delete_header(_client, "Content-Length");
    esp_http_client_delete_header(_client, "Accept");
    esp_http_client_delete_header(_client, "Accept-Encoding");

    // Note: Don't clear Host, User-Agent, Connection as they're managed by ESP client
}
