#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>

#define MAX_CONNECTIONS 10
#define BUFF_SIZE 8192
#define HEADER_SIZE 500

int sock;
int curr_connections;
pthread_mutex_t lock;

/*
* Signal Handler, closes the socket before exiting
*/
void handler(int sig) {
    close(sock);
    printf("\nSocket %i closed successfully\n", sock);
    exit(1);
}

/*
* Parses command line arguments and writes port number and doc root based on the flags
*/
int parse_argument(int argc, char **argv, int *port_number, char **document_root) {
    // Checks to see that the number of arguments is correct
    if(argc != 5) {
      printf("The number of arguments entered is incorrect.\n");
      return -1;
    }
  
    // This tells us what flag we are reading and how to interpret the next string
    int flag_type = -1;
    int parsed_args = 0;
    
    // Start from 1 because first arg is always the program name
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-port") == 0) {
	        flag_type = 1;
        } else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-document_root") == 0) {
            flag_type = 2;
        } else if(flag_type == 1) {
            *port_number = atoi(argv[i]);

            if(*port_number > 9999 || *port_number < 8000) {
                printf("Invalid port number, please use between 8000 and 9999 exclusive\n");
                return -1;
            }
            parsed_args++;
            flag_type = -1;
        } else if(flag_type == 2) {
            *document_root = argv[i];
            parsed_args++;
            flag_type = -1;
        } else {
            printf("A flag could not be interpreted\n");
            return -1;
        }
    }

    if(parsed_args != 2 || !*port_number || !*document_root) {
        printf("Error in arg parsing, please use '-d _ -p _' or '-document _ -port' format\n");
        return -1;
    }

    return 0;
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
    }

    return depth;
}

/*
* Creates a file path using concatenation. Also takes care of adding a / if forgotten in front of the file path and replacing / with the default /index.html
*/
int create_file_path(char *file, char *root, char **file_path) {
    if(file_path_depth(file) < 1) {
        return -1;
    }

    if(strcmp(file, "/") == 0) {
        file = "/index.html";
    }

    char *extra = (file[0] != '/') ? "/" : "";
    int file_path_len =  strlen(root) + strlen(extra) + strlen(file) + 1;
    *file_path = (char *) malloc(file_path_len);
    snprintf(*file_path, file_path_len, "%s%s%s", root, extra, file);
    return 0;
}

/*
* Passes the header, example below
* HTTP/1.0 200 OK
* Content-Type: text/html; charset=utf-8
* Content-Length: 500
* Date: Mon, 18 Jul 2016 16:06:00 GMT
* Last-Modified: Mon, 18 Jul 2016 02:36:04
*/
int send_header(int socket_number, char *http_type, int status_code, char *file_type, long file_size, time_t last_modified) {
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
        strcat(*last_modified_str, "N/A");
    }

    time_t t = time(NULL);
    localtime(&t);

    if(strstr(http_type, "1.1")) {
        snprintf(header, HEADER_SIZE, 
	    "%s %s\nDate: %sServer: Potato\nLast-Modified: %s\nAccept-Ranges: bytes\nContent-Length: %lu\nKeep-Alive: timeout=5, max=100\nConnection: Keep-Alive\nContent-Type: %s\r\n\r\n", 
        http_type, status_message, ctime(&t), last_modified_str, file_size, content_type);
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
int rec_all(int socket_number, char **rec_str) {
    int total_bytes = 0;
    int bytes_received = 1;
    char *temp[BUFF_SIZE];
    char *carry_over;

    // To ensure appropraite behavior, we reset these values each time
    *rec_str = "";
    *temp = "";

    while(bytes_received > 0 && !strstr(temp, "\r\n\r\n") && strncmp(temp, "\r\n", 2)) {
        bytes_received = recv(socket_number, temp, BUFF_SIZE, 0);
        total_bytes += bytes_received;

        int carry_over_len = strlen(*rec_str) + 1;
        int new_rec_len = carry_over_len + bytes_received + 1;

	    carry_over = (char *) malloc(carry_over_len);
        strcpy(carry_over, *rec_str);
        *rec_str = (char *) malloc(new_rec_len);
        snprintf(*rec_str, new_rec_len, "%s%s", carry_over, temp);

        free(carry_over);
    }

    return total_bytes;
}

/*
* Given a socket, receive the information and run the necessary processes
*/
int recieve_and_parse(int socket_number, char* root) {
    for(int i = 0; i < 1; i++) {
        char *rec_str;
        int bytes_received = rec_all(socket_number, &rec_str);

        if(bytes_received < 0) {
            perror("Error receiving information");
        } else if(bytes_received == 0) {
            printf("Client closed connection on socket %i\n", socket_number);
        } else {
            struct stat stat_buffer;
            char permissions[6];
            char *file_path;
            char *token;

            // Check to see if the request has valid elements
            char *checker;
            checker = rec_str;

            for(int i = 0; i < 4; i++) {
                checker = strstr(checker, (i == 2) ? "\r" : " ");
                if(!checker) {
                    send_header(socket_number, "N/A", 400, "N/A", 0, time(NULL));
                    return -1;
                }
            }

            // This is a hacky solution that only responds to get, but thats all we need for now
            token = strtok(rec_str, " ");

            if(strcmp(token, "GET") != 0) {
                send_header(socket_number, "N/A", 400, "N/A", 0, time(NULL));
                return -1;
            }
            
            // Creating file path
            token = strtok(NULL, " ");

            if(create_file_path(token, root, &file_path) < 0) {
                send_header(socket_number, "N/A", 403, "N/A", 0, time(NULL));
                return -1;
            }
            
            // Setting timeout if 1.1 and allowing another request, validating if 1.0, otherwise sending an error
            token = strtok(NULL, "\r");
            char http_type[strlen(token) + 1];
            strcpy(http_type, token);

            if(strcmp(http_type, "HTTP/1.1") == 0) {
                pthread_mutex_lock(&lock);
                struct timeval tv = {
                    .tv_sec = 30 / curr_connections
                };
                pthread_mutex_unlock(&lock);
                setsockopt(socket_number, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                i--;
            } else if(strcmp(http_type, "HTTP/1.0") == 0) {
                ;
            } else {
                send_header(socket_number, "N/A", 399, "N/A", 0, time(NULL));
                return -1;
            }
            
            // Checking the host
            token = strtok(NULL, " ");

            if(strcmp(strchr(token, 'H'), "Host:") != 0) {
                send_header(socket_number, http_type, 398, "N/A", 0, time(NULL));
                return -1;
            }

            // Ensuring file exists and has stats
            if(stat(file_path, &stat_buffer) < 0) {
                send_header(socket_number, http_type, 404, "N/A", 0, time(NULL));
                return -1;
            }

            // Ensuring o-read is set
            sprintf(permissions, "%o", stat_buffer.st_mode);
            if(atoi(&permissions[5]) < 4) {
                send_header(socket_number, http_type, 403, "N/A", stat_buffer.st_size, time(NULL));
                return -1;
            }
            
            // Opening file
            int fb = open(file_path, O_RDONLY);
            if(fb > 0) {
                char to_send[BUFF_SIZE];

                send_header(socket_number, http_type, 200, strrchr(file_path, '.'), stat_buffer.st_size, stat_buffer.st_atime);

                // Send until fgets reads and end of file
                int bytes_read = 1;
                while((bytes_read = read(fb, to_send, BUFF_SIZE)) > 0) {
                    send(socket_number, to_send, bytes_read, 0);
                }
            } else {
                send_header(socket_number, http_type, 380, "N/A", 0, stat_buffer.st_atime);
                return -1;
            }
            close(fb);
        }
    }
    
    return 0;
}

int run_connection(int port_number, char* document_root) {   
    struct sockaddr_in myaddr;
    int optval;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socket_setup(port_number, &myaddr) < 0) {
        printf("The socket setup failed\n");
	    perror("Binding Error: ");
        return -1;
    }

    optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    
    if(listen(sock, MAX_CONNECTIONS) < 0) {
        perror("Listening Error");
        return -1;
    }

    curr_connections = 0;

    while(1) {
        struct sockaddr clientaddr;
	    int addrlen = sizeof(clientaddr);
        int* new_socket;

        new_socket = malloc(sizeof(int));
        *new_socket = accept(sock, (struct sockaddr*)&clientaddr, &addrlen);

	    if(*new_socket < 1) {
	      perror("Accept error");
	    }

	    struct arg_struct {
	        int* socket_number;
	        char* document_root;
	    };

        // The thread routine to be used to grab files
        void* connection_routine(void* arguments) {
	        struct arg_struct *args = arguments;
            int socket_number = *(int *) args->socket_number;

            free(args->socket_number);

            if (pthread_detach(pthread_self()) == 0){
                recieve_and_parse(socket_number, args->document_root);
            } else {
                printf("Error detatching\n");
            }

            shutdown(socket_number, 0);
		    close(socket_number);

            pthread_mutex_lock(&lock);
            curr_connections--;
            pthread_mutex_unlock(&lock);

            pthread_exit(NULL);
        }

        // // If the connections are maxed out, we wait till a thread is free
        pthread_mutex_lock(&lock);
        int connections = curr_connections;
        pthread_mutex_unlock(&lock);

        while(connections >= MAX_CONNECTIONS) {
            sleep(2);
            pthread_mutex_lock(&lock);
            connections = curr_connections;
            pthread_mutex_unlock(&lock);
        }

        // Creation of thread and incrementation of global variable
        struct arg_struct args;
        pthread_t indiv_id;

        args.socket_number = new_socket;
        args.document_root = document_root;

        pthread_mutex_lock(&lock);
        curr_connections++;
        pthread_create(&indiv_id, NULL, connection_routine, (void *)&args);
        pthread_mutex_unlock(&lock);
    }
    return 0;
}

int main(int argc, char **argv) {
    signal(SIGINT, handler);

    int port_number;
    char *document_root;

    if(parse_argument(argc, argv, &port_number, &document_root) < 0) {
        printf("There was an error parsing the inputs. Please use -document_root and -port flag followed by the arguments.\n");
        return -1;
    }

    printf("Success, the port number is %i and the document root is %s\n", port_number, document_root);
    
    run_connection(port_number, document_root);

    return 0;
}
