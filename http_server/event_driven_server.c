#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <netinet/in.h>

#define MAX_CONNECTIONS 11
#define POOL_SIZE 10
#define BUFF_SIZE 8192
#define HEADER_SIZE 500
#define TIMEOUT 1
#define CONNECTION_CLOSED 32

int sock;
pthread_t thread_pool[POOL_SIZE];
time_t close_times[2 * MAX_CONNECTIONS];
pthread_mutex_t head_lock;
pthread_mutex_t closes_lock;

/*
* This code sets up everything necessary for a global linked list
*/
struct node {
    short fd;
    short http;
    long sent_bytes;
    long total_bytes;
    char *file_path;

    struct node *next;
};

struct node *head = NULL;
struct node *tail = NULL;

int enqueue(struct node *new_node) {
    if(head && tail) {
        tail->next = new_node;
        tail = new_node;
    } else if (!head && !tail) {
        head = new_node;
        tail = new_node;
    } else {
        return 0;
    }
    return 1;
}

int dequeue(struct node *new_node) {
    if(head == tail) {
        *new_node = *head;
        head = NULL;
        tail = NULL;
    } else if (head && tail) {
        *new_node = *head;
        head = head->next;
    } else {
        return 0;
    }
    return 1;
}

/*
* Signal Handler, closes the socket before exiting
*/
void handler(int sig) {
    close(sock);
    printf("\nSocket %i closed successfully\n", sock);
    for(int i = 0; i < POOL_SIZE; i++) {
        pthread_cancel(thread_pool[i]);
    }
    exit(1);
}

/*
* Parses command line arguments and writes port number and doc root based on the flags
*/
int parse_argument(int argc, char **argv, int *port_number, char **document_root) {
    // Start from 1 because first arg is always the program name
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-port") == 0) {
	        *port_number = atoi(argv[++i]);

            if(*port_number > 9999 || *port_number < 8000) {
                printf("Invalid port number, please use between 8000 and 9999 exclusive\n");
                return 0;
            }
        } else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-document_root") == 0) {
            *document_root = argv[++i];
        } else {
            printf("A flag could not be interpreted\n");
            return 0;
        }
    }

    if(!*port_number || !*document_root) {
        printf("Error in arg parsing, please use '-d _ -p _' or '-document_root _ -port' format\n");
        return 0;
    }

    return 1;
}

/*
* Sets up a socket for the given socket number and port number
*/
int socket_setup(int port_number, struct sockaddr_in *myaddr) {
    myaddr->sin_port= htons(port_number);
    myaddr->sin_family = AF_INET;
    myaddr->sin_addr.s_addr = htonl(INADDR_ANY);

    return bind(sock, (struct sockaddr*)myaddr, sizeof(*myaddr));
}

/*
* Checks the file path and counts the depth from the root directory
*/
int file_path_depth(char* file_path) {
    int depth = 0, index = 0, follows_slash = 0;

    while(file_path[index]) {
        if(file_path[index] == '/') {
            depth++;
            follows_slash = 1;
        } else if(file_path[index] == '.' && follows_slash) {
            depth--;
        } else {
            follows_slash = 0;
        }
        index++;

        // Return if the depth is ever less than zero because we don't want anyone accessing below the root. Allow 0 
        if(depth < 0) {
            return depth;
        }
    }

    return depth;
}

/*
* Creates a file path using concatenation. Also takes care of adding a / if forgotten in front of the file path and replacing / with the default /index.html
*/
int create_file_path(char *file, char *root, char **file_path) {
    if(file_path_depth(file) < 0) {
        return 0;
    }

    if(strcmp(file, "/") == 0) {
        file = "/index.html";
    }

    char *extra = (file[0] != '/') ? "/" : "";
    int file_path_len =  strlen(root) + strlen(extra) + strlen(file) + 1;
    *file_path = (char *) malloc(file_path_len);
    snprintf(*file_path, file_path_len, "%s%s%s", root, extra, file);
    return 1;
}

/*
* Passes the header, example below
* HTTP/1.0 200 OK
* Content-Type: text/html; charset=utf-8
* Content-Length: 500
* Date: Mon, 18 Jul 2016 16:06:00 GMT
* Last-Modified: Mon, 18 Jul 2016 02:36:04
*/
int send_header(int socket_number, char *http_type, int status_code, char *file_type, long file_size, time_t last_modified, int curr_connections) {
    int keep_alive;
    char *status_message;
    char *content_type;
    struct tm *tm;
    char last_modified_str[64];
    char header[HEADER_SIZE];

    if(strcmp(file_type, ".html") == 0) {
        content_type = "text/html";
    } else if(strcmp(file_type, ".jpg") == 0) {
        content_type = "image/jpeg";
    } else if(strcmp(file_type, ".png") == 0) {
        content_type = "image/png";
    } else if(strcmp(file_type, ".gif") == 0) {
        content_type = "image/gif";
    } else if(strcmp(file_type, ".mp4") == 0) {
        content_type = "video/mp4";
    } else {
        content_type = "text/plain";
    }

    switch(status_code) {
        case 200:
            status_message = "200 OK";
            break;
        case 404:
            status_message = "404 NOT FOUND";
            break;
        case 403:
            status_message = "403 FORBIDDEN";
            break;
        case 400:
            status_message = "400 BAD REQUEST";
            break;
        case 399:
            status_message = "399 USE HTTP/1.0 or HTTP/1.1";
            break;
        case 398:
            status_message = "398 NO HOST";
            break;
        case 397:
            status_message = "397 NO FILE";
            break;
        case 380:
            status_message = "380 ERROR READING FILE";
            break;
        case 304:
            status_message = "304 Not Modified";
            break;
        default:
            status_message = "SERVER ERROR";
            break;
    }

    if(last_modified) {
        tm = localtime(&last_modified);
        strftime(last_modified_str, sizeof(last_modified_str), "%c", tm);
    } else {
        strcat(last_modified_str, "N/A");
    }

    time_t t = time(NULL);
    localtime(&t);

    keep_alive =  30 / curr_connections;

    if(strstr(http_type, "1.1")) {
        snprintf(header, HEADER_SIZE, 
	    "%s %s\nDate: %sServer: Potato\nLast-Modified: %s\nAccept-Ranges: bytes\nContent-Length: %lu\nKeep-Alive: timeout=%i, max=100\nConnection: Keep-Alive\nContent-Type: %s\r\n\r\n", 
        http_type, status_message, ctime(&t), last_modified_str, file_size, keep_alive, content_type);
    } else {
        snprintf(header, HEADER_SIZE, 
	    "%s %s\nDate: %sServer: Potato\nLast-Modified: %s\nAccept-Ranges: bytes\nContent-Length: %lu\nContent-Type: %s\r\n\r\n", 
        http_type, status_message, ctime(&t), last_modified_str, file_size, content_type);
    }

    send(socket_number, header, strnlen(header, HEADER_SIZE), 0);

    return 1;
}

/*
* Method to ensure all data is received from recv
*/
char *read_all(int socket_number, char *rec_str, int *total_bytes) {
    int bytes_received = 1;
    char temp[BUFF_SIZE];
    char *carry_over;

    *total_bytes = 0;

    // To ensure appropraite behavior, we reset these values each time
    memset(rec_str, 0, 2);
    memset(temp, 0, BUFF_SIZE);

    while(bytes_received > 0 && !strstr(temp, "\r\n\r\n") && strncmp(temp, "\r\n", 2)) {
        memset(temp, 0, BUFF_SIZE);
        bytes_received = recv(socket_number, temp, BUFF_SIZE, 0);
        *total_bytes += bytes_received;

        int new_rec_len = strlen(rec_str) + bytes_received + 1;
        rec_str = (char *) realloc(rec_str, new_rec_len);
        strcat(rec_str, temp);
    }

    return rec_str;
}

/*
* Parses the request and creates a new work node if applicable, otherwise sends the appropriate error message and returns 0 (false)
*/
int create_request(struct node *new_node, char *rec_str, char* root, int curr_connections) {
    struct stat stat_buffer;
    char permissions[6];
    char *file_path;
    char *token;

    // Check to see if the request has valid elements
    char *checker = rec_str;

    for(int i = 0; i < 3; i++) {
        checker = strstr(checker, (i == 2) ? "\r" : " ");
        if(!checker) {
            send_header(new_node->fd, "N/A", 400, "N/A", 0, time(NULL), curr_connections);
            return 0;
        }
    }

    checker = strstr(checker, "Host:");

    // This is a hacky solution that only responds to get, but thats all we need for now
    token = strtok(rec_str, " ");

    if(strcmp(token, "GET") != 0) {
        send_header(new_node->fd, "N/A", 400, "N/A", 0, time(NULL), curr_connections);
        return 0;
    }
    
    // Creating file path
    token = strtok(NULL, " ");

    if(!create_file_path(token, root, &file_path)) {
        send_header(new_node->fd, "N/A", 403, "N/A", 0, time(NULL), curr_connections);
        return 0;
    }
    
    // Setting timeout if 1.1 and allowing another request, validating if 1.0, otherwise sending an error
    token = strtok(NULL, "\r");
    char http_type[strlen(token) + 1];
    strcpy(http_type, token);

    if(strcmp(http_type, "HTTP/1.1") == 0) {
        new_node->http = 11;
    } else if(strcmp(http_type, "HTTP/1.0") == 0) {
        checker = "";
        new_node->http = 10;
    } else {
        send_header(new_node->fd, "N/A", 399, "N/A", 0, time(NULL), curr_connections);
        return 0;
    }
    
    // Use variable from before to see if host was present
    if(!checker) {
        send_header(new_node->fd, http_type, 398, "N/A", 0, time(NULL), curr_connections);
        return 0;
    }

    // Ensuring file exists and has stats
    if(stat(file_path, &stat_buffer) < 0) {
        send_header(new_node->fd, http_type, 404, "N/A", 0, time(NULL), curr_connections);
        return 0;
    }

    // Ensuring o-read is set
    sprintf(permissions, "%o", stat_buffer.st_mode);
    if(atoi(&permissions[5]) < 4) {
        send_header(new_node->fd, http_type, 403, "N/A", stat_buffer.st_size, time(NULL), curr_connections);
        return 0;
    }

    new_node->file_path = file_path;
    new_node->total_bytes = stat_buffer.st_size;
    new_node->sent_bytes = 0;

    int fb = open(file_path, O_RDONLY);
    if(fb > 0) {
        send_header(new_node->fd, http_type, 200, strrchr(file_path, '.'), stat_buffer.st_size, stat_buffer.st_atime, curr_connections);
        close(fb);
    } else {
        send_header(new_node->fd, http_type, 380, "N/A", 0, stat_buffer.st_atime, curr_connections);
        return 0;
    }

    return 1;
}

void* pool_worker(void* arguments) {
    while(1) {
        struct node *curr_node = NULL;

        pthread_mutex_lock(&head_lock);
        if(head) {
            curr_node = (struct node*) malloc(sizeof(struct node));
            dequeue(curr_node);
        }
        pthread_mutex_unlock(&head_lock);

        if(curr_node) {
            int fb = open(curr_node->file_path, O_RDONLY);

            if(fb > 0) {
                char to_send[BUFF_SIZE];
                int bytes_read = read(fb, to_send, BUFF_SIZE);
                int total_bytes = bytes_read;

                // Finds the place in the file where we are
                while(total_bytes <= curr_node->sent_bytes) {
                    bytes_read = read(fb, to_send, BUFF_SIZE);
                    total_bytes += bytes_read;
                }

                close(fb);
                
                send(curr_node->fd, to_send, bytes_read, 0);

                curr_node->sent_bytes += bytes_read;

                if(curr_node->sent_bytes < curr_node->total_bytes) {
                    pthread_mutex_lock(&head_lock);
                    enqueue(curr_node);
                    pthread_mutex_unlock(&head_lock);
                } else {
                    free(curr_node->file_path);
                    if(curr_node->http == 10) {
                        shutdown(curr_node->fd, 0);
                        close(curr_node->fd);
                    } else if (curr_node->http == 11) {
                        pthread_mutex_lock(&closes_lock);
                        close_times[curr_node->fd - 4] = time(NULL);
                        pthread_mutex_unlock(&closes_lock);
                    }
                    free(curr_node);
                }
            } else {
                perror("Error reading file");
            }
        }
    }       
}

int run_connection(int port_number, char* document_root) {   
    struct sockaddr_in myaddr;
    int optval;
    
    // Configure main socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    if(socket_setup(port_number, &myaddr) < 0) {
	    perror("Binding Error: ");
        return -1;
    }
    
    if(listen(sock, MAX_CONNECTIONS) < 0) {
        perror("Listening Error");
        return -1;
    }

    // Make main socket the first connection
    int curr_connections = 1;
    struct pollfd fds[MAX_CONNECTIONS];
    fds[0].fd = sock;
    fds[0].events = POLLIN;

    // Create the thread pool
    for(int i = 0; i < POOL_SIZE; i++) {
        pthread_create(&thread_pool[i], NULL, pool_worker, NULL);
    }

    while(1) {
        poll((struct pollfd *)&fds, curr_connections, TIMEOUT * 1000);

        if(fds[0].revents & POLLIN && curr_connections < MAX_CONNECTIONS) {
            struct sockaddr clientaddr;
	        int addrlen = sizeof(clientaddr);

            fds[curr_connections].fd = accept(fds[0].fd, (struct sockaddr*)&clientaddr, &addrlen);
            fds[curr_connections].events = POLLIN;

            pthread_mutex_lock(&closes_lock);
            close_times[fds[curr_connections].fd - 4] = (time_t) NULL;
            pthread_mutex_unlock(&closes_lock);

            curr_connections++;
        }

        for(int i = 1; i < curr_connections; i++) {
            int bytes_received = 1;

            if(fds[i].revents & POLLIN) {
                char *rec_str = (char *) malloc(1);
                rec_str = read_all(fds[i].fd, rec_str, &bytes_received);

                if(bytes_received > 0) {
                    struct node *new_node = (struct node *) malloc(sizeof(struct node));
                    new_node->fd = fds[i].fd;

                    if(create_request(new_node, rec_str, document_root, curr_connections)) {
                        pthread_mutex_lock(&head_lock);
                        if(!enqueue(new_node)) {
                            printf("Error enqueuing\n");
                        }
                        pthread_mutex_unlock(&head_lock);
                    } else {
                        printf("Error creating the request\n");
                        bytes_received = 0;
                    }
                } else {
                    printf("Error reading the request %i\n", bytes_received);
                }
                free(rec_str);
            }

            if((close_times[fds[i].fd - 4] && difftime(time(NULL), close_times[fds[i].fd - 4]) > 30 / curr_connections) || bytes_received <= 0) {
                shutdown(fds[i].fd, 0);
                close(fds[i].fd);
                fds[i].revents = CONNECTION_CLOSED;
            }

            if(fds[i].revents & CONNECTION_CLOSED) {
                printf("Client closed connection on socket %i\n", fds[i].fd);

                for(int j = i; j < curr_connections; j++) {
                    if(j == curr_connections - 1) {
                        memset(&fds[j], 0, sizeof(fds[j]));
                    } else {
                        fds[j] = fds[j + 1];
                    }
                }

                curr_connections--;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    signal(SIGINT, handler);

    int port_number = 0;
    char *document_root = NULL;

    if(!parse_argument(argc, argv, &port_number, &document_root)) {
        printf("There was an error parsing the inputs. Please use -document_root and -port flag followed by the arguments.\n");
        return -1;
    }

    printf("Success, the port number is %i and the document root is %s\n", port_number, document_root);
    
    return run_connection(port_number, document_root);
}
