#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>

std::mutex coutMutex;

void manejarCliente(int clientSocket) {
    char buffer[1024] = {0};
    while (true) {
        ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRecibidos == 0) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Cliente desconectado." << std::endl;
            break;
        } else if (bytesRecibidos < 0) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Error al recibir datos" << std::endl;
            break;
        }

        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Mensaje del cliente: " << buffer << std::endl;
        memset(buffer, 0, sizeof(buffer));
    }
    close(clientSocket);
}

int main() {
    //Socket del servidor
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error al crear el socket del servidor: " << strerror(errno) << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(0);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error al vincular el socket: " << strerror(errno) << std::endl;
        close(serverSocket);
        return -1;
    }

    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(serverSocket, (struct sockaddr*)&addr, &len) == -1) {
        std::cerr << "Error al obtener el puerto asignado: " << strerror(errno) << std::endl;
        close(serverSocket);
        return -1;
    }

    std::cout << "Servidor iniciado en el puerto: " << ntohs(addr.sin_port) << std::endl;

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Error al escuchar en el socket: " << strerror(errno) << std::endl;
        close(serverSocket);
        return -1;
    }

    std::cout << "Esperando conexiones..." << std::endl;

    std::vector<std::thread> hilosClientes;

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "Error al aceptar la conexión: " << strerror(errno) << std::endl;
            continue;
        }

        std::cout << "Cliente conectado." << std::endl;

        hilosClientes.emplace_back(manejarCliente, clientSocket);
    }

    close(serverSocket);

    for (auto& hilo : hilosClientes) {
        if (hilo.joinable()) {
            hilo.join();
        }
    }

    return 0;
}
