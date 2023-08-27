#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socks.h"
#include "vec.h"

struct vec messages;

reroute(GET, "/messages", get_messages)
void get_messages(struct http_req *req) {
    size_t full_len = 0;
    for (size_t i = 0; i < messages.len; i++) {
        full_len += strlen(((char **)messages.ptr)[i]);
    }

    char *combined = malloc(full_len + 1);
    if (combined == NULL) {
        perror("malloc");
        http_res(req, 500, NULL);
        return;
    }

    full_len = 0;
    for (size_t i = 0; i < messages.len; i++) {
        const char *message = ((char **)messages.ptr)[i];
        printf("Message %s\n", message);
        const size_t len = strlen(message);
        memcpy(combined + full_len, message, len);
        full_len += len;
    }
    combined[full_len] = '\0';

    http_res(req, 200, combined);
    free(combined);
}

reroute(POST, "/messages", post_message)
void post_message(struct http_req *req) {
    char *buf = malloc(req->content_len + strlen("<p>NAME</p><div></div>") + 1);
    if (buf == NULL) {
        perror("malloc");
        http_res(req, 500, NULL);
        return;
    }
    sprintf(buf, "<p>NAME</p><div>%s</div>", req->content);
    if (vec_push(&messages, &buf) < 0) {
        perror("vec_push");
        http_res(req, 500, NULL);
        return;
    }
    printf("%s\n", buf);

    http_res(req, 200, NULL);
}

int main(void) {
    messages = vec_new(sizeof(char *));

    struct route routes[20] = { get_messages_route, post_message_route };
    size_t file_count = 0;
    if (gen_file_routes("www", 3, routes + 2, &file_count) < 0) {
        perror("gen_file_routes");
        return EXIT_FAILURE;
    }

    socks_run(8080, routes, file_count + 2);
}
