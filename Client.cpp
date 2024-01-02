// Define WIN32_LEAN_AND_MEAN to exclude unnecessary headers
#define WIN32_LEAN_AND_MEAN

// Include necessary headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>

// Link with the Winsock library
#pragma comment(lib, "Ws2_32.lib")

// Define constants
#define DEFAULT_BUFLEN 512
#define FILE_BUFLEN 1024
#define DEFAULT_PORT "27015"

// Function to print errors
void printError(const char* action) {
    // Print an error message with the given action and the corresponding error code
    printf("%s failed with error: %d\n", action, WSAGetLastError());
}

// Function to get user input
bool getUserInput(char* buffer, size_t bufferSize) {
    // Read a line of input from the user into the provided buffer
    if (fgets(buffer, static_cast<int>(bufferSize), stdin) == NULL) {
        // Print an error message if reading input fails
        printf("Error reading user input\n");
        return false;
    }

    // Remove the newline character if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return true;
}

// Function to receive a file from the server or an error message
int receiveFileOrErrorMessage(SOCKET serverSocket, const char* fileName) {
    // Buffer to store received data
    char receivedBuf[DEFAULT_BUFLEN];

    // Receive data from the server
    int bytesReceived = recv(serverSocket, receivedBuf, sizeof(receivedBuf), 0);

    if (bytesReceived > 0) {
        // Check if the received data indicates an error from the server
        if (bytesReceived >= 5 && strncmp(receivedBuf, "Error", 5) == 0) {
            // Print the server error message
            printf("Server Error: %s\n", receivedBuf + 6);
            return -1;
        }

        // Get the full path to save the file
        char fullPath[MAX_PATH];
        if (GetModuleFileNameA(NULL, fullPath, MAX_PATH) == 0) {
            printf("Error getting module filename\n");
            return -1;
        }

        char* lastBackslash = strrchr(fullPath, '\\');
        if (lastBackslash != NULL) {
            *(lastBackslash + 1) = '\0';
        }

        strcat_s(fullPath, MAX_PATH, fileName);

        // Open the file for writing
        FILE* file;
        if (fopen_s(&file, fullPath, "wb") != 0) {
            // Print an error message if opening the file fails
            printf("Error creating file: %s\n", fullPath);
            return -1;
        }

        // Write the received data to the file
        fwrite(receivedBuf, 1, bytesReceived, file);

        // Close the file
        fclose(file);

        // Print a success message
        printf("File received successfully from the server.\n");
        return 0;
    }
    else if (bytesReceived == 0) {
        // Print a message if the server closes the connection
        printf("Connection closed by the server\n");
    }
    else {
        // Handle errors during the file receiving process
        int error = WSAGetLastError();
        printf("Error receiving file: %d\n", error);

        // Get the error message details
        LPVOID errorMsg;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            (LPWSTR)&errorMsg,
            0,
            NULL
        );

        // Print the error details
        wprintf(L"Error details: %s\n", errorMsg);

        // Free the allocated error message buffer
        LocalFree(errorMsg);
    }

    return -1;
}

// Main function
int main(void) {
    // Initialize Winsock
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    int iResult;
    char serverName[256];
    char receivedFilePath[256];

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        // Print an error message if Winsock initialization fails
        printError("WSAStartup");
        return 1;
    }

    // Configure address info hints for connecting to the server
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Get the server name from the user
    printf("Enter the server name: ");
    if (!getUserInput(serverName, sizeof(serverName))) {
        WSACleanup();
        return 1;
    }

    // Resolve the server address
    iResult = getaddrinfo(serverName, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        // Print an error message if resolving the address fails
        printError("getaddrinfo");
        WSACleanup();
        return 1;
    }

    // Flag to track whether a connection is established
    bool connected = false;

    // Loop through the available address information and connect to the server
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Create a socket for connecting to the server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            // Print an error message if socket creation fails
            printError("socket");
            WSACleanup();
            return 1;
        }

        // Attempt to connect to the server
        iResult = connect(ConnectSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen));
        if (iResult == SOCKET_ERROR) {
            // Close the socket and try the next address if connection fails
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }

        // Connection successful, set the flag and break out of the loop
        connected = true;
        break;
    }

    // Free the address info structure
    freeaddrinfo(result);

    // Check if a connection was established
    if (!connected) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Main loop for file requesting and receiving
    do {
        // Get the file name from the user
        printf("Enter the name of the file to request (or type 'exit' to end): ");
        if (!getUserInput(receivedFilePath, sizeof(receivedFilePath))) {
            break;
        }

        // Check if the user wants to exit
        if (strcmp(receivedFilePath, "exit") == 0) {
            break;
        }

        // Send the file name to the server
        iResult = send(ConnectSocket, receivedFilePath, static_cast<int>(strlen(receivedFilePath)), 0);
        if (iResult == SOCKET_ERROR) {
            // Print an error message if sending the file name fails
            printf("send failed with error: %d\n", WSAGetLastError());
            break;
        }

        // Print a message indicating the file name was sent
        printf("File name '%s' sent to the server.\n", receivedFilePath);

        // Receive the file or an error message from the server
        if (receiveFileOrErrorMessage(ConnectSocket, receivedFilePath) == -1) {
            continue;
        }

        // Ask the user if they want to request another file
        printf("Do you want to request another file? (yes/no): ");
        char userChoice[10];
        if (!getUserInput(userChoice, sizeof(userChoice))) {
            break;
        }

        // Check if the user wants to exit the loop
        if (strcmp(userChoice, "no") == 0) {
            break;
        }

    } while (true);

    // Close the socket and clean up Winsock
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

