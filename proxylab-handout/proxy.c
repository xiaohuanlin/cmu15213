#include <stdio.h>
#include <regex.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


typedef struct cacheobj {
    int ref;
    char *key;
    void *content;
    size_t content_size;
    pthread_mutex_t lock;
    struct cacheobj *next;
} CacheObject;


static CacheObject *head = NULL, *tail = NULL, *prev_evict_iter = NULL, *evict_iter = NULL;
static unsigned int cache_size = 0;
static pthread_mutex_t w_lock;

void cache_init() {
    pthread_mutex_init(&w_lock, NULL);
}

void cache_free() {
    pthread_mutex_destroy(&w_lock);
}

/*
evict cache object from cache linked list.
Using Clock algorithm
if exist object being evicted, then free it,
*/ 
int cache_evict() {
    if (!evict_iter) {
        evict_iter = head;
        prev_evict_iter = tail;
    }

    while (evict_iter) {
        pthread_mutex_lock(&prev_evict_iter->lock);
        pthread_mutex_lock(&evict_iter->lock);
        if (evict_iter->ref == 1) {
            evict_iter->ref = 0;
        } else{
            // juage if the obj being used
            prev_evict_iter->next = evict_iter->next;
            cache_size -= evict_iter->content_size;
            CacheObject *tmp = evict_iter->next;

            if (evict_iter == head && evict_iter == tail) {
                // only one obj
                head = NULL; 
                tail = NULL;
            } else if (evict_iter == head) {
                head = evict_iter->next;
            } else {
                tail = prev_evict_iter;
            }
            pthread_mutex_destroy(&evict_iter->lock);
            Free(evict_iter);
            evict_iter = tmp;

            pthread_mutex_unlock(&prev_evict_iter->lock);
            return 0;
        }
        pthread_mutex_unlock(&prev_evict_iter->lock);
        prev_evict_iter = evict_iter;
        pthread_mutex_unlock(&evict_iter->lock);
        evict_iter = evict_iter->next;
    }
    return -1;
}

int cache_put(char *key, void *content, size_t content_size) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    Pthread_once(&once, cache_init);

    while (cache_size + content_size > MAX_OBJECT_SIZE) {
        // we need evict serverals objects to free size
        if (cache_evict() < 0) {
            printf("all cache objects is being used\n");
            return -1;
        }
    }
    // printf("cache key %s\n", key);

    CacheObject *cache_obj = NULL;
    cache_obj = Malloc(sizeof(CacheObject));

    if (!head) {
        pthread_mutex_lock(&w_lock);
        head = cache_obj;
        tail = cache_obj;
        pthread_mutex_unlock(&w_lock);
    } else {
        pthread_mutex_lock(&tail->lock);
        if (head != tail) {
            pthread_mutex_lock(&head->lock);
        }
        tail->next = cache_obj;
        cache_obj->next = head;
        pthread_mutex_unlock(&tail->lock);

        if (head != tail) {
            pthread_mutex_unlock(&head->lock);
        }
        tail = tail->next;
    }
    pthread_mutex_init(&cache_obj->lock, NULL);
    pthread_mutex_lock(&cache_obj->lock);
    cache_obj->ref = 1;
    cache_obj->key = Malloc(strlen(key) + 1);
    strcpy(cache_obj->key, key);
    cache_obj->content = content;
    cache_obj->content_size = content_size;
    cache_obj->next = head;
    pthread_mutex_unlock(&cache_obj->lock);


    pthread_mutex_lock(&w_lock);
    cache_size += content_size;
    pthread_mutex_unlock(&w_lock);
    return 0;
}

void *cache_get(char *path, size_t *size) {
    CacheObject *iter = head;
    while (iter) {
        pthread_mutex_lock(&iter->lock);
        // printf("iter key %s", iter->key);
        if (!strcmp(iter->key, path)) {
            iter->ref = 1;
            *size = iter->content_size;
            // printf("cache get %s\n", iter->key);
            pthread_mutex_unlock(&iter->lock);
            return iter->content;
        }
        pthread_mutex_unlock(&iter->lock);
        iter = iter->next;
        // if pointer traverses the whole linked list, then return NULL
        if (iter == head) {
            return NULL;
        }
    }
    return NULL;
}

/*
find the first alphabet which is not a space
*/
int first_not_space(char *words, int start) {
    int len = strlen(words);
    while (start < len) {
        if (words[start] != ' ') {
            return start;
        }
        start++;
    }
    return start;
}

int parse_headers(int fd, char *method, char *uri, char *version, char *host, char *other_headers) {
    char buf[MAXLINE];
    rio_t rio;
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
        return -1;
    }
    sscanf(buf, "%s %s %s", method, uri, version);

    while (strcmp(buf, "\r\n")) {
        if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
            return -1;
        }

        if (!strncmp(buf, "Host", 4)) {
            strcpy(host, buf + first_not_space(buf, 5));
        } else if (!strncmp(buf, "User-Agent", 10) || !strncmp(buf, "Proxy-Connection", 16) || !strncmp(buf, "Connection", 10)) {
            // ignore those headers.
        } else {
            strcat(other_headers, buf);
        }
    }
    return 0;
}


int parse_response(int fd, char *code, char *status, char *content_len, char *content_type,
    char *headers, char **content) {
    char buf[MAXLINE];
    rio_t rio;
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
        return -1;
    }

    sscanf(buf, "HTTP/1.0 %s %[^\r]", code, status);
    
    while (strcmp(buf, "\r\n")) {
        if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
            return -1;
        }
        if (!strncmp(buf, "Content-length", 14)) {
            strcpy(content_len, buf + first_not_space(buf, 15));
        } else if (!strncmp(buf, "Content-type", 12)) {
            strcpy(content_type, buf + first_not_space(buf, 13));
        }
        strcat(headers, buf);
    }

    int content_size;
    if (!(content_size = atoi(content_len))) {
        return -1;
    }
    if (!strncmp(content_type, "text", 4)) {
        // text should inclde \0
        content_size++;
    }
    char *contentp = malloc(content_size);
    if (rio_readnb(&rio, contentp, content_size) < 0) {
        free(contentp);
        return -1;
    }
    if (!strncmp(content_type, "text", 4)) {
        // text should inclde \0
        contentp[content_size - 1] = '\0';
    }
    *content = contentp;
    return 0;
}

/*
parse the uri into host, port and path.
*/ 
void parse_uri(char* uri, char* hostname, char* port, char* path) {
    const char *pattern = "^https?://([a-zA-Z0-9\\.]+)(:[0-9]{1,5})?";

    regmatch_t pmatch[3];
    regex_t reg;
    regcomp(&reg, pattern, REG_EXTENDED);
    regexec(&reg, uri, 3, pmatch, 0);

    if (pmatch[1].rm_eo > pmatch[1].rm_so) {
        memcpy(hostname, uri + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
        hostname[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
    }

    if (pmatch[2].rm_eo > pmatch[2].rm_so) {
        memcpy(port, uri + pmatch[2].rm_so + 1, pmatch[2].rm_eo - pmatch[2].rm_so - 1);
        port[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
    }

    if (pmatch[1].rm_eo > pmatch[1].rm_so && pmatch[2].rm_eo < strlen(uri)) {
        strcpy(path, uri + pmatch[2].rm_eo);
    }

    regfree(&reg);
}


/*
build response according to request and host/port target
*/ 
int build_response(char *request, char *hostname, char *port, char **response, size_t *response_size) {
    // get response from origin website
    int clientfd;
    if ((clientfd = open_clientfd(hostname, port)) < 0) {
        perror("can\'t connect to target server");
        return -1;
    };

    rio_t rio;
    rio_readinitb(&rio, clientfd);
    if (rio_writen(clientfd, request, strlen(request)) < 0) {
        perror("request to target server error");
        return -1;
    };

    char content_length[32] = "", content_type[32] = "", code[32] = "", status[32] = "",
        headers[MAXLINE] = "";
    char *content;
    if (parse_response(clientfd, code, status, content_length, content_type, headers, &content) < 0) {
        perror("server response parse error");
        return -1;
    }

    char *buf;
    if ((buf = malloc(MAXLINE + (content ? atoi(content_length): 0))) < 0) {
        perror("not enought memory");
        return -1;
    }

    if (content) {
        sprintf(buf, "HTTP/1.0 %s %s\r\n%s", code, status, headers);
        if (!strcmp(content_type, "text")) {
            strcat(buf, content);
            *response_size = strlen(buf);
        } else {
            *response_size = strlen(buf) + atoi(content_length);
            memcpy(buf + strlen(buf), content, atoi(content_length));
        }
    } else {
        sprintf(buf, "HTTP/1.0 %s %s\r\n%s\r\n", code, status, headers);
        *response_size = strlen(buf);
    }
    *response = buf;
    free(content);
    if (close(clientfd) < 0) {
        perror("close target fd error");
    }
    return 0;
}


/*
finish the individual connection in one thread
*/ 
void *proxy_process(void *fd) {
    int connfd = *((int*)fd);
    Pthread_detach(pthread_self());
    free(fd);

    // parse headers
    char method[MAXLINE] = "", uri[MAXLINE] = "", version[MAXLINE] = "", 
        hostname[MAXLINE] = "", port[MAXLINE] = "", path[MAXLINE] = "",
        host[MAXLINE] = "", other_headers[MAXLINE] = "";

    if (parse_headers(connfd, method, uri, version, host, other_headers) < 0) {
        printf("request connection closed\n");
        close(connfd);
        return NULL;
    }
    parse_uri(uri, hostname, port, path);
    if (!strcmp(host, "")) {
        strcpy(host, hostname);
    }

    // establish connection with target
    char user_agent[strlen(user_agent_hdr) + 14];
    sprintf(user_agent, "User-Agent: %s", user_agent_hdr);
    char connection[] = "Connection: close\r\n";
    char proxy_connection[] = "Proxy-Connection: close\r\n";

    // generate request
    char request[4 * MAXLINE];
    sprintf(request, "%s %s HTTP/1.0\r\n%s%s%s%s%s\r\n", method, path, host, user_agent, connection, 
            proxy_connection, other_headers);
    // printf("request %s", request);

    size_t response_size;
    char *response;
    if (!(response = cache_get(request, &response_size))) {
        if (!build_response(request, hostname, port, &response, &response_size)) {
            cache_put(request, response, response_size);
        }
    }
    // printf("response %s %lu\n", response, response_size);

    // return origin response
    if (rio_writen(connfd, response, response_size) < 0) {
        perror("response to origin error");
    }
    if (close(connfd) < 0) {
        perror("close client fd error");
    }
    return NULL;
}


int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        if ((connfdp = malloc(sizeof(int))) < 0) {
            // wait for seconds
            perror("not enought memory");
            sleep(1);
            continue;
        }
        if ((*connfdp = accept(listenfd, (SA *) &clientaddr, &clientlen)) < 0) {
            perror("receive connection fail");
            sleep(1);
            continue;
        }
        Pthread_create(&tid, NULL, proxy_process, connfdp);
    }
}
