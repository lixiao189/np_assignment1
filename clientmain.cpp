#include <arpa/inet.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Included to get the support library
#include "calcLib.h"

const int gVersionCount = 3;
const char gVersions[3][10] = {"1.0", "1.1", "1.2"};
const size_t gCalcResultLen = 100;

bool is_in_arith(char *target) {
  char arith[][5] = {"add", "div", "mul", "sub", "fadd", "fdiv", "fmul", "fsub"};

  const int arith_len = 8;
  for (int i = 0; i < arith_len; i++) {
    if (strncmp(target, arith[i], strlen(arith[i])) == 0) {
      return true;
    }
  }
  return false;
}

char *calculate(char *op, char *arg1, char *arg2) {
  char *result = (char *)malloc(gCalcResultLen);
  memset(result, 0, gCalcResultLen);
  if (strncmp(op, "add", 3) == 0) {
    snprintf(result, sizeof(result), "%d", atoi(arg1) + atoi(arg2));
  } else if (strncmp(op, "div", 3) == 0) {
    snprintf(result, sizeof(result), "%d", atoi(arg1) / atoi(arg2));
  } else if (strncmp(op, "mul", 3) == 0) {
    snprintf(result, sizeof(result), "%d", atoi(arg1) * atoi(arg2));
  } else if (strncmp(op, "sub", 3) == 0) {
    snprintf(result, sizeof(result), "%d", atoi(arg1) - atoi(arg2));
  } else if (strncmp(op, "fadd", 4) == 0) {
    snprintf(result, sizeof(result), "%8.8g", atof(arg1) + atof(arg2));
  } else if (strncmp(op, "fdiv", 4) == 0) {
    snprintf(result, sizeof(result), "%8.8g", atof(arg1) / atof(arg2));
  } else if (strncmp(op, "fmul", 4) == 0) {
    snprintf(result, sizeof(result), "%8.8g", atof(arg1) * atof(arg2));
  } else if (strncmp(op, "fsub", 4) == 0) {
    snprintf(result, sizeof(result), "%8.8g", atof(arg1) - atof(arg2));
  }
  return result;
}

/**
 * @brief Translate host to IP
 *
 */
sockaddr_in *host2addr(const char *host, const int port) {
  hostent *hostinfo;
  in_addr **addr_list;
  sockaddr_in *serv_addr;

  serv_addr = (sockaddr_in *)malloc(sizeof(sockaddr_in));
  memset(serv_addr, 0, sizeof(sockaddr_in));

  hostinfo = gethostbyname(host);
  if (!hostinfo) {
    printf("Invalid address/ Address not supported\n");
    return NULL;
  }

  addr_list = (struct in_addr **)hostinfo->h_addr_list;

  // Get IP address
  char *ip = inet_ntoa(*addr_list[0]);

  if (inet_pton(AF_INET, ip, &(serv_addr->sin_addr)) <= 0) {
    printf("Invalid address/ Address not supported\n");
    return NULL;
  }
  serv_addr->sin_family = AF_INET;
  serv_addr->sin_port = htons(port);

  return serv_addr;
}

int main(int argc, char *argv[]) {

  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string
    (Desthost) and one integer (port). Atm, works only on dotted notation, i.e.
    IPv4 and DNS. IPv6 does not work if its using ':'.
  */
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);
  // *Desthost now points to a sting holding whatever came before the delimiter,
  // ':'. *Dstport points to whatever string came after the delimiter.

  /* Do magic */
  int port = atoi(Destport);
  printf("Host %s, and port %d.\n", Desthost, port);

  // Translate hostname to IP address
  sockaddr_in *serv_addr = host2addr(Desthost, port);
  int sock = 0;

  // 创建套接字
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Socket creation error\n");
    return -1;
  }

  // Connenct to the server
  if (connect(sock, (struct sockaddr *)serv_addr, sizeof(sockaddr_in)) < 0) {
    printf("Connection Failed\n");
    return -1;
  } else {
#ifdef DEBUG
    sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    if (getsockname(sock, (struct sockaddr *)&cli_addr, &len) < 0) {
      printf("getsockname failed");
      return -1;
    }

    printf("Connected to %s:%s local %s:%d\n", Desthost, Destport, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
#endif
  }

  const int N = 1024;
  char buffer[N] = {0};
  int len = 0;
  bool is_header = true;

  // a stack to store package
  char package[N];
  int tail = 0;

  // The message which needed to be sent when the version is legal
  char version_ok_message[] = "OK\n";
  int version_ok_message_len = strlen(version_ok_message);

  char cur_version[10];
  memset(cur_version, 0, sizeof(cur_version));
  char last_result[gCalcResultLen];
  do {
    memset(buffer, 0, N); // Clear buffer
    len = read(sock, buffer, N);

    // Handle server message package
    for (int i = 0; i < len; i++) {
      if (buffer[i] != '\n') {
        package[tail++] = buffer[i];
      } else {
        if (tail == 0) {
          // package header finished
          is_header = false;

          // check version
          bool flag = false;
          int cur_version_len = strlen(cur_version);
          int target_version_len;
          for (int i = 0; i < gVersionCount; i++) {
            target_version_len = strlen(gVersions[i]);

            if (target_version_len != cur_version_len) {
              continue;
            } else if (!strncmp(gVersions[i], cur_version, cur_version_len)) {
              flag = true;
            }
          }

          if (flag) {
            write(sock, version_ok_message, version_ok_message_len);
          } else {
            puts("Protocol version error");
            return -1;
          }
        } else if (is_header) {
          // handle package header
          char *cur_package = package;
          char sep[] = " ";

          strtok(cur_package, sep);
          strtok(NULL, sep);
          char *version = strtok(NULL, sep);
          strncpy(cur_version, version, strlen(version) - 1);
        } else {
          // handle package body
          char *cur_package = package;
          char sep[] = " ";

          char *op = strtok(cur_package, sep);
          if (is_in_arith(op)) {
            char *arg1, *arg2, *result;
            arg1 = strtok(NULL, sep);
            arg2 = strtok(NULL, sep);
            printf("ASSIGNMENT: %s %s %s\n", op, arg1, arg2);

            result = calculate(op, arg1, arg2);
            memset(last_result, 0, sizeof(last_result));
            strncpy(last_result, result, sizeof(last_result));
#ifdef DEBUG
            printf("Calculated the result to %s\n", result);
#endif
            write(sock, result, strlen(result));
            write(sock, "\n", 1);
            free(result);
          } else {
            if (strncmp(op, "OK", 2) == 0) {
              printf("OK (myresult=%s)", last_result);
            } else {
              printf("ERROR");
            }
            return 0;
          }
        }

        tail = 0;
        memset(package, 0x0, sizeof package);
      }
    }
  } while (len >= 0);

  free(serv_addr);
}
