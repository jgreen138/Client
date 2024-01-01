#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define FILE_BUFLEN 1024
#define DEFAULT_PORT "27015"

// Function to receive a file from the server
int receiveFile(SOCKET serverSocket, const char* fileName) {
    // Construct the full path for the received file based on the client executable location
    char fullPath[MAX_PATH];
    if (GetModuleFileNameA(NULL, fullPath, MAX_PATH) == 0) {
        printf("Error getting module filename\n");
        return -1;
    }

    // Remove the filename from the full path
    char* lastBackslash = strrchr(fullPath, '\\');
    if (lastBackslash != NULL) {
        *(lastBackslash + 1) = '\0';  // Null-terminate after the last backslash
    }

    // Append the received filename to the path
    strcat_s(fullPath, MAX_PATH, fileName);

    FILE* file;
    if (fopen_s(&file, fullPath, "wb") != 0) {
        printf("Error creating file: %s\n", fullPath);
        return -1;
    }

    char fileBuf[FILE_BUFLEN];
    int bytesReceived;

    do {
        bytesReceived = recv(serverSocket, fileBuf, sizeof(fileBuf), 0);
        if (bytesReceived > 0) {
            fwrite(fileBuf, 1, bytesReceived, file);
        }
        else if (bytesReceived == 0) {
            printf("Connection closed by the server\n");
        }
        else {
            printf("Error receiving file: %d\n", WSAGetLastError());
            fclose(file);
            return -1;
        }
    } while (bytesReceived > 0);

    fclose(file);
    return 0;
}

int main(void) {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    int iResult;
    char serverName[256];  // Assuming a reasonable maximum length for the server name
    char receivedFilePath[256];  // Assuming a reasonable maximum length for the file path
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Get the server name from the user
    printf("Enter the server name: ");
    if (fgets(serverName, sizeof(serverName), stdin) == NULL) {
        printf("Error reading user input\n");
        WSACleanup();
        return 1;
    }

    // Remove the newline character from the server name
    size_t len = strlen(serverName);
    if (len > 0 && serverName[len - 1] == '\n') {
        serverName[len - 1] = '\0';
    }

    // Resolve the server address and port
    iResult = getaddrinfo(serverName, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to the server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Get the file name from the user
    printf("Enter the name of the file to request: ");
    if (fgets(receivedFilePath, sizeof(receivedFilePath), stdin) == NULL) {
        printf("Error reading user input\n");
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Remove the newline character from the file name
    len = strlen(receivedFilePath);
    if (len > 0 && receivedFilePath[len - 1] == '\n') {
        receivedFilePath[len - 1] = '\0';
    }

    // Send a request to the server
    iResult = send(ConnectSocket, receivedFilePath, static_cast<int>(strlen(receivedFilePath)), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Receive the file from the server
    if (receiveFile(ConnectSocket, receivedFilePath) == -1) {
        printf("Error receiving file from the server\n");
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

