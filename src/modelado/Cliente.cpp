#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    std::string host;
    int puerto;

    std::cout << "Ingresa la dirección del servidor: ";
    std::cin >> host;
    std::cout << "Ingresa el puerto: ";
    std::cin >> puerto;

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error al crear el socket del cliente" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << std::endl;
        close(clientSocket);
        return -1;
    }

    std::cout << "Conectado al servidor en " << host << ":" << puerto << std::endl;

    std::string mensaje;
    while (true) {
        std::cout << "Mensaje: (o '/exit' para salir): ";
        std::cin.ignore();
        std::getline(std::cin, mensaje);

        if (mensaje == "/exit") {
            break;
        }

        send(clientSocket, mensaje.c_str(), mensaje.length(), 0);
    }

    close(clientSocket);

    return 0;
}
