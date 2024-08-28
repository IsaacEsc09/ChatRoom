#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    //Socket del cliente
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error al crear el socket del cliente" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << std::endl;
        close(clientSocket);
        return -1;
    }

    std::cout << "Conectado al servidor." << std::endl;

    std::string mensaje;
    while (true) {
        std::cout << "Mensaje: (o '/exit' para salir): ";
        std::getline(std::cin, mensaje);

        if (mensaje == "/exit") {
            break;
        }

        send(clientSocket, mensaje.c_str(), mensaje.length(), 0);
    }

    close(clientSocket);

    return 0;
}
