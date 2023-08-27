#ifndef SOCKS
#define SOCKS

#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>

// Find the first solid character in a string.
// RETURN:
//      if a solid character is found, return its
//      index, -1 otherwise.
ssize_t first_solid(const char *str);

// Find the last solid character in a string.
// RETURN:
//      if a solid character is found, return its
//      index, -1 otherwise.
ssize_t last_solid(const char *str, size_t len);

// Trims str off both ends.
// Note: this will put a null terminator after
// the last solid character.
// RETURN:
//      if str has solid characters,
//      this will return a slice containing
//      all of them.
//      if str has no solid characters, or is NULL,
//      returns NULL.
char *trim(char *str);

// Finds pat in str and returns its index if any.
// Note: this doesn't care about null termination
// and will check until str_len.
// RETURN:
//      Index of first pat appearance in str.
//      On error, -1
ssize_t strnfind(const char *str, size_t str_len, const char *pat);

// HTTP/1.0 defines GET, POST, HEAD
#define HTTP_VERBLEN 5
#define HTTP_URLLEN 257

struct http_req {
    char ip[INET_ADDRSTRLEN];
    char verb[HTTP_VERBLEN];
    char url[HTTP_URLLEN];
    char path[HTTP_URLLEN];
    char *content;
    int content_len;
    int fd;
    int was_handled;
};

struct route {
    char url[HTTP_URLLEN];
    char path[HTTP_URLLEN];
    char verb[HTTP_VERBLEN];
    void (*handler)(struct http_req *req);
};

#define reroute(verb, endpoint, handler) \
    void handler (struct http_req *); \
    struct route handler##_route = { endpoint, "", #verb, handler };

// Writes a HTTP/1.0 response with status and body (nullable) to req->fd,
// sets req->was_handled to 1, closes req->fd, respectively.
void http_res(struct http_req *req, uint32_t status, char *body);
void http_resn(struct http_req *req, uint32_t status, char *body, size_t body_len);

// TODO docs
int gen_file_routes(const char *root, size_t root_len, struct route *routes, size_t *file_count);

// Opens INADDR_ANY with port, accepts connections, parses HTTP requests,
// calls routes[i].handler if routes[i].url and routes[i].verb matches the
// request. Blocks indefinitely.
void socks_run(uint16_t port, struct route *routes, size_t routes_len);

#endif /* SOCKS */
