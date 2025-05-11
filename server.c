
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define READEND 0
#define WRITEEND 1


void checkError(int status, int line) {
  if (status < 0) {
    printf("socket error(%d)-%d: [%s]\n", getpid(), line, strerror(errno));
    exit(-1);
  }
}

/* creates a server-side socket that binds to the given port number and listens
 * for TCP connect requests */
int bindAndListen(int port) {
  int sid = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  int status = bind(sid, (struct sockaddr *)&addr, sizeof(addr));
  checkError(status, __LINE__);
  status = listen(sid, 10);
  checkError(status, __LINE__);
  return sid;
}

/* reaps dead children */
void cleanupDeadChildren() {
  int status = 0;
  pid_t deadChild; // reap the dead children
  do {
    deadChild = waitpid(0, &status, WNOHANG);
  } while (deadChild > 0);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("usage is: server <port1> <port2>\n");
    return 1;
  }

  /* Create two sockets: one to receive SQL commands and another for service */
  int sid = bindAndListen(atoi(argv[1]));
  int srv = bindAndListen(atoi(argv[2]));

  
  //selection
  fd_set asfd;
  while (1) {
    FD_ZERO(&asfd);
    FD_SET(sid, &asfd);
    FD_SET(srv, &asfd);
    int fd_len = (sid > srv ? sid : srv) + 1;

   
    checkError(select(fd_len, &asfd, NULL, NULL, NULL), __LINE__);

    if (FD_ISSET(sid, &asfd)) {
      int connfd = accept(sid, NULL, NULL);
      checkError(connfd, __LINE__);

      int sql_commfd[2], sql_resfd[2];
      checkError(pipe(sql_commfd), __LINE__);
      checkError(pipe(sql_resfd), __LINE__);

      pid_t child = fork();
      checkError(child, __LINE__);

      if (child == 0) {
        close(sql_commfd[WRITEEND]);
        close(sql_resfd[READEND]);
        dup2(sql_commfd[READEND], STDIN_FILENO);
        dup2(sql_resfd[WRITEEND], STDOUT_FILENO);
        close(connfd);
        close(sid);
        close(srv);
        execlp("sqlite3", "sqlite3", "foobar.db", NULL);
        perror("execlp");
        exit(1);
      } else {
        close(sql_commfd[READEND]);
        close(sql_resfd[WRITEEND]);

        while (1) {
          char cmd[1024] = {0};
          ssize_t r, total = 0;
          while ((r = read(connfd, cmd + total, sizeof(cmd) - total - 1)) > 0) {
            total += r;
            if (strstr(cmd, ".exit") || strchr(cmd, '\n'))
              break;
          }
          if (r <= 0)
            break;

          if (cmd[total - 1] != '\n')
            strcat(cmd, "\n");
          strcat(cmd, ".print __done__\n");
          total = strlen(cmd); // FIXED

          ssize_t sent = 0;
          while (sent < total) {
            ssize_t s = write(sql_commfd[WRITEEND], cmd + sent, total - sent);
            if (s < 0)
              break;
            sent += s;
          }

          char recvline[8192] = {0};
          ssize_t t_read = 0;
          while ((r = read(sql_resfd[READEND], recvline + t_read,
                           sizeof(recvline) - t_read - 1)) > 0) {
            t_read += r;
            recvline[t_read] = '\0';
            if (strstr(recvline, "__done__\n"))
              break;
          }

          char *marker = strstr(recvline, "__done__\n");
          if (marker) {
            *marker = '\0';
            t_read = marker - recvline;
          }

          if (t_read == 0) {
            strcpy(recvline, "NULL\n");
            t_read = strlen(recvline);
          }

          sent = 0;
          while (sent < t_read) {
            ssize_t s = write(connfd, recvline + sent, t_read - sent);
            if (s < 0)
              break;
            sent += s;
          }

          if (strstr(cmd, ".quit")) {
            close(sql_commfd[WRITEEND]);
            close(sql_resfd[READEND]);
            close(connfd);
            break;
          }
        }
      }
    }

    if (FD_ISSET(srv, &asfd)) {
      int shutdownfd = accept(srv, NULL, NULL);
      checkError(shutdownfd, __LINE__);
      char buffer[128] = {0};
      ssize_t n = read(shutdownfd, buffer, sizeof(buffer) - 1);
      buffer[n] = '\0';
      if (strcmp(buffer, "$die!\n") == 0 || strcmp(buffer, "$die!") == 0) {
        close(shutdownfd);
        close(srv);
        close(sid);
        cleanupDeadChildren();
        exit(0);
      }
      close(shutdownfd);
    }

    cleanupDeadChildren();
  }

  close(srv);
  close(sid);
  return 0;
}