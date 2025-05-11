#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* readString reads input from the stdin and returns the \n\0 terminated string
 * If EOF is read, it returns NULL */
char *readString() {
  int l = 2; // start with enough space to hold the emtpy string (with \n).
  char *s = malloc(l * sizeof(char));
  int i = 0;
  char ch;
  while ((ch = getchar()) != '\n' && ch != EOF) {
    if (i == l - 2) {
      s = realloc(s, l * 2 * sizeof(char));
      l *= 2;
    }
    s[i++] = ch;
  }
  if (ch == EOF) {
    free(s);
    return NULL;
  } else {
    s[i] = ch;
    s[i + 1] = 0;
    return s;
  }
}

/* prints the error that is encountered and terminate the program */
void checkError(int status, int line) {
  if (status < 0) {
    printf("socket error(%d)-%d: [%s]\n", getpid(), line, strerror(errno));
    exit(-1);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("usage is: client <hostname> <port>\n");
    return 1;
  }

  /* Connect to the server hosted on <hostname> <port>. This should be the same
   * as port1 of server */
  char *hostname = argv[1];
  int sid = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in srv;
  struct hostent *server = gethostbyname(hostname);
  srv.sin_family = AF_INET;
  srv.sin_port = htons(atoi(argv[2])); // same as sid in server
  memcpy(&srv.sin_addr.s_addr, server->h_addr, server->h_length);
  int status = connect(sid, (struct sockaddr *)&srv, sizeof(srv));
  checkError(status, __LINE__);

  fd_set read_fds;
  char buffer[1024];

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    FD_SET(sid, &read_fds); // socket

    int max_fd = (STDIN_FILENO > sid ? STDIN_FILENO : sid) + 1;
    int ready = select(max_fd, &read_fds, NULL, NULL, NULL);
    checkError(ready, __LINE__);
    

    // From stdin → send to server
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {

      char *command = readString();
      if (command == NULL) {
        
        shutdown(sid, SHUT_WR);
        continue;
      }
      if (strcmp(command, "\n") == 0) {
        free(command);
        continue;
      }

      size_t len = strlen(command);
      ssize_t total_sent = 0;
      while (total_sent < len) {
        ssize_t sent = write(sid, command + total_sent, len - total_sent);
        if (sent < 0) {
          perror("write - client");
          free(command);
          close(sid);
          exit(1);
        }
        total_sent += sent;
      }
      
      free(command);
    }

    // From server → print to stdout
    if (FD_ISSET(sid, &read_fds)) {
      
      
      memset(buffer, 0, sizeof(buffer));
      ssize_t n = read(sid, buffer, sizeof(buffer) - 1);
      
      if (n < 0) {
          perror("read - client");
          close(sid);
          exit(1);
        } else if (n == 0) {
          break;
        } else {
          buffer[n] = '\0';
          if (strcmp(buffer, "NULL\n") == 0) {
            
          }else{
          
          printf("%s", buffer);
          }
          
        }
      
    }
  }
  close(sid);
  return 0;
}