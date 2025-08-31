#ifndef HTTPClient_H_
#define HTTPClient_H_

#include <memory>
#include <Stream.h>
#include <WString.h>
#include "esp_http_client.h"
#include <vector>

#define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)

/// HTTP client errors
#define HTTPC_ERROR_CONNECTION_REFUSED (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED (-2)
#define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
#define HTTPC_ERROR_NOT_CONNECTED (-4)
#define HTTPC_ERROR_CONNECTION_LOST (-5)
#define HTTPC_ERROR_NO_STREAM (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER (-7)
#define HTTPC_ERROR_TOO_LESS_RAM (-8)
#define HTTPC_ERROR_ENCODING (-9)
#define HTTPC_ERROR_STREAM_WRITE (-10)
#define HTTPC_ERROR_READ_TIMEOUT (-11)
#define HTTPC_ERROR_FETCH_HEADERS (-12)
#define HTTPC_ERROR_CLIENT_CONFIG (-13)
#define HTTPC_ERROR_REDIRECT_LIMIT_REACHED (-14)

/// size for the stream handling
#define HTTP_TCP_RX_BUFFER_SIZE (4096)
#define HTTP_TCP_TX_BUFFER_SIZE (1460)

/// HTTP codes see RFC7231
typedef enum
{
    HTTP_CODE_CONTINUE = 100,
    HTTP_CODE_SWITCHING_PROTOCOLS = 101,
    HTTP_CODE_PROCESSING = 102,
    HTTP_CODE_OK = 200,
    HTTP_CODE_CREATED = 201,
    HTTP_CODE_ACCEPTED = 202,
    HTTP_CODE_NON_AUTHORITATIVE_INFORMATION = 203,
    HTTP_CODE_NO_CONTENT = 204,
    HTTP_CODE_RESET_CONTENT = 205,
    HTTP_CODE_PARTIAL_CONTENT = 206,
    HTTP_CODE_MULTI_STATUS = 207,
    HTTP_CODE_ALREADY_REPORTED = 208,
    HTTP_CODE_IM_USED = 226,
    HTTP_CODE_MULTIPLE_CHOICES = 300,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_FOUND = 302,
    HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_NOT_MODIFIED = 304,
    HTTP_CODE_USE_PROXY = 305,
    HTTP_CODE_TEMPORARY_REDIRECT = 307,
    HTTP_CODE_PERMANENT_REDIRECT = 308,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_UNAUTHORIZED = 401,
    HTTP_CODE_PAYMENT_REQUIRED = 402,
    HTTP_CODE_FORBIDDEN = 403,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_NOT_ACCEPTABLE = 406,
    HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED = 407,
    HTTP_CODE_REQUEST_TIMEOUT = 408,
    HTTP_CODE_CONFLICT = 409,
    HTTP_CODE_GONE = 410,
    HTTP_CODE_LENGTH_REQUIRED = 411,
    HTTP_CODE_PRECONDITION_FAILED = 412,
    HTTP_CODE_PAYLOAD_TOO_LARGE = 413,
    HTTP_CODE_URI_TOO_LONG = 414,
    HTTP_CODE_UNSUPPORTED_MEDIA_TYPE = 415,
    HTTP_CODE_RANGE_NOT_SATISFIABLE = 416,
    HTTP_CODE_EXPECTATION_FAILED = 417,
    HTTP_CODE_MISDIRECTED_REQUEST = 421,
    HTTP_CODE_UNPROCESSABLE_ENTITY = 422,
    HTTP_CODE_LOCKED = 423,
    HTTP_CODE_FAILED_DEPENDENCY = 424,
    HTTP_CODE_UPGRADE_REQUIRED = 426,
    HTTP_CODE_PRECONDITION_REQUIRED = 428,
    HTTP_CODE_TOO_MANY_REQUESTS = 429,
    HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_NOT_IMPLEMENTED = 501,
    HTTP_CODE_BAD_GATEWAY = 502,
    HTTP_CODE_SERVICE_UNAVAILABLE = 503,
    HTTP_CODE_GATEWAY_TIMEOUT = 504,
    HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED = 505,
    HTTP_CODE_VARIANT_ALSO_NEGOTIATES = 506,
    HTTP_CODE_INSUFFICIENT_STORAGE = 507,
    HTTP_CODE_LOOP_DETECTED = 508,
    HTTP_CODE_NOT_EXTENDED = 510,
    HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED = 511
} t_http_codes;

typedef enum
{
    HTTPC_DISABLE_FOLLOW_REDIRECTS,
    HTTPC_STRICT_FOLLOW_REDIRECTS,
    HTTPC_FORCE_FOLLOW_REDIRECTS
} followRedirects_t;

class HTTPClient : public Stream
{
public:
    HTTPClient();
    ~HTTPClient();

    bool begin(String url);
    bool begin(String url, const char *CAcert, bool use_crt_bundle = false);

    bool begin(const char *host, uint16_t port, const char *uri = "/");
    bool begin(const char *host, uint16_t port, const char *uri, const char *CAcert, bool use_crt_bundle = false);
    bool begin(const char *host, uint16_t port, const char *uri, const char *CAcert, const char *cli_cert, const char *cli_key);

    void end(void);
    bool connected(void);

    void setReuse(bool reuse); /// keep-alive
    void setUserAgent(const String &userAgent);
    void setAuthorization(const char *user, const char *password);

    void setAuthorizationType(esp_http_client_auth_type_t authType);
    void setConnectTimeout(int32_t connectTimeout = 5000);
    void setKeepAlive(bool enable = true, int idle = 5000, int interval = 5000, int count = 5);

    // Redirections
    void setFollowRedirects(followRedirects_t follow);
    void setRedirectLimit(uint16_t limit); // max redirects to follow for a single request
    bool setURL(const String &url);

    int available() override;
    int read() override; // read one byte
    int read(uint8_t *buf, size_t size);
    int peek() override;
    size_t write(uint8_t byte) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    /// request handling
    int GET();
    int PATCH(uint8_t *payload, size_t size);
    int PATCH(String payload);
    int POST(uint8_t *payload, size_t size);
    int POST(String payload);
    int PUT(uint8_t *payload, size_t size);
    int PUT(String payload);
    int sendRequest(esp_http_client_method_t type, String payload);
    int sendRequest(esp_http_client_method_t type, uint8_t *payload = NULL, size_t size = 0);
    int sendRequest(esp_http_client_method_t type, Stream *stream, size_t size);

    void addHeader(const String &name, const String &value, bool first = false, bool replace = true);
    void removeHeader(const String &name);

    /// Response handling
    void collectHeaders(const char *headerKeys[], const size_t headerKeysCount);
    String header(const char *name);  // get request header value by name
    String header(size_t i);          // get request header value by number
    String headerName(size_t i);      // get request header name by number
    int headers();                    // get header count
    bool hasHeader(const char *name); // check if header exists

    int getSize(void);
    const String &getLocation(void);

    Stream &getStream(void);
    Stream *getStreamPtr(void);
    int writeToStream(Stream *stream);
    String getString(void);
    void flush() override;

    static String errorToString(int error);

public:
    bool isResponseChunked() const { return _isChunked; }
    int getCurrentChunkSize() const { return _originalChunkSize; }
    int getCurrentChunkRemaining() const { return _originalChunkSize - _chunkBytesConsumed; }
    int getCurrentChunkConsumed() const { return _chunkBytesConsumed; }

    void closeConnection() { disconnect(true); } // Preserve client for reuse

protected:
    bool beginInternal(String url, const char *expectedProtocol);
    void disconnect(bool preserveClient = false);
    void clear();
    int returnError(int error);
    esp_err_t _configClient(esp_http_client_method_t type);
    bool connect(void);
    bool sendHeader();
    int handleHeaderResponse();
    int writeToStreamDataBlock(Stream *stream, int size);
    void clearRequestHeaders();
    void clearRequestSpecificHeaders();
    void clearClientHeaders();
    bool _connected = false;
    bool _mustReinit = false; // New flag to enforce reinit when reuse is false

    esp_http_client_handle_t _client = nullptr;
    esp_http_client_config_t _config = {};

    /// request handling
    std::vector<std::pair<String, String>> _requestHeaders;
    int32_t _connectTimeout = 5000; //_config.timeout_ms
    String _userAgent = "HTTPClient";

    bool _keep_alive_enable = true;  //_config.keep_alive_enable
    int _keep_alive_idle = 5000;     // _config.keep_alive_idle
    int _keep_alive_interval = 5000; // _config.keep_alive_interval
    int _keep_alive_count = 5;       // _config.keep_alive_count

    const char *_username = nullptr;
    const char *_password = nullptr;
    esp_http_client_auth_type_t _authorizationType = HTTP_AUTH_TYPE_NONE;

    /// Response handling
    std::vector<std::pair<String, String>> responseHeaders; // response headers
    std::vector<String> collectKeys;

    String _url;
    String _lastprotocol;
    bool _secure = false;
    const char *_CAcert = nullptr;
    const char *_cli_cert = nullptr;
    const char *_cli_key = nullptr;

    bool _reuse = true;
    int _returnCode = 0;
    int _size = -1;
    int _bytesread = 0;
    int _peekedChar = -1;
    // bool _canReuse = false;
    int _write_len = 0;

    bool _isChunked = false;
    int _originalChunkSize = 0;  // Original size from get_chunk_length()
    int _chunkBytesConsumed = 0; // Bytes we've read from current chunk
    bool _chunksFinished = false;
    bool _needNewChunk = true; // Flag to get next chunk info

    followRedirects_t _followRedirects = HTTPC_DISABLE_FOLLOW_REDIRECTS;
    uint16_t _redirectLimit = 10;
    String _location;

    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

protected:
    bool updateChunkInfo();
    bool isChunkedResponse();
};

#endif /* HTTPClient_H_ */
